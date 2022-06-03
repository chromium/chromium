// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#import <UIKit/UIKit.h>

#include "base/compiler_specific.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1030168): Rewrite tests Delegate, MultipleScrollView,
// DelegateClearingUp not to depend on this, and delete this.
@interface CRWWebViewScrollViewProxy (Testing)

@property(nonatomic, readonly) CRWWebViewScrollViewDelegateProxy* delegateProxy;

@end

@interface UIScrollView (TestingCategory)
- (int)crw_categoryMethod;
@end

@implementation UIScrollView (TestingCategory)

- (int)crw_categoryMethod {
  return 1;
}

@end

@interface CRWTestObserver : NSObject

- (instancetype)initWithProxy:(CRWWebViewScrollViewProxy*)proxy
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation CRWTestObserver {
  CRWWebViewScrollViewProxy* _proxy;
}

- (instancetype)initWithProxy:(CRWWebViewScrollViewProxy*)proxy {
  self = [super init];
  if (self) {
    _proxy = proxy;
    [_proxy addObserver:self
             forKeyPath:@"contentSize"
                options:NSKeyValueObservingOptionNew
                context:nullptr];
  }
  return self;
}

- (void)dealloc {
  [_proxy removeObserver:self forKeyPath:@"contentSize"];
}

@end

namespace {

class CRWWebViewScrollViewProxyTest : public PlatformTest {
 protected:
  void SetUp() override {
    mock_underlying_scroll_view_ = OCMClassMock([UIScrollView class]);
    web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];
  }
  ~CRWWebViewScrollViewProxyTest() override {
    [web_view_scroll_view_proxy_ setScrollView:nil];
  }
  id mock_underlying_scroll_view_;
  CRWWebViewScrollViewProxy* web_view_scroll_view_proxy_;
};

// Tests that the UIScrollViewDelegate is set correctly.
TEST_F(CRWWebViewScrollViewProxyTest, Delegate) {
  OCMExpect([static_cast<UIScrollView*>(mock_underlying_scroll_view_)
      setDelegate:web_view_scroll_view_proxy_.delegateProxy]);
  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];
  EXPECT_OCMOCK_VERIFY(mock_underlying_scroll_view_);
}

// Tests that setting 2 scroll views consecutively, clears the delegate of the
// previous scroll view.
TEST_F(CRWWebViewScrollViewProxyTest, MultipleScrollView) {
  UIScrollView* mock_scroll_view1 = [[UIScrollView alloc] init];
  UIScrollView* mock_scroll_view2 = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:mock_scroll_view1];
  [web_view_scroll_view_proxy_ setScrollView:mock_scroll_view2];
  EXPECT_FALSE([mock_scroll_view1 delegate]);
  EXPECT_EQ(web_view_scroll_view_proxy_.delegateProxy,
            [mock_scroll_view2 delegate]);
  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Tests that when releasing a scroll view from the CRWWebViewScrollViewProxy,
