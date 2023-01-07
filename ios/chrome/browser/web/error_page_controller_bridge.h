// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_ERROR_PAGE_CONTROLLER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_ERROR_PAGE_CONTROLLER_BRIDGE_H_

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// A class to bridge the JS errorPageController object in the error page.
// The class receives the JS messages and handle/dispatch them when needed.
// Messages are sent in
// components/neterror/resources/error_page_controller_ios.js.
class ErrorPageControllerBridge
    : public web::WebStateObserver,
      public web::WebStateUserData<ErrorPageControllerBridge> {
 public:
  ~ErrorPageControllerBridge() override;

  // Start observing "errorPageController" commands until next navigation.
  void StartHandlingJavascriptCommands();

  // WebStateObserver overrides
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class WebStateUserData<ErrorPageControllerBridge>;

  ErrorPageControllerBridge(web::WebState* web_state);

  // Handler for "errorPageController.*" JavaScript command.
  void OnErrorPageCommand(const base::Value& message,
                          const GURL& url,
                          bool user_is_interacting,
                          web::WebFrame* sender_frame);

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Subscription for JS message.
  base::CallbackListSubscription subscription_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_ERROR_PAGE_CONTROLLER_BRIDGE_H_
