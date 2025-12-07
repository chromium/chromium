// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#import "ios/web/common/user_agent.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web_view/internal/cwv_global_state_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"

namespace ios_web_view {

class WebViewWebClientTest : public web::WebTest {
 public:
  WebViewWebClientTest() : web::WebTest(std::make_unique<WebViewWebClient>()) {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  WebViewWebClientTest(const WebViewWebClientTest&) = delete;
  WebViewWebClientTest& operator=(const WebViewWebClientTest&) = delete;

  ~WebViewWebClientTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

  void SetUp() override {
    web::WebTest::SetUp();
    CWVGlobalState.sharedInstance.customUserAgent = nil;
  }

  void TearDown() override {
    web::WebTest::TearDown();
    CWVGlobalState.sharedInstance.customUserAgent = nil;
  }
};

// Tests that WebViewWebClientTest's GetUserAgent is configured by
// CWVGlobalState.
TEST_F(WebViewWebClientTest, GetUserAgent) {
  web::WebClient* web_client = GetWebClient();

  // Test user agent when neither nor CWVGlobalState.userAgentProduct
  // CWVGlobalState.customUserAgent have been set.
  std::string user_agent_with_empty_product = web::BuildMobileUserAgent("");
  EXPECT_EQ(user_agent_with_empty_product,
            web_client->GetUserAgent(web::UserAgentType::NONE));
  EXPECT_EQ(user_agent_with_empty_product,
            web_client->GetUserAgent(web::UserAgentType::AUTOMATIC));
  EXPECT_EQ(user_agent_with_empty_product,
            web_client->GetUserAgent(web::UserAgentType::MOBILE));
  EXPECT_EQ(user_agent_with_empty_product,
            web_client->GetUserAgent(web::UserAgentType::DESKTOP));

  // Test user agent when only CWVGlobalState.userAgentProduct is set.
  CWVGlobalState.sharedInstance.userAgentProduct = @"FooProduct";
  std::string user_agent_with_product = web::BuildMobileUserAgent("FooProduct");
  EXPECT_EQ(user_agent_with_product,
            web_client->GetUserAgent(web::UserAgentType::NONE));
  EXPECT_EQ(user_agent_with_product,
            web_client->GetUserAgent(web::UserAgentType::AUTOMATIC));
  EXPECT_EQ(user_agent_with_product,
            web_client->GetUserAgent(web::UserAgentType::MOBILE));
  EXPECT_EQ(user_agent_with_product,
            web_client->GetUserAgent(web::UserAgentType::DESKTOP));

  // Test user agent when both CWVGlobalState.customUserAgent and
  // CWVGlobalState.userAgentProduct are set.
  CWVGlobalState.sharedInstance.customUserAgent = @"FooCustomUserAgent";
  EXPECT_EQ("FooCustomUserAgent",
            web_client->GetUserAgent(web::UserAgentType::NONE));
  EXPECT_EQ("FooCustomUserAgent",
            web_client->GetUserAgent(web::UserAgentType::AUTOMATIC));
  EXPECT_EQ("FooCustomUserAgent",
            web_client->GetUserAgent(web::UserAgentType::MOBILE));
  EXPECT_EQ("FooCustomUserAgent",
            web_client->GetUserAgent(web::UserAgentType::DESKTOP));
}

// Tests that `WebViewWebClientTest::EnableWebInspector` is configurable.
TEST_F(WebViewWebClientTest, EnableWebInspector) {
  web::WebClient* web_client = GetWebClient();

  CWVWebView.webInspectorEnabled = NO;
  EXPECT_FALSE(web_client->EnableWebInspector(/*browser_state=*/nullptr));

  CWVWebView.webInspectorEnabled = YES;
  EXPECT_TRUE(web_client->EnableWebInspector(/*browser_state=*/nullptr));
}

}  // namespace ios_web_view
