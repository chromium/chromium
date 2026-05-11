// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_accessibility_manager.h"

#import <vector>

#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_grabber_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Tests for AssistantContainerAccessibilityManager.
class AssistantContainerAccessibilityManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    grabber_button_ = [[AssistantGrabberButton alloc] init];
    mock_delegate_ = OCMStrictProtocolMock(
        @protocol(AssistantContainerAccessibilityManagerDelegate));
    manager_ = [[AssistantContainerAccessibilityManager alloc]
        initWithGrabberButton:grabber_button_
                     delegate:mock_delegate_];
    grabber_button_.accessibilityDelegate = manager_;
  }

  void TearDown() override {
    manager_ = nil;
    mock_delegate_ = nil;
    grabber_button_ = nil;
    PlatformTest::TearDown();
  }

  AssistantGrabberButton* grabber_button_;
  id mock_delegate_;
  AssistantContainerAccessibilityManager* manager_;
};

// Tests that the accessibility value is updated correctly based on the detent.
TEST_F(AssistantContainerAccessibilityManagerTest, TestAccessibilityValue) {
  std::vector<AssistantContainerDetent> detents = {
      AssistantContainerDetent::kMinimized, AssistantContainerDetent::kMedium,
      AssistantContainerDetent::kLarge};

  // Verify value for Minimized detent.
  [manager_ updateAccessibilityPropertiesWithCurrentDetent:
                AssistantContainerDetent::kMinimized
                                          availableDetents:detents];
  EXPECT_NSEQ(
      grabber_button_.accessibilityValue,
      l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_VALUE_MINIMIZED));

  // Verify value for Medium detent.
  [manager_ updateAccessibilityPropertiesWithCurrentDetent:
                AssistantContainerDetent::kMedium
                                          availableDetents:detents];
  EXPECT_NSEQ(grabber_button_.accessibilityValue,
              l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_VALUE_MEDIUM));

  // Verify value for Large detent.
  [manager_ updateAccessibilityPropertiesWithCurrentDetent:
                AssistantContainerDetent::kLarge
                                          availableDetents:detents];
  EXPECT_NSEQ(grabber_button_.accessibilityValue,
              l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_VALUE_LARGE));
}

// Tests that custom actions are created for available detents.
TEST_F(AssistantContainerAccessibilityManagerTest, TestCustomActions) {
  std::vector<AssistantContainerDetent> detents = {
      AssistantContainerDetent::kMinimized, AssistantContainerDetent::kMedium,
      AssistantContainerDetent::kLarge};

  [manager_ updateAccessibilityPropertiesWithCurrentDetent:
                AssistantContainerDetent::kMinimized
                                          availableDetents:detents];

  NSArray* actions = grabber_button_.accessibilityCustomActions;
  EXPECT_EQ(actions.count, 2u);

  // The first action should correspond to the next detent (Medium).
  UIAccessibilityCustomAction* action = actions[0];

  OCMExpect([mock_delegate_ accessibilityManagerDidRequestDetentChange:
                                AssistantContainerDetent::kMedium]);
  BOOL handled = action.actionHandler(action);
  EXPECT_TRUE(handled);
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that delegate callbacks are triggered by accessibility gestures.
TEST_F(AssistantContainerAccessibilityManagerTest, TestDelegateCallbacks) {
  std::vector<AssistantContainerDetent> detents = {
      AssistantContainerDetent::kMinimized, AssistantContainerDetent::kMedium,
      AssistantContainerDetent::kLarge};

  [manager_ updateAccessibilityPropertiesWithCurrentDetent:
                AssistantContainerDetent::kMedium
                                          availableDetents:detents];

  // Simulate VoiceOver increment (swipe up).
  // Should move from Medium to Large.
  OCMExpect([mock_delegate_ accessibilityManagerDidRequestDetentChange:
                                AssistantContainerDetent::kLarge]);
  [manager_ assistantGrabberButtonDidIncrement:grabber_button_];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);

  // Simulate VoiceOver decrement (swipe down).
  // Should move from Medium to Minimized.
  OCMExpect([mock_delegate_ accessibilityManagerDidRequestDetentChange:
                                AssistantContainerDetent::kMinimized]);
  [manager_ assistantGrabberButtonDidDecrement:grabber_button_];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

}  // namespace
