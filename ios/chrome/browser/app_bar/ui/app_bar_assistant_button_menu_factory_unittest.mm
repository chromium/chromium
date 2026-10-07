// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_assistant_button_menu_factory.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Number of entries of the menu when all of them are shown.
constexpr NSUInteger kAllEntriesCount = 3;

// Indexes of the entries when all of them are shown.
constexpr NSUInteger kAskGeminiEntryIndex = 0;
constexpr NSUInteger kLensEntryIndex = 1;
constexpr NSUInteger kAccountEntryIndex = 2;

}  // namespace

class AppBarAssistantButtonMenuFactoryTest : public PlatformTest {
 protected:
  AppBarAssistantButtonMenuFactoryTest() {
    mutator_ = OCMProtocolMock(@protocol(AppBarMutator));
    factory_ = [[AppBarAssistantButtonMenuFactory alloc] init];
    factory_.mutator = mutator_;
  }

  // Returns the action at `index` in `menu`.
  UIAction* ActionAtIndex(UIMenu* menu, NSUInteger index) {
    return base::apple::ObjCCastStrict<UIAction>(menu.children[index]);
  }

  // Expects the action at `index` in `menu` to be titled with `title_id`, to
  // have an icon and to be checked if `checked` is true.
  void ExpectAction(UIMenu* menu,
                    NSUInteger index,
                    int title_id,
                    bool checked) {
    SCOPED_TRACE(testing::Message() << "Action at index " << index);
    UIAction* action = ActionAtIndex(menu, index);
    EXPECT_NSEQ(l10n_util::GetNSString(title_id), action.title);
    EXPECT_TRUE(action.image);
    EXPECT_EQ(checked ? UIMenuElementStateOn : UIMenuElementStateOff,
              action.state);
  }

  id mutator_;
  AppBarAssistantButtonMenuFactory* factory_;
};

// Tests the menu when all the entries are shown.
TEST_F(AppBarAssistantButtonMenuFactoryTest, TestAllEntries) {
  UIMenu* menu =
      [factory_ menuWithCheckedState:AppBarAssistantButtonState::kLens
                       showAskGemini:YES
                            showLens:YES
                         showAccount:YES];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_APP_BAR_ASSISTANT_CUSTOMIZATION_TITLE),
      menu.title);
  EXPECT_TRUE(menu.options & UIMenuOptionsSingleSelection);
  ASSERT_EQ(kAllEntriesCount, menu.children.count);
  ExpectAction(menu, kAskGeminiEntryIndex, IDS_IOS_APP_BAR_ASK_GEMINI,
               /*checked=*/false);
  ExpectAction(menu, kLensEntryIndex, IDS_IOS_LENS_PRODUCT_NAME,
               /*checked=*/true);
  ExpectAction(menu, kAccountEntryIndex, IDS_IOS_APP_BAR_ACCOUNT,
               /*checked=*/false);
}

// Tests that the entries that aren't shown are absent from the menu.
TEST_F(AppBarAssistantButtonMenuFactoryTest, TestHiddenEntries) {
  UIMenu* menu = [factory_ menuWithCheckedState:AppBarAssistantButtonState::kAsk
                                  showAskGemini:YES
                                       showLens:NO
                                    showAccount:NO];

  ASSERT_EQ(1u, menu.children.count);
  ExpectAction(menu, kAskGeminiEntryIndex, IDS_IOS_APP_BAR_ASK_GEMINI,
               /*checked=*/true);
}

// Tests that selecting an entry sends its state to the mutator.
TEST_F(AppBarAssistantButtonMenuFactoryTest, TestSelectionNotifiesMutator) {
  UIMenu* menu = [factory_ menuWithCheckedState:AppBarAssistantButtonState::kAsk
                                  showAskGemini:YES
                                       showLens:YES
                                    showAccount:YES];
  ASSERT_EQ(kAllEntriesCount, menu.children.count);

  OCMExpect([mutator_ setPreferredAssistantButtonState:
                          AppBarAssistantButtonPreferredState::kLens]);
  [ActionAtIndex(menu, kLensEntryIndex) performWithSender:nil target:nil];
  EXPECT_OCMOCK_VERIFY(mutator_);
}
