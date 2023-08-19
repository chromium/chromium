// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>

#import "base/test/ios/wait_util.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#include "testing/gtest_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace ios_web_view {

using WebViewRestorableStateTest = ios_web_view::WebViewInttestBase;

// Tests encodeRestorableStateWithCoder: and decodeRestorableStateWithCoder:
// methods.
TEST_F(WebViewRestorableStateTest, EncodeDecode) {
  // Load 2 URLs to create non-default state.
  ASSERT_FALSE([web_view_ lastCommittedURL]);
  ASSERT_FALSE([web_view_ visibleURL]);
  ASSERT_FALSE([web_view_ canGoBack]);
  ASSERT_FALSE([web_view_ canGoForward]);
  ASSERT_TRUE(test::LoadUrl(web_view_, [NSURL URLWithString:@"about:newtab"]));
  ASSERT_NSEQ(@"about:newtab", [web_view_ lastCommittedURL].absoluteString);
  ASSERT_NSEQ(@"about:newtab", [web_view_ visibleURL].absoluteString);
  ASSERT_TRUE(test::LoadUrl(web_view_, [NSURL URLWithString:@"about:blank"]));
  ASSERT_NSEQ(@"about:blank", [web_view_ lastCommittedURL].absoluteString);
  ASSERT_NSEQ(@"about:blank", [web_view_ visibleURL].absoluteString);
  ASSERT_TRUE([web_view_ canGoBack]);
  ASSERT_FALSE([web_view_ canGoForward]);

  // Create second web view and restore its state from the first web view.
  CWVWebView* restored_web_view = test::CreateWebView();
  test::CopyWebViewState(web_view_, restored_web_view);

  // The WKWebView must be present in the view hierarchy in order to prevent
  // WebKit optimizations which may pause internal parts of the web view
  // without notice. Work around this by adding the view directly.
  UIViewController* view_controller = [GetAnyKeyWindow() rootViewController];
  [view_controller.view addSubview:restored_web_view];

  // Wait for restore to finish.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return ![restored_web_view isLoading];
  }));

  // Verify that the state has been restored correctly.
  EXPECT_NSEQ(@"about:blank",
              [restored_web_view lastCommittedURL].absoluteString);
  EXPECT_NSEQ(@"about:blank", [restored_web_view visibleURL].absoluteString);
  EXPECT_TRUE([restored_web_view canGoBack]);
  EXPECT_FALSE([restored_web_view canGoForward]);
}

}  // namespace ios_web_view
