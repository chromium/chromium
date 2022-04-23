// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_RANGE_H_
#define PRINTING_PAGE_RANGE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"

namespace printing {

struct PageRange;

using PageRanges = std::vector<PageRange>;

// Print range is inclusive. To select one page, set from == to.
struct COMPONENT_EXPORT(PRINTING) PageRange {
  uint32_t from;
  uint32_t to;

  bool operator<(const PageRange& rhs) const {
    return from < rhs.from || (from == rhs.from && to < rhs.to);
  }
  bool operator==(const PageRange& rhs) const {
    return from == rhs.from && to == rhs.to;
  }

  // Ensures entries come in monotonically increasing order and do not
  // overlap.
  static void Normalize(PageRanges& ranges);
};

}  // namespace printing

#endif  // PRINTING_PAGE_RANGE_H_
