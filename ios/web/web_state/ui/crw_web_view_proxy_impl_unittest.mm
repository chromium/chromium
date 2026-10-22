// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

#import <UIKit/UIKit.h>

#import "ios/web/web_state/ui/crw_web_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// The typedef doesn't work with OCMock. Create a real class to be able to mock
// it.
@interface CRWFakeContentView : CRWContentView
// The last values passed to `setMinimumViewportInset:maximumViewportInset:`.
@property(nonatomic, readonly) UIEdgeInsets minimumViewportInset;
@property(nonatomic, readonly) UIEdgeInsets maximumViewportInset;
// The number of calls to `setMinimumViewportInset:maximumViewportInset:`.
@property(nonatomic, readonly) int viewportInsetsCallCount;
@end

@implementation CRWFakeContentView
@synthesize contentOffset = _contentOffset;
@synthesize contentInset = _contentInset;
@synthesize obscuredInsets = _obscuredInsets;
@synthesize scrollView = _scrollView;
@synthesize shouldUseViewContentInset = _shouldUseViewContentInset;

- (BOOL)isViewAlive {
  return YES;
}

- (void)setObscuredInsets:(UIEdgeInsets)obscuredInsets
          initialVelocity:(CGFloat)initialVelocity {
  _obscuredInsets = obscuredInsets;
}

- (void)setMinimumViewportInset:(UIEdgeInsets)minInset
           maximumViewportInset:(UIEdgeInsets)maxInset {
  _minimumViewportInset = minInset;
  _maximumViewportInset = maxInset;
  _viewportInsetsCallCount++;
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

// Tests that `shouldUseViewContentInset` and the viewport insets set before a
// content view exists are applied once it is set, which happens for WebStates
// shown before their web view is created (e.g. windows opened by DOM).
TEST_F(CRWWebViewProxyImplTest, AppliesCachedValuesToLaterContentView) {
  CRWWebViewProxyImpl* proxy = [[CRWWebViewProxyImpl alloc] init];
  const UIEdgeInsets minInset = UIEdgeInsetsMake(10, 0, 0, 0);
  const UIEdgeInsets maxInset = UIEdgeInsetsMake(20, 0, 30, 0);
  proxy.shouldUseViewContentInset = YES;
  [proxy setMinimumViewportInset:minInset maximumViewportInset:maxInset];

  CRWFakeContentView* fakeContentView = [[CRWFakeContentView alloc] init];
  proxy.contentView = fakeContentView;
  EXPECT_TRUE(fakeContentView.shouldUseViewContentInset);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      minInset, fakeContentView.minimumViewportInset));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      maxInset, fakeContentView.maximumViewportInset));

  // A replacement content view receives the same values.
  CRWFakeContentView* replacementContentView =
      [[CRWFakeContentView alloc] init];
  proxy.contentView = replacementContentView;
  EXPECT_TRUE(replacementContentView.shouldUseViewContentInset);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      minInset, replacementContentView.minimumViewportInset));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      maxInset, replacementContentView.maximumViewportInset));
}

// Tests that the cached values are re-applied to a content view set after the
// previous one was cleared.
TEST_F(CRWWebViewProxyImplTest, AppliesCachedValuesAfterContentViewCleared) {
  CRWWebViewProxyImpl* proxy = [[CRWWebViewProxyImpl alloc] init];
  CRWFakeContentView* initialContentView = [[CRWFakeContentView alloc] init];
  proxy.contentView = initialContentView;
  const UIEdgeInsets minInset = UIEdgeInsetsMake(10, 0, 0, 0);
  const UIEdgeInsets maxInset = UIEdgeInsetsMake(20, 0, 30, 0);
  proxy.shouldUseViewContentInset = YES;
  [proxy setMinimumViewportInset:minInset maximumViewportInset:maxInset];

  [proxy clearContentViewAndAddPlaceholder:YES];
  EXPECT_FALSE(proxy.shouldUseViewContentInset);

  CRWFakeContentView* fakeContentView = [[CRWFakeContentView alloc] init];
  proxy.contentView = fakeContentView;
  EXPECT_TRUE(fakeContentView.shouldUseViewContentInset);
  EXPECT_TRUE(proxy.shouldUseViewContentInset);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      minInset, fakeContentView.minimumViewportInset));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      maxInset, fakeContentView.maximumViewportInset));
}

