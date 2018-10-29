// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

#import <UIKit/UIKit.h>

#import "ios/web/web_state/ui/crw_web_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The typedef doesn't work with OCMock. Create a real class to be able to mock
// it.
@interface CRWFakeContentView : CRWContentView
@end

@implementation CRWFakeContentView
@synthesize contentOffset = _contentOffset;
@synthesize contentInset = _contentInset;
@synthesize scrollView = _scrollView;
@synthesize shouldUseViewContentInset = _shouldUseViewContentInset;

- (BOOL)isViewAlive {
  return YES;
}

@end

namespace {

using CRWWebViewProxyImplTest = PlatformTest;

// Tests that CRWWebViewProxyImpl returns the correct property values from
// the underlying CRWContentView.
TEST_F(CRWWebViewProxyImplTest, ContentViewPresent) {
  CRWWebViewProxyImpl* proxy = [[CRWWebViewProxyImpl alloc] init];
  CRWFakeContentView* fakeContentView = [[CRWFakeContentView alloc] init];
  proxy.contentView = fakeContentView;

  // Content inset.
  const UIEdgeInsets contentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  fakeContentView.contentInset = contentInset;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(contentInset, proxy.contentInset));

  // Set content inset.
  fakeContentView.contentInset = UIEdgeInsetsZero;
  proxy.contentInset = contentInset;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(contentInset,
                                            fakeContentView.contentInset));

  // Should use inset.
  fakeContentView.shouldUseViewContentInset = YES;
  EXPECT_TRUE(proxy.shouldUseViewContentInset);

  // Set should use inset.
  fakeContentView.shouldUseViewContentInset = NO;
  proxy.shouldUseViewContentInset = YES;
  EXPECT_TRUE(fakeContentView.shouldUseViewContentInset);
}

// Tests allowsBackForwardNavigationGestures property is delegated to
// CWVWebController.
TEST_F(CRWWebViewProxyImplTest, AllowsBackForwardNavigationGestures) {
  CRWWebController* mockWebController =
      OCMStrictClassMock([CRWWebController class]);
  CRWWebViewProxyImpl* proxy =
      [[CRWWebViewProxyImpl alloc] initWithWebController:mockWebController];

  OCMStub([mockWebController allowsBackForwardNavigationGestures])
      .andReturn(YES);
  EXPECT_TRUE(proxy.allowsBackForwardNavigationGestures);

  OCMExpect([mockWebController setAllowsBackForwardNavigationGestures:YES]);
  proxy.allowsBackForwardNavigationGestures = YES;
  EXPECT_OCMOCK_VERIFY((id)mockWebController);
}

}  // namespace
