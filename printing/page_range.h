// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_RANGE_H_
#define PRINTING_PAGE_RANGE_H_

#include <stdint.h>

#include <vector>

#include "printing/printing_export.h"

namespace printing {

struct PageRange;

using PageRanges = std::vector<PageRange>;

// Print range is inclusive. To select one page, set from == to.
struct PRINTING_EXPORT PageRange {
  uint32_t from;
  uint32_t to;

  bool operator==(const PageRange& rhs) const {
    return from == rhs.from && to == rhs.to;
  }

  // Retrieves the sorted list of unique pages in the page ranges.
  static std::vector<uint32_t> GetPages(const PageRanges& ranges);
};

}  // namespace printing

#endif  // PRINTING_PAGE_RANGE_H_
