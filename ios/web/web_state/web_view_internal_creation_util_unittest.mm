// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import <CoreGraphics/CoreGraphics.h>
#import <WebKit/WebKit.h>

#import "base/memory/ptr_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/crw_web_view.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"

namespace {

// An arbitrary sized frame for testing web view creation.
const CGRect kTestFrame = CGRectMake(5.0f, 10.0f, 15.0f, 20.0f);

// A WebClient that stubs PreWebViewCreation call for testing purposes.
class CreationUtilsWebClient : public web::FakeWebClient {
 public:
  MOCK_CONST_METHOD0(PreWebViewCreation, void());
};
}  // namespace

namespace web {

// Test fixture for testing web view creation.
class WebViewCreationUtilsTest : public WebTest {
 public:
  WebViewCreationUtilsTest()
      : web_client_(base::WrapUnique(new CreationUtilsWebClient)) {}

 protected:
  CreationUtilsWebClient* creation_utils_web_client() {
    return static_cast<CreationUtilsWebClient*>(web_client_.Get());
  }

 private:
  // WebClient that stubs PreWebViewCreation.
  web::ScopedTestingWebClient web_client_;
};

// Tests web::BuildWKWebView function that it correctly returns a WKWebView
// with the correct frame and calls WebClient::PreWebViewCreation method.
TEST_F(WebViewCreationUtilsTest, WKWebViewCreationWithBrowserState) {
  EXPECT_CALL(*creation_utils_web_client(), PreWebViewCreation()).Times(1);

  WKWebView* web_view = BuildWKWebView(kTestFrame, GetBrowserState());

  EXPECT_TRUE([web_view isKindOfClass:[WKWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));
}

// Tests web::BuildWKWebView function that it correctly returns a WKWebView
// with the correct frame and calls WebClient::PreWebViewCreation method.
TEST_F(WebViewCreationUtilsTest, BuildWKWebViewForQueries) {
  EXPECT_CALL(*creation_utils_web_client(), PreWebViewCreation()).Times(0);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKWebView* web_view = BuildWKWebViewForQueries(
      config_provider.GetWebViewConfiguration(), GetBrowserState());

  EXPECT_TRUE([web_view isKindOfClass:[WKWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(CGRectZero, [web_view frame]));
}

}  // namespace web
