// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_browser_provider.h"

#include <memory>

#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrowserProvider::ChromiumBrowserProvider()
    : user_feedback_provider_(std::make_unique<UserFeedbackProvider>()),
      follow_provider_(std::make_unique<FollowProvider>()) {}

ChromiumBrowserProvider::~ChromiumBrowserProvider() {}

UserFeedbackProvider* ChromiumBrowserProvider::GetUserFeedbackProvider() const {
  return user_feedback_provider_.get();
}

FollowProvider* ChromiumBrowserProvider::GetFollowProvider() const {
  return follow_provider_.get();
}

std::unique_ptr<ios::ChromeIdentityService>
ChromiumBrowserProvider::CreateChromeIdentityService() {
  return std::make_unique<ios::ChromeIdentityService>();
}
