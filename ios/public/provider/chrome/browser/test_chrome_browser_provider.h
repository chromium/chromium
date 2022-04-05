// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/user_feedback/test_user_feedback_provider.h"

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
  ChromeTrustedVaultService* GetChromeTrustedVaultService() override;
  TestUserFeedbackProvider* GetUserFeedbackProvider() const override;
  FollowProvider* GetFollowProvider() const override;

 private:
  // ChromeBrowserProvider:
  std::unique_ptr<ChromeIdentityService> CreateChromeIdentityService() override;

  std::unique_ptr<ChromeTrustedVaultService> chrome_trusted_vault_service_;
  std::unique_ptr<TestUserFeedbackProvider> user_feedback_provider_;
  std::unique_ptr<FollowProvider> follow_provider_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