// the UIScrollView's delegate is also cleared.
TEST_F(CRWWebViewScrollViewProxyTest, DelegateClearingUp) {
  UIScrollView* mock_scroll_view1 = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:mock_scroll_view1];
  EXPECT_EQ(web_view_scroll_view_proxy_.delegateProxy,
            [mock_scroll_view1 delegate]);
  [web_view_scroll_view_proxy_ setScrollView:nil];
  EXPECT_FALSE([mock_scroll_view1 delegate]);
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values from
// the underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewPresent) {
  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];
  OCMStub([mock_underlying_scroll_view_ isZooming]).andReturn(YES);
  EXPECT_TRUE([web_view_scroll_view_proxy_ isZooming]);

  // Arbitrary point.
  const CGPoint point = CGPointMake(10, 10);
  OCMStub([mock_underlying_scroll_view_ contentOffset]).andReturn(point);
  EXPECT_TRUE(
      CGPointEqualToPoint(point, [web_view_scroll_view_proxy_ contentOffset]));

  // Arbitrary inset.
  const UIEdgeInsets content_inset = UIEdgeInsetsMake(10, 10, 10, 10);
  OCMStub([mock_underlying_scroll_view_ contentInset]).andReturn(content_inset);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      content_inset, [web_view_scroll_view_proxy_ contentInset]));

  // Arbitrary inset.
  const UIEdgeInsets scroll_indicator_insets = UIEdgeInsetsMake(20, 20, 20, 20);
  OCMStub([mock_underlying_scroll_view_ scrollIndicatorInsets])
      .andReturn(scroll_indicator_insets);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      scroll_indicator_insets,
      [web_view_scroll_view_proxy_ scrollIndicatorInsets]));

  // Arbitrary size.
  const CGSize content_size = CGSizeMake(19, 19);
  OCMStub([mock_underlying_scroll_view_ contentSize]).andReturn(content_size);
  EXPECT_TRUE(CGSizeEqualToSize(content_size,
                                [web_view_scroll_view_proxy_ contentSize]));

  // Arbitrary rect.
  const CGRect frame = CGRectMake(2, 4, 5, 1);
  OCMStub([mock_underlying_scroll_view_ frame]).andReturn(frame);
  EXPECT_TRUE(CGRectEqualToRect(frame, [web_view_scroll_view_proxy_ frame]));

  OCMExpect([mock_underlying_scroll_view_ isDecelerating]).andReturn(YES);
  EXPECT_TRUE([web_view_scroll_view_proxy_ isDecelerating]);

  OCMExpect([mock_underlying_scroll_view_ isDecelerating]).andReturn(NO);
  EXPECT_FALSE([web_view_scroll_view_proxy_ isDecelerating]);

  OCMExpect([mock_underlying_scroll_view_ isDragging]).andReturn(YES);
  EXPECT_TRUE([web_view_scroll_view_proxy_ isDragging]);

  OCMExpect([mock_underlying_scroll_view_ isDragging]).andReturn(NO);
  EXPECT_FALSE([web_view_scroll_view_proxy_ isDragging]);

  OCMExpect([mock_underlying_scroll_view_ isTracking]).andReturn(YES);
  EXPECT_TRUE([web_view_scroll_view_proxy_ isTracking]);

  OCMExpect([mock_underlying_scroll_view_ isTracking]).andReturn(NO);
  EXPECT_FALSE([web_view_scroll_view_proxy_ isTracking]);

  OCMExpect([mock_underlying_scroll_view_ scrollsToTop]).andReturn(YES);
  EXPECT_TRUE([web_view_scroll_view_proxy_ scrollsToTop]);

  OCMExpect([mock_underlying_scroll_view_ scrollsToTop]).andReturn(NO);
  EXPECT_FALSE([web_view_scroll_view_proxy_ scrollsToTop]);

  NSArray<__kindof UIView*>* subviews = [NSArray array];
  OCMExpect([mock_underlying_scroll_view_ subviews]).andReturn(subviews);
  EXPECT_EQ(subviews, [web_view_scroll_view_proxy_ subviews]);

  OCMExpect([mock_underlying_scroll_view_ contentInsetAdjustmentBehavior])
      .andReturn(UIScrollViewContentInsetAdjustmentAutomatic);
  EXPECT_EQ(UIScrollViewContentInsetAdjustmentAutomatic,
            [web_view_scroll_view_proxy_ contentInsetAdjustmentBehavior]);

  OCMExpect([mock_underlying_scroll_view_ contentInsetAdjustmentBehavior])
      .andReturn(UIScrollViewContentInsetAdjustmentNever);
  EXPECT_EQ(UIScrollViewContentInsetAdjustmentNever,
            [web_view_scroll_view_proxy_ contentInsetAdjustmentBehavior]);

  OCMExpect([mock_underlying_scroll_view_ clipsToBounds]).andReturn(NO);
  EXPECT_FALSE([web_view_scroll_view_proxy_ clipsToBounds]);

  OCMExpect([mock_underlying_scroll_view_ clipsToBounds]).andReturn(YES);
  EXPECT_TRUE([web_view_scroll_view_proxy_ clipsToBounds]);
}

