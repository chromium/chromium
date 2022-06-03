// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/voice_search_keyboard_accessory_button.h"

#import "ios/chrome/browser/voice/fake_voice_search_availability.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for VoiceSearchKeyboardAccessoryButton.
class VoiceSearchKeyboardAccessoryButtonTest : public PlatformTest {
 public:
  VoiceSearchKeyboardAccessoryButtonTest()
      : superview_([[UIView alloc] initWithFrame:CGRectZero]) {
    std::unique_ptr<FakeVoiceSearchAvailability> availability =
        std::make_unique<FakeVoiceSearchAvailability>();
    availability_ = availability.get();
    availability_->SetVoiceOverEnabled(false);
    availability_->SetVoiceProviderEnabled(true);
    button_ = [[VoiceSearchKeyboardAccessoryButton alloc]
        initWithVoiceSearchAvailability:std::move(availability)];
    [superview_ addSubview:button_];
  }

 protected:
  FakeVoiceSearchAvailability* availability_ = nullptr;
  UIView* superview_ = nil;
  VoiceSearchKeyboardAccessoryButton* button_ = nil;
};

// Tests that the button is disabled when VoiceOver is enabled.
TEST_F(VoiceSearchKeyboardAccessoryButtonTest, DisableForVoiceOver) {
  ASSERT_TRUE(button_.enabled);

  availability_->SetVoiceOverEnabled(true);
  EXPECT_FALSE(button_.enabled);

  availability_->SetVoiceOverEnabled(false);
  EXPECT_TRUE(button_.enabled);
}
