/************************************************************
 Copyright (c) 1996 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be
 used in advertising or publicity pertaining to distribution
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.

 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/

#include <stdio.h>
#include <ctype.h>

#include "rules.h"

#ifdef DEBUG
#define PR_DEBUG(s)		fprintf(stderr,s)
#define PR_DEBUG1(s,a)		fprintf(stderr,s,a)
#define PR_DEBUG2(s,a,b)	fprintf(stderr,s,a,b)
#else
#define PR_DEBUG(s)
#define PR_DEBUG1(s,a)
#define PR_DEBUG2(s,a,b)
#endif

/***====================================================================***/

#define DFLT_LINE_SIZE	128

typedef struct {
	int	line_num;
	int	sz_line;
	int	num_line;
	char	buf[DFLT_LINE_SIZE];
	char *	line;
} InputLine;

static void
InitInputLine(InputLine *line)
{
    line->line_num= 1;
    line->num_line= 0;
    line->sz_line= DFLT_LINE_SIZE;
    line->line=	line->buf;
}

static void
FreeInputLine(InputLine *line)
{
    if (line->line!=line->buf)
	free(line->line);
    line->line_num= 1;
    line->num_line= 0;
    line->sz_line= DFLT_LINE_SIZE;
    line->line= line->buf;
}

static int
InputLineAddChar(InputLine *line,int ch)
{
    if (line->num_line>=line->sz_line) {
	if (line->line==line->buf) {
            line->line = malloc(line->sz_line * 2);
	    memcpy(line->line,line->buf,line->sz_line);
	}
	else {
            line->line = realloc(line->line, line->sz_line * 2);
	}
	line->sz_line*= 2;
    }
    line->line[line->num_line++]= ch;
    return ch;
}

#define	ADD_CHAR(l,c)	((l)->num_line<(l)->sz_line?\
				(int)((l)->line[(l)->num_line++]= (c)):\
				InputLineAddChar(l,c))

static bool
GetInputLine(FILE *file,InputLine *line,bool checkbang)
{
     int ch;
     bool endOfFile,spacePending,slashPending,inComment;

     endOfFile= false;
     while ((!endOfFile)&&(line->num_line==0)) {
	spacePending= slashPending= inComment= false;
	while (((ch=getc(file))!='\n')&&(ch!=EOF)) {
	    if (ch=='\\') {
		if ((ch=getc(file))==EOF)
		    break;
		if (ch=='\n') {
		    inComment= false;
		    ch= ' ';
		    line->line_num++;
		}
	    }
	    if (inComment)
		continue;
	    if (ch=='/') {
		if (slashPending) {
		    inComment= true;
		    slashPending= false;
		}
		else {
		    slashPending= true;
		}
		continue;
	    }
	    else if (slashPending) {
		if (spacePending) {
		    ADD_CHAR(line,' ');
		    spacePending= false;
		}
		ADD_CHAR(line,'/');
		slashPending= false;
	    }
	    if (isspace(ch)) {
		while (isspace(ch)&&(ch!='\n')&&(ch!=EOF)) {
		    ch= getc(file);
		}
		if (ch==EOF)
		    break;
		if ((ch!='\n')&&(line->num_line>0))
		    spacePending= true;
		ungetc(ch,file);
	    }
	    else {
		if (spacePending) {
		    ADD_CHAR(line,' ');
		    spacePending= false;
		}
		if (checkbang && ch=='!') {
		    if (line->num_line!=0) {
			PR_DEBUG("The '!' legal only at start of line\n");
			PR_DEBUG("Line containing '!' ignored\n");
			line->num_line= 0;
			break;
		    }

		}
		ADD_CHAR(line,ch);
	    }
	}
	if (ch==EOF)
	     endOfFile= true;
/*	else line->num_line++;*/
     }
     if ((line->num_line==0)&&(endOfFile))
	return false;
      ADD_CHAR(line,'\0');
      return true;
}

/***====================================================================***/

#define	MODEL		0
#define	LAYOUT		1
#define	VARIANT		2
#define	OPTION		3
#define	KEYCODES	4
#define SYMBOLS		5
#define	TYPES		6
#define	COMPAT		7
#define	GEOMETRY	8
#define	KEYMAP		9
#define	MAX_WORDS	10

