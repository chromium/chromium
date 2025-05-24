// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_H_
#define PDF_ACCESSIBILITY_H_

#include <stdint.h>

#include <vector>

namespace chrome_pdf {

class PDFiumEngine;
struct AccessibilityCharInfo;
struct AccessibilityPageInfo;
struct AccessibilityPageObjects;
struct AccessibilityTextRunInfo;

// Retrieve `page_info`, `text_runs`, `chars`, and `page_objects` from
// `engine` for the page at 0-indexed `page_index`.
// Expects the `page_index` to be valid.
void GetAccessibilityInfo(PDFiumEngine* engine,
                          int32_t page_index,
                          AccessibilityPageInfo& page_info,
                          std::vector<AccessibilityTextRunInfo>& text_runs,
                          std::vector<AccessibilityCharInfo>& chars,
                          AccessibilityPageObjects& page_objects);

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_H_
