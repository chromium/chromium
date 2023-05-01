// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_ACTION_HANDLER_H_
#define PDF_PDF_ACCESSIBILITY_ACTION_HANDLER_H_

namespace chrome_pdf {

struct AccessibilityActionData;

class PdfAccessibilityActionHandler {
 public:
  virtual ~PdfAccessibilityActionHandler() = default;

  virtual void EnableAccessibility() = 0;
  virtual void HandleAccessibilityAction(
      const AccessibilityActionData& action_data) = 0;
  virtual void LoadOrReloadAccessibility() = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_ACTION_HANDLER_H_
