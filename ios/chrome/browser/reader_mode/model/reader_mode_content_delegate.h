// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_CONTENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_CONTENT_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/web_state_policy_decider.h"

class ReaderModeContentTabHelper;

// Delegate for the content presented in Reader mode.
class ReaderModeContentDelegate {
 public:
  virtual ~ReaderModeContentDelegate() = default;

  // Called when the content completed loading Reader Mode data.
  virtual void ReaderModeContentDidLoadData(
      ReaderModeContentTabHelper* reader_mode_content_tab_helper) = 0;

  // Called when the content just denied a request to navigate away from the
  // current page.
  virtual void ReaderModeContentDidCancelRequest(
      ReaderModeContentTabHelper* reader_mode_content_tab_helper,
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info) = 0;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_CONTENT_DELEGATE_H_
