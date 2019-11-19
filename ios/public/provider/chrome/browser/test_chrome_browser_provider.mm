// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "ios/public/provider/chrome/browser/distribution/test_app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/images/test_branded_image_provider.h"
#include "ios/public/provider/chrome/browser/mailto/test_mailto_handler_provider.h"
#include "ios/public/provider/chrome/browser/omaha/test_omaha_service_provider.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/test_signin_resources_provider.h"
#import "ios/public/provider/chrome/browser/spotlight/test_spotlight_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/test_user_feedback_provider.h"
#import "ios/public/provider/chrome/browser/voice/test_voice_search_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_language.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : app_distribution_provider_(
          std::make_unique<TestAppDistributionProvider>()),
      branded_image_provider_(std::make_unique<TestBrandedImageProvider>()),
      omaha_service_provider_(std::make_unique<TestOmahaServiceProvider>()),
      signin_resources_provider_(
          std::make_unique<TestSigninResourcesProvider>()),
      voice_search_provider_(std::make_unique<TestVoiceSearchProvider>()),
      user_feedback_provider_(std::make_unique<TestUserFeedbackProvider>()),
      spotlight_provider_(std::make_unique<TestSpotlightProvider>()),
      mailto_handler_provider_(std::make_unique<TestMailtoHandlerProvider>()),
      fullscreen_provider_(std::make_unique<FullscreenProvider>()) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider* TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider* provider = GetChromeBrowserProvider();
  DCHECK(provider);
  return static_cast<TestChromeBrowserProvider*>(provider);
}

SigninResourcesProvider*
TestChromeBrowserProvider::GetSigninResourcesProvider() {
  return signin_resources_provider_.get();
}

void TestChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {
  chrome_identity_service_.swap(service);
}

ChromeIdentityService* TestChromeBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_.reset(new FakeChromeIdentityService());
  }
  return chrome_identity_service_.get();
}

UITextField* TestChromeBrowserProvider::CreateStyledTextField() const {
  return [[UITextField alloc] initWithFrame:CGRectZero];
}

VoiceSearchProvider* TestChromeBrowserProvider::GetVoiceSearchProvider() const {
  return voice_search_provider_.get();
}

AppDistributionProvider* TestChromeBrowserProvider::GetAppDistributionProvider()
    const {
  return app_distribution_provider_.get();
}

OmahaServiceProvider* TestChromeBrowserProvider::GetOmahaServiceProvider()
    const {
  return omaha_service_provider_.get();
}

UserFeedbackProvider* TestChromeBrowserProvider::GetUserFeedbackProvider()
    const {
  return user_feedback_provider_.get();
}

SpotlightProvider* TestChromeBrowserProvider::GetSpotlightProvider() const {
  return spotlight_provider_.get();
}

FullscreenProvider* TestChromeBrowserProvider::GetFullscreenProvider() const {
  return fullscreen_provider_.get();
}

BrandedImageProvider* TestChromeBrowserProvider::GetBrandedImageProvider()
    const {
  return branded_image_provider_.get();
}

MailtoHandlerProvider* TestChromeBrowserProvider::GetMailtoHandlerProvider()
    const {
  return mailto_handler_provider_.get();
}

}  // namespace ios
