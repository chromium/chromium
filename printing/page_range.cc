// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_range.h"

#include <stddef.h>

#include <algorithm>
#include <set>

namespace printing {

// static
void PageRange::Normalize(PageRanges& ranges) {
  if (ranges.empty()) {
    return;
  }

  std::sort(ranges.begin(), ranges.end());
  PageRanges::iterator dst = ranges.begin();
  for (PageRanges::iterator src = ranges.begin() + 1; src < ranges.end();
       ++src) {
    if (dst->to + 1 < src->from) {
      *++dst = *src;
      continue;
    }
    dst->to = std::max(dst->to, src->to);
  }
  if (dst < ranges.end())
    dst++;
  ranges.resize(dst - ranges.begin());
}

}  // namespace printing
