// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_H_

#include "base/values.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper_delegate.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

// Receives JavaScript console log messages and forwards them to the delegate.
class JavaScriptConsoleTabHelper
    : public web::WebStateUserData<JavaScriptConsoleTabHelper>,
      public web::WebStateObserver {
 public:
  ~JavaScriptConsoleTabHelper() override;

  // The delegate associated with the receiver. The delegate will be notified of
  // new JavaScript messages.
  void SetDelegate(JavaScriptConsoleTabHelperDelegate* delegate);

 private:
  // Handles the received JavaScript messages.
  void OnJavaScriptConsoleMessage(const base::DictionaryValue& message,
                                  const GURL& page_url,
                                  bool user_is_interacting,
                                  web::WebFrame* sender_frame);

  // WebStateObserver overrides.
  void WebStateDestroyed(web::WebState* web_state) override;

  explicit JavaScriptConsoleTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<JavaScriptConsoleTabHelper>;

  // The delegate associated with the receiver.
  JavaScriptConsoleTabHelperDelegate* delegate_ = nullptr;

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(JavaScriptConsoleTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_TAB_HELPER_H_
