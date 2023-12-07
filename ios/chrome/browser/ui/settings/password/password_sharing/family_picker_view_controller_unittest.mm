// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using password_manager::RecipientInfo;

constexpr char kEmail[] = "test@gmail.com";
constexpr char kName[] = "user";
constexpr char kPublicKey[] = "123456789";
const uint32_t kPublicKeyVersion = 0;
const CGFloat kAccessorySymbolSize = 22;

}  // namespace

class FamilyPickerViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  FamilyPickerViewControllerTest() = default;

  LegacyChromeTableViewController* InstantiateController() override {
    return [[FamilyPickerViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

  void SetFamilyWithSize(int size) {
    NSMutableArray<RecipientInfoForIOSDisplay*>* recipients =
        [NSMutableArray array];

    for (int i = 0; i < size; i++) {
      password_manager::RecipientInfo recipient;
      const std::string num_str = base::NumberToString(i);
      recipient.email = "test" + num_str + "@gmail.com";
      recipient.user_name = kName + num_str;
      recipient.public_key.key = kPublicKey + num_str;
      recipient.public_key.key_version = kPublicKeyVersion;
      [recipients addObject:([[RecipientInfoForIOSDisplay alloc]
                                initWithRecipientInfo:recipient])];
    }

    FamilyPickerViewController* family_controller =
        static_cast<FamilyPickerViewController*>(controller());
    [family_controller setRecipients:recipients];
  }

  void SetFamilyWithRecipients(const std::vector<RecipientInfo>& recipients) {
    NSMutableArray<RecipientInfoForIOSDisplay*>* recipients_for_display =
        [NSMutableArray array];

    for (const RecipientInfo& recipient : recipients) {
      [recipients_for_display addObject:([[RecipientInfoForIOSDisplay alloc]
                                            initWithRecipientInfo:recipient])];
    }

    FamilyPickerViewController* family_controller =
        static_cast<FamilyPickerViewController*>(controller());
    [family_controller setRecipients:recipients_for_display];
  }

  void CheckCellText(NSString* expected_text, int section, int row) {
    SettingsImageDetailTextItem* item =
        static_cast<SettingsImageDetailTextItem*>(
            GetTableViewItem(section, row));
    EXPECT_NSEQ(expected_text, item.text);
  }

  void CheckCellDetailText(NSString* expected_text, int section, int row) {
    SettingsImageDetailTextItem* item =
        static_cast<SettingsImageDetailTextItem*>(
            GetTableViewItem(section, row));
    EXPECT_NSEQ(expected_text, item.detailText);
  }

  void CheckCellAccessoryViewImage(UIImage* expected_image,
                                   int section,
                                   int row) {
    UITableViewCell* cell =
        [controller() tableView:controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                     inSection:section]];
    ASSERT_TRUE([cell.accessoryView isKindOfClass:[UIImageView class]]);
    EXPECT_TRUE(
        [expected_image isEqual:((UIImageView*)cell.accessoryView).image]);
  }

  void CheckCellAccessoryViewButton(UIImage* expected_button,
                                    int section,
                                    int row) {
    UITableViewCell* cell =
        [controller() tableView:controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                     inSection:section]];
    ASSERT_TRUE([cell.accessoryView isKindOfClass:[UIButton class]]);
    EXPECT_TRUE(
        [expected_button isEqual:[((UIButton*)cell.accessoryView)
                                     imageForState:UIControlStateNormal]]);
  }
};

TEST_F(FamilyPickerViewControllerTest, TestFamilyPickerLayout) {
  SetFamilyWithSize(5);

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 5);
  CheckSectionHeader(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PICKER_SUBTITLE),
      0);
  for (int i = 0; i < 5; i++) {
    CheckCellText([NSString stringWithFormat:@"%@%d", @"user", i], 0, i);
    CheckCellDetailText(
        [NSString stringWithFormat:@"%@%d%@", @"test", i, @"@gmail.com"], 0, i);
    CheckCellAccessoryViewImage(
        DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize), 0, i);
  }
}

// Tests that ineligible password sharing recipient (without a public key) has
// an info button instead of a selectable checkmark icon.
TEST_F(FamilyPickerViewControllerTest, TestAccessoryViewOfIneligibleRecipient) {
  RecipientInfo recipient;
  recipient.email = kEmail;
  recipient.user_name = kName;
  recipient.public_key.key = "";
  SetFamilyWithRecipients({recipient});

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  CheckCellText(@"user", 0, 0);
  CheckCellDetailText(@"test@gmail.com", 0, 0);
  CheckCellAccessoryViewButton(
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kAccessorySymbolSize), 0,
      0);
}

