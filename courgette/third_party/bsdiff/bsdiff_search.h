// Copyright 2003, 2004 Colin Percival
// All rights reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted providing that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// For the terms under which this work may be distributed, please see
// the adjoining file "LICENSE".
//
// ChangeLog:
// 2005-05-05 - Use the modified header struct from bspatch.h; use 32-bit
//              values throughout.
//                --Benjamin Smedberg <benjamin@smedbergs.us>
// 2015-08-03 - Change search() to template to allow PagedArray usage.
//                --Samuel Huang <huangs@chromium.org>
// 2015-08-19 - Optimized search() to be non-recursive.
//                --Samuel Huang <huangs@chromium.org>
// 2016-06-28 - Moved matchlen() and search() to a new file; format; changed
//              search() use std::lexicographical_compare().
// 2016-06-30 - Changed matchlen() input; changed search() to return struct.
//                --Samuel Huang <huangs@chromium.org>

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_SEARCH_H_
#define COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_SEARCH_H_

#include <algorithm>

namespace bsdiff {

// Return values of search().
struct SearchResult {
  SearchResult(int pos_in, int size_in) : pos(pos_in), size(size_in) {}
  int pos;
  int size;
};

// Similar to ::memcmp(), but assumes equal |size| and returns match length.
inline int matchlen(const unsigned char* buf1,
                    const unsigned char* buf2,
                    int size) {
  for (int i = 0; i < size; ++i)
    if (buf1[i] != buf2[i])
      return i;
  return size;
}

// Finds a suffix in |old| that has the longest common prefix with |keybuf|,
// aided by suffix array |I| of |old|. Returns the match length, and writes to
// |pos| a position of best match in |old|. If multiple such positions exist,
// |pos| would take an arbitrary one.
template <class T>
SearchResult search(T I,
                    const unsigned char* srcbuf,
                    int srcsize,
                    const unsigned char* keybuf,
                    int keysize) {
  int lo = 0;
  int hi = srcsize;
  while (hi - lo >= 2) {
    int mid = (lo + hi) / 2;
    if (std::lexicographical_compare(
            srcbuf + I[mid], srcbuf + srcsize, keybuf, keybuf + keysize)) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  int x = matchlen(srcbuf + I[lo], keybuf, std::min(srcsize - I[lo], keysize));
  int y = matchlen(srcbuf + I[hi], keybuf, std::min(srcsize - I[hi], keysize));
  return (x > y) ? SearchResult(I[lo], x) : SearchResult(I[hi], y);
}

}  // namespace bsdiff

#endif  // COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_SEARCH_H_
