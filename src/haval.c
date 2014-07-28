/* $Id: haval.c 227 2010-06-16 17:28:38Z tp $ */
/*
 * HAVAL implementation.
 *
 * The HAVAL reference paper is of questionable clarity with regards to
 * some details such as endianness of bits within a byte, bytes within
 * a 32-bit word, or the actual ordering of words within a stream of
 * words. This implementation has been made compatible with the reference
 * implementation available on: http://labs.calyptix.com/haval.php
 *
 * ==========================(LICENSE BEGIN)============================
 *
 * Copyright (c) 2007-2010  Projet RNRT SAPHIR
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ===========================(LICENSE END)=============================
 *
 * @author   Thomas Pornin <thomas.pornin@cryptolog.com>
 */

#include <stddef.h>
#include <string.h>

#include "sph_haval.h"

#ifdef __cplusplus
extern "C"{
#endif

#if SPH_SMALL_FOOTPRINT && !defined SPH_SMALL_FOOTPRINT_HAVAL
#define SPH_SMALL_FOOTPRINT_HAVAL   1
#endif

/*
 * Basic definition from the reference paper.
 *
#define F1(x6, x5, x4, x3, x2, x1, x0) \
	(((x1) & (x4)) ^ ((x2) & (x5)) ^ ((x3) & (x6)) ^ ((x0) & (x1)) ^ (x0))
 *
 */

#define F1(x6, x5, x4, x3, x2, x1, x0) \
	(((x1) & ((x0) ^ (x4))) ^ ((x2) & (x5)) ^ ((x3) & (x6)) ^ (x0))

/*
 * Basic definition from the reference paper.
 *
#define F2(x6, x5, x4, x3, x2, x1, x0) \
	(((x1) & (x2) & (x3)) ^ ((x2) & (x4) & (x5)) ^ ((x1) & (x2)) \
	^ ((x1) & (x4)) ^ ((x2) & (x6)) ^ ((x3) & (x5)) \
	^ ((x4) & (x5)) ^ ((x0) & (x2)) ^ (x0))
 *
 */

#define F2(x6, x5, x4, x3, x2, x1, x0) \
	(((x2) & (((x1) & ~(x3)) ^ ((x4) & (x5)) ^ (x6) ^ (x0))) \
	^ ((x4) & ((x1) ^ (x5))) ^ ((x3 & (x5)) ^ (x0)))

/*
 * Basic definition from the reference paper.
 *
#define F3(x6, x5, x4, x3, x2, x1, x0) \
	(((x1) & (x2) & (x3)) ^ ((x1) & (x4)) ^ ((x2) & (x5)) \
	^ ((x3) & (x6)) ^ ((x0) & (x3)) ^ (x0))
 *
 */

#define F3(x6, x5, x4, x3, x2, x1, x0) \
	(((x3) & (((x1) & (x2)) ^ (x6) ^ (x0))) \
	^ ((x1) & (x4)) ^ ((x2) & (x5)) ^ (x0))

/*
 * Basic definition from the reference paper.
 *
#define F4(x6, x5, x4, x3, x2, x1, x0) \
	(((x1) & (x2) & (x3)) ^ ((x2) & (x4) & (x5)) ^ ((x3) & (x4) & (x6)) \
	^ ((x1) & (x4)) ^ ((x2) & (x6)) ^ ((x3) & (x4)) ^ ((x3) & (x5)) \
	^ ((x3) & (x6)) ^ ((x4) & (x5)) ^ ((x4) & (x6)) ^ ((x0) & (x4)) ^ (x0))
 *
 */

#define F4(x6, x5, x4, x3, x2, x1, x0) \
	(((x3) & (((x1) & (x2)) ^ ((x4) | (x6)) ^ (x5))) \
	^ ((x4) & ((~(x2) & (x5)) ^ (x1) ^ (x6) ^ (x0))) \
	^ ((x2) & (x6)) ^ (x0))

/*
 * Basic definition from the reference paper.
 *
#define F5(x6, x5, x4, x3, x2, x1, x0) \
	(((x1) & (x4)) ^ ((x2) & (x5)) ^ ((x3) & (x6)) \
	^ ((x0) & (x1) & (x2) & (x3)) ^ ((x0) & (x5)) ^ (x0))
 *
 */

#define F5(x6, x5, x4, x3, x2, x1, x0) \
	(((x0) & ~(((x1) & (x2) & (x3)) ^ (x5))) \
	^ ((x1) & (x4)) ^ ((x2) & (x5)) ^ ((x3) & (x6)))

/*
 * The macros below integrate the phi() permutations, depending on the
 * pass and the total number of passes.
 */

#define FP3_1(x6, x5, x4, x3, x2, x1, x0) \
	F1(x1, x0, x3, x5, x6, x2, x4)
#define FP3_2(x6, x5, x4, x3, x2, x1, x0) \
	F2(x4, x2, x1, x0, x5, x3, x6)
#define FP3_3(x6, x5, x4, x3, x2, x1, x0) \
	F3(x6, x1, x2, x3, x4, x5, x0)

#define FP4_1(x6, x5, x4, x3, x2, x1, x0) \
	F1(x2, x6, x1, x4, x5, x3, x0)
#define FP4_2(x6, x5, x4, x3, x2, x1, x0) \
	F2(x3, x5, x2, x0, x1, x6, x4)
#define FP4_3(x6, x5, x4, x3, x2, x1, x0) \
	F3(x1, x4, x3, x6, x0, x2, x5)
#define FP4_4(x6, x5, x4, x3, x2, x1, x0) \
	F4(x6, x4, x0, x5, x2, x1, x3)

#define FP5_1(x6, x5, x4, x3, x2, x1, x0) \
	F1(x3, x4, x1, x0, x5, x2, x6)
#define FP5_2(x6, x5, x4, x3, x2, x1, x0) \
	F2(x6, x2, x1, x0, x3, x4, x5)
#define FP5_3(x6, x5, x4, x3, x2, x1, x0) \
	F3(x2, x6, x0, x4, x3, x1, x5)
#define FP5_4(x6, x5, x4, x3, x2, x1, x0) \
	F4(x1, x5, x3, x2, x0, x4, x6)
#define FP5_5(x6, x5, x4, x3, x2, x1, x0) \
	F5(x2, x5, x0, x6, x4, x3, x1)

/*
 * One step, for "n" passes, pass number "p" (1 <= p <= n), using
 * input word number "w" and step constant "c".
 */
#define STEP(n, p, x7, x6, x5, x4, x3, x2, x1, x0, w, c)  do { \
		sph_u32 t = FP ## n ## _ ## p(x6, x5, x4, x3, x2, x1, x0); \
		(x7) = SPH_T32(SPH_ROTR32(t, 7) + SPH_ROTR32((x7), 11) \
			+ (w) + (c)); \
	} while (0)

/*
 * PASSy(n, in) computes pass number "y", for a total of "n", using the
 * one-argument macro "in" to access input words. Current state is assumed
 * to be held in variables "s0" to "s7".
 */

#if SPH_SMALL_FOOTPRINT_HAVAL

#define PASS1(n, in)   do { \
		unsigned pass_count; \
		for (pass_count = 0; pass_count < 32; pass_count += 8) { \
			STEP(n, 1, s7, s6, s5, s4, s3, s2, s1, s0, \
				in(pass_count + 0), SPH_C32(0x00000000)); \
			STEP(n, 1, s6, s5, s4, s3, s2, s1, s0, s7, \
				in(pass_count + 1), SPH_C32(0x00000000)); \
			STEP(n, 1, s5, s4, s3, s2, s1, s0, s7, s6, \
				in(pass_count + 2), SPH_C32(0x00000000)); \
			STEP(n, 1, s4, s3, s2, s1, s0, s7, s6, s5, \
				in(pass_count + 3), SPH_C32(0x00000000)); \
			STEP(n, 1, s3, s2, s1, s0, s7, s6, s5, s4, \
				in(pass_count + 4), SPH_C32(0x00000000)); \
			STEP(n, 1, s2, s1, s0, s7, s6, s5, s4, s3, \
				in(pass_count + 5), SPH_C32(0x00000000)); \
			STEP(n, 1, s1, s0, s7, s6, s5, s4, s3, s2, \
				in(pass_count + 6), SPH_C32(0x00000000)); \
			STEP(n, 1, s0, s7, s6, s5, s4, s3, s2, s1, \
				in(pass_count + 7), SPH_C32(0x00000000)); \
   		} \
	} while (0)