#define	PART_MASK	0x000F
#define	COMPONENT_MASK	0x03F0

static	const char * cname[MAX_WORDS] = {
	"model", "layout", "variant", "option",
	"keycodes", "symbols", "types", "compat", "geometry", "keymap"
};

typedef	struct _RemapSpec {
	int			number;
	int			num_remap;
	struct	{
		int	word;
		int	index;
                }		remap[MAX_WORDS];
} RemapSpec;

typedef struct _FileSpec {
	char *			name[MAX_WORDS];
	struct _FileSpec *	pending;
} FileSpec;

typedef struct {
	const char *		model;
	const char *		layout[XkbNumKbdGroups+1];
	const char *		variant[XkbNumKbdGroups+1];
	char *			options;
} XkbRF_MultiDefsRec, *XkbRF_MultiDefsPtr;

#define NDX_BUFF_SIZE	4

/***====================================================================***/

static char*
get_index(char *str, int *ndx)
{
   char ndx_buf[NDX_BUFF_SIZE];
   char *end;

   if (*str != '[') {
       *ndx = 0;
       return str;
   }
   str++;
   end = strchr(str, ']');
   if (end == NULL) {
       *ndx = -1;
       return str - 1;
   }
   if ( (end - str) >= NDX_BUFF_SIZE) {
       *ndx = -1;
       return end + 1;
   }
   strncpy(ndx_buf, str, end - str);
   ndx_buf[end - str] = '\0';
   *ndx = atoi(ndx_buf);
   return end + 1;
}

static void
SetUpRemap(InputLine *line,RemapSpec *remap)
{
   char *tok, *str;
   unsigned present, l_ndx_present, v_ndx_present;
   int i;
   size_t len;
   int ndx;
   char *strtok_buf;
#ifdef DEBUG
   bool found;
#endif


   l_ndx_present = v_ndx_present = present= 0;
   str= &line->line[1];
   len = remap->number;
   memset(remap, 0, sizeof(RemapSpec));
   remap->number = len;
   while ((tok = strtok_r(str, " ", &strtok_buf)) != NULL) {
#ifdef DEBUG
	found= false;
#endif
	str= NULL;
	if (strcmp(tok,"=")==0)
	    continue;
	for (i=0;i<MAX_WORDS;i++) {
            len = strlen(cname[i]);
	    if (strncmp(cname[i],tok,len)==0) {
		if(strlen(tok) > len) {
		    char *end = get_index(tok+len, &ndx);
		    if ((i != LAYOUT && i != VARIANT) ||
			*end != '\0' || ndx == -1)
		        break;
		     if (ndx < 1 || ndx > XkbNumKbdGroups) {
		        PR_DEBUG2("Illegal %s index: %d\n", cname[i], ndx);
		        PR_DEBUG1("Index must be in range 1..%d\n",
				   XkbNumKbdGroups);
			break;
		     }
                } else {
		    ndx = 0;
                }
#ifdef DEBUG
		found= true;
#endif
		if (present&(1<<i)) {
		    if ((i == LAYOUT && l_ndx_present&(1<<ndx)) ||
			(i == VARIANT && v_ndx_present&(1<<ndx)) ) {
		        PR_DEBUG1("Component \"%s\" listed twice\n",tok);
		        PR_DEBUG("Second definition ignored\n");
		        break;
		    }
		}
		present |= (1<<i);
                if (i == LAYOUT)
                    l_ndx_present |= 1 << ndx;
                if (i == VARIANT)
                    v_ndx_present |= 1 << ndx;
		remap->remap[remap->num_remap].word= i;
		remap->remap[remap->num_remap++].index= ndx;
		break;
	    }
	}
#ifdef DEBUG
	if (!found) {
	    fprintf(stderr,"Unknown component \"%s\" ignored\n",tok);
	}
#endif
   }
   if ((present&PART_MASK)==0) {
#ifdef DEBUG
	unsigned mask= PART_MASK;
	fprintf(stderr,"Mapping needs at least one of ");
	for (i=0; (i<MAX_WORDS); i++) {
	    if ((1L<<i)&mask) {
		mask&= ~(1L<<i);
		if (mask)	fprintf(stderr,"\"%s,\" ",cname[i]);
		else		fprintf(stderr,"or \"%s\"\n",cname[i]);
	    }
	}
	fprintf(stderr,"Illegal mapping ignored\n");
#endif
	remap->num_remap= 0;
	return;
   }
   if ((present&COMPONENT_MASK)==0) {
	PR_DEBUG("Mapping needs at least one component\n");
	PR_DEBUG("Illegal mapping ignored\n");
	remap->num_remap= 0;
	return;
   }
   if (((present&COMPONENT_MASK)&(1<<KEYMAP))&&
				((present&COMPONENT_MASK)!=(1<<KEYMAP))) {
	PR_DEBUG("Keymap cannot appear with other components\n");
	PR_DEBUG("Illegal mapping ignored\n");
	remap->num_remap= 0;
	return;
   }
   remap->number++;
}

