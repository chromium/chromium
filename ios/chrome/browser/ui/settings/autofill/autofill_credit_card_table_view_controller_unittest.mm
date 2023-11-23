// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
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
    TestChromeBrowserState::Builder test_cbs_builder;
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    test_cbs_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    // Set circular SyncService dependency to null.
    autofill::PersonalDataManagerFactory::GetForBrowserState(
        chrome_browser_state_.get())
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
                     const std::string& card_number) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);

    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(), origin);
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::ASCIIToUTF16(card_holder_name));
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::ASCIIToUTF16(card_number));
    personal_data_manager->OnAcceptedLocalCreditCardSave(credit_card);
    personal_data_manager->personal_data_manager_cleaner_for_testing()
        ->alternative_state_name_map_updater_for_testing()
        ->set_local_state_for_testing(local_state_.Get());
    waiter.Wait();  // Wait for completion of the asynchronous operation.
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

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
};

// Default test case of no credit cards.
TEST_F(AutofillCreditCardTableViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();

  // Expect one switch section.
  EXPECT_EQ(1, NumberOfSections());
  // Expect switch section to contain one row (the credit card Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
}

// Adding a single credit card results in a credit card section.
TEST_F(AutofillCreditCardTableViewControllerTest, TestOneCreditCard) {
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  // Expect two sections (switch and credit card section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Deleting the only credit card results in item deletion and section deletion.
TEST_F(AutofillCreditCardTableViewControllerTest,
       TestOneCreditCardItemDeleted) {
  AddCreditCard("https://www.example.com/", "John Doe", "378282246310005");
  CreateController();
  CheckController();

  // Expect two sections (switch and credit card section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect credit card section to contain one row (the credit card itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));

  // Delete the credit card item and check that the section is removed.
  EXPECT_TRUE(DeleteItemAndWait(1, 0, ^{
    return NumberOfSections() == 1;
  }));
}

// Tests that when the MandatoryReauth feature is enabled a switch
// appears.
TEST_F(AutofillCreditCardTableViewControllerTest,
       TestMandatoryReauthSwitchExists) {
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillEnablePaymentsMandatoryReauth};
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_browser_state_.get());
  EXPECT_TRUE(personal_data_manager->IsPaymentMethodsMandatoryReauthEnabled());

  CreateController();
  CheckController();

  // Expect 2 sections, 1 for switches and 1 for mandatory reauth.
  EXPECT_EQ(2, NumberOfSections());

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

}  // namespace
