// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_credit_card_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager_test_utils.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_card_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class AutofillCreditCardTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  AutofillCreditCardTableViewControllerTest() {
    TestProfileIOS::Builder builder;
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestProfileIOS by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnableCvcStorageAndFilling);
    // Set circular SyncService dependency to null.
    autofill::PersonalDataManagerFactory::GetForProfile(profile_.get())
        ->SetSyncServiceForTest(nullptr);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillCreditCardTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<AutofillCreditCardTableViewController>(
        controller()) settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  void AddCreditCard(const std::string& origin,
                     const std::string& card_holder_name,
                     const std::string& card_number,
                     const std::string& cvc = "") {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_.get());
    autofill::PersonalDataChangedWaiter waiter(*personal_data_manager);

    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(), origin);
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::ASCIIToUTF16(card_holder_name));
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::ASCIIToUTF16(card_number));
    if (!cvc.empty()) {
      credit_card.SetRawInfo(autofill::CREDIT_CARD_VERIFICATION_CODE,
                             base::ASCIIToUTF16(cvc));
    }
    personal_data_manager->payments_data_manager()
        .OnAcceptedLocalCreditCardSave(credit_card);
    personal_data_manager->address_data_manager()
        .get_alternative_state_name_map_updater_for_testing()
        ->set_local_state_for_testing(local_state());
    std::move(waiter).Wait();  // Wait for completion of the async operation.
  }

  // Deletes the item at (section, row) and waits util condition returns true or
  // timeout.
  bool DeleteItemAndWait(int section, int row, ConditionBlock condition) {
    AutofillCreditCardTableViewController* view_controller =
        base::apple::ObjCCastStrict<AutofillCreditCardTableViewController>(
            controller());
    [view_controller deleteItems:@[ [NSIndexPath indexPathForRow:row
                                                       inSection:section] ]];
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, condition);
  }

  // Checks whether device reauth is supported.
  bool CheckCanAttemptReauth() {
    ReauthenticationModule* reauthModule =
        [[ReauthenticationModule alloc] init];
    return [reauthModule canAttemptReauth];
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  base::test::ScopedFeatureList feature_list_;
};

// Default test case of no credit cards.
TEST_F(AutofillCreditCardTableViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();

  // Expect three switch sections (the credit card Autofill switch, CVC
  // storage autofill and mandatory reauth switch).
  ASSERT_EQ(3, NumberOfSections());
  // Expect each switch section to contain one row.
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Adding a single credit card results in a credit card section.
TEST_F(AutofillCreditCardTableViewControllerTest, TestOneCreditCardWithoutCvc) {
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  // Expect four sections (credit card switch, mandatory reauth switch,
  // CVC storage switch and credit card section).
  ASSERT_EQ(4, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(3));

  // Check that the CVC indicator is not present when CVC is not stored.
  AutofillCardItem* item =
      base::apple::ObjCCastStrict<AutofillCardItem>(GetTableViewItem(3, 0));
  NSString* cvc_indicator = l10n_util::GetNSString(
      IDS_AUTOFILL_SETTINGS_PAGE_CVC_TAG_FOR_CREDIT_CARD_LIST_ENTRY);
  EXPECT_FALSE([item.leadingDetailText containsString:cvc_indicator])
      << "Expected to find CVC indicator in text: " << item.leadingDetailText;
}

// Deleting the only credit card results in item deletion and section deletion.
TEST_F(AutofillCreditCardTableViewControllerTest,
       TestOneCreditCardItemDeleted) {
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  // Expect four sections (credit card Autofill switch, mandatory reauth switch,
  // CVC storage switch and credit card section).
  ASSERT_EQ(4, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(3));

  // Delete the credit card item and check that the section is removed.
  EXPECT_TRUE(DeleteItemAndWait(3, 0, ^{
    return NumberOfSections() == 3;
  }));
}

// Tests that when the MandatoryReauth feature is enabled a switch
// appears.
TEST_F(AutofillCreditCardTableViewControllerTest,
       TestMandatoryReauthSwitchExists) {
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(personal_data_manager->payments_data_manager()
                  .IsPaymentMethodsMandatoryReauthEnabled());

  CreateController();
  CheckController();

  // Expect 3 sections, 1 for switches, 1 for mandatory reauth and 1 for CVC
  // storage switch.
  EXPECT_EQ(3, NumberOfSections());

  // Expect Mandatory Reauth section to have 2 items, the switch and the
  // subtitle.
  EXPECT_EQ(1, NumberOfItemsInSection(1));

  // Confirm the text to the side of the switch. Whether the switch is turned on
  // depends on whether the device supports device reauth.
  CheckSwitchCellStateAndText(
      CheckCanAttemptReauth(),
      l10n_util::GetNSString(
          IDS_PAYMENTS_AUTOFILL_ENABLE_MANDATORY_REAUTH_TOGGLE_LABEL),
      1, 0);

  // Confirm the sublabel of the switch.
  CheckSectionFooter(
      l10n_util::GetNSString(
          IDS_PAYMENTS_AUTOFILL_ENABLE_MANDATORY_REAUTH_TOGGLE_SUBLABEL),
      1);
}

// Tests that when the CVC storage feature is enabled, a CVC storage button
// appears.
TEST_F(AutofillCreditCardTableViewControllerTest, TestCVCStorageButtonExists) {
  CreateController();
  CheckController();

  // Expect 3 sections: Autofill switch, mandatory reauth switch, and CVC
  // storage.
  EXPECT_EQ(3, NumberOfSections());

  // Expect CVC storage section to have 1 item, the button.
  EXPECT_EQ(1, NumberOfItemsInSection(2));

  // Confirm the text of the button.
  CheckTextCellText(l10n_util::GetNSString(
                        IDS_PAYMENTS_AUTOFILL_ENABLE_SAVE_SECURITY_CODES_LABEL),
                    2, 0);

  // Confirm the sublabel of the button.
  CheckSectionFooter(
      l10n_util::GetNSString(
          IDS_PAYMENTS_AUTOFILL_ENABLE_SAVE_SECURITY_CODES_SUBLABEL),
      2);
}

// Tests that when the CVC storage feature is disabled, a CVC storage button
// does not appear.
TEST_F(AutofillCreditCardTableViewControllerTest,
       TestCVCStorageButtonNotExists_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillEnableCvcStorageAndFilling);

  CreateController();
  CheckController();

  // Expect 2 sections: Autofill switch, mandatory reauth switch.
  EXPECT_EQ(2, NumberOfSections());

  // Confirm the text of Autofill switch.
  CheckTextCellText(
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_LABEL), 0,
      0);

  // Confirm the text of the mandatory reauth.
  CheckTextCellText(
      l10n_util::GetNSString(
          IDS_PAYMENTS_AUTOFILL_ENABLE_MANDATORY_REAUTH_TOGGLE_LABEL),
      1, 0);
}