static bool
MatchOneOf(char *wanted,char *vals_defined)
{
    char *str, *next;
    int want_len = strlen(wanted);

    for (str=vals_defined,next=NULL;str!=NULL;str=next) {
	int len;
	next= strchr(str,',');
	if (next) {
	    len= next-str;
	    next++;
	}
	else {
	    len= strlen(str);
	}
	if ((len==want_len)&&(strncmp(wanted,str,len)==0))
	    return true;
    }
    return false;
}

/***====================================================================***/

static bool
CheckLine(	InputLine *		line,
		RemapSpec *		remap,
		XkbRF_RulePtr		rule,
		XkbRF_GroupPtr		group)
{
    char *str, *tok;
    int nread, i;
    FileSpec tmp;
    char *strtok_buf;
    bool append = false;

    if (line->line[0]=='!') {
        if (line->line[1] == '$' ||
            (line->line[1] == ' ' && line->line[2] == '$')) {
            char *gname = strchr(line->line, '$');
            char *words = strchr(gname, ' ');
            if(!words)
                return false;
            *words++ = '\0';
            for (; *words; words++) {
                if (*words != '=' && *words != ' ')
                    break;
            }
            if (*words == '\0')
                return false;
            group->name = uDupString(gname);
            group->words = uDupString(words);
            for (i = 1, words = group->words; *words; words++) {
                 if ( *words == ' ') {
                     *words++ = '\0';
                     i++;
                 }
            }
            group->number = i;
            return true;
        } else {
	    SetUpRemap(line,remap);
	    return false;
        }
    }

    if (remap->num_remap==0) {
	PR_DEBUG("Must have a mapping before first line of data\n");
	PR_DEBUG("Illegal line of data ignored\n");
	return false;
    }
    memset(&tmp, 0, sizeof(FileSpec));
    str= line->line;
    for (nread = 0; (tok = strtok_r(str, " ", &strtok_buf)) != NULL; nread++) {
	str= NULL;
	if (strcmp(tok,"=")==0) {
	    nread--;
	    continue;
	}
	if (nread>remap->num_remap) {
	    PR_DEBUG("Too many words on a line\n");
	    PR_DEBUG1("Extra word \"%s\" ignored\n",tok);
	    continue;
	}
	tmp.name[remap->remap[nread].word]= tok;
	if (*tok == '+' || *tok == '|')
	    append = true;
    }
    if (nread<remap->num_remap) {
	PR_DEBUG1("Too few words on a line: %s\n", line->line);
	PR_DEBUG("line ignored\n");
	return false;
    }

    rule->flags= 0;
    rule->number = remap->number;
    if (tmp.name[OPTION])
	 rule->flags|= XkbRF_Option;
    else if (append)
	 rule->flags|= XkbRF_Append;
    else
	 rule->flags|= XkbRF_Normal;
    rule->model= uDupString(tmp.name[MODEL]);
    rule->layout= uDupString(tmp.name[LAYOUT]);
    rule->variant= uDupString(tmp.name[VARIANT]);
    rule->option= uDupString(tmp.name[OPTION]);

    rule->keycodes= uDupString(tmp.name[KEYCODES]);
    rule->symbols= uDupString(tmp.name[SYMBOLS]);
    rule->types= uDupString(tmp.name[TYPES]);
    rule->compat= uDupString(tmp.name[COMPAT]);
    rule->keymap= uDupString(tmp.name[KEYMAP]);

    rule->layout_num = rule->variant_num = 0;
    for (i = 0; i < nread; i++) {
        if (remap->remap[i].index) {
	    if (remap->remap[i].word == LAYOUT)
	        rule->layout_num = remap->remap[i].index;
	    if (remap->remap[i].word == VARIANT)
	        rule->variant_num = remap->remap[i].index;
        }
    }
    return true;
}

