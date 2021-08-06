// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace ios {

class TestChromeBrowserProvider : public ChromeBrowserProvider {
 public:
  TestChromeBrowserProvider();
  ~TestChromeBrowserProvider() override;

  TestChromeBrowserProvider(const TestChromeBrowserProvider&) = delete;
  TestChromeBrowserProvider& operator=(const TestChromeBrowserProvider&) =
      delete;

  // Returns the current provider as a |TestChromeBrowserProvider|.
  static TestChromeBrowserProvider& GetTestProvider();

  // ChromeBrowserProvider:
  SigninResourcesProvider* GetSigninResourcesProvider() override;
  void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ChromeIdentityService> service) override;
  ChromeIdentityService* GetChromeIdentityService() override;
  ChromeTrustedVaultService* GetChromeTrustedVaultService() override;
  UITextField* CreateStyledTextField() const override NS_RETURNS_RETAINED;
  VoiceSearchProvider* GetVoiceSearchProvider() const override;
  OmahaServiceProvider* GetOmahaServiceProvider() const override;
  UserFeedbackProvider* GetUserFeedbackProvider() const override;
  MailtoHandlerProvider* GetMailtoHandlerProvider() const override;
  DiscoverFeedProvider* GetDiscoverFeedProvider() const override;

 private:
  std::unique_ptr<ChromeIdentityService> chrome_identity_service_;
  std::unique_ptr<ChromeTrustedVaultService> chrome_trusted_vault_service_;
  std::unique_ptr<OmahaServiceProvider> omaha_service_provider_;
  std::unique_ptr<SigninResourcesProvider> signin_resources_provider_;
  std::unique_ptr<VoiceSearchProvider> voice_search_provider_;
  std::unique_ptr<UserFeedbackProvider> user_feedback_provider_;
  std::unique_ptr<MailtoHandlerProvider> mailto_handler_provider_;
  std::unique_ptr<DiscoverFeedProvider> discover_feed_provider_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
