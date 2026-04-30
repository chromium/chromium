// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/chrome/browser/translate/model/translate_pdf_delegate.h"

class Browser;
namespace web {
class WebState;
}

// A browser agent responsible for wiring the TranslatePDFDelegate into
// TranslatePDFMetricLogger instances as they are added to the browser's tab
// grid.
class TranslatePDFBrowserAgent
    : public BrowserUserData<TranslatePDFBrowserAgent>,
      public TabsDependencyInstaller,
      public TranslatePDFDelegate {
 public:
  TranslatePDFBrowserAgent(const TranslatePDFBrowserAgent&) = delete;
  TranslatePDFBrowserAgent& operator=(const TranslatePDFBrowserAgent&) = delete;

  ~TranslatePDFBrowserAgent() override;

  // TranslatePDFDelegate implementation
  bool IsOpenerTabTranslatedForWebState(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<TranslatePDFBrowserAgent>;

  explicit TranslatePDFBrowserAgent(Browser* browser);

  // TabsDependencyInstaller implementation
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_BROWSER_AGENT_H_
