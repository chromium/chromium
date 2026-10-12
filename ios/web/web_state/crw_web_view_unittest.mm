// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_web_view.h"

#import <UIKit/UIKit.h>

#import "ios/web/common/crw_input_view_provider.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake `CRWInputViewProvider` and `CRWResponderInputView` implementation for
// testing custom input views and view controllers on `CRWWebView`.
@interface CRWFakeResponderInputView
    : NSObject <CRWInputViewProvider, CRWResponderInputView>
// Redeclares `CRWResponderInputView` properties as writable for testing.
@property(nonatomic, strong) UIView* inputView;
@property(nonatomic, strong) UIView* inputAccessoryView;
@property(nonatomic, strong) UIInputViewController* inputViewController;
@property(nonatomic, strong)
    UIInputViewController* inputAccessoryViewController;
@end

@implementation CRWFakeResponderInputView
- (id<CRWResponderInputView>)responderInputView {
  return self;
}
@end

// Fake first-responder subview that captures its superview's `inputView` when
// `reloadInputViews` is called.
@interface CRWTestFirstResponderSubview : UIView
// Number of times `reloadInputViews` was invoked on this view.
@property(nonatomic, assign) int reloadInputViewsCount;
// Value of `superview.inputView` captured during the last `reloadInputViews`.
@property(nonatomic, strong) UIView* observedSuperviewInputView;
@end

@implementation CRWTestFirstResponderSubview
- (BOOL)isFirstResponder {
  return YES;
}
- (void)reloadInputViews {
  _reloadInputViewsCount++;
  _observedSuperviewInputView = self.superview.inputView;
  [super reloadInputViews];
}
@end

namespace {

using CRWWebViewTest = PlatformTest;

// Tests that `shouldSuppressInputViews` replaces `inputView` with a zero-sized
// view when `YES`, restores it to `nil` when `NO`, and reloads first-responder
// input views on state transitions.
TEST_F(CRWWebViewTest, SuppressesInputViews) {
  CRWWebView* web_view = [[CRWWebView alloc] initWithFrame:CGRectZero];
  CRWTestFirstResponderSubview* subview =
      [[CRWTestFirstResponderSubview alloc] initWithFrame:CGRectZero];
  [web_view addSubview:subview];

  EXPECT_FALSE(web_view.shouldSuppressInputViews);
  EXPECT_EQ(web_view.inputView, nil);
  EXPECT_EQ(web_view.inputAccessoryView, nil);

  web_view.shouldSuppressInputViews = YES;
  EXPECT_TRUE(web_view.shouldSuppressInputViews);
  ASSERT_NE(web_view.inputView, nil);
  EXPECT_TRUE(CGRectIsEmpty(web_view.inputView.frame));
  EXPECT_EQ(web_view.inputAccessoryView, nil);
  EXPECT_EQ(subview.reloadInputViewsCount, 1);
  EXPECT_EQ(subview.observedSuperviewInputView, web_view.inputView);

  // Setting `YES` again should be a no-op.
  web_view.shouldSuppressInputViews = YES;
  EXPECT_EQ(subview.reloadInputViewsCount, 1);

  web_view.shouldSuppressInputViews = NO;
  EXPECT_FALSE(web_view.shouldSuppressInputViews);
  EXPECT_EQ(web_view.inputView, nil);
  EXPECT_EQ(web_view.inputAccessoryView, nil);
  EXPECT_EQ(subview.reloadInputViewsCount, 2);
  EXPECT_EQ(subview.observedSuperviewInputView, nil);

  // Setting `NO` again should be a no-op.
  web_view.shouldSuppressInputViews = NO;
  EXPECT_EQ(subview.reloadInputViewsCount, 2);
}

// Tests that `shouldSuppressInputViews` overrides views and view controllers
// from `inputViewProvider` while `YES`, and restores them when `NO`.
TEST_F(CRWWebViewTest, OverridesInputViewProvider) {
  CRWWebView* web_view = [[CRWWebView alloc] initWithFrame:CGRectZero];
  CRWFakeResponderInputView* provider =
      [[CRWFakeResponderInputView alloc] init];
  provider.inputView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 50)];
  provider.inputAccessoryView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 30)];
  provider.inputViewController = [[UIInputViewController alloc] init];
  provider.inputAccessoryViewController = [[UIInputViewController alloc] init];
  web_view.inputViewProvider = provider;

  EXPECT_EQ(web_view.inputView, provider.inputView);
  EXPECT_EQ(web_view.inputAccessoryView, provider.inputAccessoryView);
  EXPECT_EQ(web_view.inputViewController, provider.inputViewController);
  EXPECT_EQ(web_view.inputAccessoryViewController,
            provider.inputAccessoryViewController);

  web_view.shouldSuppressInputViews = YES;
  ASSERT_NE(web_view.inputView, nil);
  EXPECT_NE(web_view.inputView, provider.inputView);
  EXPECT_TRUE(CGRectIsEmpty(web_view.inputView.frame));
  EXPECT_EQ(web_view.inputAccessoryView, nil);
  EXPECT_EQ(web_view.inputViewController, nil);
  EXPECT_EQ(web_view.inputAccessoryViewController, nil);

  web_view.shouldSuppressInputViews = NO;
  EXPECT_EQ(web_view.inputView, provider.inputView);
  EXPECT_EQ(web_view.inputAccessoryView, provider.inputAccessoryView);
  EXPECT_EQ(web_view.inputViewController, provider.inputViewController);
  EXPECT_EQ(web_view.inputAccessoryViewController,
            provider.inputAccessoryViewController);
}

}  // namespace
