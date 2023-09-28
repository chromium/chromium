// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

class PasswordPickerViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  PasswordPickerViewControllerTest() = default;

  ChromeTableViewController* InstantiateController() override {
    return [[PasswordPickerViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

  void SetCredentials(int amount) {
    std::vector<password_manager::CredentialUIEntry> credentials;

    for (int i = 0; i < amount; i++) {
      const std::u16string num_str = base::NumberToString16(i);
      const std::u16string url = u"http://www.example" + num_str + u".com/";

      auto form = password_manager::PasswordForm();
      form.username_value = u"user" + num_str + u"@gmail.com";
      form.url = GURL(url);
      form.signon_realm = form.url.spec();
      form.federation_origin = url::Origin::Create(GURL(url));

      auto credential = password_manager::CredentialUIEntry(form);
      credentials.push_back(credential);
    }

    PasswordPickerViewController* password_picker_controller =
        static_cast<PasswordPickerViewController*>(controller());
    [password_picker_controller setCredentials:credentials];
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
