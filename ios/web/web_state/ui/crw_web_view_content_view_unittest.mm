// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_content_view.h"

#import "ios/web/common/crw_viewport_controller.h"
#import "ios/web/web_state/crw_web_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface CRWWebViewContentView (Testing)
@property(nonatomic, assign) WebViewResizingType webViewResizingType;
- (void)setMinimumViewportInset:(UIEdgeInsets)minInset
           maximumViewportInset:(UIEdgeInsets)maxInset;
@end

namespace {

using CRWWebViewContentViewTest = PlatformTest;

// Tests the ContentInset method when shouldUseViewContentInset is set to YES.
TEST_F(CRWWebViewContentViewTest, ContentInsetWithInsetForPadding) {
  CRWWebView* webView = [[CRWWebView alloc] init];
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  [webView addSubview:scrollView];
  CRWWebViewContentView* contentView = [[CRWWebViewContentView alloc]
      initWithWebView:webView
           scrollView:scrollView
      fullscreenState:CrFullscreenState::kNotInFullScreen];
  contentView.shouldUseViewContentInset = YES;

  const UIEdgeInsets contentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  scrollView.contentInset = contentInset;
  EXPECT_TRUE(
      UIEdgeInsetsEqualToEdgeInsets(contentInset, contentView.contentInset));

  scrollView.contentInset = UIEdgeInsetsZero;
  contentView.contentInset = contentInset;
  EXPECT_TRUE(
      UIEdgeInsetsEqualToEdgeInsets(contentInset, scrollView.contentInset));
}

// Tests that viewport insets are not applied to the web view if its frame is
// too small, and that they are applied once the frame is expanded.
TEST_F(CRWWebViewContentViewTest, ViewportInsetsDeferredWhenFrameTooSmall) {
  CRWWebView* webView = [[CRWWebView alloc] initWithFrame:CGRectZero];
  webView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  [webView addSubview:scrollView];
  id mockWebView = OCMPartialMock(webView);

  CRWWebViewContentView* contentView = [[CRWWebViewContentView alloc]
      initWithWebView:webView
           scrollView:scrollView
      fullscreenState:CrFullscreenState::kNotInFullScreen];

  contentView.webViewResizingType = WebViewResizingType::kContentInset;

  UIWindow* window = [[UIWindow alloc] init];
  [window addSubview:contentView];
  [contentView layoutIfNeeded];

  UIEdgeInsets minInset = UIEdgeInsetsZero;
  UIEdgeInsets maxInset = UIEdgeInsetsMake(100, 0, 300, 0);

  // Frame is CGRectZero (width 0, height 0).
  // The inset requested is much larger. Verify it is safely caught and NOT
  // passed through to the WKWebView.
  __block int insetCallCount = 0;
  [[[mockWebView stub] andDo:^(NSInvocation* invocation) {
    insetCallCount++;
  }] setMinimumViewportInset:minInset maximumViewportInset:maxInset];

  [contentView setMinimumViewportInset:minInset maximumViewportInset:maxInset];
  EXPECT_EQ(0, insetCallCount);

  // Expand the frame to be large enough (e.g. 800x1000).
  // Expect the pending inset to finally be passed through during
  // layoutSubviews.
  contentView.bounds = CGRectMake(0, 0, 800, 1000);
  [contentView layoutSubviews];
  EXPECT_EQ(1, insetCallCount);
}

}  // namespace
