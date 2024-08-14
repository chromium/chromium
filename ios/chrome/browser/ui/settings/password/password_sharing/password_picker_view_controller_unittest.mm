// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

using password_manager::CredentialUIEntry;
using password_manager::PasskeyCredential;
using password_manager::PasswordForm;

class PasswordPickerViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  PasswordPickerViewControllerTest() = default;

  LegacyChromeTableViewController* InstantiateController() override {
    return [[PasswordPickerViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

  void SetCredentials(int amount) {
    std::vector<CredentialUIEntry> credentials;

    for (int i = 0; i < amount; i++) {
      const std::u16string num_str = base::NumberToString16(i);
      const std::u16string url = u"http://www.example" + num_str + u".com/";

      auto form = PasswordForm();
      form.username_value = u"user" + num_str + u"@gmail.com";
      form.url = GURL(url);
      form.signon_realm = form.url.spec();
      form.federation_origin = url::SchemeHostPort(GURL(url));
      credentials.push_back(CredentialUIEntry(form));
    }

    PasswordPickerViewController* password_picker_controller =
        static_cast<PasswordPickerViewController*>(controller());
    [password_picker_controller setCredentials:credentials];
  }

  void SetPasswordAndPasskey() {
    std::vector<CredentialUIEntry> credentials;

    const std::u16string url = u"http://www.example.com/";
    auto form = PasswordForm();
    form.username_value = u"user@gmail.com";
    form.url = GURL(url);
    form.signon_realm = form.url.spec();
    form.federation_origin = url::SchemeHostPort(GURL(url));
    credentials.push_back(CredentialUIEntry(form));

    PasskeyCredential passkey_credential(
        PasskeyCredential::Source::kGooglePasswordManager,
        PasskeyCredential::RpId("example.com"),
        PasskeyCredential::CredentialId({1, 2, 3, 4}),
        PasskeyCredential::UserId(), PasskeyCredential::Username("user"));
    CredentialUIEntry passkey(std::move(passkey_credential));
    credentials.push_back(passkey);

    PasswordPickerViewController* password_picker_controller =
        static_cast<PasswordPickerViewController*>(controller());
    [password_picker_controller setCredentials:credentials];
  }

  void CheckCellAccessoryType(UITableViewCellAccessoryType accessoryType,
                              int section,
                              int row) {
    UITableViewCell* cell =
        [controller() tableView:controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                     inSection:section]];
    EXPECT_EQ(cell.accessoryType, accessoryType);
  }

  void CheckCellUserInteraction(bool enabled, int section, int row) {
    UITableViewCell* cell =
        [controller() tableView:controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                     inSection:section]];
    EXPECT_EQ(cell.userInteractionEnabled, enabled);
  }

  void CheckCellDetailText(NSString* expected_text, int section, int row) {
    UITableViewCell* cell =
        [controller() tableView:controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                     inSection:section]];
    TableViewURLCell* URLCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cell);
    EXPECT_NSEQ(expected_text, URLCell.URLLabel.text);
  }

  void CheckCellTitleText(NSString* expected_text, int section, int row) {
    UITableViewCell* cell =
        [controller() tableView:controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row
                                                     inSection:section]];
    TableViewURLCell* URLCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cell);
    EXPECT_NSEQ(expected_text, URLCell.titleLabel.text);
  }
};

TEST_F(PasswordPickerViewControllerTest, TestPasswordPickerLayout) {
  SetCredentials(/*amount=*/5);

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 5);
  for (int i = 0; i < 5; i++) {
    TableViewURLItem* item =
        static_cast<TableViewURLItem*>(GetTableViewItem(/*section=*/0, i));
    EXPECT_NSEQ(
        ([NSString stringWithFormat:@"%@%d%@", @"user", i, @"@gmail.com"]),
        item.title);
    EXPECT_NSEQ(
        ([NSString stringWithFormat:@"%@%d%@", @"www.example", i, @".com"]),
        base::SysUTF8ToNSString(item.URL.gurl.host()));
  }
}

TEST_F(PasswordPickerViewControllerTest, TestPasskeyAndPasswordLayout) {
  SetPasswordAndPasskey();

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 2);

  CheckCellAccessoryType(UITableViewCellAccessoryCheckmark, 0, 0);
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 1);

  CheckCellUserInteraction(/*enabled=*/true, 0, 0);
  CheckCellUserInteraction(/*enabled=*/false, 0, 1);

  CheckCellDetailText(@"example.com", 0, 0);
  CheckCellDetailText(
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SHARING_PASSWORD_PICKER_PASSKEY_INFO),
      0, 1);
}

TEST_F(PasswordPickerViewControllerTest, TestNextButtonEnabledByDefault) {
  SetCredentials(/*amount=*/3);

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 3);

  PasswordPickerViewController* passwordPickerController =
      base::apple::ObjCCastStrict<PasswordPickerViewController>(controller());
  EXPECT_TRUE(
      passwordPickerController.navigationItem.rightBarButtonItem.isEnabled);
}

TEST_F(PasswordPickerViewControllerTest, TestSettingAccessoryType) {
  SetCredentials(/*amount=*/4);

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 4);

  // Check that the first row has a checkmark by default.
  CheckCellAccessoryType(UITableViewCellAccessoryCheckmark, 0, 0);
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 2);
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 3);

  // Select last row.
  PasswordPickerViewController* passwordPickerController =
      base::apple::ObjCCastStrict<PasswordPickerViewController>(controller());
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:3 inSection:0];
  [passwordPickerController.tableView
      selectRowAtIndexPath:indexPath
                  animated:NO
            scrollPosition:UITableViewScrollPositionNone];
  [passwordPickerController tableView:passwordPickerController.tableView
              didSelectRowAtIndexPath:indexPath];

  // Check that the last row has a checkmark.
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckCellAccessoryType(UITableViewCellAccessoryNone, 0, 2);
  CheckCellAccessoryType(UITableViewCellAccessoryCheckmark, 0, 3);
}

TEST_F(PasswordPickerViewControllerTest, TestPasswordsAreSortedBeforePasskeys) {
  // Create 3 passkey credentials and 3 password credentials with sequential
  // usernames. After the `setCredentials` call passwords should end up sorted
  // before passkeys.
  std::vector<CredentialUIEntry> credentials;

  for (int i = 1; i <= 3; i++) {
    PasskeyCredential passkey_credential(
        PasskeyCredential::Source::kGooglePasswordManager,
        PasskeyCredential::RpId("example.com"),
        PasskeyCredential::CredentialId({1, 2, 3, 4}),
        PasskeyCredential::UserId(),
        PasskeyCredential::Username("user" + base::NumberToString(i)));
    CredentialUIEntry passkey(std::move(passkey_credential));
    credentials.push_back(passkey);
  }

  for (int i = 4; i <= 6; i++) {
    const std::u16string url = u"http://www.example.com/";
    auto form = PasswordForm();
    form.username_value = u"user" + base::NumberToString16(i);
    form.url = GURL(url);
    form.signon_realm = form.url.spec();
    form.federation_origin = url::SchemeHostPort(GURL(url));
    credentials.push_back(CredentialUIEntry(form));
  }

  PasswordPickerViewController* password_picker_controller =
      static_cast<PasswordPickerViewController*>(controller());
  [password_picker_controller setCredentials:credentials];

  CheckCellTitleText(@"user4", 0, 0);
  CheckCellTitleText(@"user5", 0, 1);
  CheckCellTitleText(@"user6", 0, 2);
  CheckCellTitleText(@"user1", 0, 3);
  CheckCellTitleText(@"user2", 0, 4);
  CheckCellTitleText(@"user3", 0, 5);
}
