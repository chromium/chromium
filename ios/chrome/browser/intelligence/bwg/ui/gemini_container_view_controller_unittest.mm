// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeGeminiContainerViewControllerDelegate
    : NSObject <GeminiContainerViewControllerDelegate>
@property(nonatomic, assign) BOOL keyboardDidShowCalled;
@end

@implementation FakeGeminiContainerViewControllerDelegate
- (void)geminiContainerViewController:
            (GeminiContainerViewController*)viewController
          didShowKeyboardWithDuration:(NSTimeInterval)duration
                                curve:(UIViewAnimationCurve)curve {
  self.keyboardDidShowCalled = YES;
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
        initWithGeminiViewController:child_view_controller_];
    delegate_ = [[FakeGeminiContainerViewControllerDelegate alloc] init];
    container_view_controller_.delegate = delegate_;

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
  FakeGeminiContainerViewControllerDelegate* delegate_;
};

// Tests that when an external view is first responder, keyboard notifications
// are ignored by the GeminiContainerViewController.
TEST_F(GeminiContainerViewControllerTest,
       IgnoreKeyboardWhenExternalViewIsFirstResponder) {
  [external_text_field_ becomeFirstResponder];
  EXPECT_TRUE([external_text_field_ isFirstResponder]);

  PostKeyboardWillShowNotification();

  EXPECT_FALSE(delegate_.keyboardDidShowCalled);
}

// Tests that when a view inside GeminiContainerViewController is first
// responder, keyboard notifications are forwarded to the delegate.
TEST_F(GeminiContainerViewControllerTest,
       HandleKeyboardWhenInternalViewIsFirstResponder) {
  [internal_text_field_ becomeFirstResponder];
  EXPECT_TRUE([internal_text_field_ isFirstResponder]);

  PostKeyboardWillShowNotification();

  EXPECT_TRUE(delegate_.keyboardDidShowCalled);
}

// Tests that dismissKeyboard ends editing on the view.
TEST_F(GeminiContainerViewControllerTest, TestDismissKeyboard) {
  [internal_text_field_ becomeFirstResponder];
  EXPECT_TRUE([internal_text_field_ isFirstResponder]);

  [container_view_controller_ dismissKeyboard];

  EXPECT_FALSE([internal_text_field_ isFirstResponder]);
}
