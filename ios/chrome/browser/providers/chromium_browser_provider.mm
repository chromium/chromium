// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_browser_provider.h"

#include <memory>

#import "ios/chrome/browser/providers/chromium_logo_controller.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrowserProvider::ChromiumBrowserProvider()
    : user_feedback_provider_(std::make_unique<UserFeedbackProvider>()),
      discover_feed_provider_(std::make_unique<DiscoverFeedProvider>()) {}

ChromiumBrowserProvider::~ChromiumBrowserProvider() {}

UITextField* ChromiumBrowserProvider::CreateStyledTextField() const {
  return [[UITextField alloc] initWithFrame:CGRectZero];
}

id<LogoVendor> ChromiumBrowserProvider::CreateLogoVendor(
    Browser* browser,
    web::WebState* web_state) const {
  return [[ChromiumLogoController alloc] init];
}

UserFeedbackProvider* ChromiumBrowserProvider::GetUserFeedbackProvider() const {
  return user_feedback_provider_.get();
}

DiscoverFeedProvider* ChromiumBrowserProvider::GetDiscoverFeedProvider() const {
  return discover_feed_provider_.get();
}

std::unique_ptr<ios::ChromeIdentityService>
ChromiumBrowserProvider::CreateChromeIdentityService() {
  return std::make_unique<ios::ChromeIdentityService>();
}
