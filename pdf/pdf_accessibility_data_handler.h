// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_DATA_HANDLER_H_
#define PDF_PDF_ACCESSIBILITY_DATA_HANDLER_H_

#include <memory>
#include <vector>

#include "services/screen_ai/buildflags/buildflags.h"

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
  // `doc_info` must be non-nullptr.
  virtual void SetAccessibilityDocInfo(
      std::unique_ptr<AccessibilityDocInfo> doc_info) = 0;
  virtual void SetAccessibilityPageInfo(
      AccessibilityPageInfo page_info,
      std::vector<AccessibilityTextRunInfo> text_runs,
      std::vector<AccessibilityCharInfo> chars,
      AccessibilityPageObjects page_objects) = 0;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Notifies that at least one page is searchified. This function is called at
  // most once.
  virtual void OnHasSearchifyText() = 0;
#endif
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_DATA_HANDLER_H_