static char *
_Concat(char *str1,char *str2)
{
    int len;

    if ((!str1)||(!str2))
	return str1;
    len= strlen(str1)+strlen(str2)+1;
    str1 = uTypedRealloc(str1, len, char);
    if (str1)
	strcat(str1,str2);
    return str1;
}

static void
squeeze_spaces(char *p1)
{
   char *p2;
   for (p2 = p1; *p2; p2++) {
       *p1 = *p2;
       if (*p1 != ' ') p1++;
   }
   *p1 = '\0';
}

static bool
MakeMultiDefs(XkbRF_MultiDefsPtr mdefs, XkbRF_VarDefsPtr defs)
{
   memset(mdefs, 0, sizeof(XkbRF_MultiDefsRec));
   mdefs->model = defs->model;
   mdefs->options = uDupString(defs->options);
   if (mdefs->options) squeeze_spaces(mdefs->options);

   if (defs->layout) {
       if (!strchr(defs->layout, ',')) {
           mdefs->layout[0] = defs->layout;
       } else {
           char *p;
           int i;
           p = uDupString(defs->layout);
           if (p == NULL)
              return false;
           squeeze_spaces(p);
           mdefs->layout[1] = p;
           for (i = 2; i <= XkbNumKbdGroups; i++) {
              if ((p = strchr(p, ','))) {
                 *p++ = '\0';
                 mdefs->layout[i] = p;
              } else {
                 break;
              }
           }
           if (p && (p = strchr(p, ',')))
              *p = '\0';
       }
   }

   if (defs->variant) {
       if (!strchr(defs->variant, ',')) {
           mdefs->variant[0] = defs->variant;
       } else {
           char *p;
           int i;
           p = uDupString(defs->variant);
           if (p == NULL)
              return false;
           squeeze_spaces(p);
           mdefs->variant[1] = p;
           for (i = 2; i <= XkbNumKbdGroups; i++) {
              if ((p = strchr(p, ','))) {
                 *p++ = '\0';
                 mdefs->variant[i] = p;
              } else {
                 break;
              }
           }
           if (p && (p = strchr(p, ',')))
              *p = '\0';
       }
   }
   return true;
}

static void
FreeMultiDefs(XkbRF_MultiDefsPtr defs)
{
    free(defs->options);
    free(UNCONSTIFY(defs->layout[1]));
    free(UNCONSTIFY(defs->variant[1]));
}

static void
Apply(char *src, char **dst)
{
    if (src) {
        if (*src == '+' || *src == '!') {
	    *dst= _Concat(*dst, src);
        } else {
            if (*dst == NULL)
	        *dst= uDupString(src);
        }
    }
}

static void
XkbRF_ApplyRule(	XkbRF_RulePtr 		rule,
			struct xkb_component_names *	names)
{
    rule->flags&= ~XkbRF_PendingMatch; /* clear the flag because it's applied */

    Apply(rule->keycodes, &names->keycodes);
    Apply(rule->symbols,  &names->symbols);
    Apply(rule->types,    &names->types);
    Apply(rule->compat,   &names->compat);
    Apply(rule->keymap,   &names->keymap);
}

static bool
CheckGroup(	XkbRF_RulesPtr          rules,
		const char * 		group_name,
		const char * 		name)
{
   int i;
   const char *p;
   XkbRF_GroupPtr group;

   for (i = 0, group = rules->groups; i < rules->num_groups; i++, group++) {
       if (! strcmp(group->name, group_name)) {
           break;
       }
   }
   if (i == rules->num_groups)
       return false;
   for (i = 0, p = group->words; i < group->number; i++, p += strlen(p)+1) {
       if (! strcmp(p, name)) {
           return true;
       }
   }
   return false;
}

