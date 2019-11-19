// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace ios {

class TestChromeBrowserProvider : public ChromeBrowserProvider {
 public:
  TestChromeBrowserProvider();
  ~TestChromeBrowserProvider() override;

  // Returns the current provider as a |TestChromeBrowserProvider|.
  static TestChromeBrowserProvider* GetTestProvider();

  // ChromeBrowserProvider:
  SigninResourcesProvider* GetSigninResourcesProvider() override;
  void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ChromeIdentityService> service) override;
  ChromeIdentityService* GetChromeIdentityService() override;
  UITextField* CreateStyledTextField() const override NS_RETURNS_RETAINED;
  VoiceSearchProvider* GetVoiceSearchProvider() const override;
  AppDistributionProvider* GetAppDistributionProvider() const override;
  OmahaServiceProvider* GetOmahaServiceProvider() const override;
  UserFeedbackProvider* GetUserFeedbackProvider() const override;
  SpotlightProvider* GetSpotlightProvider() const override;
  FullscreenProvider* GetFullscreenProvider() const override;
  BrandedImageProvider* GetBrandedImageProvider() const override;
  MailtoHandlerProvider* GetMailtoHandlerProvider() const override;

 private:
  std::unique_ptr<AppDistributionProvider> app_distribution_provider_;
  std::unique_ptr<BrandedImageProvider> branded_image_provider_;
  std::unique_ptr<ChromeIdentityService> chrome_identity_service_;
  std::unique_ptr<OmahaServiceProvider> omaha_service_provider_;
  std::unique_ptr<SigninResourcesProvider> signin_resources_provider_;
  std::unique_ptr<VoiceSearchProvider> voice_search_provider_;
  std::unique_ptr<UserFeedbackProvider> user_feedback_provider_;
  std::unique_ptr<SpotlightProvider> spotlight_provider_;
  std::unique_ptr<MailtoHandlerProvider> mailto_handler_provider_;
  std::unique_ptr<FullscreenProvider> fullscreen_provider_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeBrowserProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
