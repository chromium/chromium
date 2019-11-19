// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PRINT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_PRINT_TAB_HELPER_H_

#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol WebStatePrinter;
class GURL;

namespace base {
class DictionaryValue;
}  // namespace base

// Handles print requests from JavaScript window.print.
class PrintTabHelper : public web::WebStateObserver,
                       public web::WebStateUserData<PrintTabHelper> {
 public:
  explicit PrintTabHelper(web::WebState* web_state);
  ~PrintTabHelper() override;

  // Sets the |printer|, which is held weakly by this object.
  void set_printer(id<WebStatePrinter> printer);

 private:
  friend class web::WebStateUserData<PrintTabHelper>;

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;

  // Called when print message is sent by the web page.
  void OnPrintCommand(web::WebState* web_state,
                      const base::DictionaryValue& command,
                      const GURL& page_url,
                      bool user_initiated,
                      web::WebFrame* sender_frame);

  __weak id<WebStatePrinter> printer_;

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PrintTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_PRINT_TAB_HELPER_H_
