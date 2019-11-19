// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

class ChromiumBrowserProvider : public ios::ChromeBrowserProvider {
 public:
  ChromiumBrowserProvider();
  ~ChromiumBrowserProvider() override;

  // ChromeBrowserProvider implementation
  ios::SigninErrorProvider* GetSigninErrorProvider() override;
  ios::SigninResourcesProvider* GetSigninResourcesProvider() override;
  void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ios::ChromeIdentityService> service) override;
  ios::ChromeIdentityService* GetChromeIdentityService() override;
  UITextField* CreateStyledTextField() const override NS_RETURNS_RETAINED;
  VoiceSearchProvider* GetVoiceSearchProvider() const override;
  id<LogoVendor> CreateLogoVendor(ios::ChromeBrowserState* browser_state)
      const override NS_RETURNS_RETAINED;
  UserFeedbackProvider* GetUserFeedbackProvider() const override;
  AppDistributionProvider* GetAppDistributionProvider() const override;
  BrandedImageProvider* GetBrandedImageProvider() const override;
  SpotlightProvider* GetSpotlightProvider() const override;
  FullscreenProvider* GetFullscreenProvider() const override;
  OverridesProvider* GetOverridesProvider() const override;

 private:
  std::unique_ptr<AppDistributionProvider> app_distribution_provider_;
  std::unique_ptr<BrandedImageProvider> branded_image_provider_;
  std::unique_ptr<ios::SigninErrorProvider> signin_error_provider_;
  std::unique_ptr<ios::SigninResourcesProvider> signin_resources_provider_;
  std::unique_ptr<ios::ChromeIdentityService> chrome_identity_service_;
  std::unique_ptr<UserFeedbackProvider> user_feedback_provider_;
  std::unique_ptr<VoiceSearchProvider> voice_search_provider_;
  std::unique_ptr<SpotlightProvider> spotlight_provider_;
  std::unique_ptr<FullscreenProvider> fullscreen_provider_;
  std::unique_ptr<OverridesProvider> overrides_provider_;
};

#endif  // IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_
