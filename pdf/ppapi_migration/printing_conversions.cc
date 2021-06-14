// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/printing_conversions.h"

#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "ppapi/c/dev/ppp_printing_dev.h"

namespace chrome_pdf {

std::vector<int> PageNumbersFromPPPrintPageNumberRange(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  DCHECK(page_range_count);

  std::vector<int> page_numbers;
  for (uint32_t i = 0; i < page_range_count; ++i) {
    for (uint32_t page_number = page_ranges[i].first_page_number;
         page_number <= page_ranges[i].last_page_number; ++page_number) {
      page_numbers.push_back(page_number);
    }
  }

  return page_numbers;
}

}  // namespace chrome_pdf
