// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

#import "base/strings/string_number_conversions.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr char kName[] = "user";

}  // namespace

class FamilyPickerViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  FamilyPickerViewControllerTest() = default;

  ChromeTableViewController* InstantiateController() override {
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
      [recipients addObject:([[RecipientInfoForIOSDisplay alloc]
                                initWithRecipientInfo:recipient])];
    }

    FamilyPickerViewController* family_controller =
        static_cast<FamilyPickerViewController*>(controller());
    [family_controller setRecipients:recipients];
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
    SettingsImageDetailTextItem* item =
        static_cast<SettingsImageDetailTextItem*>(
            GetTableViewItem(/*section=*/0, i));
    EXPECT_NSEQ(item.text, ([NSString stringWithFormat:@"%@%d", @"user", i]));
    EXPECT_NSEQ(
        item.detailText,
        ([NSString stringWithFormat:@"%@%d%@", @"test", i, @"@gmail.com"]));
  }
}
