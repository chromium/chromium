// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_input_accessory_view_handler.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

class FormInputAccessoryViewHandlerTest : public PlatformTest {
 public:
  FormInputAccessoryViewHandlerTest() {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that trying to programmatically dismiss the keyboard when it isn't
// visible doesn't crash the browser.
TEST_F(FormInputAccessoryViewHandlerTest, FormInputAccessoryViewHandler) {
  FormInputAccessoryViewHandler* accessory_view_delegate =
      [[FormInputAccessoryViewHandler alloc] init];
  ASSERT_TRUE(accessory_view_delegate);
  [accessory_view_delegate closeKeyboardWithoutButtonPress];
  accessory_view_delegate.webState = web_state();
  [accessory_view_delegate closeKeyboardWithoutButtonPress];
}
