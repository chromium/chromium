// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_view_only/wk_web_view_configuration_util.h"

#import <WebKit/WebKit.h>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "url/gurl.h"

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

// Tests that web::PrewarmWebView() creates the web view without creating a
// navigation item or registering a web frame.
TEST_F(WebViewOnlyAPITest, PrewarmWebView) {
  [web_controller() removeWebView];
  ASSERT_FALSE(web_controller().webViewNavigationProxy);
  web::PrewarmWebView(web_state());
  id web_view = web_controller().webViewNavigationProxy;
  ASSERT_TRUE(web_view);

  // No navigation item is created.
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  EXPECT_EQ(0, navigation_manager->GetItemCount());
  EXPECT_FALSE(navigation_manager->GetPendingItem());

  // Calling it again keeps the same web view.
  web::PrewarmWebView(web_state());
  EXPECT_EQ(web_view, web_controller().webViewNavigationProxy);

  // Nothing is loaded later either, so no navigation item or main frame shows
  // up.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  EXPECT_EQ(0, navigation_manager->GetItemCount());
  EXPECT_FALSE(navigation_manager->GetPendingItem());
  EXPECT_FALSE(web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame());

  [web_controller() removeWebView];
}

// Tests that the first page loaded after web::PrewarmWebView() is the only
// navigation item, and that web::PrewarmWebView() does nothing afterwards.
TEST_F(WebViewOnlyAPITest, PrewarmWebViewThenLoad) {
  [web_controller() removeWebView];
  web::PrewarmWebView(web_state());
  ASSERT_TRUE(web_controller().webViewNavigationProxy);

  const GURL url("https://chromium.test/");
  LoadHtml(@"<p>prewarmed</p>", url);
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  EXPECT_EQ(1, navigation_manager->GetItemCount());
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());

  web::PrewarmWebView(web_state());
  EXPECT_EQ(1, navigation_manager->GetItemCount());
  EXPECT_FALSE(navigation_manager->GetPendingItem());
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());
}

// Tests that web::PrewarmWebView() does nothing when web usage is disabled.
TEST_F(WebViewOnlyAPITest, PrewarmWebViewWebUsageDisabled) {
  web_state()->SetWebUsageEnabled(false);
  web::PrewarmWebView(web_state());
  EXPECT_FALSE(web_controller().webViewNavigationProxy);
  web_state()->SetWebUsageEnabled(true);
}

}  // namespace web
