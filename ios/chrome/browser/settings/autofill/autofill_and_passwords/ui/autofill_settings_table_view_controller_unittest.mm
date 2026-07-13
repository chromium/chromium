// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

@interface FakeAutofillSettingsTableViewControllerDelegate
    : NSObject <AutofillSettingsTableViewControllerDelegate>
@property(nonatomic, assign) BOOL wasRemovedCalled;
@property(nonatomic, assign) BOOL wasWalletPromoCardTapped;
@end

@implementation FakeAutofillSettingsTableViewControllerDelegate

- (void)autofillSettingsTableViewControllerDidRemove:
    (AutofillSettingsTableViewController*)controller {
  self.wasRemovedCalled = YES;
}

- (void)autofillSettingsTableViewControllerDidTapWalletPromoCard:
    (AutofillSettingsTableViewController*)controller {
  self.wasWalletPromoCardTapped = YES;
}

@end

@interface FakeAutofillSettingsMutator : NSObject <AutofillSettingsMutator>
@property(nonatomic, assign) BOOL enhancedAutofillEnabled;
@property(nonatomic, assign) NSInteger setEnhancedAutofillEnabledCallCount;
@property(nonatomic, assign) BOOL userVerificationEnabled;
@property(nonatomic, assign) NSInteger setUserVerificationEnabledCallCount;
@property(nonatomic, weak) id<AutofillSettingsConsumer> consumer;
@end

@implementation FakeAutofillSettingsMutator

- (void)setEnhancedAutofillEnabled:(BOOL)enabled {
  _enhancedAutofillEnabled = enabled;
  _setEnhancedAutofillEnabledCallCount++;
  [self.consumer setEnhancedAutofillEnabled:enabled];
}

- (void)setUserVerificationEnabled:(BOOL)enabled {
  _userVerificationEnabled = enabled;
  _setUserVerificationEnabledCallCount++;
  [self.consumer setUserVerificationEnabled:enabled];
}

@end

namespace {

class AutofillSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    fake_delegate_ =
        [[FakeAutofillSettingsTableViewControllerDelegate alloc] init];
    fake_mutator_ = [[FakeAutofillSettingsMutator alloc] init];
    CreateController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    AutofillSettingsTableViewController* controller =
        [[AutofillSettingsTableViewController alloc]
            initWithStyle:UITableViewStyleInsetGrouped];
    controller.delegate = fake_delegate_;
    controller.mutator = fake_mutator_;
    fake_mutator_.consumer = controller;
    return controller;
  }

  FakeAutofillSettingsTableViewControllerDelegate* fake_delegate_;
  FakeAutofillSettingsMutator* fake_mutator_;
};

TEST_F(AutofillSettingsTableViewControllerTest, TestInitialization) {
  CheckController();
  CheckTitleWithId(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS);
}

// Test model when Autofill AI is allowed by policy and enabled.
TEST_F(AutofillSettingsTableViewControllerTest,
       TestModelWithPolicyAllowedAndEnabled) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setEnhancedAutofillEnabled:YES];
  [view_controller setAutofillAIAllowedByPolicy:YES];
  [view_controller reloadData];

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Switches
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTextWithId(YES, IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE, 0,
                                    0);
  CheckSectionFooterWithId(IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL, 0);

  // Section 1: When On
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON, 1);
  CheckTextCellTextWithId(
      IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS, 1, 0);

  // Section 2: Things to Consider
  EXPECT_EQ(1, NumberOfItemsInSection(2));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER, 2);
  CheckTextCellTextWithId(IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE, 2,
                          0);
}

// Test model when Autofill AI is allowed by policy but disabled.
TEST_F(AutofillSettingsTableViewControllerTest,
       TestModelWithPolicyAllowedAndDisabled) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setEnhancedAutofillEnabled:NO];
  [view_controller setAutofillAIAllowedByPolicy:YES];
  [view_controller reloadData];

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Switches
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTextWithId(NO, IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE, 0,
                                    0);
  CheckSectionFooterWithId(IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL, 0);

  // Section 1: When On
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON, 1);
  CheckTextCellTextWithId(
      IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS, 1, 0);

  // Section 2: Things to Consider
  EXPECT_EQ(1, NumberOfItemsInSection(2));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER, 2);
  CheckTextCellTextWithId(IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE, 2,
                          0);
}

// Test model when Autofill AI is not allowed by policy but enabled.
TEST_F(AutofillSettingsTableViewControllerTest,
       TestModelWithPolicyDisallowedAndEnabled) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setEnhancedAutofillEnabled:YES];
  [view_controller setAutofillAIAllowedByPolicy:NO];
  [view_controller reloadData];

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Switches
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTextWithId(YES, IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE, 0,
                                    0);
  CheckSectionFooterWithId(IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL, 0);

  // Section 1: When On
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON, 1);
  CheckTextCellTextWithId(
      IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS, 1, 0);

  // Section 2: Things to Consider
  EXPECT_EQ(2, NumberOfItemsInSection(2));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER, 2);
  CheckTextCellTextWithId(IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE, 2,
                          0);
  CheckTextCellTextWithId(
      IDS_SETTINGS_AUTOFILL_AI_ENTERPRISE_LOGGING_MANAGED_DISABLED, 2, 1);
}

