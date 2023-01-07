// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_DATA_HANDLER_H_
#define PDF_PDF_ACCESSIBILITY_DATA_HANDLER_H_

#include <vector>

namespace chrome_pdf {

struct AccessibilityCharInfo;
struct AccessibilityDocInfo;
struct AccessibilityPageInfo;
struct AccessibilityPageObjects;
struct AccessibilityTextRunInfo;
struct AccessibilityViewportInfo;

class PdfAccessibilityDataHandler {
 public:
  virtual ~PdfAccessibilityDataHandler() = default;

  virtual void SetAccessibilityViewportInfo(
      AccessibilityViewportInfo viewport_info) = 0;
  virtual void SetAccessibilityDocInfo(AccessibilityDocInfo doc_info) = 0;
  virtual void SetAccessibilityPageInfo(
      AccessibilityPageInfo page_info,
      std::vector<AccessibilityTextRunInfo> text_runs,
      std::vector<AccessibilityCharInfo> chars,
      AccessibilityPageObjects page_objects) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_DATA_HANDLER_H_