// Tests that CRWWebViewScrollViewProxy returns the default values of
// UIScrollView's properties when there is no underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewAbsent) {
  [web_view_scroll_view_proxy_ setScrollView:nil];

  EXPECT_TRUE(CGPointEqualToPoint(CGPointZero,
                                  [web_view_scroll_view_proxy_ contentOffset]));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, [web_view_scroll_view_proxy_ contentInset]));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, [web_view_scroll_view_proxy_ scrollIndicatorInsets]));
  EXPECT_TRUE(
      CGSizeEqualToSize(CGSizeZero, [web_view_scroll_view_proxy_ contentSize]));
  EXPECT_TRUE(
      CGRectEqualToRect(CGRectZero, [web_view_scroll_view_proxy_ frame]));
  EXPECT_FALSE([web_view_scroll_view_proxy_ isDecelerating]);
  EXPECT_FALSE([web_view_scroll_view_proxy_ isDragging]);
  EXPECT_FALSE([web_view_scroll_view_proxy_ isTracking]);
  EXPECT_TRUE([web_view_scroll_view_proxy_ scrollsToTop]);
  EXPECT_EQ((NSUInteger)0, [web_view_scroll_view_proxy_ subviews].count);
  EXPECT_EQ(UIScrollViewContentInsetAdjustmentAutomatic,
            [web_view_scroll_view_proxy_ contentInsetAdjustmentBehavior]);
  EXPECT_TRUE([web_view_scroll_view_proxy_ clipsToBounds]);

  // Make sure setting the properties is fine too.
  // Arbitrary point.
  const CGPoint kPoint = CGPointMake(10, 10);
  [web_view_scroll_view_proxy_ setContentOffset:kPoint];
  // Arbitrary inset.
  const UIEdgeInsets kContentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  [web_view_scroll_view_proxy_ setContentInset:kContentInset];
  [web_view_scroll_view_proxy_ setScrollIndicatorInsets:kContentInset];
  // Arbitrary size.
  const CGSize kContentSize = CGSizeMake(19, 19);
  [web_view_scroll_view_proxy_ setContentSize:kContentSize];
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values when
// they are set while there isn't an underlying scroll view, then a new scroll
// view is set.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewAbsentThenReset) {
  [web_view_scroll_view_proxy_ setScrollView:nil];
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];

  OCMExpect([mock_underlying_scroll_view_ setClipsToBounds:YES]);
  [web_view_scroll_view_proxy_ setClipsToBounds:YES];
  OCMExpect([mock_underlying_scroll_view_
      setContentInsetAdjustmentBehavior:
          UIScrollViewContentInsetAdjustmentNever]);
  [web_view_scroll_view_proxy_ setContentInsetAdjustmentBehavior:
                                   UIScrollViewContentInsetAdjustmentNever];

  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];

  EXPECT_OCMOCK_VERIFY(mock_underlying_scroll_view_);
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values when
// they are set while there is an underlying scroll view, then a new scroll view
// is set.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewPresentThenReset) {
  [web_view_scroll_view_proxy_ setScrollView:nil];
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];

  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];
  OCMExpect([mock_underlying_scroll_view_ setClipsToBounds:YES]);
  [web_view_scroll_view_proxy_ setClipsToBounds:YES];
  OCMExpect([mock_underlying_scroll_view_
      setContentInsetAdjustmentBehavior:
          UIScrollViewContentInsetAdjustmentNever]);
  [web_view_scroll_view_proxy_ setContentInsetAdjustmentBehavior:
                                   UIScrollViewContentInsetAdjustmentNever];

  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];

  EXPECT_OCMOCK_VERIFY(mock_underlying_scroll_view_);
}

// Tests releasing a scroll view when none is owned by the
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest, ReleasingAScrollView) {
  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Tests that CRWWebViewScrollViewProxy correctly delegates property setters to
// the underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewSetProperties) {
  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];

  OCMExpect([mock_underlying_scroll_view_
      setContentInsetAdjustmentBehavior:
          UIScrollViewContentInsetAdjustmentNever]);
  [web_view_scroll_view_proxy_ setContentInsetAdjustmentBehavior:
                                   UIScrollViewContentInsetAdjustmentNever];
  EXPECT_OCMOCK_VERIFY(mock_underlying_scroll_view_);
}

// Tests that -setContentInsetAdjustmentBehavior: works even if it is called
// before setting the scroll view.
TEST_F(CRWWebViewScrollViewProxyTest,
       SetContentInsetAdjustmentBehaviorBeforeSettingScrollView) {
  OCMExpect([mock_underlying_scroll_view_
      setContentInsetAdjustmentBehavior:
          UIScrollViewContentInsetAdjustmentNever]);

  [web_view_scroll_view_proxy_ setScrollView:nil];
  [web_view_scroll_view_proxy_ setContentInsetAdjustmentBehavior:
                                   UIScrollViewContentInsetAdjustmentNever];
  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];

  EXPECT_OCMOCK_VERIFY(mock_underlying_scroll_view_);
}

// Tests that -setClipsToBounds: works even if it is called before setting the
// scroll view.
TEST_F(CRWWebViewScrollViewProxyTest, SetClipsToBoundsBeforeSettingScrollView) {
  OCMExpect([mock_underlying_scroll_view_ setClipsToBounds:YES]);

  [web_view_scroll_view_proxy_ setScrollView:nil];
  [web_view_scroll_view_proxy_ setClipsToBounds:YES];
  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];

  EXPECT_OCMOCK_VERIFY(mock_underlying_scroll_view_);
}

