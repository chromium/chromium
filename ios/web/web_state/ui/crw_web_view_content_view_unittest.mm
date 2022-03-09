// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/crw_web_view_content_view.h"

#import <UIKit/UIKit.h>

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

}  // namespace
