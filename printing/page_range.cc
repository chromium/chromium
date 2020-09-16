// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_range.h"

#include <stddef.h>

#include <set>

namespace printing {

// static
std::vector<uint32_t> PageRange::GetPages(const PageRanges& ranges) {
  // TODO(vitalybuka): crbug.com/95548 Remove this method as part fix.
  std::set<uint32_t> pages;
  for (const PageRange& range : ranges) {
    // Ranges are inclusive.
    for (uint32_t i = range.from; i <= range.to; ++i) {
      static constexpr size_t kMaxNumberOfPages = 100000;
      pages.insert(i);
      if (pages.size() >= kMaxNumberOfPages)
        return std::vector<uint32_t>(pages.begin(), pages.end());
    }
  }
  return std::vector<uint32_t>(pages.begin(), pages.end());
}

}  // namespace printing
