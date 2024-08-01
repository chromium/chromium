// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_RANGE_H_
#define PRINTING_PAGE_RANGE_H_

#include <stdint.h>

#include <limits>
#include <vector>

#include "base/component_export.h"

namespace printing {

struct PageRange;

using PageRanges = std::vector<PageRange>;

// Print range is inclusive. To select one page, set from == to.
struct COMPONENT_EXPORT(PRINTING_SETTINGS) PageRange {
  // Any value above maximum practical page count (enforced by PageNumber)
  // would work, but we chose something that works even where the page
  // numbers are 1-based (i.e. can be increased by one without overflow).
  static constexpr uint32_t kMaxPage = std::numeric_limits<uint32_t>::max() - 1;

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
