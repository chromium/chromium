// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
  AutofillAndPasswordsTableViewControllerTest() {
    feature_list_.InitAndEnableFeature(kYourSavedInfoSettingsPageIos);
  }

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillAndPasswordsTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AutofillAndPasswordsTableViewControllerTest, TestModel) {
  AutofillAndPasswordsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAndPasswordsTableViewController>(
          controller());

  [view_controller setPasswordsEnabled:YES];
  [view_controller setAutofillCreditCardEnabled:NO];
  [view_controller setAutofillProfileEnabled:YES];
  [view_controller setIdentityDocsEnabled:YES];
  [view_controller setTravelInfoEnabled:NO];
  [view_controller setShouldShowAutofillAIFeatures:YES];

  [view_controller loadModel];

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(5, NumberOfItemsInSection(0));

  CheckDetailItemTextWithIds(IDS_IOS_PASSWORD_MANAGER, IDS_IOS_SETTING_ON, 0,
                             0);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_PAYMENTS_TITLE, IDS_IOS_SETTING_OFF,
                             0, 1);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_CONTACT_INFO_TITLE,
                             IDS_IOS_SETTING_ON, 0, 2);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_IDENTITY_DOCS_TITLE,
                             IDS_IOS_SETTING_ON, 0, 3);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_TRAVEL_TITLE, IDS_IOS_SETTING_OFF, 0,
                             4);
}

TEST_F(AutofillAndPasswordsTableViewControllerTest,
       TestIdentityDocsAndTravelInfoHidden) {
  AutofillAndPasswordsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAndPasswordsTableViewController>(
          controller());

  [view_controller setPasswordsEnabled:YES];
  [view_controller setAutofillCreditCardEnabled:NO];
  [view_controller setAutofillProfileEnabled:YES];
  [view_controller setIdentityDocsEnabled:YES];
  [view_controller setTravelInfoEnabled:NO];
  [view_controller setShouldShowAutofillAIFeatures:NO];

  [view_controller loadModel];

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  CheckDetailItemTextWithIds(IDS_IOS_PASSWORD_MANAGER, IDS_IOS_SETTING_ON, 0,
                             0);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_PAYMENTS_TITLE, IDS_IOS_SETTING_OFF,
                             0, 1);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_CONTACT_INFO_TITLE,
                             IDS_IOS_SETTING_ON, 0, 2);
}

}  // namespace
