// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class AutofillAndPasswordsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillAndPasswordsTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }
};

TEST_F(AutofillAndPasswordsTableViewControllerTest, TestModel) {
  AutofillAndPasswordsTableViewController* controller =
      base::apple::ObjCCastStrict<AutofillAndPasswordsTableViewController>(
          this->controller());

  [controller setPasswordsEnabled:YES];
  [controller setAutofillCreditCardEnabled:NO];
  [controller setAutofillProfileEnabled:YES];

  [controller loadModel];

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  CheckDetailItemTextWithIds(IDS_IOS_PASSWORD_MANAGER, IDS_IOS_SETTING_ON, 0,
                             0);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_PAYMENT_METHODS, IDS_IOS_SETTING_OFF,
                             0, 1);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE,
                             IDS_IOS_SETTING_ON, 0, 2);
}

}  // namespace
