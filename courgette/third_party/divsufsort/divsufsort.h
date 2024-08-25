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

#ifndef COURGETTE_BSDIFF_THIRD_PARTY_DIVSUFSORT_H_
#define COURGETTE_BSDIFF_THIRD_PARTY_DIVSUFSORT_H_

#include <stdint.h>

#include "courgette/third_party/bsdiff/paged_array.h"

namespace divsuf {

/*- Datatypes -*/
typedef int32_t saint_t;
typedef int32_t saidx_t;
typedef uint8_t sauchar_t;

#ifdef DIVSUFSORT_NO_PAGED_ARRAY
typedef saidx_t* saidx_it;
typedef const saidx_t* const_saidx_it;
#else
typedef courgette::PagedArray<saidx_t>::iterator saidx_it;
typedef courgette::PagedArray<saidx_t>::const_iterator const_saidx_it;
#endif

/*- Prototypes -*/

/**
 * Constructs the suffix array of a given string, excluding the empty string.
 * @param T[0..n-1] The input string.
 * @param SA[0..n-1] The output array of suffixes.
 * @param n The length of the given string.
 * @return 0 if no error occurred, -1 or -2 otherwise.
 */
saint_t divsufsort(const sauchar_t *T, saidx_it SA, saidx_t n);

/**
 * Constructs the suffix array of a given string, including the empty string.
 * @param T[0..n-1] The input string.
 * @param SA[0..n] The output array of suffixes (includes empty string).
 * @param n The length of the given string.
 * @return 0 if no error occurred, -1 or -2 otherwise.
 */
saint_t divsufsort_include_empty(const sauchar_t *T, saidx_it SA, saidx_t n);

}  // namespace divsuf

#endif  // COURGETTE_BSDIFF_THIRD_PARTY_DIVSUFSORT_H_