// Tests that frame changes are communicated to observers.
TEST_F(CRWWebViewScrollViewProxyTest, FrameDidChange) {
  UIScrollView* underlying_scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectZero];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];
  id mock_delegate =
      OCMProtocolMock(@protocol(CRWWebViewScrollViewProxyObserver));
  [web_view_scroll_view_proxy_ addObserver:mock_delegate];
  OCMExpect([mock_delegate
      webViewScrollViewFrameDidChange:web_view_scroll_view_proxy_]);
  underlying_scroll_view.frame = CGRectMake(1, 2, 3, 4);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Tests that contentInset changes are communicated to observers.
TEST_F(CRWWebViewScrollViewProxyTest, ContentInsetDidChange) {
  UIScrollView* underlying_scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectZero];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];
  id mock_delegate =
      OCMProtocolMock(@protocol(CRWWebViewScrollViewProxyObserver));
  [web_view_scroll_view_proxy_ addObserver:mock_delegate];
  OCMExpect([mock_delegate
      webViewScrollViewDidResetContentInset:web_view_scroll_view_proxy_]);
  underlying_scroll_view.contentInset = UIEdgeInsetsMake(0, 1, 2, 3);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Verifies that method calls to -asUIScrollView are simply forwarded to the
// underlying scroll view if the method is not implemented in
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest, AsUIScrollViewWithUnderlyingScrollView) {
  [web_view_scroll_view_proxy_ setScrollView:mock_underlying_scroll_view_];

  // Verifies that a return value is properly propagated.
  // -viewPrintFormatter is not implemented in CRWWebViewScrollViewProxy.
  UIViewPrintFormatter* print_formatter_mock =
      OCMClassMock([UIViewPrintFormatter class]);
  OCMStub([mock_underlying_scroll_view_ viewPrintFormatter])
      .andReturn(print_formatter_mock);
  EXPECT_EQ(print_formatter_mock,
            [[web_view_scroll_view_proxy_ asUIScrollView] viewPrintFormatter]);

  // Verifies that a parameter is properly propagated.
  // -drawRect: is not implemented in CRWWebViewScrollViewProxy.
  CGRect rect = CGRectMake(0, 0, 1, 1);
  OCMExpect([mock_underlying_scroll_view_ drawRect:rect]);
  [[web_view_scroll_view_proxy_ asUIScrollView] drawRect:rect];
  EXPECT_OCMOCK_VERIFY((id)mock_underlying_scroll_view_);

  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Verifies that method calls to -asUIScrollView are no-op if the underlying
// scroll view is not set and the method is not implemented in
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest,
       AsUIScrollViewWithoutUnderlyingScrollView) {
  [web_view_scroll_view_proxy_ setScrollView:nil];

  // Any methods should return nil when the underlying scroll view is not set.
  EXPECT_EQ(nil, [[web_view_scroll_view_proxy_ asUIScrollView]
                     restorationIdentifier]);

  // It is expected that nothing happens. Just verifies that it doesn't crash.
  CGRect rect = CGRectMake(0, 0, 1, 1);
  [[web_view_scroll_view_proxy_ asUIScrollView] drawRect:rect];
}

// Verify that -[CRWWebViewScrollViewProxy isKindOfClass:] works as expected.
TEST_F(CRWWebViewScrollViewProxyTest, IsKindOfClass) {
  // The proxy is a kind of its own class.
  EXPECT_TRUE([web_view_scroll_view_proxy_
      isKindOfClass:[CRWWebViewScrollViewProxy class]]);

  // The proxy prentends itself to be a kind of UIScrollView.
  EXPECT_TRUE([web_view_scroll_view_proxy_ isKindOfClass:[UIScrollView class]]);

  // It should return YES for ancestor classes of UIScrollView.
  EXPECT_TRUE([web_view_scroll_view_proxy_ isKindOfClass:[UIView class]]);

  // Returns NO if none of above applies.
  EXPECT_FALSE([web_view_scroll_view_proxy_ isKindOfClass:[NSString class]]);
}

