// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/cross_device_signin_url_interceptor.h"

#import <optional>
#import <utility>

#import "components/signin/public/base/signin_deep_link_parser.h"
#import "components/signin/public/base/signin_deep_link_payload.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

CrossDeviceSigninURLInterceptor::CrossDeviceSigninURLInterceptor(
    InterceptCallback callback)
    : callback_(std::move(callback)) {
  set_active(true);
  // We want to prevent the normal loading flow of the intercepted sign-in URL.
  set_prevent_normal_flow(true);
  // We want the interceptor to stay active across multiple navigations.
  set_deactivates_on_match(false);
}

CrossDeviceSigninURLInterceptor::~CrossDeviceSigninURLInterceptor() = default;

bool CrossDeviceSigninURLInterceptor::OnIntercept(const UrlLoadParams& params) {
  if (params.in_incognito) {
    return false;
  }

  std::optional<signin::SigninDeepLinkParser> parser =
      signin::SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  if (!parser.has_value()) {
    return false;
  }

  std::optional<signin::SigninDeepLinkPayload> payload =
      parser->Parse(params.web_params.url);
  if (payload.has_value() && payload->HasAllRequiredFields()) {
    CHECK(payload->email.has_value());
    callback_.Run(payload->email.value());
    return true;
  }

  return false;
}