// Tests that the CVC indicator is present when CVC is stored.
TEST_F(AutofillCreditCardTableViewControllerTest, TestOneCreditCardWithCvc) {
  base::test::ScopedFeatureList feature_list(
      {autofill::features::kAutofillEnableCvcStorageAndFilling});
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005",
                "123");
  CreateController();
  CheckController();

  // Expect four sections (credit card switch, mandatory reauth switch,
  // CVC storage switch and credit card section).
  ASSERT_EQ(4, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(3));

  // Check that the CVC indicator is present when CVC is stored.
  AutofillCardItem* item =
      base::apple::ObjCCastStrict<AutofillCardItem>(GetTableViewItem(3, 0));
  NSString* cvc_indicator = l10n_util::GetNSString(
      IDS_AUTOFILL_SETTINGS_PAGE_CVC_TAG_FOR_CREDIT_CARD_LIST_ENTRY);
  EXPECT_TRUE([item.leadingDetailText containsString:cvc_indicator])
      << "Expected to find CVC indicator in text: " << item.leadingDetailText;
}

TEST_F(AutofillCreditCardTableViewControllerTest,
       TestOneCreditCardWithCvcItemDeleted) {
  base::test::ScopedFeatureList feature_list(
      {autofill::features::kAutofillEnableCvcStorageAndFilling});
  // Add a credit card with a CVC.
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005",
                "123");
  CreateController();
  CheckController();

  base::UserActionTester user_action_tester;

  // Expect four sections: the credit card Autofill switch, the CVC storage
  // switch, the mandatory reauth switch, and the credit card section.
  ASSERT_EQ(4, NumberOfSections());
  // Expect the credit card section to contain one row for the card itself.
  EXPECT_EQ(1, NumberOfItemsInSection(3));
  // The action should not have been recorded yet.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "AutofillCreditCardDeletedAndHadCvc"));

  // Delete the credit card item and check that the section is removed.
  EXPECT_TRUE(DeleteItemAndWait(3, 0, ^{
    return NumberOfSections() == 3;
  }));

  // Verify that the user action was recorded exactly once.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "AutofillCreditCardDeletedAndHadCvc"));
}

TEST_F(AutofillCreditCardTableViewControllerTest,
       TestOneCreditCardWithoutCvcItemDeleted_MetricNotLogged) {
  // Add a credit card without a CVC.
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  base::UserActionTester user_action_tester;

  // Expect four sections initially.
  ASSERT_EQ(4, NumberOfSections());
  // The action should not have been recorded yet.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "AutofillCreditCardDeletedAndHadCvc"));

  // Delete the credit card item and check that the section is removed.
  EXPECT_TRUE(DeleteItemAndWait(3, 0, ^{
    return NumberOfSections() == 3;
  }));

  // Verify that the user action was NOT recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "AutofillCreditCardDeletedAndHadCvc"));
}

}  // namespace