static int
XkbRF_CheckApplyRule(	XkbRF_RulePtr 		rule,
			XkbRF_MultiDefsPtr	mdefs,
			struct xkb_component_names *	names,
			XkbRF_RulesPtr          rules)
{
    bool pending = false;

    if (rule->model != NULL) {
        if(mdefs->model == NULL)
            return 0;
        if (strcmp(rule->model, "*") == 0) {
            pending = true;
        } else {
            if (rule->model[0] == '$') {
               if (!CheckGroup(rules, rule->model, mdefs->model))
                  return 0;
            } else {
	       if (strcmp(rule->model, mdefs->model) != 0)
	          return 0;
	    }
	}
    }
    if (rule->option != NULL) {
	if (mdefs->options == NULL)
	    return 0;
	if ((!MatchOneOf(rule->option,mdefs->options)))
	    return 0;
    }

    if (rule->layout != NULL) {
	if(mdefs->layout[rule->layout_num] == NULL ||
	   *mdefs->layout[rule->layout_num] == '\0')
	    return 0;
        if (strcmp(rule->layout, "*") == 0) {
            pending = true;
        } else {
            if (rule->layout[0] == '$') {
               if (!CheckGroup(rules, rule->layout,
                               mdefs->layout[rule->layout_num]))
                  return 0;
	    } else {
	       if (strcmp(rule->layout, mdefs->layout[rule->layout_num]) != 0)
	           return 0;
	    }
	}
    }
    if (rule->variant != NULL) {
	if (mdefs->variant[rule->variant_num] == NULL ||
	    *mdefs->variant[rule->variant_num] == '\0')
	    return 0;
        if (strcmp(rule->variant, "*") == 0) {
            pending = true;
        } else {
            if (rule->variant[0] == '$') {
               if (!CheckGroup(rules, rule->variant,
                               mdefs->variant[rule->variant_num]))
                  return 0;
            } else {
	       if (strcmp(rule->variant,
                          mdefs->variant[rule->variant_num]) != 0)
	           return 0;
	    }
	}
    }
    if (pending) {
        rule->flags|= XkbRF_PendingMatch;
	return rule->number;
    }
    /* exact match, apply it now */
    XkbRF_ApplyRule(rule,names);
    return rule->number;
}

static void
XkbRF_ClearPartialMatches(XkbRF_RulesPtr rules)
{
    int i;
    XkbRF_RulePtr rule;

    for (i=0,rule=rules->rules;i<rules->num_rules;i++,rule++) {
	rule->flags&= ~XkbRF_PendingMatch;
    }
}

static void
XkbRF_ApplyPartialMatches(XkbRF_RulesPtr rules,struct xkb_component_names * names)
{
    int i;
    XkbRF_RulePtr rule;

    for (rule = rules->rules, i = 0; i < rules->num_rules; i++, rule++) {
	if ((rule->flags&XkbRF_PendingMatch)==0)
	    continue;
	XkbRF_ApplyRule(rule,names);
    }
}

static void
XkbRF_CheckApplyRules(	XkbRF_RulesPtr 		rules,
			XkbRF_MultiDefsPtr	mdefs,
			struct xkb_component_names *	names,
			unsigned int			flags)
{
    int i;
    XkbRF_RulePtr rule;
    int skip;

    for (rule = rules->rules, i=0; i < rules->num_rules; rule++, i++) {
	if ((rule->flags & flags) != flags)
	    continue;
	skip = XkbRF_CheckApplyRule(rule, mdefs, names, rules);
	if (skip && !(flags & XkbRF_Option)) {
	    for ( ;(i < rules->num_rules) && (rule->number == skip);
		  rule++, i++);
	    rule--; i--;
	}
    }
}

/***====================================================================***/

