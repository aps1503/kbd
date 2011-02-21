#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <unistd.h>		/* readlink */
#include "paths.h"
#include "getfd.h"
#include "findfile.h"
#include "modifiers.h"
#include "nls.h"
#include "version.h"
#include "kbd.h"

#include "loadkeys.actions.h"

void mktable(u_short *kmap[], char *func_table[]);
void bkeymap(u_short *kmap[]);

extern int prefer_unicode;
extern char func_buf[];
extern accent_entry accent_table[];
extern unsigned int accent_table_size;

/*
 * mktable.c
 *
 */
static char *modifiers[8] = {
    "shift", "altgr", "ctrl", "alt", "shl", "shr", "ctl", "ctr"
};

static char *mk_mapname(char modifier) {
    static char buf[60];
    int i;

    if (!modifier)
      return "plain";
    buf[0] = 0;
    for (i=0; i<8; i++)
      if (modifier & (1<<i)) {
	  if (buf[0])
	    strcat(buf, "_");
	  strcat(buf, modifiers[i]);
      }
    return buf;
}


static void
outchar (unsigned char c, int comma) {
        printf("'");
        printf((c == '\'' || c == '\\') ? "\\%c" : isgraph(c) ? "%c"
	       : "\\%03o", c);
	printf(comma ? "', " : "'");
}

void attr_noreturn
mktable (u_short *key_map[], char *func_table[]) {
	int j;
	unsigned int i, imax;

	char *ptr;
	unsigned int maxfunc;
	unsigned int keymap_count = 0;

	printf(
/* not to be translated... */
"/* Do not edit this file! It was automatically generated by   */\n");
	printf(
"/*    loadkeys --mktable defkeymap.map > defkeymap.c          */\n\n");
	printf("#include <linux/types.h>\n");
	printf("#include <linux/keyboard.h>\n");
	printf("#include <linux/kd.h>\n\n");

	for (i = 0; i < MAX_NR_KEYMAPS; i++)
	  if (key_map[i]) {
	      keymap_count++;
	      if (i)
		   printf("static ");
	      printf("u_short %s_map[NR_KEYS] = {", mk_mapname(i));
	      for (j = 0; j < NR_KEYS; j++) {
		  if (!(j % 8))
		    printf("\n");
		  printf("\t0x%04x,", U((key_map[i])[j]));
	      }
	      printf("\n};\n\n");
	  }

	for (imax = MAX_NR_KEYMAPS-1; imax > 0; imax--)
	  if (key_map[imax])
	    break;
	printf("ushort *key_maps[MAX_NR_KEYMAPS] = {");
	for (i = 0; i <= imax; i++) {
	    printf((i%4) ? " " : "\n\t");
	    if (key_map[i])
	      printf("%s_map,", mk_mapname(i));
	    else
	      printf("0,");
	}
	if (imax < MAX_NR_KEYMAPS-1)
	  printf("\t0");
	printf("\n};\n\nunsigned int keymap_count = %d;\n\n", keymap_count);

/* uglified just for xgettext - it complains about nonterminated strings */
	printf(
"/*\n"
" * Philosophy: most people do not define more strings, but they who do\n"
" * often want quite a lot of string space. So, we statically allocate\n"
" * the default and allocate dynamically in chunks of 512 bytes.\n"
" */\n"
"\n");
	for (maxfunc = MAX_NR_FUNC; maxfunc; maxfunc--)
	  if(func_table[maxfunc-1])
	    break;

	printf("char func_buf[] = {\n");
	for (i = 0; i < maxfunc; i++) {
	    ptr = func_table[i];
	    if (ptr) {
		printf("\t");
		for ( ; *ptr; ptr++)
		        outchar(*ptr, 1);
		printf("0, \n");
	    }
	}
	if (!maxfunc)
	  printf("\t0\n");
	printf("};\n\n");

	printf(
"char *funcbufptr = func_buf;\n"
"int funcbufsize = sizeof(func_buf);\n"
"int funcbufleft = 0;          /* space left */\n"
"\n");

	printf("char *func_table[MAX_NR_FUNC] = {\n");
	for (i = 0; i < maxfunc; i++) {
	    if (func_table[i])
	      printf("\tfunc_buf + %ld,\n", (long) (func_table[i] - func_buf));
	    else
	      printf("\t0,\n");
	}
	if (maxfunc < MAX_NR_FUNC)
	  printf("\t0,\n");
	printf("};\n");

#ifdef KDSKBDIACRUC
	if (prefer_unicode) {
		printf("\nstruct kbdiacruc accent_table[MAX_DIACR] = {\n");
		for (i = 0; i < accent_table_size; i++) {
			printf("\t{");
			outchar(accent_table[i].diacr, 1);
			outchar(accent_table[i].base, 1);
			printf("0x%04x},", accent_table[i].result);
			if(i%2) printf("\n");
		}
		if(i%2) printf("\n");
		printf("};\n\n");
	}
	else
#endif
	{
		printf("\nstruct kbdiacr accent_table[MAX_DIACR] = {\n");
		for (i = 0; i < accent_table_size; i++) {
			printf("\t{");
			outchar(accent_table[i].diacr, 1);
			outchar(accent_table[i].base, 1);
			outchar(accent_table[i].result, 0);
			printf("},");
			if(i%2) printf("\n");
		}
		if(i%2) printf("\n");
		printf("};\n\n");
	}
	printf("unsigned int accent_table_size = %d;\n",
	       accent_table_size);

	exit(0);
}

void attr_noreturn
bkeymap (u_short *key_map[]) {
	int i, j;

	//u_char *p;
	char flag, magic[] = "bkeymap";
	unsigned short v;

	if (write(1, magic, 7) == -1)
		goto fail;
	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		flag = key_map[i] ? 1 : 0;
		if (write(1, &flag, 1) == -1)
			goto fail;
	}
	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (key_map[i]) {
			for (j = 0; j < NR_KEYS / 2; j++) {
				v = key_map[i][j];
				if (write(1, &v, 2) == -1)
					goto fail;
			}
		}
	}
	exit(0);

fail:	fprintf(stderr, _("Error writing map to file\n"));
	exit(1);
}
