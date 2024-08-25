// Copyright (c) 2003-2008 Yuta Mori All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// ChangeLog:
// 2016-07-22 - Initial commit and adaption to use PagedArray.
//                --Samuel Huang <huangs@chromium.org>

#ifndef COURGETTE_BSDIFF_THIRD_PARTY_DIVSUFSORT_PRIVATE_H_
#define COURGETTE_BSDIFF_THIRD_PARTY_DIVSUFSORT_PRIVATE_H_

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "courgette/third_party/divsufsort/divsufsort.h"

namespace divsuf {

/*- Constants -*/
#if !defined(UINT8_MAX)
# define UINT8_MAX (255)
#endif /* UINT8_MAX */
#if defined(ALPHABET_SIZE) && (ALPHABET_SIZE < 1)
# undef ALPHABET_SIZE
#endif
#if !defined(ALPHABET_SIZE)
# define ALPHABET_SIZE (UINT8_MAX + 1)
#endif

/*- Macros -*/
#ifndef SWAP
# define SWAP(_a, _b) do { t = (_a); (_a) = (_b); (_b) = t; } while(0)
#endif /* SWAP */
#ifndef MIN
# define MIN(_a, _b) (((_a) < (_b)) ? (_a) : (_b))
#endif /* MIN */
#ifndef MAX
# define MAX(_a, _b) (((_a) > (_b)) ? (_a) : (_b))
#endif /* MAX */

/*- Private Prototypes -*/
/* sssort.c */
void
sssort(const sauchar_t *T, const_saidx_it PA,
       saidx_it first, saidx_it last,
       saidx_it buf, saidx_t bufsize,
       saidx_t depth, saidx_t n, saint_t lastsuffix);

/* trsort.c */
void
trsort(saidx_it ISA, saidx_it SA, saidx_t n, saidx_t depth);

}  // namespace divsuf

#endif  // COURGETTE_BSDIFF_THIRD_PARTY_DIVSUFSORT_PRIVATE_H_
