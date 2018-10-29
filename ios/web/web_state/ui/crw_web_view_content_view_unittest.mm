// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"

#import <UIKit/UIKit.h>

#import "ios/web/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CRWWebViewContentViewTest = PlatformTest;

// Tests the ContentInset method when shouldUseViewContentInset is set to YES.
TEST_F(CRWWebViewContentViewTest, ContentInsetWithInsetForPadding) {
  UIView* webView = [[UIView alloc] init];
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  [webView addSubview:scrollView];
  CRWWebViewContentView* contentView =
      [[CRWWebViewContentView alloc] initWithWebView:webView
                                          scrollView:scrollView];
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

// Tests the ContentInset method when shouldUseViewContentInset is set to NO.
TEST_F(CRWWebViewContentViewTest, ContentInsetWithoutInsetForPadding) {
  // This functionality has been moved out of the web// layer when
  // kOutOfWebFullscreen is enabled.
  if (base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen))
    return;

  UIView* webView = [[UIView alloc] init];
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  [webView addSubview:scrollView];
  CRWWebViewContentView* contentView =
      [[CRWWebViewContentView alloc] initWithWebView:webView
                                          scrollView:scrollView];
  contentView.shouldUseViewContentInset = NO;
  const CGRect frame = CGRectMake(0, 0, 100, 100);
  contentView.frame = frame;

  // Check that the content inset of the scroll view is not taken into account.
  const UIEdgeInsets contentInset = UIEdgeInsetsMake(10, 20, 30, 40);
  scrollView.contentInset = contentInset;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(UIEdgeInsetsZero,
                                            contentView.contentInset));

  // Set the content inset.
  const CGRect resultFrame = CGRectMake(20, 10, 40, 60);
  webView.frame = CGRectZero;
  contentView.contentInset = contentInset;
  EXPECT_TRUE(CGRectEqualToRect(resultFrame, webView.frame));
}

}  // namespace
