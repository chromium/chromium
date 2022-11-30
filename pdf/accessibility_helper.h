// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_HELPER_H_
#define PDF_ACCESSIBILITY_HELPER_H_

#include <vector>

namespace chrome_pdf {

struct AccessibilityTextRunInfo;
struct AccessibilityTextRunRangeInfo;

AccessibilityTextRunRangeInfo GetEnclosingTextRunRangeForCharRange(
    const std::vector<AccessibilityTextRunInfo>& text_runs,
    int start_char_index,
    int char_count);

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_HELPER_H_