// Test model when Autofill AI is not allowed by policy and disabled.
TEST_F(AutofillSettingsTableViewControllerTest,
       TestModelWithPolicyDisallowedAndDisabled) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setEnhancedAutofillEnabled:NO];
  [view_controller setAutofillAIAllowedByPolicy:NO];
  [view_controller reloadData];

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Switches
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTextWithId(NO, IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE, 0,
                                    0);
  CheckSectionFooterWithId(IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL, 0);

  // Section 1: When On
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON, 1);
  CheckTextCellTextWithId(
      IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS, 1, 0);

  // Section 2: Things to Consider
  EXPECT_EQ(2, NumberOfItemsInSection(2));
  CheckSectionHeaderWithId(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER, 2);
  CheckTextCellTextWithId(IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE, 2,
                          0);
  CheckTextCellTextWithId(
      IDS_SETTINGS_AUTOFILL_AI_ENTERPRISE_LOGGING_MANAGED_DISABLED, 2, 1);
}

// Test moving away from the view controller invokes didRemove delegate.
TEST_F(AutofillSettingsTableViewControllerTest, TestDelegateOnRemove) {
  EXPECT_FALSE(fake_delegate_.wasRemovedCalled);
  [controller() didMoveToParentViewController:nil];
  EXPECT_TRUE(fake_delegate_.wasRemovedCalled);
}

// Test tapping the wallet promo card invokes didTapWalletPromoCard delegate.
TEST_F(AutofillSettingsTableViewControllerTest, TestWalletPromoCardTapped) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setShouldShowWalletPromo:YES];
  [view_controller reloadData];

  EXPECT_FALSE(fake_delegate_.wasWalletPromoCardTapped);

  NSIndexPath* path = [NSIndexPath indexPathForItem:1 inSection:3];
  [view_controller tableView:view_controller.tableView
      didSelectRowAtIndexPath:path];

  EXPECT_TRUE(fake_delegate_.wasWalletPromoCardTapped);
}

// Test switch changed.
TEST_F(AutofillSettingsTableViewControllerTest, TestSwitchChanged) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setEnhancedAutofillEnabled:NO];
  [view_controller reloadData];

  // Verify initial state.
  CheckSwitchCellStateAndTextWithId(NO, IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE, 0,
                                    0);

  // Toggle switch.
  TableViewCellContentView* content_view =
      base::apple::ObjCCastStrict<TableViewCellContentView>([[view_controller
                      tableView:view_controller.tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0
                                                    inSection:0]] contentView]);
  ASSERT_TRUE(content_view);
  SwitchContentView* switch_content_view =
      base::apple::ObjCCastStrict<SwitchContentView>(
          [content_view trailingContentViewForTesting]);
  ASSERT_TRUE(switch_content_view);
  UISwitch* switch_view = [switch_content_view switchForTesting];
  switch_view.on = YES;
  [switch_view sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_TRUE(fake_mutator_.enhancedAutofillEnabled);
  EXPECT_EQ(1, fake_mutator_.setEnhancedAutofillEnabledCallCount);
  CheckSwitchCellStateAndTextWithId(YES, IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE, 0,
                                    0);
}

// Test user verification switch changed.
TEST_F(AutofillSettingsTableViewControllerTest,
       TestUserVerificationSwitchChanged) {
  AutofillSettingsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillSettingsTableViewController>(
          controller());

  [view_controller setUserVerificationSettingVisible:YES];
  [view_controller setUserVerificationSwitchEnabled:YES];
  [view_controller setUserVerificationEnabled:NO];
  [view_controller view];

  // Verify initial state. Row 0 in section 3.
  CheckSwitchCellStateAndTextWithId(
      NO, IDS_IOS_AUTOFILL_VERIFICATION_INFO_LABEL, 3, 0);

  // Toggle switch.
  TableViewCellContentView* content_view =
      base::apple::ObjCCastStrict<TableViewCellContentView>([[view_controller
                      tableView:view_controller.tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0
                                                    inSection:3]] contentView]);
  ASSERT_TRUE(content_view);
  SwitchContentView* switch_content_view =
      base::apple::ObjCCastStrict<SwitchContentView>(
          [content_view trailingContentViewForTesting]);
  ASSERT_TRUE(switch_content_view);
  UISwitch* switch_view = [switch_content_view switchForTesting];
  switch_view.on = YES;
  [switch_view sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_TRUE(fake_mutator_.userVerificationEnabled);
  EXPECT_EQ(1, fake_mutator_.setUserVerificationEnabledCallCount);
  CheckSwitchCellStateAndTextWithId(
      YES, IDS_IOS_AUTOFILL_VERIFICATION_INFO_LABEL, 3, 0);
}

}  // namespace
