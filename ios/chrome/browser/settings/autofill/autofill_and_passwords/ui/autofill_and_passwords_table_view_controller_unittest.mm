// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_signin_promo_item.h"
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
  EXPECT_EQ(6, NumberOfItemsInSection(0));

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
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS), nil, 0, 5);
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
  EXPECT_EQ(4, NumberOfItemsInSection(0));

  CheckDetailItemTextWithIds(IDS_IOS_PASSWORD_MANAGER, IDS_IOS_SETTING_ON, 0,
                             0);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_PAYMENTS_TITLE, IDS_IOS_SETTING_OFF,
                             0, 1);
  CheckDetailItemTextWithIds(IDS_AUTOFILL_CONTACT_INFO_TITLE,
                             IDS_IOS_SETTING_ON, 0, 2);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS), nil, 0, 3);
}

TEST_F(AutofillAndPasswordsTableViewControllerTest, PromoStateChanged) {
  AutofillAndPasswordsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAndPasswordsTableViewController>(
          controller());

  // Before loadModel is called, promoStateChanged should be a no-op.
  SigninPromoViewConfigurator* configurator =
      [[SigninPromoViewConfigurator alloc]
          initWithSigninPromoViewMode:SigninPromoViewModeNoAccounts
                            userEmail:nil
                        userGivenName:nil
                            userImage:nil
                       hasCloseButton:YES
                     hasSignInSpinner:NO];
  NSString* promo_text =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS);
  [view_controller promoStateChanged:YES
                   promoConfigurator:configurator
                           promoText:promo_text];

  [view_controller loadModel];

  EXPECT_EQ(1, NumberOfSections());

  // Add promo.
  [view_controller promoStateChanged:YES
                   promoConfigurator:configurator
                           promoText:promo_text];

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  TableViewSigninPromoItem* promo_item =
      base::apple::ObjCCastStrict<TableViewSigninPromoItem>(
          GetTableViewItem(0, 0));
  EXPECT_EQ(promo_item.configurator, configurator);
  EXPECT_NSEQ(promo_item.text, promo_text);

  // Calling promoStateChanged:YES again when promo already exists should be a
  // no-op.
  [view_controller promoStateChanged:YES
                   promoConfigurator:configurator
                           promoText:promo_text];

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  // Remove promo.
  [view_controller promoStateChanged:NO promoConfigurator:nil promoText:nil];

  EXPECT_EQ(1, NumberOfSections());

  // Calling promoStateChanged:NO again when promo is already removed should be
  // a no-op.
  [view_controller promoStateChanged:NO promoConfigurator:nil promoText:nil];

  EXPECT_EQ(1, NumberOfSections());
}

TEST_F(AutofillAndPasswordsTableViewControllerTest,
       ConfigureSigninPromoWithConfigurator) {
  AutofillAndPasswordsTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAndPasswordsTableViewController>(
          controller());

  [view_controller loadModel];

  SigninPromoViewConfigurator* configurator1 =
      [[SigninPromoViewConfigurator alloc]
          initWithSigninPromoViewMode:SigninPromoViewModeNoAccounts
                            userEmail:nil
                        userGivenName:nil
                            userImage:nil
                       hasCloseButton:YES
                     hasSignInSpinner:NO];

  // Calling configureSigninPromoWithConfigurator before promo exists should be
  // a no-op.
  [view_controller configureSigninPromoWithConfigurator:configurator1];

  NSString* promo_text =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS);
  [view_controller promoStateChanged:YES
                   promoConfigurator:configurator1
                           promoText:promo_text];

  TableViewSigninPromoItem* promo_item =
      base::apple::ObjCCastStrict<TableViewSigninPromoItem>(
          GetTableViewItem(0, 0));
  EXPECT_EQ(promo_item.configurator, configurator1);

  SigninPromoViewConfigurator* configurator2 =
      [[SigninPromoViewConfigurator alloc]
          initWithSigninPromoViewMode:SigninPromoViewModeNoAccounts
                            userEmail:nil
                        userGivenName:nil
                            userImage:nil
                       hasCloseButton:YES
                     hasSignInSpinner:NO];

  [view_controller configureSigninPromoWithConfigurator:configurator2];

  promo_item = base::apple::ObjCCastStrict<TableViewSigninPromoItem>(
      GetTableViewItem(0, 0));
  EXPECT_EQ(promo_item.configurator, configurator2);
}

}  // namespace
