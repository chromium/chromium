// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_DELEGATE_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_DELEGATE_H_

namespace web {
class WebState;
}  // namespace web

// A delegate protocol used by TranslatePDFMetricLogger to query window-level
// translation state from the PDFTranslateBrowserAgent.
class TranslatePDFDelegate {
 public:
  virtual ~TranslatePDFDelegate() = default;

  // Returns true if the referring opener tab for `web_state` was translated
  // at the time this tab was created.
  virtual bool IsOpenerTabTranslatedForWebState(web::WebState* web_state) = 0;
};
#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_DELEGATE_H_
