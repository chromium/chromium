// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_PRINTING_CONVERSIONS_H_
#define PDF_PPAPI_MIGRATION_PRINTING_CONVERSIONS_H_

#include <stdint.h>

#include <vector>

struct PP_PrintPageNumberRange_Dev;

namespace chrome_pdf {

std::vector<int> PageNumbersFromPPPrintPageNumberRange(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_PRINTING_CONVERSIONS_H_
