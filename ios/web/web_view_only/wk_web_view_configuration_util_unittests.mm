// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_view_only/wk_web_view_configuration_util.h"

#import <WebKit/WebKit.h>

#import "ios/web/test/web_test_with_web_controller.h"

namespace web {

// Tests for all web_view_only APIs in //ios/web/public/web_view_only
using WebViewOnlyAPITest = web::WebTestWithWebController;

// A test for web::EnsureWebViewCreatedWithConfiguration()
TEST_F(WebViewOnlyAPITest, EnsureWebViewCreatedWithConfiguration) {
  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  const CGFloat kMinimumFontSize = 99;
  config.preferences.minimumFontSize = kMinimumFontSize;

  [web_controller() removeWebView];
  WKWebView* web_view =
      web::EnsureWebViewCreatedWithConfiguration(web_state(), config);

  ASSERT_TRUE(web_view);
  EXPECT_EQ(kMinimumFontSize,
            web_view.configuration.preferences.minimumFontSize);

  [web_controller() removeWebView];
}

}  // namespace web