// Tests that the last `shouldUseViewContentInset` value is the one applied to
// a new content view.
TEST_F(CRWWebViewProxyImplTest, AppliesLatestShouldUseViewContentInset) {
  CRWWebViewProxyImpl* proxy = [[CRWWebViewProxyImpl alloc] init];
  proxy.shouldUseViewContentInset = YES;
  proxy.shouldUseViewContentInset = NO;

  CRWFakeContentView* fakeContentView = [[CRWFakeContentView alloc] init];
  fakeContentView.shouldUseViewContentInset = YES;
  proxy.contentView = fakeContentView;
  EXPECT_FALSE(fakeContentView.shouldUseViewContentInset);
}

// Tests that viewport insets are not forwarded to a new content view if they
// were never set.
TEST_F(CRWWebViewProxyImplTest, DoesNotApplyUnsetViewportInsets) {
  CRWWebViewProxyImpl* proxy = [[CRWWebViewProxyImpl alloc] init];
  CRWFakeContentView* fakeContentView = [[CRWFakeContentView alloc] init];
  proxy.contentView = fakeContentView;
  EXPECT_EQ(0, fakeContentView.viewportInsetsCallCount);
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

// Tests allowsLinkPreview property is delegated to CWVWebController.
TEST_F(CRWWebViewProxyImplTest, AllowsLinkPreview) {
  CRWWebController* mockWebController =
      OCMStrictClassMock([CRWWebController class]);
  CRWWebViewProxyImpl* proxy =
      [[CRWWebViewProxyImpl alloc] initWithWebController:mockWebController];

  OCMStub([mockWebController allowsLinkPreview]).andReturn(YES);
  EXPECT_TRUE(proxy.allowsLinkPreview);

  OCMExpect([mockWebController setAllowsLinkPreview:YES]);
  proxy.allowsLinkPreview = YES;
  EXPECT_OCMOCK_VERIFY((id)mockWebController);
}

// Tests that setting obscuredInsets has no effect when ignoreObscuredInsets is
// YES.
TEST_F(CRWWebViewProxyImplTest, IgnoreObscuredInsets) {
  CRWWebViewProxyImpl* proxy = [[CRWWebViewProxyImpl alloc] init];
  CRWFakeContentView* fakeContentView = [[CRWFakeContentView alloc] init];
  proxy.contentView = fakeContentView;

  proxy.ignoreObscuredInsets = YES;
  EXPECT_TRUE(proxy.ignoreObscuredInsets);

  const UIEdgeInsets obscuredInsets = UIEdgeInsetsMake(10, 10, 10, 10);
  proxy.obscuredInsets = obscuredInsets;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(UIEdgeInsetsZero,
                                            fakeContentView.obscuredInsets));

  proxy.ignoreObscuredInsets = NO;
  EXPECT_FALSE(proxy.ignoreObscuredInsets);

  proxy.obscuredInsets = obscuredInsets;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(obscuredInsets,
                                            fakeContentView.obscuredInsets));
}

// Tests obscuredContentInsets property is delegated to CRWWebController.
TEST_F(CRWWebViewProxyImplTest, ObscuredContentInsets) {
  if (@available(iOS 26.0, *)) {
    CRWWebController* mockWebController =
        OCMStrictClassMock([CRWWebController class]);
    CRWWebViewProxyImpl* proxy =
        [[CRWWebViewProxyImpl alloc] initWithWebController:mockWebController];

    UIEdgeInsets test_insets = UIEdgeInsetsMake(10, 20, 30, 40);
    OCMStub([mockWebController obscuredContentInsets]).andReturn(test_insets);
    EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(test_insets,
                                              proxy.obscuredContentInsets));

    OCMExpect([mockWebController setObscuredContentInsets:test_insets]);
    proxy.obscuredContentInsets = test_insets;
    EXPECT_OCMOCK_VERIFY((id)mockWebController);
  }
}

// Tests that `shouldSuppressInputViews` is delegated to `CRWWebController`.
TEST_F(CRWWebViewProxyImplTest, ShouldSuppressInputViews) {
  CRWWebController* mockWebController =
      OCMStrictClassMock([CRWWebController class]);
  CRWWebViewProxyImpl* proxy =
      [[CRWWebViewProxyImpl alloc] initWithWebController:mockWebController];

  OCMExpect([mockWebController shouldSuppressInputViews]).andReturn(YES);
  EXPECT_TRUE(proxy.shouldSuppressInputViews);

  OCMExpect([mockWebController setShouldSuppressInputViews:YES]);
  proxy.shouldSuppressInputViews = YES;

  OCMExpect([mockWebController shouldSuppressInputViews]).andReturn(NO);
  EXPECT_FALSE(proxy.shouldSuppressInputViews);

  OCMExpect([mockWebController setShouldSuppressInputViews:NO]);
  proxy.shouldSuppressInputViews = NO;
  EXPECT_OCMOCK_VERIFY((id)mockWebController);
}

}  // namespace
