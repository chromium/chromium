// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/web_state_keyboard_util.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for `DismissKeyboardForWebState()`. Installs a text field in
// the view of a fake `WebState` and makes it first responder.
class WebStateKeyboardUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    UIView* web_state_view =
        [[UIView alloc] initWithFrame:scoped_key_window_.Get().bounds];
    text_field_ = [[UITextField alloc] initWithFrame:web_state_view.bounds];
    [web_state_view addSubview:text_field_];
    [scoped_key_window_.Get() addSubview:web_state_view];
    web_state_.SetView(web_state_view);
    ASSERT_TRUE([text_field_ becomeFirstResponder]);
  }

  void TearDown() override {
    [text_field_ resignFirstResponder];
    [web_state_.GetView() removeFromSuperview];
    PlatformTest::TearDown();
  }

  ScopedKeyWindow scoped_key_window_;
  web::FakeWebState web_state_;
  UITextField* text_field_ = nil;
};

// Tests that a field being edited in the `WebState`'s view stops being edited.
TEST_F(WebStateKeyboardUtilTest, EndsEditingInWebStateView) {
  DismissKeyboardForWebState(&web_state_);

  EXPECT_FALSE(text_field_.isFirstResponder);
}

// Tests that an unrealized `WebState` is left untouched.
TEST_F(WebStateKeyboardUtilTest, IgnoresUnrealizedWebState) {
  web_state_.SetIsRealized(false);

  DismissKeyboardForWebState(&web_state_);

  EXPECT_FALSE(web_state_.IsRealized());
  EXPECT_TRUE(text_field_.isFirstResponder);
}

// Tests that a null `WebState` is accepted.
TEST_F(WebStateKeyboardUtilTest, IgnoresNullWebState) {
  DismissKeyboardForWebState(nullptr);

  EXPECT_TRUE(text_field_.isFirstResponder);
}