// Verify that -[CRWWebViewScrollViewProxy respondsToSelector:] works as
// expected.
TEST_F(CRWWebViewScrollViewProxyTest, RespondsToSelector) {
  // A method defined in CRWWebViewScrollViewProxy but not in UIScrollView.
  EXPECT_TRUE(
      [web_view_scroll_view_proxy_ respondsToSelector:@selector(addObserver:)]);

  // A method defined in CRWWebViewScrollViewProxy and also in UIScrollView.
  EXPECT_TRUE([web_view_scroll_view_proxy_
      respondsToSelector:@selector(contentOffset)]);

  // A method defined in UIScrollView but not in CRWWebViewScrollViewProxy.
  EXPECT_TRUE([web_view_scroll_view_proxy_
      respondsToSelector:@selector(indexDisplayMode)]);

  // A method defined in UIView (a superclass of UIScrollView) but not in
  // CRWWebViewScrollViewProxy.
  EXPECT_TRUE([web_view_scroll_view_proxy_
      respondsToSelector:@selector(viewPrintFormatter)]);

  // A method defined in none of above.
  EXPECT_FALSE([web_view_scroll_view_proxy_
      respondsToSelector:@selector(containsString:)]);
}

// Tests delegate method forwarding to [web_view_scroll_view_proxy_
// asUIScrollView].delegate when:
//   - [web_view_scroll_view_proxy_ asUIScrollView].delegate is not nil
//   - CRWWebViewScrollViewDelegateProxy implements the method
//
// Expects that a method call to the delegate of the underlying scroll view is
// forwarded to [web_view_scroll_view_proxy_ asUIScrollView].delegate.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForImplementedMethod) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  id<UIScrollViewDelegate> mock_proxy_delegate =
      OCMProtocolMock(@protocol(UIScrollViewDelegate));
  [web_view_scroll_view_proxy_ asUIScrollView].delegate = mock_proxy_delegate;

  UIView* mock_view = OCMClassMock([UIView class]);
  OCMExpect([mock_proxy_delegate
      scrollViewWillBeginZooming:[web_view_scroll_view_proxy_ asUIScrollView]
                        withView:mock_view]);

  EXPECT_TRUE([underlying_scroll_view.delegate
      respondsToSelector:@selector(scrollViewWillBeginZooming:withView:)]);
  [underlying_scroll_view.delegate
      scrollViewWillBeginZooming:underlying_scroll_view
                        withView:mock_view];

  EXPECT_OCMOCK_VERIFY(static_cast<id>(mock_proxy_delegate));
  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Tests delegate method forwarding to [web_view_scroll_view_proxy_
// asUIScrollView].delegate when:
//   - [web_view_scroll_view_proxy_ asUIScrollView].delegate is not nil
//   - CRWWebViewScrollViewDelegateProxy does *not* implement the method
//
// Expects that a method call to the delegate of the underlying scroll view is
// forwarded to [web_view_scroll_view_proxy_ asUIScrollView].delegate.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForUnimplementedMethod) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  id<UIScrollViewDelegate> mock_proxy_delegate =
      OCMProtocolMock(@protocol(UIScrollViewDelegate));
  [web_view_scroll_view_proxy_ asUIScrollView].delegate = mock_proxy_delegate;

  UIView* mock_view = OCMClassMock([UIView class]);
  OCMExpect([mock_proxy_delegate
                viewForZoomingInScrollView:[web_view_scroll_view_proxy_
                                               asUIScrollView]])
      .andReturn(mock_view);

  EXPECT_TRUE([underlying_scroll_view.delegate
      respondsToSelector:@selector(viewForZoomingInScrollView:)]);
  EXPECT_EQ(mock_view, [underlying_scroll_view.delegate
                           viewForZoomingInScrollView:underlying_scroll_view]);

  EXPECT_OCMOCK_VERIFY(static_cast<id>(mock_proxy_delegate));
  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Tests delegate method forwarding to [web_view_scroll_view_proxy_
// asUIScrollView].delegate when:
//   - [web_view_scroll_view_proxy_ asUIScrollView].delegate is nil
//   - CRWWebViewScrollViewDelegateProxy implements the method
//
// Expects that the delegate of the underlying scroll view responds to the
// method but does nothing.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForImplementedMethodWhenDelegateIsNil) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  [web_view_scroll_view_proxy_ asUIScrollView].delegate = nil;

  EXPECT_TRUE([underlying_scroll_view.delegate
      respondsToSelector:@selector(scrollViewWillBeginZooming:withView:)]);
  UIView* mock_view = OCMClassMock([UIView class]);
  // Expects that nothing happens by calling this.
  [underlying_scroll_view.delegate
      scrollViewWillBeginZooming:underlying_scroll_view
                        withView:mock_view];

  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Tests delegate method forwarding to [web_view_scroll_view_proxy_
// asUIScrollView].delegate when:
//   - [web_view_scroll_view_proxy_ asUIScrollView].delegate is nil
//   - CRWWebViewScrollViewDelegateProxy does *not* implement the method
//
// Expects that the delegate of the underlying scroll view does *not* respond to
// the method.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForUnimplementedMethodWhenDelegateIsNil) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  [web_view_scroll_view_proxy_ asUIScrollView].delegate = nil;

  EXPECT_FALSE([underlying_scroll_view.delegate
      respondsToSelector:@selector(viewForZoomingInScrollView:)]);

  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Verifies that adding a key-value observer to a CRWWebViewScrollViewProxy
// works as expected.
TEST_F(CRWWebViewScrollViewProxyTest, AddKVObserver) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  underlying_scroll_view.contentOffset = CGPointZero;
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  // Add a key-value observer to a CRWWebViewScrollViewProxy.
  NSObject* observer = OCMClassMock([NSObject class]);
  int context = 0;
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context];

  // Setting |contentOffset| of the underlying scroll view should trigger a KVO
  // notification. The |object| of the notification should be the
  // CRWWebViewScrollViewProxy, not the underlying scroll view.
  CGPoint new_offset = CGPointMake(10, 20);
  NSDictionary<NSKeyValueChangeKey, id>* expected_change = @{
    NSKeyValueChangeKindKey : @(NSKeyValueChangeSetting),
    NSKeyValueChangeOldKey : @(CGPointZero),
    NSKeyValueChangeNewKey : @(new_offset)
  };
  OCMExpect([observer observeValueForKeyPath:@"contentOffset"
                                    ofObject:web_view_scroll_view_proxy_
                                      change:expected_change
                                     context:&context]);
  underlying_scroll_view.contentOffset = new_offset;

  EXPECT_OCMOCK_VERIFY(static_cast<id>(observer));
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"];
}

// Verifies that a key-value observer is kept after the underlying scroll view
// is set.
TEST_F(CRWWebViewScrollViewProxyTest,
       KVObserversAreKeptAfterSettingUnderlyingScrollView) {
  // Add a key-value observer to a CRWWebViewScrollViewProxy.
  NSObject* observer = OCMClassMock([NSObject class]);
  int context = 0;
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context];

  // Set the underlying scroll view.
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  underlying_scroll_view.contentOffset = CGPointZero;
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  // KVO is inherited to the new underlying scroll view.
  CGPoint new_offset = CGPointMake(10, 20);
  NSDictionary<NSKeyValueChangeKey, id>* expected_change = @{
    NSKeyValueChangeKindKey : @(NSKeyValueChangeSetting),
    NSKeyValueChangeOldKey : @(CGPointZero),
    NSKeyValueChangeNewKey : @(new_offset)
  };
  OCMExpect([observer observeValueForKeyPath:@"contentOffset"
                                    ofObject:web_view_scroll_view_proxy_
                                      change:expected_change
                                     context:&context]);
  underlying_scroll_view.contentOffset = new_offset;

  EXPECT_OCMOCK_VERIFY(static_cast<id>(observer));
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"];
}

