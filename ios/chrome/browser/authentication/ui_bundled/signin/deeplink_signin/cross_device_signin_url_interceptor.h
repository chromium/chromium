// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_CROSS_DEVICE_SIGNIN_URL_INTERCEPTOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_CROSS_DEVICE_SIGNIN_URL_INTERCEPTOR_H_

#import <string>

#import "base/functional/callback.h"
#import "ios/chrome/browser/url_loading/model/url_interceptor.h"

// A URL interceptor that intercepts cross-device sign-in URLs, parses the email
// and entry point ID using `SigninDeepLinkParser`, and calls the callback on
// successful parsing.
class CrossDeviceSigninURLInterceptor : public URLInterceptor {
 public:
  using InterceptCallback =
      base::RepeatingCallback<void(const std::string& email)>;

  explicit CrossDeviceSigninURLInterceptor(InterceptCallback callback);

  CrossDeviceSigninURLInterceptor(const CrossDeviceSigninURLInterceptor&) =
      delete;
  CrossDeviceSigninURLInterceptor& operator=(
      const CrossDeviceSigninURLInterceptor&) = delete;

  ~CrossDeviceSigninURLInterceptor() override;

  bool OnIntercept(const UrlLoadParams& params) override;

 private:
  // Callback executed upon successful intercept and parsing.
  InterceptCallback callback_;
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_DEEPLINK_SIGNIN_CROSS_DEVICE_SIGNIN_URL_INTERCEPTOR_H_
