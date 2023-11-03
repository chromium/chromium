// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_SHARED_MODEL_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_SHARED_MODEL_WEB_STATE_LIST_DELEGATE_H_

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"

// WebStateList delegate for the old architecture.
class BrowserWebStateListDelegate : public WebStateListDelegate {
 public:
  explicit BrowserWebStateListDelegate(bool force_realization_on_activation);

  BrowserWebStateListDelegate(const BrowserWebStateListDelegate&) = delete;
  BrowserWebStateListDelegate& operator=(const BrowserWebStateListDelegate&) =
      delete;

  ~BrowserWebStateListDelegate() override;

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override;
  void WillActivateWebState(web::WebState* web_state) override;

 private:
  // Controls whether WebState are forced to the realized state when
  // activated or not.
  const bool force_realization_on_activation_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_SHARED_MODEL_WEB_STATE_LIST_DELEGATE_H_
