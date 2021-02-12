// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_HELPER_H_
#define PDF_ACCESSIBILITY_HELPER_H_

#include <vector>

namespace chrome_pdf {

struct AccessibilityTextRunInfo;
struct AccessibilityTextRunRangeInfo;

// Find the text run range encompassing the char range denoted by
// |start_char_index| and |char_count|. If a valid text run range is not found
// for the char range then return the fallback value.
AccessibilityTextRunRangeInfo GetEnclosingTextRunRangeForCharRange(
    const std::vector<AccessibilityTextRunInfo>& text_runs,
    int start_char_index,
    int char_count);

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_HELPER_H_
