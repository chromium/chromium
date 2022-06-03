// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/voice_search_keyboard_bar_button_item.h"

#import "ios/chrome/browser/voice/fake_voice_search_availability.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for VoiceSearchKeyboardBarButtonItem.
class VoiceSearchKeyboardBarButtonItemTest : public PlatformTest {
 public:
  VoiceSearchKeyboardBarButtonItemTest() {
    std::unique_ptr<FakeVoiceSearchAvailability> availability =
        std::make_unique<FakeVoiceSearchAvailability>();
    availability_ = availability.get();
    availability_->SetVoiceOverEnabled(false);
    availability_->SetVoiceProviderEnabled(true);
    item_ = [[VoiceSearchKeyboardBarButtonItem alloc]
                  initWithImage:nil
                          style:UIBarButtonItemStylePlain
                         target:nil
                         action:nil
        voiceSearchAvailability:std::move(availability)];
  }

 protected:
  FakeVoiceSearchAvailability* availability_ = nullptr;
  VoiceSearchKeyboardBarButtonItem* item_ = nil;
};

// Tests that the item is disabled when VoiceOver is enabled.
TEST_F(VoiceSearchKeyboardBarButtonItemTest, DisableForVoiceOver) {
  ASSERT_TRUE(item_.enabled);

  availability_->SetVoiceOverEnabled(true);
  EXPECT_FALSE(item_.enabled);

  availability_->SetVoiceOverEnabled(false);
  EXPECT_TRUE(item_.enabled);
}
