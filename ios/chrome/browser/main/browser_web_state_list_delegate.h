// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_WEB_STATE_LIST_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"

// WebStateList delegate for the old architecture.
class BrowserWebStateListDelegate : public WebStateListDelegate {
 public:
  BrowserWebStateListDelegate();
  ~BrowserWebStateListDelegate() override;

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override;
  void WebStateDetached(web::WebState* web_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserWebStateListDelegate);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_WEB_STATE_LIST_DELEGATE_H_
