// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/cross_device_signin_url_interceptor.h"

#import <string>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

class CrossDeviceSigninURLInterceptorTest : public PlatformTest {
 protected:
  CrossDeviceSigninURLInterceptorTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        switches::kCrossDeviceSignin, {{switches::kCrossDeviceSigninUrl.name,
                                        "https://signin.example.com/"}});
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CrossDeviceSigninURLInterceptorTest, InterceptsValidUrl) {
  std::string intercepted_email;
  auto interceptor = std::make_unique<CrossDeviceSigninURLInterceptor>(
      base::BindRepeating([](std::string* out_email,
                             const std::string& email) { *out_email = email; },
                          &intercepted_email));

  EXPECT_TRUE(interceptor->active());
  EXPECT_TRUE(interceptor->prevent_normal_flow());
  EXPECT_FALSE(interceptor->deactivates_on_match());

  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(
      "https://signin.example.com/?email=user@example.com&entry_point_id=1"));
  interceptor->OnIntercept(params);

  EXPECT_EQ(intercepted_email, "user@example.com");
}

TEST_F(CrossDeviceSigninURLInterceptorTest,
       DoesNotInterceptIfMissingRequiredFields) {
  std::string intercepted_email;
  auto interceptor = std::make_unique<CrossDeviceSigninURLInterceptor>(
      base::BindRepeating([](std::string* out_email,
                             const std::string& email) { *out_email = email; },
                          &intercepted_email));

  // Missing entry_point_id.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(
      GURL("https://signin.example.com/?email=user@example.com"));
  interceptor->OnIntercept(params);

  EXPECT_TRUE(intercepted_email.empty());
}

TEST_F(CrossDeviceSigninURLInterceptorTest, DoesNotInterceptIfFeatureDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(switches::kCrossDeviceSignin);

  std::string intercepted_email;
  auto interceptor = std::make_unique<CrossDeviceSigninURLInterceptor>(
      base::BindRepeating([](std::string* out_email,
                             const std::string& email) { *out_email = email; },
                          &intercepted_email));

  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(
      "https://signin.example.com/?email=user@example.com&entry_point_id=1"));
  interceptor->OnIntercept(params);

  EXPECT_TRUE(intercepted_email.empty());
}

TEST_F(CrossDeviceSigninURLInterceptorTest, DoesNotInterceptIfIncognito) {
  std::string intercepted_email;
  auto interceptor = std::make_unique<CrossDeviceSigninURLInterceptor>(
      base::BindRepeating([](std::string* out_email,
                             const std::string& email) { *out_email = email; },
                          &intercepted_email));

  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(
      "https://signin.example.com/?email=user@example.com&entry_point_id=1"));
  params.in_incognito = true;

  interceptor->OnIntercept(params);

  EXPECT_TRUE(intercepted_email.empty());
  EXPECT_TRUE(interceptor->prevent_normal_flow());
}

}  // namespace
