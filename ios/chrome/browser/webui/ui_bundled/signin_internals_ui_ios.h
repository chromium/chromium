// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_SIGNIN_INTERNALS_UI_IOS_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_SIGNIN_INTERNALS_UI_IOS_H_

#include <string>

#include "base/values.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// The implementation for the chrome://signin-internals page.
class SignInInternalsUIIOS : public web::WebUIIOSController {
 public:
  SignInInternalsUIIOS(web::WebUIIOS* web_ui, const std::string& host);
  ~SignInInternalsUIIOS() override;
  SignInInternalsUIIOS(const SignInInternalsUIIOS&) = delete;
  SignInInternalsUIIOS& operator=(const SignInInternalsUIIOS&) = delete;
};

class SignInInternalsHandlerIOS : public web::WebUIIOSMessageHandler,
                                  public AboutSigninInternals::Observer {
 public:
  SignInInternalsHandlerIOS();
  ~SignInInternalsHandlerIOS() override;

  SignInInternalsHandlerIOS(const SignInInternalsHandlerIOS&) = delete;
  SignInInternalsHandlerIOS& operator=(const SignInInternalsHandlerIOS&) =
      delete;

  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  void HandleGetSignInInfo(const base::Value::List& args);

  // AboutSigninInternals::Observer::OnSigninStateChanged implementation.
  void OnSigninStateChanged(const base::Value::Dict& info) override;

  // Notification that the cookie accounts are ready to be displayed.
  void OnCookieAccountsFetched(const base::Value::Dict& info) override;

 private:
  base::ScopedObservation<AboutSigninInternals, AboutSigninInternals::Observer>
      about_signin_internals_observeration_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_SIGNIN_INTERNALS_UI_IOS_H_
