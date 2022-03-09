// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#import <UIKit/UIKit.h>

#include "base/check.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#include "ios/public/provider/chrome/browser/mailto/test_mailto_handler_provider.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_trusted_vault_service.h"
#import "ios/public/provider/chrome/browser/user_feedback/test_user_feedback_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : user_feedback_provider_(std::make_unique<TestUserFeedbackProvider>()),
      mailto_handler_provider_(std::make_unique<TestMailtoHandlerProvider>()),
      follow_provider_(std::make_unique<FollowProvider>()) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider& TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider& provider = GetChromeBrowserProvider();
  return static_cast<TestChromeBrowserProvider&>(provider);
}

ChromeTrustedVaultService*
TestChromeBrowserProvider::GetChromeTrustedVaultService() {
  if (!chrome_trusted_vault_service_) {
    chrome_trusted_vault_service_.reset(new FakeChromeTrustedVaultService());
  }
  return chrome_trusted_vault_service_.get();
}

TestUserFeedbackProvider* TestChromeBrowserProvider::GetUserFeedbackProvider()
    const {
  return user_feedback_provider_.get();
}

MailtoHandlerProvider* TestChromeBrowserProvider::GetMailtoHandlerProvider()
    const {
  return mailto_handler_provider_.get();
}

FollowProvider* TestChromeBrowserProvider::GetFollowProvider() const {
  return follow_provider_.get();
}

std::unique_ptr<ChromeIdentityService>
TestChromeBrowserProvider::CreateChromeIdentityService() {
  return std::make_unique<FakeChromeIdentityService>();
}

}  // namespace ios
