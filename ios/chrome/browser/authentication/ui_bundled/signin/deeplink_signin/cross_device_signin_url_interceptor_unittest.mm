// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/cross_device_signin_url_interceptor.h"

#import <string>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_deep_link_payload.h"
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
  signin::SigninDeepLinkPayload intercepted_payload;
  auto interceptor =
      std::make_unique<CrossDeviceSigninURLInterceptor>(base::BindRepeating(
          [](signin::SigninDeepLinkPayload* out_payload,
             const signin::SigninDeepLinkPayload& payload) {
            *out_payload = payload;
          },
          &intercepted_payload));

  EXPECT_TRUE(interceptor->active());
  EXPECT_TRUE(interceptor->prevent_normal_flow());
  EXPECT_FALSE(interceptor->deactivates_on_match());

  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(
      "https://signin.example.com/?email=user@example.com&entry_point_id=1"));
  EXPECT_TRUE(interceptor->OnIntercept(params));

  EXPECT_EQ(intercepted_payload.email, "user@example.com");
  EXPECT_EQ(intercepted_payload.entry_point_id,
            signin::ExternalEntryPoint::kDesktopDefault);
  EXPECT_EQ(intercepted_payload.entry_point_id_raw_value_for_metrics, 1);
}

TEST_F(CrossDeviceSigninURLInterceptorTest,
       DoesNotInterceptIfMissingRequiredFields) {
  signin::SigninDeepLinkPayload intercepted_payload;
  auto interceptor =
      std::make_unique<CrossDeviceSigninURLInterceptor>(base::BindRepeating(
          [](signin::SigninDeepLinkPayload* out_payload,
             const signin::SigninDeepLinkPayload& payload) {
            *out_payload = payload;
          },
          &intercepted_payload));

  // Missing entry_point_id.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(
      GURL("https://signin.example.com/?email=user@example.com"));
  EXPECT_FALSE(interceptor->OnIntercept(params));

  EXPECT_FALSE(intercepted_payload.email.has_value());
}

TEST_F(CrossDeviceSigninURLInterceptorTest, DoesNotInterceptIfFeatureDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(switches::kCrossDeviceSignin);

  signin::SigninDeepLinkPayload intercepted_payload;
  auto interceptor =
      std::make_unique<CrossDeviceSigninURLInterceptor>(base::BindRepeating(
          [](signin::SigninDeepLinkPayload* out_payload,
             const signin::SigninDeepLinkPayload& payload) {
            *out_payload = payload;
          },
          &intercepted_payload));

  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(
      "https://signin.example.com/?email=user@example.com&entry_point_id=1"));
  EXPECT_FALSE(interceptor->OnIntercept(params));

  EXPECT_FALSE(intercepted_payload.email.has_value());
}

TEST_F(CrossDeviceSigninURLInterceptorTest, DoesNotInterceptIfIncognito) {
  signin::SigninDeepLinkPayload intercepted_payload;
  auto interceptor =
      std::make_unique<CrossDeviceSigninURLInterceptor>(base::BindRepeating(
          [](signin::SigninDeepLinkPayload* out_payload,
             const signin::SigninDeepLinkPayload& payload) {
            *out_payload = payload;
          },
          &intercepted_payload));

  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(
      "https://signin.example.com/?email=user@example.com&entry_point_id=1"));
  params.in_incognito = true;

  EXPECT_FALSE(interceptor->OnIntercept(params));

  EXPECT_FALSE(intercepted_payload.email.has_value());
  EXPECT_TRUE(interceptor->prevent_normal_flow());
}

}  // namespace