// Verifies that removing a key-value observer from a CRWWebViewScrollViewProxy
// works as expected.
TEST_F(CRWWebViewScrollViewProxyTest, RemoveKVObserver) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  underlying_scroll_view.contentOffset = CGPointZero;
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  // Add and then remove a key-value observer.
  NSObject* observer = OCMClassMock([NSObject class]);
  int context = 0;
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context];
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"];

  // The observer should not be notified of a change after the removal.
  CGPoint new_offset = CGPointMake(10, 20);
  NSDictionary<NSKeyValueChangeKey, id>* expected_change = @{
    NSKeyValueChangeKindKey : @(NSKeyValueChangeSetting),
    NSKeyValueChangeOldKey : @(CGPointZero),
    NSKeyValueChangeNewKey : @(new_offset)
  };
  [[static_cast<id>(observer) reject]
      observeValueForKeyPath:@"contentOffset"
                    ofObject:web_view_scroll_view_proxy_
                      change:expected_change
                     context:&context];
  underlying_scroll_view.contentOffset = new_offset;

  EXPECT_OCMOCK_VERIFY(static_cast<id>(observer));
}

// When -addObserver:forKeyPath:options:context: is called multiple times with
// the same observer and key path, -removeObserver:forKeyPath: removes the last
// observation.
//
// This matches the (undocumented) behavior of the built-in KVO.
TEST_F(CRWWebViewScrollViewProxyTest, RemoveKVObserverRemovesLastObservation) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  underlying_scroll_view.contentOffset = CGPointZero;
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  // Add an observer twice with |context1| and then with |context2|.
  NSObject* observer = OCMClassMock([NSObject class]);
  int context1 = 0;
  int context2 = 0;
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context1];
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context2];

  // Remove an observer once. This should remove the observation with
  // |context2|.
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"];

  // The observer should be notified of a change with |context1| but not with
  // |context2|.
  CGPoint new_offset = CGPointMake(10, 20);
  NSDictionary<NSKeyValueChangeKey, id>* expected_change = @{
    NSKeyValueChangeKindKey : @(NSKeyValueChangeSetting),
    NSKeyValueChangeOldKey : @(CGPointZero),
    NSKeyValueChangeNewKey : @(new_offset)
  };
  OCMExpect([observer observeValueForKeyPath:@"contentOffset"
                                    ofObject:web_view_scroll_view_proxy_
                                      change:expected_change
                                     context:&context1]);
  [[static_cast<id>(observer) reject]
      observeValueForKeyPath:@"contentOffset"
                    ofObject:web_view_scroll_view_proxy_
                      change:expected_change
                     context:&context2];

  underlying_scroll_view.contentOffset = new_offset;

  EXPECT_OCMOCK_VERIFY(static_cast<id>(observer));
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"];
}

