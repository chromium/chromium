// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_input_assistant_items.h"

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

@interface FakeOmniboxAssistiveKeyboardDelegate
    : NSObject <OmniboxAssistiveKeyboardDelegate>
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
@property(nonatomic, strong) UIButton* lensButton;
@end

@implementation FakeOmniboxAssistiveKeyboardDelegate
- (void)keyboardAccessoryVoiceSearchTapped:(id)sender {
}
- (void)keyboardAccessoryCameraSearchTapped {
}
- (void)keyboardAccessoryLensTapped {
}
- (void)keyboardAccessoryDebuggerTapped {
}
- (void)keyPressed:(NSString*)title {
}
- (void)presentLensKeyboardInProductHelper {
}
@end

class OmniboxInputAssistantItemsTest : public PlatformTest {
 public:
  OmniboxInputAssistantItemsTest() {
    delegate_ = [[FakeOmniboxAssistiveKeyboardDelegate alloc] init];
  }

 protected:
  FakeOmniboxAssistiveKeyboardDelegate* delegate_;
};

// Tests leading bar button groups when Lens is disabled.
TEST_F(OmniboxInputAssistantItemsTest, TestLeadingBarButtonGroups_WithoutLens) {
  NSArray<UIBarButtonItemGroup*>* groups =
      OmniboxAssistiveKeyboardLeadingBarButtonGroups(delegate_, nil,
                                                     /*use_lens=*/false);
  ASSERT_EQ(groups.count, 1u);
  UIBarButtonItemGroup* group = groups[0];
  ASSERT_GE(group.barButtonItems.count, 2u);

  // The second item is the camera / QR search button.
  UIBarButtonItem* cameraItem = group.barButtonItems[1];
  EXPECT_NSEQ(
      cameraItem.accessibilityLabel,
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH));
  EXPECT_EQ(delegate_.lensButton, nil);

  UIButton* button = (UIButton*)cameraItem.customView;
  ASSERT_TRUE([button isKindOfClass:[UIButton class]]);
  NSArray<NSString*>* actions =
      [button actionsForTarget:delegate_
               forControlEvent:UIControlEventTouchUpInside];
  EXPECT_TRUE([actions containsObject:@"keyboardAccessoryCameraSearchTapped"]);
}

// Tests leading bar button groups when Lens is enabled.
TEST_F(OmniboxInputAssistantItemsTest, TestLeadingBarButtonGroups_WithLens) {
  NSArray<UIBarButtonItemGroup*>* groups =
      OmniboxAssistiveKeyboardLeadingBarButtonGroups(delegate_, nil,
                                                     /*use_lens=*/true);
  ASSERT_EQ(groups.count, 1u);
  UIBarButtonItemGroup* group = groups[0];
  ASSERT_GE(group.barButtonItems.count, 2u);

  // The second item is the Lens button.
  UIBarButtonItem* cameraItem = group.barButtonItems[1];
  EXPECT_NSEQ(cameraItem.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_LENS));

  UIButton* button = (UIButton*)cameraItem.customView;
  ASSERT_TRUE([button isKindOfClass:[UIButton class]]);
  EXPECT_EQ(delegate_.lensButton, button);
  NSArray<NSString*>* actions =
      [button actionsForTarget:delegate_
               forControlEvent:UIControlEventTouchUpInside];
  EXPECT_TRUE([actions containsObject:@"keyboardAccessoryLensTapped"]);
}