static char *
XkbRF_SubstituteVars(char *name, XkbRF_MultiDefsPtr mdefs)
{
    char *str, *outstr, *orig, *var;
    size_t len;
    int ndx;

    orig= name;
    str= strchr(name,'%');
    if (str==NULL)
	return name;
    len= strlen(name);
    while (str!=NULL) {
	char pfx= str[1];
	int   extra_len= 0;
	if ((pfx=='+')||(pfx=='|')||(pfx=='_')||(pfx=='-')) {
	    extra_len= 1;
	    str++;
	}
	else if (pfx=='(') {
	    extra_len= 2;
	    str++;
	}
	var = str + 1;
	str = get_index(var + 1, &ndx);
	if (ndx == -1) {
	    str = strchr(str,'%');
	    continue;
        }
	if ((*var=='l') && mdefs->layout[ndx] && *mdefs->layout[ndx])
	    len+= strlen(mdefs->layout[ndx])+extra_len;
	else if ((*var=='m')&&mdefs->model)
	    len+= strlen(mdefs->model)+extra_len;
	else if ((*var=='v') && mdefs->variant[ndx] && *mdefs->variant[ndx])
	    len+= strlen(mdefs->variant[ndx])+extra_len;
	if ((pfx=='(')&&(*str==')')) {
	    str++;
	}
	str= strchr(&str[0],'%');
    }
    name = malloc(len + 1);
    str= orig;
    outstr= name;
    while (*str!='\0') {
	if (str[0]=='%') {
	    char pfx,sfx;
	    str++;
	    pfx= str[0];
	    sfx= '\0';
	    if ((pfx=='+')||(pfx=='|')||(pfx=='_')||(pfx=='-')) {
		str++;
	    }
	    else if (pfx=='(') {
		sfx= ')';
		str++;
	    }
	    else pfx= '\0';

	    var = str;
	    str = get_index(var + 1, &ndx);
	    if (ndx == -1) {
	        continue;
            }
	    if ((*var=='l') && mdefs->layout[ndx] && *mdefs->layout[ndx]) {
		if (pfx) *outstr++= pfx;
		strcpy(outstr,mdefs->layout[ndx]);
		outstr+= strlen(mdefs->layout[ndx]);
		if (sfx) *outstr++= sfx;
	    }
	    else if ((*var=='m')&&(mdefs->model)) {
		if (pfx) *outstr++= pfx;
		strcpy(outstr,mdefs->model);
		outstr+= strlen(mdefs->model);
		if (sfx) *outstr++= sfx;
	    }
	    else if ((*var=='v') && mdefs->variant[ndx] && *mdefs->variant[ndx]) {
		if (pfx) *outstr++= pfx;
		strcpy(outstr,mdefs->variant[ndx]);
		outstr+= strlen(mdefs->variant[ndx]);
		if (sfx) *outstr++= sfx;
	    }
	    if ((pfx=='(')&&(*str==')'))
		str++;
	}
	else {
	    *outstr++= *str++;
	}
    }
    *outstr++= '\0';
    if (orig!=name)
	free(orig);
    return name;
}

/***====================================================================***/

bool
XkbcRF_GetComponents(	XkbRF_RulesPtr		rules,
			XkbRF_VarDefsPtr	defs,
			struct xkb_component_names *	names)
{
    XkbRF_MultiDefsRec mdefs;

    MakeMultiDefs(&mdefs, defs);

    memset(names, 0, sizeof(struct xkb_component_names));
    XkbRF_ClearPartialMatches(rules);
    XkbRF_CheckApplyRules(rules, &mdefs, names, XkbRF_Normal);
    XkbRF_ApplyPartialMatches(rules, names);
    XkbRF_CheckApplyRules(rules, &mdefs, names, XkbRF_Append);
    XkbRF_ApplyPartialMatches(rules, names);
    XkbRF_CheckApplyRules(rules, &mdefs, names, XkbRF_Option);
    XkbRF_ApplyPartialMatches(rules, names);

    if (names->keycodes)
	names->keycodes= XkbRF_SubstituteVars(names->keycodes, &mdefs);
    if (names->symbols)
	names->symbols=	XkbRF_SubstituteVars(names->symbols, &mdefs);
    if (names->types)
	names->types= XkbRF_SubstituteVars(names->types, &mdefs);
    if (names->compat)
	names->compat= XkbRF_SubstituteVars(names->compat, &mdefs);
    if (names->keymap)
	names->keymap= XkbRF_SubstituteVars(names->keymap, &mdefs);

    FreeMultiDefs(&mdefs);
    return (names->keycodes && names->symbols && names->types &&
		names->compat) || names->keymap;
}