// Verifies that removing a key-value observer from a CRWWebViewScrollViewProxy
// works as expected when given a context.
TEST_F(CRWWebViewScrollViewProxyTest, RemoveKVObserverWithContext) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  underlying_scroll_view.contentOffset = CGPointZero;
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  // Add an observer twice with |context1| and then with |context2|.
  NSObject* observer = OCMClassMock([NSObject class]);
  int context1 = 0;
  int context2 = 0;
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context1];
  [web_view_scroll_view_proxy_
      addObserver:observer
       forKeyPath:@"contentOffset"
          options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew
          context:&context2];

  // Remove the observation with |context1|.
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"
                                      context:&context1];

  // The observer should be notified of a change with |context2| but not with
  // |context1|.
  CGPoint new_offset = CGPointMake(10, 20);
  NSDictionary<NSKeyValueChangeKey, id>* expected_change = @{
    NSKeyValueChangeKindKey : @(NSKeyValueChangeSetting),
    NSKeyValueChangeOldKey : @(CGPointZero),
    NSKeyValueChangeNewKey : @(new_offset)
  };
  OCMExpect([observer observeValueForKeyPath:@"contentOffset"
                                    ofObject:web_view_scroll_view_proxy_
                                      change:expected_change
                                     context:&context2]);
  [[static_cast<id>(observer) reject]
      observeValueForKeyPath:@"contentOffset"
                    ofObject:web_view_scroll_view_proxy_
                      change:expected_change
                     context:&context1];

  underlying_scroll_view.contentOffset = new_offset;

  EXPECT_OCMOCK_VERIFY(static_cast<id>(observer));
  [web_view_scroll_view_proxy_ removeObserver:observer
                                   forKeyPath:@"contentOffset"
                                      context:&context2];
}

// Verifies that it is safe to call -removeObserver:forKeyPath: against the
// proxy during -dealloc of the observer.
TEST_F(CRWWebViewScrollViewProxyTest,
       RemoveKVObserverWhileDeallocatingObserver) {
  // CRWTestObserver adds itself as a key-value observer of the proxy in its
  // initializer, and removes itself as a observer during its -dealloc.
  CRWTestObserver* observer __attribute__((unused)) =
      [[CRWTestObserver alloc] initWithProxy:web_view_scroll_view_proxy_];
}