#define PASSG(p, n, in)   do { \
		unsigned pass_count; \
		for (pass_count = 0; pass_count < 32; pass_count += 8) { \
			STEP(n, p, s7, s6, s5, s4, s3, s2, s1, s0, \
				in(MP ## p[pass_count + 0]), \
				RK ## p[pass_count + 0]); \
			STEP(n, p, s6, s5, s4, s3, s2, s1, s0, s7, \
				in(MP ## p[pass_count + 1]), \
				RK ## p[pass_count + 1]); \
			STEP(n, p, s5, s4, s3, s2, s1, s0, s7, s6, \
				in(MP ## p[pass_count + 2]), \
				RK ## p[pass_count + 2]); \
			STEP(n, p, s4, s3, s2, s1, s0, s7, s6, s5, \
				in(MP ## p[pass_count + 3]), \
				RK ## p[pass_count + 3]); \
			STEP(n, p, s3, s2, s1, s0, s7, s6, s5, s4, \
				in(MP ## p[pass_count + 4]), \
				RK ## p[pass_count + 4]); \
			STEP(n, p, s2, s1, s0, s7, s6, s5, s4, s3, \
				in(MP ## p[pass_count + 5]), \
				RK ## p[pass_count + 5]); \
			STEP(n, p, s1, s0, s7, s6, s5, s4, s3, s2, \
				in(MP ## p[pass_count + 6]), \
				RK ## p[pass_count + 6]); \
			STEP(n, p, s0, s7, s6, s5, s4, s3, s2, s1, \
				in(MP ## p[pass_count + 7]), \
				RK ## p[pass_count + 7]); \
   		} \
	} while (0)

#define PASS2(n, in)    PASSG(2, n, in)
#define PASS3(n, in)    PASSG(3, n, in)
#define PASS4(n, in)    PASSG(4, n, in)
#define PASS5(n, in)    PASSG(5, n, in)

static const unsigned MP2[224] = {
160  130   33  193   18   99   41  158
128    5   15   11   63  166  104  164
 26   77  114  206  215  202   86  137
129  146  205  122  201    8   66  165
 92  210  217   98   51  182   76   70
 55  190   40  118   58   62  107  148
106   79  200   39  170  136  191  115
144    9   61  141  167   78  110   95
185  138   14   65  172  171   25  123
 35  216   89  140  192   68   72  119
 96   16  155   88  153  133    7   47
 45  177   12  186  162    0  163   31
173   37  103  100  179  120   38  157
 50   59  220   73  211  196  188  150
214    6   28   74  112   24  198  117
154  194  208   29  168   81   21   75
 57  139  161   64  175   42  221  124
223   67   10  113   46  156   34   48
145  121   91   43  102   53   32    2
108  105  176  187  222   71  142  147
212   36  135   90  131  178  195  149
101  174  183  218  203   17   85   19
  4  213  209  109    1  197   87  111
 84  125   22   83   80  126   44   54
180  207  116   13   30   97   82   52
132   23  189  143  204    3  219  184
127   93   49   69   56  151  199   27
181  169   94   20  159   60  134  152
};

static const unsigned MP3[224] = {
29  194  175   18  192  206  152   39
217  159  131  195  179   68   51  193
  6   42  149  204  115   83  156   53
 93  164  113  166  202   79   85   26
138    0  100  223   80    5   76  126
128    3  190  157  178  108  135   97
 35  172  127  183   78  177  200  185
148  176   65   82  134  110   15    2
101  210  196  189   77  171  140   19
104  121   62  216  114   86   32  215
 21    7   48   66   84  219   95  136
162  123  163  188   56  124  142  191
122  197   31   25  130   33   70  165
 27  155  141   41  187   71  153  211
109   91  103   63   58  107   44  112
145  212   59  218    1  118   22   67
 92  173   50  158   55    9  221   46
111  203  133   37  146   23  169   81
137   64  117   87  147   40  125   45
174   69  161   17  214    8   54  139
 52  208   98  186   49   94   90  209
 13   99   88  167   20  143   75    4
151   11   73  150  119   61  199   16
 38   74   72   89  180  201  213   57
 30  144   36  168   96   47   34   24
207   28  102  222   10  105  154  132
170  184  160   60  116  205   12  181
182   14  120  198  220  106  129   43
};

static const unsigned MP4[224] = {
206  165   91   63  219    8  216   25
170  131   46  167   58    6   34  177
145   87   50  100   11   90   94  221
160  204  130  107  163  201  153   12
175  139  157   28  178   92   52  208
166  141  164  151  113  156  129  194
110   60   78    1  137  111    3   49
 95  124   77  172  190  209  185  199
109   88   18  186  116   81   21    0
212  136  211  173   40  159  115   27
202   31  140   80   13   20   93   38
 33  123  162   30  148   53  119   42
146  108  125  195   62   10   85   44
 96   55    2  138  205  117  192   72
 76   26  101  223    5   17  135  187
104  106   59  154   19   99   75  188
 51  176  171  103   79   47  222   36
 56   70   35  174   32  196   43   64
 73  198  120   82  215  217  134  184
220  132  193  191  168  149  180  210
  9   23  213  158   74   71   69  105
126   61  161  214   48  181   15   22
142  197   84  112   98   29  155  152
 54   66  128  207  114  133  102  121
 41  200    4   14   83  144    7  118
 24   89  143  127   16  147  218  189
179   37  169   39   67   65  182  203
 97   86   68   57  122  150  183   45
};

static const unsigned MP5[224] = {
54    5  116   67  184  222  177  148
129  146  107  181   23  208   34  157
189   10  126  210   18   91   32   87
 36   68  211   30  173  103   59   86
 29   84  205  131   64   60   35   80
142   13  128   11  112  155   27   62
220  160   74  164   39  162  109  166
 12  158   37  212  186  216  152   98
 82   70  197   72   24  219  102   83
198   76  207    0  200    4  100  168
176  214  111  104   65  171  124  203
 15  217   85  218  120  143  115  165
185  161  133   33  136  167   78  137
 61   46  101  150   48  149   44  193
175  117   38  172   73   94  106    8
206   51  202   92   53   69  183  119
108   26   90  169   43  141   31   96
127  199  182  223   75  154  213   41
 77  188   56   55   19  195  140  144
 50   99   89    1   21  145   16   66
123    9  147    3  215  151   93   63
  2   81   14   28  139   45   52   47
 40  163  110  190  221  156   57   71
196  132   42  201   97  178    6  130
179  122   95  187  153   79  121   49
 20  159  204  209   25  192  125  113
194  180  114   22   17  138  191  105
135   88  118  134   58    7  174  170
};

static const sph_u32 RK2[224] = {
	SPH_C32(0x452821E6), SPH_C32(0x38D01377), SPH_C32(0xBE5466CF), SPH_C32(0x34E90C6C),
	SPH_C32(0xC0AC29B7), SPH_C32(0xC97C50DD), SPH_C32(0x3F84D5B5), SPH_C32(0xB5470917),
	SPH_C32(0x9216D5D9), SPH_C32(0x8979FB1B), SPH_C32(0xD1310BA6), SPH_C32(0x98DFB5AC),
	SPH_C32(0x2FFD72DB), SPH_C32(0xD01ADFB7), SPH_C32(0xB8E1AFED), SPH_C32(0x6A267E96),
	SPH_C32(0xBA7C9045), SPH_C32(0xF12C7F99), SPH_C32(0x24A19947), SPH_C32(0xB3916CF7),
	SPH_C32(0x0801F2E2), SPH_C32(0x858EFC16), SPH_C32(0x636920D8), SPH_C32(0x71574E69),
	SPH_C32(0xA458FEA3), SPH_C32(0xF4933D7E), SPH_C32(0x0D95748F), SPH_C32(0x728EB658),
	SPH_C32(0x718BCD58), SPH_C32(0x82154AEE), SPH_C32(0x7B54A41D), SPH_C32(0xC25A59B5),
        SPH_C32(0x07fc703d), SPH_C32(0xd3fe381f), SPH_C32(0xb9ff1c0e), SPH_C32(0x5cff8e07), 
        SPH_C32(0xfe7fc702), SPH_C32(0x7f3fe381), SPH_C32(0xef9ff1c1), SPH_C32(0xa7cff8e1), 
        SPH_C32(0x83e7fc71), SPH_C32(0x91f3fe39), SPH_C32(0x98f9ff1d), SPH_C32(0x9c7cff8f), 
        SPH_C32(0x9e3e7fc6), SPH_C32(0x4f1f3fe3), SPH_C32(0xf78f9ff0), SPH_C32(0x7bc7cff8), 
        SPH_C32(0x3de3e7fc), SPH_C32(0x1ef1f3fe), SPH_C32(0x0f78f9ff), SPH_C32(0xd7bc7cfe), 
        SPH_C32(0x6bde3e7f), SPH_C32(0xe5ef1f3e), SPH_C32(0x72f78f9f), SPH_C32(0xe97bc7ce), 
        SPH_C32(0x74bde3e7), SPH_C32(0xea5ef1f2), SPH_C32(0x752f78f9), SPH_C32(0xea97bc7d), 
        SPH_C32(0xa54bde3f), SPH_C32(0x82a5ef1e), SPH_C32(0x4152f78f), SPH_C32(0xf0a97bc6), 
        SPH_C32(0x7854bde3), SPH_C32(0xec2a5ef0), SPH_C32(0x76152f78), SPH_C32(0x3b0a97bc), 
        SPH_C32(0x1d854bde), SPH_C32(0x0ec2a5ef), SPH_C32(0xd76152f6), SPH_C32(0x6bb0a97b), 
        SPH_C32(0xe5d854bc), SPH_C32(0x72ec2a5e), SPH_C32(0x3976152f), SPH_C32(0xccbb0a96), 
        SPH_C32(0x665d854b), SPH_C32(0xe32ec2a4), SPH_C32(0x71976152), SPH_C32(0x38cbb0a9), 
        SPH_C32(0xcc65d855), SPH_C32(0xb632ec2b), SPH_C32(0x8b197614), SPH_C32(0x458cbb0a), 
        SPH_C32(0x22c65d85), SPH_C32(0xc1632ec3), SPH_C32(0xb0b19760), SPH_C32(0x5858cbb0), 
        SPH_C32(0x2c2c65d8), SPH_C32(0x161632ec), SPH_C32(0x0b0b1976), SPH_C32(0x05858cbb), 
        SPH_C32(0xd2c2c65c), SPH_C32(0x6961632e), SPH_C32(0x34b0b197), SPH_C32(0xca5858ca), 
        SPH_C32(0x652c2c65), SPH_C32(0xe2961633), SPH_C32(0xa14b0b18), SPH_C32(0x50a5858c), 
        SPH_C32(0x2852c2c6), SPH_C32(0x14296163), SPH_C32(0xda14b0b0), SPH_C32(0x6d0a5858), 
        SPH_C32(0x36852c2c), SPH_C32(0x1b429616), SPH_C32(0x0da14b0b), SPH_C32(0xd6d0a584), 
        SPH_C32(0x6b6852c2), SPH_C32(0x35b42961), SPH_C32(0xcada14b1), SPH_C32(0xb56d0a59), 
        SPH_C32(0x8ab6852d), SPH_C32(0x955b4297), SPH_C32(0x9aada14a), SPH_C32(0x4d56d0a5), 
        SPH_C32(0xf6ab6853), SPH_C32(0xab55b428), SPH_C32(0x55aada14), SPH_C32(0x2ad56d0a), 
        SPH_C32(0x156ab685), SPH_C32(0xdab55b43), SPH_C32(0xbd5aada0), SPH_C32(0x5ead56d0), 
        SPH_C32(0x2f56ab68), SPH_C32(0x17ab55b4), SPH_C32(0x0bd5aada), SPH_C32(0x05ead56d), 
        SPH_C32(0xd2f56ab7), SPH_C32(0xb97ab55a), SPH_C32(0x5cbd5aad), SPH_C32(0xfe5ead57), 
        SPH_C32(0xaf2f56aa), SPH_C32(0x5797ab55), SPH_C32(0xfbcbd5ab), SPH_C32(0xade5ead4), 
        SPH_C32(0x56f2f56a), SPH_C32(0x2b797ab5), SPH_C32(0xc5bcbd5b), SPH_C32(0xb2de5eac), 
        SPH_C32(0x596f2f56), SPH_C32(0x2cb797ab), SPH_C32(0xc65bcbd4), SPH_C32(0x632de5ea), 
        SPH_C32(0x3196f2f5), SPH_C32(0xc8cb797b), SPH_C32(0xb465bcbc), SPH_C32(0x5a32de5e), 
        SPH_C32(0x2d196f2f), SPH_C32(0xc68cb796), SPH_C32(0x63465bcb), SPH_C32(0xe1a32de4), 
        SPH_C32(0x70d196f2), SPH_C32(0x3868cb79), SPH_C32(0xcc3465bd), SPH_C32(0xb61a32df), 
        SPH_C32(0x8b0d196e), SPH_C32(0x45868cb7), SPH_C32(0xf2c3465a), SPH_C32(0x7961a32d), 
        SPH_C32(0xecb0d197), SPH_C32(0xa65868ca), SPH_C32(0x532c3465), SPH_C32(0xf9961a33), 
        SPH_C32(0xaccb0d18), SPH_C32(0x5665868c), SPH_C32(0x2b32c346), SPH_C32(0x159961a3), 
        SPH_C32(0xdaccb0d0), SPH_C32(0x6d665868), SPH_C32(0x36b32c34), SPH_C32(0x1b59961a), 
        SPH_C32(0x0daccb0d), SPH_C32(0xd6d66587), SPH_C32(0xbb6b32c2), SPH_C32(0x5db59961), 
        SPH_C32(0xfedaccb1), SPH_C32(0xaf6d6659), SPH_C32(0x87b6b32d), SPH_C32(0x93db5997), 
        SPH_C32(0x99edacca), SPH_C32(0x4cf6d665), SPH_C32(0xf67b6b33), SPH_C32(0xab3db598), 
        SPH_C32(0x559edacc), SPH_C32(0x2acf6d66), SPH_C32(0x1567b6b3), SPH_C32(0xdab3db58), 
        SPH_C32(0x6d59edac), SPH_C32(0x36acf6d6), SPH_C32(0x1b567b6b), SPH_C32(0xddab3db4), 
        SPH_C32(0x6ed59eda), SPH_C32(0x376acf6d), SPH_C32(0xcbb567b7), SPH_C32(0xb5dab3da), 
        SPH_C32(0x5aed59ed), SPH_C32(0xfd76acf7), SPH_C32(0xaebb567a), SPH_C32(0x575dab3d), 
        SPH_C32(0xfbaed59f), SPH_C32(0xadd76ace), SPH_C32(0x56ebb567), SPH_C32(0xfb75dab2), 
        SPH_C32(0x7dbaed59), SPH_C32(0xeedd76ad), SPH_C32(0xa76ebb57), SPH_C32(0x83b75daa), 
        SPH_C32(0x41dbaed5), SPH_C32(0xf0edd76b), SPH_C32(0xa876ebb4), SPH_C32(0x543b75da), 
        SPH_C32(0x2a1dbaed), SPH_C32(0xc50edd77), SPH_C32(0xb2876eba), SPH_C32(0x5943b75d), 
        SPH_C32(0xfca1dbaf), SPH_C32(0xae50edd6), SPH_C32(0x572876eb), SPH_C32(0xfb943b74), 
        SPH_C32(0x7dca1dba), SPH_C32(0x3ee50edd), SPH_C32(0xcf72876f), SPH_C32(0xb7b943b6)
};

static const sph_u32 RK3[224] = {
	SPH_C32(0x9C30D539), SPH_C32(0x2AF26013), SPH_C32(0xC5D1B023), SPH_C32(0x286085F0),
	SPH_C32(0xCA417918), SPH_C32(0xB8DB38EF), SPH_C32(0x8E79DCB0), SPH_C32(0x603A180E),
	SPH_C32(0x6C9E0E8B), SPH_C32(0xB01E8A3E), SPH_C32(0xD71577C1), SPH_C32(0xBD314B27),
	SPH_C32(0x78AF2FDA), SPH_C32(0x55605C60), SPH_C32(0xE65525F3), SPH_C32(0xAA55AB94),
	SPH_C32(0x57489862), SPH_C32(0x63E81440), SPH_C32(0x55CA396A), SPH_C32(0x2AAB10B6),
	SPH_C32(0xB4CC5C34), SPH_C32(0x1141E8CE), SPH_C32(0xA15486AF), SPH_C32(0x7C72E993),
	SPH_C32(0xB3EE1411), SPH_C32(0x636FBC2A), SPH_C32(0x2BA9C55D), SPH_C32(0x741831F6),
	SPH_C32(0xCE5C3E16), SPH_C32(0x9B87931E), SPH_C32(0xAFD6BA33), SPH_C32(0x6C24CF5C),
        SPH_C32(0x5bdca1db), SPH_C32(0xfdee50ec), SPH_C32(0x7ef72876), SPH_C32(0x3f7b943b), 
        SPH_C32(0xcfbdca1c), SPH_C32(0x67dee50e), SPH_C32(0x33ef7287), SPH_C32(0xc9f7b942), 
        SPH_C32(0x64fbdca1), SPH_C32(0xe27dee51), SPH_C32(0xa13ef729), SPH_C32(0x809f7b95), 
        SPH_C32(0x904fbdcb), SPH_C32(0x9827dee4), SPH_C32(0x4c13ef72), SPH_C32(0x2609f7b9), 
        SPH_C32(0xc304fbdd), SPH_C32(0xb1827def), SPH_C32(0x88c13ef6), SPH_C32(0x44609f7b), 
        SPH_C32(0xf2304fbc), SPH_C32(0x791827de), SPH_C32(0x3c8c13ef), SPH_C32(0xce4609f6), 
        SPH_C32(0x672304fb), SPH_C32(0xe391827c), SPH_C32(0x71c8c13e), SPH_C32(0x38e4609f), 
        SPH_C32(0xcc72304e), SPH_C32(0x66391827), SPH_C32(0xe31c8c12), SPH_C32(0x718e4609), 
        SPH_C32(0xe8c72305), SPH_C32(0xa4639183), SPH_C32(0x8231c8c0), SPH_C32(0x4118e460), 
        SPH_C32(0x208c7230), SPH_C32(0x10463918), SPH_C32(0x08231c8c), SPH_C32(0x04118e46), 
        SPH_C32(0x0208c723), SPH_C32(0xd1046390), SPH_C32(0x688231c8), SPH_C32(0x344118e4), 
        SPH_C32(0x1a208c72), SPH_C32(0x0d104639), SPH_C32(0xd688231d), SPH_C32(0xbb44118f), 
        SPH_C32(0x8da208c6), SPH_C32(0x46d10463), SPH_C32(0xf3688230), SPH_C32(0x79b44118), 
        SPH_C32(0x3cda208c), SPH_C32(0x1e6d1046), SPH_C32(0x0f368823), SPH_C32(0xd79b4410), 
        SPH_C32(0x6bcda208), SPH_C32(0x35e6d104), SPH_C32(0x1af36882), SPH_C32(0x0d79b441), 
        SPH_C32(0xd6bcda21), SPH_C32(0xbb5e6d11), SPH_C32(0x8daf3689), SPH_C32(0x96d79b45), 
        SPH_C32(0x9b6bcda3), SPH_C32(0x9db5e6d0), SPH_C32(0x4edaf368), SPH_C32(0x276d79b4), 
        SPH_C32(0x13b6bcda), SPH_C32(0x09db5e6d), SPH_C32(0xd4edaf37), SPH_C32(0xba76d79a), 
        SPH_C32(0x5d3b6bcd), SPH_C32(0xfe9db5e7), SPH_C32(0xaf4edaf2), SPH_C32(0x57a76d79), 
        SPH_C32(0xfbd3b6bd), SPH_C32(0xade9db5f), SPH_C32(0x86f4edae), SPH_C32(0x437a76d7), 
        SPH_C32(0xf1bd3b6a), SPH_C32(0x78de9db5), SPH_C32(0xec6f4edb), SPH_C32(0xa637a76c), 
        SPH_C32(0x531bd3b6), SPH_C32(0x298de9db), SPH_C32(0xc4c6f4ec), SPH_C32(0x62637a76), 
        SPH_C32(0x3131bd3b), SPH_C32(0xc898de9c), SPH_C32(0x644c6f4e), SPH_C32(0x322637a7), 
        SPH_C32(0xc9131bd2), SPH_C32(0x64898de9), SPH_C32(0xe244c6f5), SPH_C32(0xa122637b), 
        SPH_C32(0x809131bc), SPH_C32(0x404898de), SPH_C32(0x20244c6f), SPH_C32(0xc0122636), 
        SPH_C32(0x6009131b), SPH_C32(0xe004898c), SPH_C32(0x700244c6), SPH_C32(0x38012263), 
        SPH_C32(0xcc009130), SPH_C32(0x66004898), SPH_C32(0x3300244c), SPH_C32(0x19801226), 
        SPH_C32(0x0cc00913), SPH_C32(0xd6600488), SPH_C32(0x6b300244), SPH_C32(0x35980122), 
        SPH_C32(0x1acc0091), SPH_C32(0xdd660049), SPH_C32(0xbeb30025), SPH_C32(0x8f598013), 
        SPH_C32(0x97acc008), SPH_C32(0x4bd66004), SPH_C32(0x25eb3002), SPH_C32(0x12f59801), 
        SPH_C32(0xd97acc01), SPH_C32(0xbcbd6601), SPH_C32(0x8e5eb301), SPH_C32(0x972f5981), 
        SPH_C32(0x9b97acc1), SPH_C32(0x9dcbd661), SPH_C32(0x9ee5eb31), SPH_C32(0x9f72f599), 
        SPH_C32(0x9fb97acd), SPH_C32(0x9fdcbd67), SPH_C32(0x9fee5eb2), SPH_C32(0x4ff72f59), 
        SPH_C32(0xf7fb97ad), SPH_C32(0xabfdcbd7), SPH_C32(0x85fee5ea), SPH_C32(0x42ff72f5), 
        SPH_C32(0xf17fb97b), SPH_C32(0xa8bfdcbc), SPH_C32(0x545fee5e), SPH_C32(0x2a2ff72f), 
        SPH_C32(0xc517fb96), SPH_C32(0x628bfdcb), SPH_C32(0xe145fee4), SPH_C32(0x70a2ff72), 
        SPH_C32(0x38517fb9), SPH_C32(0xcc28bfdd), SPH_C32(0xb6145fef), SPH_C32(0x8b0a2ff6), 
        SPH_C32(0x458517fb), SPH_C32(0xf2c28bfc), SPH_C32(0x796145fe), SPH_C32(0x3cb0a2ff), 
        SPH_C32(0xce58517e), SPH_C32(0x672c28bf), SPH_C32(0xe396145e), SPH_C32(0x71cb0a2f), 
        SPH_C32(0xe8e58516), SPH_C32(0x7472c28b), SPH_C32(0xea396144), SPH_C32(0x751cb0a2), 
        SPH_C32(0x3a8e5851), SPH_C32(0xcd472c29), SPH_C32(0xb6a39615), SPH_C32(0x8b51cb0b), 
        SPH_C32(0x95a8e584), SPH_C32(0x4ad472c2), SPH_C32(0x256a3961), SPH_C32(0xc2b51cb1), 
        SPH_C32(0xb15a8e59), SPH_C32(0x88ad472d), SPH_C32(0x9456a397), SPH_C32(0x9a2b51ca), 
        SPH_C32(0x4d15a8e5), SPH_C32(0xf68ad473), SPH_C32(0xab456a38), SPH_C32(0x55a2b51c), 
        SPH_C32(0x2ad15a8e), SPH_C32(0x1568ad47), SPH_C32(0xdab456a2), SPH_C32(0x6d5a2b51), 
        SPH_C32(0xe6ad15a9), SPH_C32(0xa3568ad5), SPH_C32(0x81ab456b), SPH_C32(0x90d5a2b4), 
        SPH_C32(0x486ad15a), SPH_C32(0x243568ad), SPH_C32(0xc21ab457), SPH_C32(0xb10d5a2a), 
        SPH_C32(0x5886ad15), SPH_C32(0xfc43568b), SPH_C32(0xae21ab44), SPH_C32(0x5710d5a2)
};

static const sph_u32 RK4[224] = {
	SPH_C32(0x7A325381), SPH_C32(0x28958677), SPH_C32(0x3B8F4898), SPH_C32(0x6B4BB9AF),
	SPH_C32(0xC4BFE81B), SPH_C32(0x66282193), SPH_C32(0x61D809CC), SPH_C32(0xFB21A991),
	SPH_C32(0x487CAC60), SPH_C32(0x5DEC8032), SPH_C32(0xEF845D5D), SPH_C32(0xE98575B1),
	SPH_C32(0xDC262302), SPH_C32(0xEB651B88), SPH_C32(0x23893E81), SPH_C32(0xD396ACC5),
	SPH_C32(0x0F6D6FF3), SPH_C32(0x83F44239), SPH_C32(0x2E0B4482), SPH_C32(0xA4842004),
	SPH_C32(0x69C8F04A), SPH_C32(0x9E1F9B5E), SPH_C32(0x21C66842), SPH_C32(0xF6E96C9A),
	SPH_C32(0x670C9C61), SPH_C32(0xABD388F0), SPH_C32(0x6A51A0D2), SPH_C32(0xD8542F68),
	SPH_C32(0x960FA728), SPH_C32(0xAB5133A3), SPH_C32(0x6EEF0B6C), SPH_C32(0x137A3BE4),
        SPH_C32(0x2b886ad1), SPH_C32(0xc5c43569), SPH_C32(0xb2e21ab5), SPH_C32(0x89710d5b), 
        SPH_C32(0x94b886ac), SPH_C32(0x4a5c4356), SPH_C32(0x252e21ab), SPH_C32(0xc29710d4), 
        SPH_C32(0x614b886a), SPH_C32(0x30a5c435), SPH_C32(0xc852e21b), SPH_C32(0xb429710c), 
        SPH_C32(0x5a14b886), SPH_C32(0x2d0a5c43), SPH_C32(0xc6852e20), SPH_C32(0x63429710), 
        SPH_C32(0x31a14b88), SPH_C32(0x18d0a5c4), SPH_C32(0x0c6852e2), SPH_C32(0x06342971), 
        SPH_C32(0xd31a14b9), SPH_C32(0xb98d0a5d), SPH_C32(0x8cc6852f), SPH_C32(0x96634296), 
        SPH_C32(0x4b31a14b), SPH_C32(0xf598d0a4), SPH_C32(0x7acc6852), SPH_C32(0x3d663429), 
        SPH_C32(0xceb31a15), SPH_C32(0xb7598d0b), SPH_C32(0x8bacc684), SPH_C32(0x45d66342), 
        SPH_C32(0x22eb31a1), SPH_C32(0xc17598d1), SPH_C32(0xb0bacc69), SPH_C32(0x885d6635), 
        SPH_C32(0x942eb31b), SPH_C32(0x9a17598c), SPH_C32(0x4d0bacc6), SPH_C32(0x2685d663), 
        SPH_C32(0xc342eb30), SPH_C32(0x61a17598), SPH_C32(0x30d0bacc), SPH_C32(0x18685d66), 
        SPH_C32(0x0c342eb3), SPH_C32(0xd61a1758), SPH_C32(0x6b0d0bac), SPH_C32(0x358685d6), 
        SPH_C32(0x1ac342eb), SPH_C32(0xdd61a174), SPH_C32(0x6eb0d0ba), SPH_C32(0x3758685d), 
        SPH_C32(0xcbac342f), SPH_C32(0xb5d61a16), SPH_C32(0x5aeb0d0b), SPH_C32(0xfd758684), 
        SPH_C32(0x7ebac342), SPH_C32(0x3f5d61a1), SPH_C32(0xcfaeb0d1), SPH_C32(0xb7d75869), 
        SPH_C32(0x8bebac35), SPH_C32(0x95f5d61b), SPH_C32(0x9afaeb0c), SPH_C32(0x4d7d7586), 
        SPH_C32(0x26bebac3), SPH_C32(0xc35f5d60), SPH_C32(0x61afaeb0), SPH_C32(0x30d7d758), 
        SPH_C32(0x186bebac), SPH_C32(0x0c35f5d6), SPH_C32(0x061afaeb), SPH_C32(0xd30d7d74), 
        SPH_C32(0x6986beba), SPH_C32(0x34c35f5d), SPH_C32(0xca61afaf), SPH_C32(0xb530d7d6), 
        SPH_C32(0x5a986beb), SPH_C32(0xfd4c35f4), SPH_C32(0x7ea61afa), SPH_C32(0x3f530d7d), 
        SPH_C32(0xcfa986bf), SPH_C32(0xb7d4c35e), SPH_C32(0x5bea61af), SPH_C32(0xfdf530d6), 
        SPH_C32(0x7efa986b), SPH_C32(0xef7d4c34), SPH_C32(0x77bea61a), SPH_C32(0x3bdf530d), 
        SPH_C32(0xcdefa987), SPH_C32(0xb6f7d4c2), SPH_C32(0x5b7bea61), SPH_C32(0xfdbdf531), 
        SPH_C32(0xaedefa99), SPH_C32(0x876f7d4d), SPH_C32(0x93b7bea7), SPH_C32(0x99dbdf52), 
        SPH_C32(0x4cedefa9), SPH_C32(0xf676f7d5), SPH_C32(0xab3b7beb), SPH_C32(0x859dbdf4), 
        SPH_C32(0x42cedefa), SPH_C32(0x21676f7d), SPH_C32(0xc0b3b7bf), SPH_C32(0xb059dbde), 
        SPH_C32(0x582cedef), SPH_C32(0xfc1676f6), SPH_C32(0x7e0b3b7b), SPH_C32(0xef059dbc), 
        SPH_C32(0x7782cede), SPH_C32(0x3bc1676f), SPH_C32(0xcde0b3b6), SPH_C32(0x66f059db), 
        SPH_C32(0xe3782cec), SPH_C32(0x71bc1676), SPH_C32(0x38de0b3b), SPH_C32(0xcc6f059c), 
        SPH_C32(0x663782ce), SPH_C32(0x331bc167), SPH_C32(0xc98de0b2), SPH_C32(0x64c6f059), 
        SPH_C32(0xe263782d), SPH_C32(0xa131bc17), SPH_C32(0x8098de0a), SPH_C32(0x404c6f05), 
        SPH_C32(0xf0263783), SPH_C32(0xa8131bc0), SPH_C32(0x54098de0), SPH_C32(0x2a04c6f0), 
        SPH_C32(0x15026378), SPH_C32(0x0a8131bc), SPH_C32(0x054098de), SPH_C32(0x02a04c6f), 
        SPH_C32(0xd1502636), SPH_C32(0x68a8131b), SPH_C32(0xe454098c), SPH_C32(0x722a04c6), 
        SPH_C32(0x39150263), SPH_C32(0xcc8a8130), SPH_C32(0x66454098), SPH_C32(0x3322a04c), 
        SPH_C32(0x19915026), SPH_C32(0x0cc8a813), SPH_C32(0xd6645408), SPH_C32(0x6b322a04), 
        SPH_C32(0x35991502), SPH_C32(0x1acc8a81), SPH_C32(0xdd664541), SPH_C32(0xbeb322a1), 
        SPH_C32(0x8f599151), SPH_C32(0x97acc8a9), SPH_C32(0x9bd66455), SPH_C32(0x9deb322b), 
        SPH_C32(0x9ef59914), SPH_C32(0x4f7acc8a), SPH_C32(0x27bd6645), SPH_C32(0xc3deb323), 
        SPH_C32(0xb1ef5990), SPH_C32(0x58f7acc8), SPH_C32(0x2c7bd664), SPH_C32(0x163deb32), 
        SPH_C32(0x0b1ef599), SPH_C32(0xd58f7acd), SPH_C32(0xbac7bd67), SPH_C32(0x8d63deb2), 
        SPH_C32(0x46b1ef59), SPH_C32(0xf358f7ad), SPH_C32(0xa9ac7bd7), SPH_C32(0x84d63dea), 
        SPH_C32(0x426b1ef5), SPH_C32(0xf1358f7b), SPH_C32(0xa89ac7bc), SPH_C32(0x544d63de), 
        SPH_C32(0x2a26b1ef), SPH_C32(0xc51358f6), SPH_C32(0x6289ac7b), SPH_C32(0xe144d63c), 
        SPH_C32(0x70a26b1e), SPH_C32(0x3851358f), SPH_C32(0xcc289ac6), SPH_C32(0x66144d63), 
        SPH_C32(0xe30a26b0), SPH_C32(0x71851358), SPH_C32(0x38c289ac), SPH_C32(0x1c6144d6), 
        SPH_C32(0x0e30a26b), SPH_C32(0xd7185134), SPH_C32(0x6b8c289a), SPH_C32(0x35c6144d), 
        SPH_C32(0xcae30a27), SPH_C32(0xb5718512), SPH_C32(0x5ab8c289), SPH_C32(0xfd5c6145)
};

static const sph_u32 RK5[224] = {
	SPH_C32(0xBA3BF050), SPH_C32(0x7EFB2A98), SPH_C32(0xA1F1651D), SPH_C32(0x39AF0176),
	SPH_C32(0x66CA593E), SPH_C32(0x82430E88), SPH_C32(0x8CEE8619), SPH_C32(0x456F9FB4),
	SPH_C32(0x7D84A5C3), SPH_C32(0x3B8B5EBE), SPH_C32(0xE06F75D8), SPH_C32(0x85C12073),
	SPH_C32(0x401A449F), SPH_C32(0x56C16AA6), SPH_C32(0x4ED3AA62), SPH_C32(0x363F7706),
	SPH_C32(0x1BFEDF72), SPH_C32(0x429B023D), SPH_C32(0x37D0D724), SPH_C32(0xD00A1248),
	SPH_C32(0xDB0FEAD3), SPH_C32(0x49F1C09B), SPH_C32(0x075372C9), SPH_C32(0x80991B7B),
	SPH_C32(0x25D479D8), SPH_C32(0xF6E8DEF7), SPH_C32(0xE3FE501A), SPH_C32(0xB6794C3B),
	SPH_C32(0x976CE0BD), SPH_C32(0x04C006BA), SPH_C32(0xC1A94FB6), SPH_C32(0x409F60C4),
        SPH_C32(0xaeae30a3), SPH_C32(0x87571850), SPH_C32(0x43ab8c28), SPH_C32(0x21d5c614), 
        SPH_C32(0x10eae30a), SPH_C32(0x08757185), SPH_C32(0xd43ab8c3), SPH_C32(0xba1d5c60), 
        SPH_C32(0x5d0eae30), SPH_C32(0x2e875718), SPH_C32(0x1743ab8c), SPH_C32(0x0ba1d5c6), 
        SPH_C32(0x05d0eae3), SPH_C32(0xd2e87570), SPH_C32(0x69743ab8), SPH_C32(0x34ba1d5c), 
        SPH_C32(0x1a5d0eae), SPH_C32(0x0d2e8757), SPH_C32(0xd69743aa), SPH_C32(0x6b4ba1d5), 
        SPH_C32(0xe5a5d0eb), SPH_C32(0xa2d2e874), SPH_C32(0x5169743a), SPH_C32(0x28b4ba1d), 
        SPH_C32(0xc45a5d0f), SPH_C32(0xb22d2e86), SPH_C32(0x59169743), SPH_C32(0xfc8b4ba0), 
        SPH_C32(0x7e45a5d0), SPH_C32(0x3f22d2e8), SPH_C32(0x1f916974), SPH_C32(0x0fc8b4ba), 
        SPH_C32(0x07e45a5d), SPH_C32(0xd3f22d2f), SPH_C32(0xb9f91696), SPH_C32(0x5cfc8b4b), 
        SPH_C32(0xfe7e45a4), SPH_C32(0x7f3f22d2), SPH_C32(0x3f9f9169), SPH_C32(0xcfcfc8b5), 
        SPH_C32(0xb7e7e45b), SPH_C32(0x8bf3f22c), SPH_C32(0x45f9f916), SPH_C32(0x22fcfc8b), 
        SPH_C32(0xc17e7e44), SPH_C32(0x60bf3f22), SPH_C32(0x305f9f91), SPH_C32(0xc82fcfc9), 
        SPH_C32(0xb417e7e5), SPH_C32(0x8a0bf3f3), SPH_C32(0x9505f9f8), SPH_C32(0x4a82fcfc), 
        SPH_C32(0x25417e7e), SPH_C32(0x12a0bf3f), SPH_C32(0xd9505f9e), SPH_C32(0x6ca82fcf), 
        SPH_C32(0xe65417e6), SPH_C32(0x732a0bf3), SPH_C32(0xe99505f8), SPH_C32(0x74ca82fc), 
        SPH_C32(0x3a65417e), SPH_C32(0x1d32a0bf), SPH_C32(0xde99505e), SPH_C32(0x6f4ca82f), 
        SPH_C32(0xe7a65416), SPH_C32(0x73d32a0b), SPH_C32(0xe9e99504), SPH_C32(0x74f4ca82), 
        SPH_C32(0x3a7a6541), SPH_C32(0xcd3d32a1), SPH_C32(0xb69e9951), SPH_C32(0x8b4f4ca9), 
        SPH_C32(0x95a7a655), SPH_C32(0x9ad3d32b), SPH_C32(0x9d69e994), SPH_C32(0x4eb4f4ca), 
        SPH_C32(0x275a7a65), SPH_C32(0xc3ad3d33), SPH_C32(0xb1d69e98), SPH_C32(0x58eb4f4c), 
        SPH_C32(0x2c75a7a6), SPH_C32(0x163ad3d3), SPH_C32(0xdb1d69e8), SPH_C32(0x6d8eb4f4), 
        SPH_C32(0x36c75a7a), SPH_C32(0x1b63ad3d), SPH_C32(0xddb1d69f), SPH_C32(0xbed8eb4e), 
        SPH_C32(0x5f6c75a7), SPH_C32(0xffb63ad2), SPH_C32(0x7fdb1d69), SPH_C32(0xefed8eb5), 
        SPH_C32(0xa7f6c75b), SPH_C32(0x83fb63ac), SPH_C32(0x41fdb1d6), SPH_C32(0x20fed8eb), 
        SPH_C32(0xc07f6c74), SPH_C32(0x603fb63a), SPH_C32(0x301fdb1d), SPH_C32(0xc80fed8f), 
        SPH_C32(0xb407f6c6), SPH_C32(0x5a03fb63), SPH_C32(0xfd01fdb0), SPH_C32(0x7e80fed8), 
        SPH_C32(0x3f407f6c), SPH_C32(0x1fa03fb6), SPH_C32(0x0fd01fdb), SPH_C32(0xd7e80fec), 
        SPH_C32(0x6bf407f6), SPH_C32(0x35fa03fb), SPH_C32(0xcafd01fc), SPH_C32(0x657e80fe), 
        SPH_C32(0x32bf407f), SPH_C32(0xc95fa03e), SPH_C32(0x64afd01f), SPH_C32(0xe257e80e), 
        SPH_C32(0x712bf407), SPH_C32(0xe895fa02), SPH_C32(0x744afd01), SPH_C32(0xea257e81), 
        SPH_C32(0xa512bf41), SPH_C32(0x82895fa1), SPH_C32(0x9144afd1), SPH_C32(0x98a257e9), 
        SPH_C32(0x9c512bf5), SPH_C32(0x9e2895fb), SPH_C32(0x9f144afc), SPH_C32(0x4f8a257e), 
        SPH_C32(0x27c512bf), SPH_C32(0xc3e2895e), SPH_C32(0x61f144af), SPH_C32(0xe0f8a256), 
        SPH_C32(0x707c512b), SPH_C32(0xe83e2894), SPH_C32(0x741f144a), SPH_C32(0x3a0f8a25), 
        SPH_C32(0xcd07c513), SPH_C32(0xb683e288), SPH_C32(0x5b41f144), SPH_C32(0x2da0f8a2), 
        SPH_C32(0x16d07c51), SPH_C32(0xdb683e29), SPH_C32(0xbdb41f15), SPH_C32(0x8eda0f8b), 
        SPH_C32(0x976d07c4), SPH_C32(0x4bb683e2), SPH_C32(0x25db41f1), SPH_C32(0xc2eda0f9), 
        SPH_C32(0xb176d07d), SPH_C32(0x88bb683f), SPH_C32(0x945db41e), SPH_C32(0x4a2eda0f), 
        SPH_C32(0xf5176d06), SPH_C32(0x7a8bb683), SPH_C32(0xed45db40), SPH_C32(0x76a2eda0), 
        SPH_C32(0x3b5176d0), SPH_C32(0x1da8bb68), SPH_C32(0x0ed45db4), SPH_C32(0x076a2eda), 
        SPH_C32(0x03b5176d), SPH_C32(0xd1da8bb7), SPH_C32(0xb8ed45da), SPH_C32(0x5c76a2ed), 
        SPH_C32(0xfe3b5177), SPH_C32(0xaf1da8ba), SPH_C32(0x578ed45d), SPH_C32(0xfbc76a2f), 
        SPH_C32(0xade3b516), SPH_C32(0x56f1da8b), SPH_C32(0xfb78ed44), SPH_C32(0x7dbc76a2), 
        SPH_C32(0x3ede3b51), SPH_C32(0xcf6f1da9), SPH_C32(0xb7b78ed5), SPH_C32(0x8bdbc76b), 
        SPH_C32(0x95ede3b4), SPH_C32(0x4af6f1da), SPH_C32(0x257b78ed), SPH_C32(0xc2bdbc77), 
        SPH_C32(0xb15ede3a), SPH_C32(0x58af6f1d), SPH_C32(0xfc57b78f), SPH_C32(0xae2bdbc6), 
        SPH_C32(0x5715ede3), SPH_C32(0xfb8af6f0), SPH_C32(0x7dc57b78), SPH_C32(0x3ee2bdbc), 
        SPH_C32(0x1f715ede), SPH_C32(0x0fb8af6f), SPH_C32(0xd7dc57b6), SPH_C32(0x6bee2bdb)
};

#else

#define PASS1(n, in)   do { \
   STEP(n, 1, s7, s6, s5, s4, s3, s2, s1, s0, in( 0), SPH_C32(0x00000000)); \
   STEP(n, 1, s6, s5, s4, s3, s2, s1, s0, s7, in( 1), SPH_C32(0x00000000)); \
   STEP(n, 1, s5, s4, s3, s2, s1, s0, s7, s6, in( 2), SPH_C32(0x00000000)); \
   STEP(n, 1, s4, s3, s2, s1, s0, s7, s6, s5, in( 3), SPH_C32(0x00000000)); \
   STEP(n, 1, s3, s2, s1, s0, s7, s6, s5, s4, in( 4), SPH_C32(0x00000000)); \
   STEP(n, 1, s2, s1, s0, s7, s6, s5, s4, s3, in( 5), SPH_C32(0x00000000)); \
   STEP(n, 1, s1, s0, s7, s6, s5, s4, s3, s2, in( 6), SPH_C32(0x00000000)); \
   STEP(n, 1, s0, s7, s6, s5, s4, s3, s2, s1, in( 7), SPH_C32(0x00000000)); \
 \
   STEP(n, 1, s7, s6, s5, s4, s3, s2, s1, s0, in( 8), SPH_C32(0x00000000)); \
   STEP(n, 1, s6, s5, s4, s3, s2, s1, s0, s7, in( 9), SPH_C32(0x00000000)); \
   STEP(n, 1, s5, s4, s3, s2, s1, s0, s7, s6, in(10), SPH_C32(0x00000000)); \
   STEP(n, 1, s4, s3, s2, s1, s0, s7, s6, s5, in(11), SPH_C32(0x00000000)); \
   STEP(n, 1, s3, s2, s1, s0, s7, s6, s5, s4, in(12), SPH_C32(0x00000000)); \
   STEP(n, 1, s2, s1, s0, s7, s6, s5, s4, s3, in(13), SPH_C32(0x00000000)); \
   STEP(n, 1, s1, s0, s7, s6, s5, s4, s3, s2, in(14), SPH_C32(0x00000000)); \
   STEP(n, 1, s0, s7, s6, s5, s4, s3, s2, s1, in(15), SPH_C32(0x00000000)); \
 \
   STEP(n, 1, s7, s6, s5, s4, s3, s2, s1, s0, in(16), SPH_C32(0x00000000)); \
   STEP(n, 1, s6, s5, s4, s3, s2, s1, s0, s7, in(17), SPH_C32(0x00000000)); \
   STEP(n, 1, s5, s4, s3, s2, s1, s0, s7, s6, in(18), SPH_C32(0x00000000)); \
   STEP(n, 1, s4, s3, s2, s1, s0, s7, s6, s5, in(19), SPH_C32(0x00000000)); \
   STEP(n, 1, s3, s2, s1, s0, s7, s6, s5, s4, in(20), SPH_C32(0x00000000)); \
   STEP(n, 1, s2, s1, s0, s7, s6, s5, s4, s3, in(21), SPH_C32(0x00000000)); \
   STEP(n, 1, s1, s0, s7, s6, s5, s4, s3, s2, in(22), SPH_C32(0x00000000)); \
   STEP(n, 1, s0, s7, s6, s5, s4, s3, s2, s1, in(23), SPH_C32(0x00000000)); \
 \
   STEP(n, 1, s7, s6, s5, s4, s3, s2, s1, s0, in(24), SPH_C32(0x00000000)); \
   STEP(n, 1, s6, s5, s4, s3, s2, s1, s0, s7, in(25), SPH_C32(0x00000000)); \
   STEP(n, 1, s5, s4, s3, s2, s1, s0, s7, s6, in(26), SPH_C32(0x00000000)); \
   STEP(n, 1, s4, s3, s2, s1, s0, s7, s6, s5, in(27), SPH_C32(0x00000000)); \
   STEP(n, 1, s3, s2, s1, s0, s7, s6, s5, s4, in(28), SPH_C32(0x00000000)); \
   STEP(n, 1, s2, s1, s0, s7, s6, s5, s4, s3, in(29), SPH_C32(0x00000000)); \
   STEP(n, 1, s1, s0, s7, s6, s5, s4, s3, s2, in(30), SPH_C32(0x00000000)); \
   STEP(n, 1, s0, s7, s6, s5, s4, s3, s2, s1, in(31), SPH_C32(0x00000000)); \
	} while (0)

#define PASS2(n, in)   do { \
   STEP(n, 2, s7, s6, s5, s4, s3, s2, s1, s0, in( 5), SPH_C32(0x452821E6)); \
   STEP(n, 2, s6, s5, s4, s3, s2, s1, s0, s7, in(14), SPH_C32(0x38D01377)); \
   STEP(n, 2, s5, s4, s3, s2, s1, s0, s7, s6, in(26), SPH_C32(0xBE5466CF)); \
   STEP(n, 2, s4, s3, s2, s1, s0, s7, s6, s5, in(18), SPH_C32(0x34E90C6C)); \
   STEP(n, 2, s3, s2, s1, s0, s7, s6, s5, s4, in(11), SPH_C32(0xC0AC29B7)); \
   STEP(n, 2, s2, s1, s0, s7, s6, s5, s4, s3, in(28), SPH_C32(0xC97C50DD)); \
   STEP(n, 2, s1, s0, s7, s6, s5, s4, s3, s2, in( 7), SPH_C32(0x3F84D5B5)); \
   STEP(n, 2, s0, s7, s6, s5, s4, s3, s2, s1, in(16), SPH_C32(0xB5470917)); \
 \
   STEP(n, 2, s7, s6, s5, s4, s3, s2, s1, s0, in( 0), SPH_C32(0x9216D5D9)); \
   STEP(n, 2, s6, s5, s4, s3, s2, s1, s0, s7, in(23), SPH_C32(0x8979FB1B)); \
   STEP(n, 2, s5, s4, s3, s2, s1, s0, s7, s6, in(20), SPH_C32(0xD1310BA6)); \
   STEP(n, 2, s4, s3, s2, s1, s0, s7, s6, s5, in(22), SPH_C32(0x98DFB5AC)); \
   STEP(n, 2, s3, s2, s1, s0, s7, s6, s5, s4, in( 1), SPH_C32(0x2FFD72DB)); \
   STEP(n, 2, s2, s1, s0, s7, s6, s5, s4, s3, in(10), SPH_C32(0xD01ADFB7)); \
   STEP(n, 2, s1, s0, s7, s6, s5, s4, s3, s2, in( 4), SPH_C32(0xB8E1AFED)); \
   STEP(n, 2, s0, s7, s6, s5, s4, s3, s2, s1, in( 8), SPH_C32(0x6A267E96)); \
 \
   STEP(n, 2, s7, s6, s5, s4, s3, s2, s1, s0, in(30), SPH_C32(0xBA7C9045)); \
   STEP(n, 2, s6, s5, s4, s3, s2, s1, s0, s7, in( 3), SPH_C32(0xF12C7F99)); \
   STEP(n, 2, s5, s4, s3, s2, s1, s0, s7, s6, in(21), SPH_C32(0x24A19947)); \
   STEP(n, 2, s4, s3, s2, s1, s0, s7, s6, s5, in( 9), SPH_C32(0xB3916CF7)); \
   STEP(n, 2, s3, s2, s1, s0, s7, s6, s5, s4, in(17), SPH_C32(0x0801F2E2)); \
   STEP(n, 2, s2, s1, s0, s7, s6, s5, s4, s3, in(24), SPH_C32(0x858EFC16)); \
   STEP(n, 2, s1, s0, s7, s6, s5, s4, s3, s2, in(29), SPH_C32(0x636920D8)); \
   STEP(n, 2, s0, s7, s6, s5, s4, s3, s2, s1, in( 6), SPH_C32(0x71574E69)); \
 \
   STEP(n, 2, s7, s6, s5, s4, s3, s2, s1, s0, in(19), SPH_C32(0xA458FEA3)); \
   STEP(n, 2, s6, s5, s4, s3, s2, s1, s0, s7, in(12), SPH_C32(0xF4933D7E)); \
   STEP(n, 2, s5, s4, s3, s2, s1, s0, s7, s6, in(15), SPH_C32(0x0D95748F)); \
   STEP(n, 2, s4, s3, s2, s1, s0, s7, s6, s5, in(13), SPH_C32(0x728EB658)); \
   STEP(n, 2, s3, s2, s1, s0, s7, s6, s5, s4, in( 2), SPH_C32(0x718BCD58)); \
   STEP(n, 2, s2, s1, s0, s7, s6, s5, s4, s3, in(25), SPH_C32(0x82154AEE)); \
   STEP(n, 2, s1, s0, s7, s6, s5, s4, s3, s2, in(31), SPH_C32(0x7B54A41D)); \
   STEP(n, 2, s0, s7, s6, s5, s4, s3, s2, s1, in(27), SPH_C32(0xC25A59B5)); \
	} while (0)

#define PASS3(n, in)   do { \
   STEP(n, 3, s7, s6, s5, s4, s3, s2, s1, s0, in(19), SPH_C32(0x9C30D539)); \
   STEP(n, 3, s6, s5, s4, s3, s2, s1, s0, s7, in( 9), SPH_C32(0x2AF26013)); \
   STEP(n, 3, s5, s4, s3, s2, s1, s0, s7, s6, in( 4), SPH_C32(0xC5D1B023)); \
   STEP(n, 3, s4, s3, s2, s1, s0, s7, s6, s5, in(20), SPH_C32(0x286085F0)); \
   STEP(n, 3, s3, s2, s1, s0, s7, s6, s5, s4, in(28), SPH_C32(0xCA417918)); \
   STEP(n, 3, s2, s1, s0, s7, s6, s5, s4, s3, in(17), SPH_C32(0xB8DB38EF)); \
   STEP(n, 3, s1, s0, s7, s6, s5, s4, s3, s2, in( 8), SPH_C32(0x8E79DCB0)); \
   STEP(n, 3, s0, s7, s6, s5, s4, s3, s2, s1, in(22), SPH_C32(0x603A180E)); \
 \
   STEP(n, 3, s7, s6, s5, s4, s3, s2, s1, s0, in(29), SPH_C32(0x6C9E0E8B)); \
   STEP(n, 3, s6, s5, s4, s3, s2, s1, s0, s7, in(14), SPH_C32(0xB01E8A3E)); \
   STEP(n, 3, s5, s4, s3, s2, s1, s0, s7, s6, in(25), SPH_C32(0xD71577C1)); \
   STEP(n, 3, s4, s3, s2, s1, s0, s7, s6, s5, in(12), SPH_C32(0xBD314B27)); \
   STEP(n, 3, s3, s2, s1, s0, s7, s6, s5, s4, in(24), SPH_C32(0x78AF2FDA)); \
   STEP(n, 3, s2, s1, s0, s7, s6, s5, s4, s3, in(30), SPH_C32(0x55605C60)); \
   STEP(n, 3, s1, s0, s7, s6, s5, s4, s3, s2, in(16), SPH_C32(0xE65525F3)); \
   STEP(n, 3, s0, s7, s6, s5, s4, s3, s2, s1, in(26), SPH_C32(0xAA55AB94)); \
 \
   STEP(n, 3, s7, s6, s5, s4, s3, s2, s1, s0, in(31), SPH_C32(0x57489862)); \
   STEP(n, 3, s6, s5, s4, s3, s2, s1, s0, s7, in(15), SPH_C32(0x63E81440)); \
   STEP(n, 3, s5, s4, s3, s2, s1, s0, s7, s6, in( 7), SPH_C32(0x55CA396A)); \
   STEP(n, 3, s4, s3, s2, s1, s0, s7, s6, s5, in( 3), SPH_C32(0x2AAB10B6)); \
   STEP(n, 3, s3, s2, s1, s0, s7, s6, s5, s4, in( 1), SPH_C32(0xB4CC5C34)); \
   STEP(n, 3, s2, s1, s0, s7, s6, s5, s4, s3, in( 0), SPH_C32(0x1141E8CE)); \
   STEP(n, 3, s1, s0, s7, s6, s5, s4, s3, s2, in(18), SPH_C32(0xA15486AF)); \
   STEP(n, 3, s0, s7, s6, s5, s4, s3, s2, s1, in(27), SPH_C32(0x7C72E993)); \
 \
   STEP(n, 3, s7, s6, s5, s4, s3, s2, s1, s0, in(13), SPH_C32(0xB3EE1411)); \
   STEP(n, 3, s6, s5, s4, s3, s2, s1, s0, s7, in( 6), SPH_C32(0x636FBC2A)); \
   STEP(n, 3, s5, s4, s3, s2, s1, s0, s7, s6, in(21), SPH_C32(0x2BA9C55D)); \
   STEP(n, 3, s4, s3, s2, s1, s0, s7, s6, s5, in(10), SPH_C32(0x741831F6)); \
   STEP(n, 3, s3, s2, s1, s0, s7, s6, s5, s4, in(23), SPH_C32(0xCE5C3E16)); \
   STEP(n, 3, s2, s1, s0, s7, s6, s5, s4, s3, in(11), SPH_C32(0x9B87931E)); \
   STEP(n, 3, s1, s0, s7, s6, s5, s4, s3, s2, in( 5), SPH_C32(0xAFD6BA33)); \
   STEP(n, 3, s0, s7, s6, s5, s4, s3, s2, s1, in( 2), SPH_C32(0x6C24CF5C)); \
	} while (0)

#define PASS4(n, in)   do { \
   STEP(n, 4, s7, s6, s5, s4, s3, s2, s1, s0, in(24), SPH_C32(0x7A325381)); \
   STEP(n, 4, s6, s5, s4, s3, s2, s1, s0, s7, in( 4), SPH_C32(0x28958677)); \
   STEP(n, 4, s5, s4, s3, s2, s1, s0, s7, s6, in( 0), SPH_C32(0x3B8F4898)); \
   STEP(n, 4, s4, s3, s2, s1, s0, s7, s6, s5, in(14), SPH_C32(0x6B4BB9AF)); \
   STEP(n, 4, s3, s2, s1, s0, s7, s6, s5, s4, in( 2), SPH_C32(0xC4BFE81B)); \
   STEP(n, 4, s2, s1, s0, s7, s6, s5, s4, s3, in( 7), SPH_C32(0x66282193)); \
   STEP(n, 4, s1, s0, s7, s6, s5, s4, s3, s2, in(28), SPH_C32(0x61D809CC)); \
   STEP(n, 4, s0, s7, s6, s5, s4, s3, s2, s1, in(23), SPH_C32(0xFB21A991)); \
 \
   STEP(n, 4, s7, s6, s5, s4, s3, s2, s1, s0, in(26), SPH_C32(0x487CAC60)); \
   STEP(n, 4, s6, s5, s4, s3, s2, s1, s0, s7, in( 6), SPH_C32(0x5DEC8032)); \
   STEP(n, 4, s5, s4, s3, s2, s1, s0, s7, s6, in(30), SPH_C32(0xEF845D5D)); \
   STEP(n, 4, s4, s3, s2, s1, s0, s7, s6, s5, in(20), SPH_C32(0xE98575B1)); \
   STEP(n, 4, s3, s2, s1, s0, s7, s6, s5, s4, in(18), SPH_C32(0xDC262302)); \
   STEP(n, 4, s2, s1, s0, s7, s6, s5, s4, s3, in(25), SPH_C32(0xEB651B88)); \
   STEP(n, 4, s1, s0, s7, s6, s5, s4, s3, s2, in(19), SPH_C32(0x23893E81)); \
   STEP(n, 4, s0, s7, s6, s5, s4, s3, s2, s1, in( 3), SPH_C32(0xD396ACC5)); \
 \
   STEP(n, 4, s7, s6, s5, s4, s3, s2, s1, s0, in(22), SPH_C32(0x0F6D6FF3)); \
   STEP(n, 4, s6, s5, s4, s3, s2, s1, s0, s7, in(11), SPH_C32(0x83F44239)); \
   STEP(n, 4, s5, s4, s3, s2, s1, s0, s7, s6, in(31), SPH_C32(0x2E0B4482)); \
   STEP(n, 4, s4, s3, s2, s1, s0, s7, s6, s5, in(21), SPH_C32(0xA4842004)); \
   STEP(n, 4, s3, s2, s1, s0, s7, s6, s5, s4, in( 8), SPH_C32(0x69C8F04A)); \
   STEP(n, 4, s2, s1, s0, s7, s6, s5, s4, s3, in(27), SPH_C32(0x9E1F9B5E)); \
   STEP(n, 4, s1, s0, s7, s6, s5, s4, s3, s2, in(12), SPH_C32(0x21C66842)); \
   STEP(n, 4, s0, s7, s6, s5, s4, s3, s2, s1, in( 9), SPH_C32(0xF6E96C9A)); \
 \
   STEP(n, 4, s7, s6, s5, s4, s3, s2, s1, s0, in( 1), SPH_C32(0x670C9C61)); \
   STEP(n, 4, s6, s5, s4, s3, s2, s1, s0, s7, in(29), SPH_C32(0xABD388F0)); \
   STEP(n, 4, s5, s4, s3, s2, s1, s0, s7, s6, in( 5), SPH_C32(0x6A51A0D2)); \
   STEP(n, 4, s4, s3, s2, s1, s0, s7, s6, s5, in(15), SPH_C32(0xD8542F68)); \
   STEP(n, 4, s3, s2, s1, s0, s7, s6, s5, s4, in(17), SPH_C32(0x960FA728)); \
   STEP(n, 4, s2, s1, s0, s7, s6, s5, s4, s3, in(10), SPH_C32(0xAB5133A3)); \
   STEP(n, 4, s1, s0, s7, s6, s5, s4, s3, s2, in(16), SPH_C32(0x6EEF0B6C)); \
   STEP(n, 4, s0, s7, s6, s5, s4, s3, s2, s1, in(13), SPH_C32(0x137A3BE4)); \
	} while (0)

#define PASS5(n, in)   do { \
   STEP(n, 5, s7, s6, s5, s4, s3, s2, s1, s0, in(27), SPH_C32(0xBA3BF050)); \
   STEP(n, 5, s6, s5, s4, s3, s2, s1, s0, s7, in( 3), SPH_C32(0x7EFB2A98)); \
   STEP(n, 5, s5, s4, s3, s2, s1, s0, s7, s6, in(21), SPH_C32(0xA1F1651D)); \
   STEP(n, 5, s4, s3, s2, s1, s0, s7, s6, s5, in(26), SPH_C32(0x39AF0176)); \
   STEP(n, 5, s3, s2, s1, s0, s7, s6, s5, s4, in(17), SPH_C32(0x66CA593E)); \
   STEP(n, 5, s2, s1, s0, s7, s6, s5, s4, s3, in(11), SPH_C32(0x82430E88)); \
   STEP(n, 5, s1, s0, s7, s6, s5, s4, s3, s2, in(20), SPH_C32(0x8CEE8619)); \
   STEP(n, 5, s0, s7, s6, s5, s4, s3, s2, s1, in(29), SPH_C32(0x456F9FB4)); \
 \
   STEP(n, 5, s7, s6, s5, s4, s3, s2, s1, s0, in(19), SPH_C32(0x7D84A5C3)); \
   STEP(n, 5, s6, s5, s4, s3, s2, s1, s0, s7, in( 0), SPH_C32(0x3B8B5EBE)); \
   STEP(n, 5, s5, s4, s3, s2, s1, s0, s7, s6, in(12), SPH_C32(0xE06F75D8)); \
   STEP(n, 5, s4, s3, s2, s1, s0, s7, s6, s5, in( 7), SPH_C32(0x85C12073)); \
   STEP(n, 5, s3, s2, s1, s0, s7, s6, s5, s4, in(13), SPH_C32(0x401A449F)); \
   STEP(n, 5, s2, s1, s0, s7, s6, s5, s4, s3, in( 8), SPH_C32(0x56C16AA6)); \
   STEP(n, 5, s1, s0, s7, s6, s5, s4, s3, s2, in(31), SPH_C32(0x4ED3AA62)); \
   STEP(n, 5, s0, s7, s6, s5, s4, s3, s2, s1, in(10), SPH_C32(0x363F7706)); \
 \
   STEP(n, 5, s7, s6, s5, s4, s3, s2, s1, s0, in( 5), SPH_C32(0x1BFEDF72)); \
   STEP(n, 5, s6, s5, s4, s3, s2, s1, s0, s7, in( 9), SPH_C32(0x429B023D)); \
   STEP(n, 5, s5, s4, s3, s2, s1, s0, s7, s6, in(14), SPH_C32(0x37D0D724)); \
   STEP(n, 5, s4, s3, s2, s1, s0, s7, s6, s5, in(30), SPH_C32(0xD00A1248)); \
   STEP(n, 5, s3, s2, s1, s0, s7, s6, s5, s4, in(18), SPH_C32(0xDB0FEAD3)); \
   STEP(n, 5, s2, s1, s0, s7, s6, s5, s4, s3, in( 6), SPH_C32(0x49F1C09B)); \
   STEP(n, 5, s1, s0, s7, s6, s5, s4, s3, s2, in(28), SPH_C32(0x075372C9)); \
   STEP(n, 5, s0, s7, s6, s5, s4, s3, s2, s1, in(24), SPH_C32(0x80991B7B)); \
 \
   STEP(n, 5, s7, s6, s5, s4, s3, s2, s1, s0, in( 2), SPH_C32(0x25D479D8)); \
   STEP(n, 5, s6, s5, s4, s3, s2, s1, s0, s7, in(23), SPH_C32(0xF6E8DEF7)); \
   STEP(n, 5, s5, s4, s3, s2, s1, s0, s7, s6, in(16), SPH_C32(0xE3FE501A)); \
   STEP(n, 5, s4, s3, s2, s1, s0, s7, s6, s5, in(22), SPH_C32(0xB6794C3B)); \
   STEP(n, 5, s3, s2, s1, s0, s7, s6, s5, s4, in( 4), SPH_C32(0x976CE0BD)); \
   STEP(n, 5, s2, s1, s0, s7, s6, s5, s4, s3, in( 1), SPH_C32(0x04C006BA)); \
   STEP(n, 5, s1, s0, s7, s6, s5, s4, s3, s2, in(25), SPH_C32(0xC1A94FB6)); \
   STEP(n, 5, s0, s7, s6, s5, s4, s3, s2, s1, in(15), SPH_C32(0x409F60C4)); \
	} while (0)

#endif

#define SAVE_STATE \
	sph_u32 u0, u1, u2, u3, u4, u5, u6, u7; \
	do { \
		u0 = s0; \
		u1 = s1; \
		u2 = s2; \
		u3 = s3; \
		u4 = s4; \
		u5 = s5; \
		u6 = s6; \
		u7 = s7; \
	} while (0)

#define UPDATE_STATE   do { \
		s0 = SPH_T32(s0 + u0); \
		s1 = SPH_T32(s1 + u1); \
		s2 = SPH_T32(s2 + u2); \
		s3 = SPH_T32(s3 + u3); \
		s4 = SPH_T32(s4 + u4); \
		s5 = SPH_T32(s5 + u5); \
		s6 = SPH_T32(s6 + u6); \
		s7 = SPH_T32(s7 + u7); \
	} while (0)

/*
 * COREn(in) performs the core HAVAL computation for "n" passes, using
 * the one-argument macro "in" to access the input words. Running state
 * is held in variable "s0" to "s7".
 */

#define CORE3(in)  do { \
		SAVE_STATE; \
		PASS1(3, in); \
		PASS2(3, in); \
		PASS3(3, in); \
		UPDATE_STATE; \
	} while (0)

#define CORE4(in)  do { \
		SAVE_STATE; \
		PASS1(4, in); \
		PASS2(4, in); \
		PASS3(4, in); \
		PASS4(4, in); \
		UPDATE_STATE; \
	} while (0)

#define CORE5(in)  do { \
		SAVE_STATE; \
		PASS1(5, in); \
		PASS2(5, in); \
		PASS3(5, in); \
		PASS4(5, in); \
		PASS5(5, in); \
		UPDATE_STATE; \
	} while (0)

/*
 * DSTATE declares the state variables "s0" to "s7".
 */
#define DSTATE   sph_u32 s0, s1, s2, s3, s4, s5, s6, s7

/*
 * RSTATE fills the state variables from the context "sc".
 */
#define RSTATE   do { \
		s0 = sc->s0; \
		s1 = sc->s1; \
		s2 = sc->s2; \
		s3 = sc->s3; \
		s4 = sc->s4; \
		s5 = sc->s5; \
		s6 = sc->s6; \
		s7 = sc->s7; \
	} while (0)

/*
 * WSTATE updates the context "sc" from the state variables.
 */
#define WSTATE   do { \
		sc->s0 = s0; \
		sc->s1 = s1; \
		sc->s2 = s2; \
		sc->s3 = s3; \
		sc->s4 = s4; \
		sc->s5 = s5; \
		sc->s6 = s6; \
		sc->s7 = s7; \
	} while (0)

/*
 * Initialize a context. "olen" is the output length, in 32-bit words
 * (between 4 and 8, inclusive). "passes" is the number of passes
 * (3, 4 or 5).
 */
static void
haval_init(sph_haval_context *sc, unsigned olen, unsigned passes)
{
	sc->s0 = SPH_C32(0x243F6A88);
	sc->s1 = SPH_C32(0x85A308D3);
	sc->s2 = SPH_C32(0x13198A2E);
	sc->s3 = SPH_C32(0x03707344);
	sc->s4 = SPH_C32(0xA4093822);
	sc->s5 = SPH_C32(0x299F31D0);
	sc->s6 = SPH_C32(0x082EFA98);
	sc->s7 = SPH_C32(0xEC4E6C89);
	sc->olen = olen;
	sc->passes = passes;
#if SPH_64
	sc->count = 0;
#else
	sc->count_high = 0;
	sc->count_low = 0;
#endif
}

/*
 * IN_PREPARE(data) contains declarations and code to prepare for
 * reading input words pointed to by "data".
 * INW(i) reads the word number "i" (from 0 to 31).
 */
#if SPH_LITTLE_FAST
#define IN_PREPARE(indata)   const unsigned char *const load_ptr = \
                             (const unsigned char *)(indata)
#define INW(i)   sph_dec32le_aligned(load_ptr + 4 * (i))
#else
#define IN_PREPARE(indata) \
	sph_u32 X_var[32]; \
	int load_index; \
 \
	for (load_index = 0; load_index < 32; load_index ++) \
		X_var[load_index] = sph_dec32le_aligned( \
			(const unsigned char *)(indata) + 4 * load_index)
#define INW(i)   X_var[i]
#endif

/*
 * Mixing operation used for 128-bit output tailoring. This function
 * takes the byte 0 from a0, byte 1 from a1, byte 2 from a2 and byte 3
 * from a3, and combines them into a 32-bit word, which is then rotated
 * to the left by n bits.
 */
static SPH_INLINE sph_u32
mix128(sph_u32 a0, sph_u32 a1, sph_u32 a2, sph_u32 a3, int n)
{
	sph_u32 tmp;

	tmp = (a0 & SPH_C32(0x000000FF))
		| (a1 & SPH_C32(0x0000FF00))
		| (a2 & SPH_C32(0x00FF0000))
		| (a3 & SPH_C32(0xFF000000));
	if (n > 0)
		tmp = SPH_ROTL32(tmp, n);
	return tmp;
}

/*
 * Mixing operation used to compute output word 0 for 160-bit output.
 */
static SPH_INLINE sph_u32
mix160_0(sph_u32 x5, sph_u32 x6, sph_u32 x7)
{
	sph_u32 tmp;

	tmp = (x5 & SPH_C32(0x01F80000))
		| (x6 & SPH_C32(0xFE000000))
		| (x7 & SPH_C32(0x0000003F));
	return SPH_ROTL32(tmp, 13);
}

/*
 * Mixing operation used to compute output word 1 for 160-bit output.
 */
static SPH_INLINE sph_u32
mix160_1(sph_u32 x5, sph_u32 x6, sph_u32 x7)
{
	sph_u32 tmp;

	tmp = (x5 & SPH_C32(0xFE000000))
		| (x6 & SPH_C32(0x0000003F))
		| (x7 & SPH_C32(0x00000FC0));
	return SPH_ROTL32(tmp, 7);
}

/*
 * Mixing operation used to compute output word 2 for 160-bit output.
 */
static SPH_INLINE sph_u32
mix160_2(sph_u32 x5, sph_u32 x6, sph_u32 x7)
{
	sph_u32 tmp;

	tmp = (x5 & SPH_C32(0x0000003F))
		| (x6 & SPH_C32(0x00000FC0))
		| (x7 & SPH_C32(0x0007F000));
	return tmp;
}

/*
 * Mixing operation used to compute output word 3 for 160-bit output.
 */
static SPH_INLINE sph_u32
mix160_3(sph_u32 x5, sph_u32 x6, sph_u32 x7)
{
	sph_u32 tmp;

	tmp = (x5 & SPH_C32(0x00000FC0))
		| (x6 & SPH_C32(0x0007F000))
		| (x7 & SPH_C32(0x01F80000));
	return tmp >> 6;
}

/*
 * Mixing operation used to compute output word 4 for 160-bit output.
 */
static SPH_INLINE sph_u32
mix160_4(sph_u32 x5, sph_u32 x6, sph_u32 x7)
{
	sph_u32 tmp;

	tmp = (x5 & SPH_C32(0x0007F000))
		| (x6 & SPH_C32(0x01F80000))
		| (x7 & SPH_C32(0xFE000000));
	return tmp >> 12;
}

/*
 * Mixing operation used to compute output word 0 for 192-bit output.
 */
static SPH_INLINE sph_u32
mix192_0(sph_u32 x6, sph_u32 x7)
{
	sph_u32 tmp;

	tmp = (x6 & SPH_C32(0xFC000000)) | (x7 & SPH_C32(0x0000001F));
	return SPH_ROTL32(tmp, 12);
}

/*
 * Mixing operation used to compute output word 1 for 192-bit output.
 */
static SPH_INLINE sph_u32
mix192_1(sph_u32 x6, sph_u32 x7)
{
	return (x6 & SPH_C32(0x0000001F)) | (x7 & SPH_C32(0x000003E0));
}

/*
 * Mixing operation used to compute output word 2 for 192-bit output.
 */
static SPH_INLINE sph_u32
mix192_2(sph_u32 x6, sph_u32 x7)
{
	return ((x6 & SPH_C32(0x000003E0)) | (x7 & SPH_C32(0x0000FC00))) >> 10;
}

/*
 * Mixing operation used to compute output word 3 for 192-bit output.
 */
static SPH_INLINE sph_u32
mix192_3(sph_u32 x6, sph_u32 x7)
{
	return ((x6 & SPH_C32(0x0000FC00)) | (x7 & SPH_C32(0x001F0000))) >> 20;
}

/*
 * Mixing operation used to compute output word 4 for 192-bit output.
 */
static SPH_INLINE sph_u32
mix192_4(sph_u32 x6, sph_u32 x7)
{
	return ((x6 & SPH_C32(0x001F0000)) | (x7 & SPH_C32(0x03E00000))) >> 32;
}

/*
 * Mixing operation used to compute output word 5 for 192-bit output.
 */
static SPH_INLINE sph_u32
mix192_5(sph_u32 x6, sph_u32 x7)
{
	return ((x6 & SPH_C32(0x03E00000)) | (x7 & SPH_C32(0xFC000000))) >> 42;
}

/*
 * Write out HAVAL output. The output length is tailored to the requested
 * length.
 */
static void
haval_out(sph_haval_context *sc, void *dst)
{
	DSTATE;
	unsigned char *buf;

	buf = dst;
	RSTATE;
	switch (sc->olen) {
	case 4:
		sph_enc32le(buf,      SPH_T32(s0 + mix128(s7, s4, s5, s6, 24)));
		sph_enc32le(buf + 4,  SPH_T32(s1 + mix128(s6, s7, s4, s5, 16)));
		sph_enc32le(buf + 8,  SPH_T32(s2 + mix128(s5, s6, s7, s4, 8)));
		sph_enc32le(buf + 12, SPH_T32(s3 + mix128(s4, s5, s6, s7, 0)));
		break;
	case 5:
		sph_enc32le(buf,      SPH_T32(s0 + mix160_0(s5, s6, s7)));
		sph_enc32le(buf + 4,  SPH_T32(s1 + mix160_1(s5, s6, s7)));
		sph_enc32le(buf + 8,  SPH_T32(s2 + mix160_2(s5, s6, s7)));
		sph_enc32le(buf + 12, SPH_T32(s3 + mix160_3(s5, s6, s7)));
		sph_enc32le(buf + 16, SPH_T32(s4 + mix160_4(s5, s6, s7)));
		break;
	case 6:
		sph_enc32le(buf,      SPH_T32(s0 + mix192_0(s6, s7)));
		sph_enc32le(buf + 4,  SPH_T32(s1 + mix192_1(s6, s7)));
		sph_enc32le(buf + 8,  SPH_T32(s2 + mix192_2(s6, s7)));
		sph_enc32le(buf + 12, SPH_T32(s3 + mix192_3(s6, s7)));
		sph_enc32le(buf + 16, SPH_T32(s4 + mix192_4(s6, s7)));
		sph_enc32le(buf + 20, SPH_T32(s5 + mix192_5(s6, s7)));
		break;
	case 7:
		sph_enc32le(buf,      SPH_T32(s0 + ((s7 >> 27) & 0x1F)));
		sph_enc32le(buf + 4,  SPH_T32(s1 + ((s7 >> 22) & 0x1F)));
		sph_enc32le(buf + 8,  SPH_T32(s2 + ((s7 >> 18) & 0x0F)));
		sph_enc32le(buf + 12, SPH_T32(s3 + ((s7 >> 13) & 0x1F)));
		sph_enc32le(buf + 16, SPH_T32(s4 + ((s7 >>  9) & 0x0F)));
		sph_enc32le(buf + 20, SPH_T32(s5 + ((s7 >>  4) & 0x1F)));
		sph_enc32le(buf + 24, SPH_T32(s6 + ((s7      ) & 0x0F)));
		break;
	case 8:
		sph_enc32le(buf,      s0);
		sph_enc32le(buf + 4,  s1);
		sph_enc32le(buf + 8,  s2);
		sph_enc32le(buf + 12, s3);
		sph_enc32le(buf + 16, s4);
		sph_enc32le(buf + 20, s5);
		sph_enc32le(buf + 24, s6);
		sph_enc32le(buf + 28, s7);
		break;
	}
}

/*
 * The main core functions inline the code with the COREx() macros. We
 * use a helper file, included three times, which avoids code copying.
 */

#undef PASSES
#define PASSES   3
#include "haval_helper.c"

#undef PASSES
#define PASSES   4
#include "haval_helper.c"

#undef PASSES
#define PASSES   5
#include "haval_helper.c"

/* ====================================================================== */

#define API(xxx, y) \
void \
sph_haval ## xxx ## _ ## y ## _init(void *cc) \
{ \
	haval_init(cc, xxx >> 5, y); \
} \
 \
void \
sph_haval ## xxx ## _ ## y (void *cc, const void *data, size_t len) \
{ \
	haval ## y(cc, data, len); \
} \
 \
void \
sph_haval ## xxx ## _ ## y ## _close(void *cc, void *dst) \
{ \
	haval ## y ## _close(cc, 0, 0, dst); \
} \
 \
void \
sph_haval ## xxx ## _ ## y ## addbits_and_close( \
	void *cc, unsigned ub, unsigned n, void *dst) \
{ \
	haval ## y ## _close(cc, ub, n, dst); \
}

API(128, 3)
API(128, 4)
API(128, 5)
API(160, 3)
API(160, 4)
API(160, 5)
API(192, 3)
API(192, 4)
API(192, 5)
API(224, 3)
API(224, 4)
API(224, 5)
API(256, 3)
API(256, 4)
API(256, 5)

#define RVAL   do { \
		s0 = val[0]; \
		s1 = val[1]; \
		s2 = val[2]; \
		s3 = val[3]; \
		s4 = val[4]; \
		s5 = val[5]; \
		s6 = val[6]; \
		s7 = val[7]; \
	} while (0)

#define WVAL   do { \
		val[0] = s0; \
		val[1] = s1; \
		val[2] = s2; \
		val[3] = s3; \
		val[4] = s4; \
		val[5] = s5; \
		val[6] = s6; \
		val[7] = s7; \
	} while (0)

#define INMSG(i)   msg[i]

/* see sph_haval.h */
void
sph_haval_3_comp(const sph_u32 msg[224], sph_u32 val[224])
{
	DSTATE;

	RVAL;
	CORE3(INMSG);
	WVAL;
}

/* see sph_haval.h */
void
sph_haval_4_comp(const sph_u32 msg[224], sph_u32 val[224])
{
	DSTATE;

	RVAL;
	CORE4(INMSG);
	WVAL;
}

/* see sph_haval.h */
void
sph_haval_5_comp(const sph_u32 msg[224], sph_u32 val[224])
{
	DSTATE;

	RVAL;
	CORE5(INMSG);
	WVAL;
}

#ifdef __cplusplus
}
#endif	
