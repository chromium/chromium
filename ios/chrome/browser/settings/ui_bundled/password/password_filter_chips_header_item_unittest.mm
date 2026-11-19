// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_filter_chips_header_item.h"

#import <optional>

#import "ios/chrome/browser/settings/password/public/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface FakePasswordFilterChipsDelegate
    : NSObject <PasswordFilterChipsDelegate>
@property(nonatomic, assign) std::optional<PasswordCredentialFilter>
    lastSelectedFilter;
@end

@implementation FakePasswordFilterChipsDelegate

- (void)passwordFilterChipsHeaderView:(PasswordFilterChipsHeaderView*)headerView
                      didSelectFilter:(PasswordCredentialFilter)filter {
  self.lastSelectedFilter = filter;
}

@end

namespace {

using PasswordFilterChipsHeaderItemTest = PlatformTest;

// Tests that filter buttons are configured with titles and selection states and
// report selection events to the delegate.
TEST_F(PasswordFilterChipsHeaderItemTest, ConfiguresButtonsAndHandlesTap) {
  PasswordFilterChipsHeaderItem* item =
      [[PasswordFilterChipsHeaderItem alloc] initWithType:0];
  FakePasswordFilterChipsDelegate* delegate =
      [[FakePasswordFilterChipsDelegate alloc] init];

  PasswordFilterChipsHeaderView* view =
      [[PasswordFilterChipsHeaderView alloc] initWithReuseIdentifier:@"test"];
  view.delegate = delegate;
  [item configureHeaderFooterView:view];

  NSArray<ChromeButton*>* chip_buttons = view.chipButtons;
  ASSERT_EQ(chip_buttons.count, 3u);
  ChromeButton* all_button = chip_buttons[0];
  ChromeButton* passwords_button = chip_buttons[1];
  ChromeButton* passkeys_button = chip_buttons[2];

  EXPECT_NSEQ(all_button.accessibilityIdentifier,
              kPasswordManagerFilterChipAllIdentifier);
  EXPECT_NSEQ(passwords_button.accessibilityIdentifier,
              kPasswordManagerFilterChipPasswordsIdentifier);
  EXPECT_NSEQ(passkeys_button.accessibilityIdentifier,
              kPasswordManagerFilterChipPasskeysIdentifier);
  EXPECT_NSEQ(all_button.title,
              l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_FILTER_ALL));

  EXPECT_EQ(all_button.style, ChromeButtonStylePrimary);
  EXPECT_NE(all_button.accessibilityTraits & UIAccessibilityTraitSelected,
            0ULL);
  EXPECT_EQ(passkeys_button.style, ChromeButtonStyleSecondary);
  EXPECT_EQ(passkeys_button.accessibilityTraits & UIAccessibilityTraitSelected,
            0ULL);

  [passkeys_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate.lastSelectedFilter, PasswordCredentialFilter::kPasskeys);

  item.selectedFilter = PasswordCredentialFilter::kPasskeys;
  item.enabled = NO;
  [item configureHeaderFooterView:view];

  EXPECT_EQ(all_button.style, ChromeButtonStyleSecondary);
  EXPECT_EQ(passkeys_button.style, ChromeButtonStylePrimary);
  EXPECT_NE(passkeys_button.accessibilityTraits & UIAccessibilityTraitSelected,
            0ULL);
  EXPECT_FALSE(all_button.enabled);
}

}  // namespace