// Verifies that properties registered to |propertiesStore| are preserved if:
//   - the setter is called when the underlying scroll view is not set
//   - the getter is called after the underlying scroll view is still not set
TEST_F(CRWWebViewScrollViewProxyTest,
       PreservePropertiesWhileUnderlyingScrollViewIsAbsent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kPreserveScrollViewProperties);

  // Recreate CRWWebViewScrollViewProxy with the updated feature flags.
  web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];

  [web_view_scroll_view_proxy_ setScrollView:nil];

  // A preserved property with a primitive type.
  [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled = YES;
  EXPECT_TRUE(
      [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled);
  [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled = NO;
  EXPECT_FALSE(
      [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled);

  // A preserved property with an object type.
  [web_view_scroll_view_proxy_ asUIScrollView].tintColor = UIColor.redColor;
  EXPECT_EQ(UIColor.redColor,
            [web_view_scroll_view_proxy_ asUIScrollView].tintColor);
}

// Verifies that properties registered to |propertiesStore| are preserved if:
//   - the setter is called when the underlying scroll view is not set
//   - the getter is called after the underlying scroll view is set
TEST_F(CRWWebViewScrollViewProxyTest,
       PreservePropertiesWhenUnderlyingScrollViewIsNewlyAssigned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kPreserveScrollViewProperties);

  // Recreate CRWWebViewScrollViewProxy with the updated feature flags.
  web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];

  [web_view_scroll_view_proxy_ setScrollView:nil];

  [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled = YES;
  [web_view_scroll_view_proxy_ asUIScrollView].tintColor = UIColor.redColor;

  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  // The properties are restored on the underlying scroll view.
  EXPECT_TRUE(underlying_scroll_view.directionalLockEnabled);
  EXPECT_EQ(UIColor.redColor, underlying_scroll_view.tintColor);

  // The same property values are available via the scroll view proxy as well.
  EXPECT_TRUE(
      [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled);
  EXPECT_EQ(UIColor.redColor,
            [web_view_scroll_view_proxy_ asUIScrollView].tintColor);

  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Verifies that properties registered to |propertiesStore| are preserved if:
//   - the setter is called when the underlying scroll view is set
//   - the getter is called after the underlying scroll view is reassigned
TEST_F(CRWWebViewScrollViewProxyTest,
       PreservePropertiesWhenUnderlyingScrollViewIsReassigned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kPreserveScrollViewProperties);

  // Recreate CRWWebViewScrollViewProxy with the updated feature flags.
  web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];

  UIScrollView* underlying_scroll_view1 = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view1];

  [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled = YES;
  [web_view_scroll_view_proxy_ asUIScrollView].tintColor = UIColor.redColor;

  UIScrollView* underlying_scroll_view2 = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view2];

  // The properties are restored on the underlying scroll view.
  EXPECT_TRUE(underlying_scroll_view2.directionalLockEnabled);
  EXPECT_EQ(UIColor.redColor, underlying_scroll_view2.tintColor);

  // The same property values are available via the scroll view proxy as well.
  EXPECT_TRUE(
      [web_view_scroll_view_proxy_ asUIScrollView].directionalLockEnabled);
  EXPECT_EQ(UIColor.redColor,
            [web_view_scroll_view_proxy_ asUIScrollView].tintColor);

  [web_view_scroll_view_proxy_ setScrollView:nil];
}

// Verifies that the proxy uses the real implementation of a method defined in a
// category of UIScrollView while the underlying scroll view is not set.
TEST_F(CRWWebViewScrollViewProxyTest,
       UIScrollViewCategoryWithoutUnderlyingScrollView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kPreserveScrollViewProperties);

  // Recreate CRWWebViewScrollViewProxy with the updated feature flags.
  web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];

  [web_view_scroll_view_proxy_ setScrollView:nil];

  EXPECT_EQ(1,
            [[web_view_scroll_view_proxy_ asUIScrollView] crw_categoryMethod]);
}

// Verifies that the proxy uses the real implementation of a method defined in a
// category of UIScrollView while the underlying scroll view is set.
TEST_F(CRWWebViewScrollViewProxyTest,
       UIScrollViewCategoryWithUnderlyingScrollView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kPreserveScrollViewProperties);

  // Recreate CRWWebViewScrollViewProxy with the updated feature flags.
  web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];

  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view];

  EXPECT_EQ(1,
            [[web_view_scroll_view_proxy_ asUIScrollView] crw_categoryMethod]);
}

// Verifies that the scroll view backgound color is not preserved between
// scroll views.  Used to prevent regression of crbug.com/1078790.
TEST_F(CRWWebViewScrollViewProxyTest, DontPreserveBackgroundColor) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kPreserveScrollViewProperties);

  // Recreate CRWWebViewScrollViewProxy with the updated feature flags.
  web_view_scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];

  // Set an underlying UIScrollView, and update its background color to red.
  UIScrollView* underlying_scroll_view1 = [[UIScrollView alloc] init];
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view1];
  [web_view_scroll_view_proxy_ asUIScrollView].backgroundColor =
      UIColor.redColor;

  // Create a second UIScrollView and set its background color to black.
  UIScrollView* underlying_scroll_view2 = [[UIScrollView alloc] init];
  underlying_scroll_view2.backgroundColor = UIColor.blackColor;
  [web_view_scroll_view_proxy_ setScrollView:underlying_scroll_view2];

  // Verify that the second scroll view's background color remains black.
  EXPECT_EQ(UIColor.blackColor, underlying_scroll_view2.backgroundColor);
}

}  // namespace
