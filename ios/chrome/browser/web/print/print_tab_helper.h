// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PRINT_PRINT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_PRINT_PRINT_TAB_HELPER_H_

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol WebStatePrinter;

// Handles print requests from JavaScript window.print.
class PrintTabHelper : public web::WebStateUserData<PrintTabHelper> {
 public:
  explicit PrintTabHelper(web::WebState* web_state);

  PrintTabHelper(const PrintTabHelper&) = delete;
  PrintTabHelper& operator=(const PrintTabHelper&) = delete;

  ~PrintTabHelper() override;

  // Sets the `printer`, which is held weakly by this object.
  void set_printer(id<WebStatePrinter> printer);

  // Prints `web_state_` using `printer_`. Does nothing if printing is
  // disabled, for example by policy.
  void Print();

 private:
  friend class web::WebStateUserData<PrintTabHelper>;

  web::WebState* web_state_;
  __weak id<WebStatePrinter> printer_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_PRINT_PRINT_TAB_HELPER_H_
