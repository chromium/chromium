// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"

#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class AutofillCreditCardTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  AutofillCreditCardTableViewControllerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    WebStateList* web_state_list = nullptr;
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list);
    // Credit card import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    chrome_browser_state_->CreateWebDataService();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[AutofillCreditCardTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  void AddCreditCard(const std::string& origin,
                     const std::string& card_holder_name,
                     const std::string& card_number) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);

    autofill::CreditCard credit_card(base::GenerateGUID(), origin);
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::ASCIIToUTF16(card_holder_name));
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::ASCIIToUTF16(card_number));
    personal_data_manager->OnAcceptedLocalCreditCardSave(credit_card);
    waiter.Wait();  // Wait for completion of the asynchronous operation.
  }

  // Deletes the item at (section, row) and waits util condition returns true or
  // timeout.
  bool deleteItemAndWait(int section, int row, ConditionBlock condition) {
    AutofillCreditCardTableViewController* view_controller =
        base::mac::ObjCCastStrict<AutofillCreditCardTableViewController>(
            controller());
    [view_controller deleteItems:@[ [NSIndexPath indexPathForRow:row
                                                       inSection:section] ]];
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, condition);
  }

  web::WebTaskEnvironment task_environment_;
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
  EXPECT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfSections() == 1;
  }));
}

}  // namespace
