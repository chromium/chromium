// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"

#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class AutofillProfileTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  AutofillProfileTableViewControllerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    // Profile import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    chrome_browser_state_->CreateWebDataService();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  void AddProfile(const std::string& origin,
                  const std::string& name,
                  const std::string& address) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);

    autofill::AutofillProfile autofill_profile(base::GenerateGUID(), origin);
    autofill_profile.SetRawInfo(autofill::NAME_FULL, base::ASCIIToUTF16(name));
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                                base::ASCIIToUTF16(address));
    personal_data_manager->SaveImportedProfile(autofill_profile);
    waiter.Wait();  // Wait for completion of the asynchronous operation.
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Default test case of no addresses.
TEST_F(AutofillProfileTableViewControllerTest, TestInitialization) {
  ChromeTableViewController* controller =
      ChromeTableViewControllerTest::controller();
  CheckController();

  // Expect only the header section.
  EXPECT_EQ(1, NumberOfSections());
  // Expect header section to contain one row (the address Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Expect subtitle section to contain one row (the address Autofill toggle
  // subtitle).
  EXPECT_NE(nil, [controller.tableViewModel footerForSection:0]);

  // Check the footer of the first section.
  CheckSectionFooterWithId(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL, 0);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfile) {
  AddProfile("https://www.example.com/", "John Doe", "1 Main Street");
  CreateController();
  CheckController();

  // Expect two sections (header, and addresses section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Deleting the only profile results in item deletion and section deletion.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfileItemDeleted) {
  AddProfile("https://www.example.com/", "John Doe", "1 Main Street");
  CreateController();
  CheckController();

  // Expect two sections (header and addresses section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));

  AutofillProfileTableViewController* view_controller =
      base::mac::ObjCCastStrict<AutofillProfileTableViewController>(
          controller());
  // Put the tableView in 'edit' mode.
  [view_controller editButtonPressed];

  AutofillProfileTableViewController* autofill_controller =
      static_cast<AutofillProfileTableViewController*>(controller());
  [autofill_controller deleteItems:@[ [NSIndexPath indexPathForRow:0
                                                         inSection:1] ]];

  // Verify the resulting UI.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return NumberOfSections() == 1;
      }));

  // Exit 'edit' mode.
  [view_controller editButtonPressed];

  // Expect address section to have been removed.
  EXPECT_EQ(1, NumberOfSections());
}

}  // namespace