static XkbRF_RulePtr
XkbcRF_AddRule(XkbRF_RulesPtr	rules)
{
    if (rules->sz_rules<1) {
	rules->sz_rules= 16;
	rules->num_rules= 0;
	rules->rules= uTypedCalloc(rules->sz_rules,XkbRF_RuleRec);
    }
    else if (rules->num_rules>=rules->sz_rules) {
	rules->sz_rules*= 2;
	rules->rules= uTypedRealloc(rules->rules,rules->sz_rules,
							XkbRF_RuleRec);
    }
    if (!rules->rules) {
	rules->sz_rules= rules->num_rules= 0;
#ifdef DEBUG
	fprintf(stderr,"Allocation failure in XkbcRF_AddRule\n");
#endif
	return NULL;
    }
    memset(&rules->rules[rules->num_rules], 0, sizeof(XkbRF_RuleRec));
    return &rules->rules[rules->num_rules++];
}

static XkbRF_GroupPtr
XkbcRF_AddGroup(XkbRF_RulesPtr	rules)
{
    if (rules->sz_groups<1) {
	rules->sz_groups= 16;
	rules->num_groups= 0;
	rules->groups= uTypedCalloc(rules->sz_groups,XkbRF_GroupRec);
    }
    else if (rules->num_groups >= rules->sz_groups) {
	rules->sz_groups *= 2;
	rules->groups= uTypedRealloc(rules->groups,rules->sz_groups,
                                     XkbRF_GroupRec);
    }
    if (!rules->groups) {
	rules->sz_groups= rules->num_groups= 0;
	return NULL;
    }

    memset(&rules->groups[rules->num_groups], 0, sizeof(XkbRF_GroupRec));
    return &rules->groups[rules->num_groups++];
}

bool
XkbcRF_LoadRules(FILE *file, XkbRF_RulesPtr rules)
{
InputLine	line;
RemapSpec	remap;
XkbRF_RuleRec	trule,*rule;
XkbRF_GroupRec  tgroup,*group;

    if (!(rules && file))
        return false;
    memset(&remap, 0, sizeof(RemapSpec));
    memset(&tgroup, 0, sizeof(XkbRF_GroupRec));
    InitInputLine(&line);
    while (GetInputLine(file, &line, true)) {
	if (CheckLine(&line,&remap,&trule,&tgroup)) {
            if (tgroup.number) {
	        if ((group= XkbcRF_AddGroup(rules))!=NULL) {
		    *group= tgroup;
		    memset(&tgroup, 0, sizeof(XkbRF_GroupRec));
	        }
	    } else {
	        if ((rule= XkbcRF_AddRule(rules))!=NULL) {
		    *rule= trule;
		    memset(&trule, 0, sizeof(XkbRF_RuleRec));
	        }
	    }
	}
	line.num_line= 0;
    }
    FreeInputLine(&line);
    return true;
}

static void
XkbRF_ClearVarDescriptions(XkbRF_DescribeVarsPtr var)
{
    int i;

    for (i=0;i<var->num_desc;i++) {
	free(var->desc[i].name);
	free(var->desc[i].desc);
	var->desc[i].name= var->desc[i].desc= NULL;
    }
    free(var->desc);
    var->desc= NULL;
}

void
XkbcRF_Free(XkbRF_RulesPtr rules)
{
    int i;
    XkbRF_RulePtr rule;
    XkbRF_GroupPtr group;

    if (!rules)
	return;
    XkbRF_ClearVarDescriptions(&rules->models);
    XkbRF_ClearVarDescriptions(&rules->layouts);
    XkbRF_ClearVarDescriptions(&rules->variants);
    XkbRF_ClearVarDescriptions(&rules->options);
    if (rules->extra) {
	for (i = 0; i < rules->num_extra; i++) {
	    XkbRF_ClearVarDescriptions(&rules->extra[i]);
	}
	free(rules->extra);
    }
    for (i=0, rule = rules->rules; i < rules->num_rules && rules; i++, rule++) {
        free(rule->model);
        free(rule->layout);
        free(rule->variant);
        free(rule->option);
        free(rule->keycodes);
        free(rule->symbols);
        free(rule->types);
        free(rule->compat);
        free(rule->keymap);
    }
    free(rules->rules);

    for (i=0, group = rules->groups; i < rules->num_groups && group; i++, group++) {
        free(group->name);
        free(group->words);
    }
    free(rules->groups);

    free(rules);
}