// Tests accessory views on selecting and deselecting eligible password sharing
// recipient (with a public key).
TEST_F(FamilyPickerViewControllerTest, TestAccessoryViewOfEligibleRecipient) {
  SetFamilyWithSize(3);
  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 3);

  FamilyPickerViewController* family_controller =
      base::apple::ObjCCastStrict<FamilyPickerViewController>(controller());
  UITableView* tableView = family_controller.tableView;
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:1 inSection:0];

  CheckCellAccessoryViewImage(
      DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize), 0, 1);

  [tableView selectRowAtIndexPath:indexPath
                         animated:NO
                   scrollPosition:UITableViewScrollPositionNone];
  CheckCellAccessoryViewImage(
      DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                 kAccessorySymbolSize),
      0, 1);

  [tableView deselectRowAtIndexPath:indexPath animated:NO];
  CheckCellAccessoryViewImage(
      DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize), 0, 1);
}

TEST_F(FamilyPickerViewControllerTest, TestShareButtonEnabledWithSelectedRows) {
  SetFamilyWithSize(2);
  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 2);

  FamilyPickerViewController* family_controller =
      base::apple::ObjCCastStrict<FamilyPickerViewController>(controller());
  UITableView* tableView = family_controller.tableView;
  UIBarButtonItem* shareButton =
      family_controller.navigationItem.rightBarButtonItem;
  NSIndexPath* indexPath1 = [NSIndexPath indexPathForRow:0 inSection:0];
  NSIndexPath* indexPath2 = [NSIndexPath indexPathForRow:1 inSection:0];

  EXPECT_FALSE(shareButton.isEnabled);

  [tableView selectRowAtIndexPath:indexPath1
                         animated:NO
                   scrollPosition:UITableViewScrollPositionNone];
  [family_controller tableView:tableView didSelectRowAtIndexPath:indexPath1];
  EXPECT_TRUE(shareButton.isEnabled);

  [tableView selectRowAtIndexPath:indexPath2
                         animated:NO
                   scrollPosition:UITableViewScrollPositionNone];
  [family_controller tableView:tableView didSelectRowAtIndexPath:indexPath2];
  EXPECT_TRUE(shareButton.isEnabled);

  [tableView deselectRowAtIndexPath:indexPath1 animated:NO];
  [family_controller tableView:tableView didDeselectRowAtIndexPath:indexPath1];
  EXPECT_TRUE(shareButton.isEnabled);

  [tableView deselectRowAtIndexPath:indexPath2 animated:NO];
  [family_controller tableView:tableView didDeselectRowAtIndexPath:indexPath2];
  EXPECT_FALSE(shareButton.isEnabled);
}

// Tests that recipients are sorted in the table by eligibility for sharing
// (having public key) first and then by their name.
TEST_F(FamilyPickerViewControllerTest,
       RecipientsAreSortedByEligibilityAndName) {
  RecipientInfo recipient1;
  recipient1.user_name = "test2";
  RecipientInfo recipient2;
  recipient2.user_name = "test1";
  RecipientInfo recipient3;
  recipient3.public_key.key = kPublicKey;
  recipient3.public_key.key_version = kPublicKeyVersion;
  recipient3.user_name = "test4";
  RecipientInfo recipient4;
  recipient4.public_key.key = kPublicKey;
  recipient4.public_key.key_version = kPublicKeyVersion;
  recipient4.user_name = "test3";
  SetFamilyWithRecipients({recipient1, recipient2, recipient3, recipient4});

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 4);
  CheckCellText(@"test3", 0, 0);
  CheckCellText(@"test4", 0, 1);
  CheckCellText(@"test1", 0, 2);
  CheckCellText(@"test2", 0, 3);
}

// Tests that with one eligible recipient the cell should be preselected.
TEST_F(FamilyPickerViewControllerTest,
       TestAccessoryViewWithOneEligibleRecipient) {
  SetFamilyWithSize(1);
  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 1);

  FamilyPickerViewController* family_controller =
      base::apple::ObjCCastStrict<FamilyPickerViewController>(controller());

  // Simulate the view appearing.
  [family_controller viewWillAppear:YES];
  [family_controller viewDidAppear:YES];
  CheckCellAccessoryViewImage(
      DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                 kAccessorySymbolSize),
      0, 0);

  // Check that the cell still can be deselected.
  [family_controller.tableView
      deselectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                    animated:NO];
  CheckCellAccessoryViewImage(
      DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize), 0, 0);
}
