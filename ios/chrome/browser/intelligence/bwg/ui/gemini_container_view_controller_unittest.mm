// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_mutator.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeGeminiContainerMutator : NSObject <GeminiContainerMutator>
@property(nonatomic, assign) BOOL keyboardDidShowCalled;
@end

@implementation FakeGeminiContainerMutator
- (void)containerKeyboardDidShowWithDuration:(NSTimeInterval)duration
                                       curve:(UIViewAnimationCurve)curve {
  self.keyboardDidShowCalled = YES;
}

- (void)containerDidChangeActuationHeight:(CGFloat)height {
}
@end

class GeminiContainerViewControllerTest : public PlatformTest {
 protected:
  GeminiContainerViewControllerTest() {
    child_view_controller_ = [[UIViewController alloc] init];
    internal_text_field_ =
        [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 30)];
    [child_view_controller_.view addSubview:internal_text_field_];

    container_view_controller_ = [[GeminiContainerViewController alloc]
        initWithGeminiViewController:child_view_controller_
               worklogViewController:nil];
    mutator_ = [[FakeGeminiContainerMutator alloc] init];
    container_view_controller_.mutator = mutator_;

    [scoped_key_window_.Get() addSubview:container_view_controller_.view];

    external_text_field_ =
        [[UITextField alloc] initWithFrame:CGRectMake(0, 100, 100, 30)];
    [scoped_key_window_.Get() addSubview:external_text_field_];
  }

  ~GeminiContainerViewControllerTest() override {
    [internal_text_field_ resignFirstResponder];
    [external_text_field_ resignFirstResponder];
  }

  void PostKeyboardWillShowNotification() {
    NSDictionary* user_info = @{
      UIKeyboardAnimationDurationUserInfoKey : @(0),
      UIKeyboardAnimationCurveUserInfoKey : @(UIViewAnimationCurveLinear),
    };
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIKeyboardWillShowNotification
                      object:nil
                    userInfo:user_info];
  }

  ScopedKeyWindow scoped_key_window_;
  UIViewController* child_view_controller_;
  UITextField* internal_text_field_;
  UITextField* external_text_field_;
  GeminiContainerViewController* container_view_controller_;
  FakeGeminiContainerMutator* mutator_;
};

// Tests that when an external view is first responder, keyboard notifications
// are ignored by the GeminiContainerViewController.
TEST_F(GeminiContainerViewControllerTest,
       IgnoreKeyboardWhenExternalViewIsFirstResponder) {
  [external_text_field_ becomeFirstResponder];
  EXPECT_TRUE([external_text_field_ isFirstResponder]);

  PostKeyboardWillShowNotification();

  EXPECT_FALSE(mutator_.keyboardDidShowCalled);
}

// Tests that when a view inside GeminiContainerViewController is first
// responder, keyboard notifications are forwarded to the delegate.
TEST_F(GeminiContainerViewControllerTest,
       HandleKeyboardWhenInternalViewIsFirstResponder) {
  [internal_text_field_ becomeFirstResponder];
  EXPECT_TRUE([internal_text_field_ isFirstResponder]);

  PostKeyboardWillShowNotification();

  EXPECT_TRUE(mutator_.keyboardDidShowCalled);
}

// Tests that dismissKeyboard ends editing on the view.
TEST_F(GeminiContainerViewControllerTest, TestDismissKeyboard) {
  [internal_text_field_ becomeFirstResponder];
  EXPECT_TRUE([internal_text_field_ isFirstResponder]);

  [container_view_controller_ dismissKeyboard];

  EXPECT_FALSE([internal_text_field_ isFirstResponder]);
}

// Tests that updateZeroStateVisibility updates
// zeroStateViewController view hidden state and contentHeight reflects the
// visible content.
TEST_F(GeminiContainerViewControllerTest, TestUpdateZeroStateVisibility) {
  UIViewController* zero_state_view_controller =
      [[UIViewController alloc] init];
  [zero_state_view_controller.view.heightAnchor constraintEqualToConstant:280.0]
      .active = YES;
  [child_view_controller_.view.heightAnchor constraintEqualToConstant:80.0]
      .active = YES;

  GeminiContainerViewController* container =
      [[GeminiContainerViewController alloc]
          initWithGeminiViewController:child_view_controller_
                 worklogViewController:nil];
  container.zeroStateViewController = zero_state_view_controller;
  [scoped_key_window_.Get() addSubview:container.view];
  [container loadViewIfNeeded];

  [container updateZeroStateVisibility:YES];
  EXPECT_FALSE(zero_state_view_controller.view.hidden);
  EXPECT_EQ([container contentHeight], 360.0);

  [container updateZeroStateVisibility:NO];
  EXPECT_TRUE(zero_state_view_controller.view.hidden);
  EXPECT_EQ([container contentHeight], 80.0);

  [container.view removeFromSuperview];
}
