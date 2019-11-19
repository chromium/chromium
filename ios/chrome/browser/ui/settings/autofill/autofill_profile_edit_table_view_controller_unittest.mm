// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#include <memory>

#include "base/guid.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kTestFullName[] = "That Guy John";
const char kTestAddressLine1[] = "Some person's garage";

static NSArray* FindTextFieldDescendants(UIView* root) {
  NSMutableArray* textFields = [NSMutableArray array];
  NSMutableArray* descendants = [NSMutableArray array];

  [descendants addObject:root];

  while ([descendants count]) {
    UIView* view = [descendants objectAtIndex:0];
    if ([view isKindOfClass:[UITextField class]])
      [textFields addObject:view];

    [descendants addObjectsFromArray:[view subviews]];
    [descendants removeObjectAtIndex:0];
  }

  return textFields;
}

class AutofillProfileEditTableViewControllerTest : public PlatformTest {
 protected:
  AutofillProfileEditTableViewControllerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateWebDataService();
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    PersonalDataManagerFinishedProfileTasksWaiter waiter(
        personal_data_manager_);

    std::string guid = base::GenerateGUID();

    autofill::AutofillProfile autofill_profile;
    autofill_profile =
        autofill::AutofillProfile(guid, "https://www.example.com/");
    autofill_profile.SetRawInfo(autofill::NAME_FULL,
                                base::UTF8ToUTF16(kTestFullName));
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                                base::UTF8ToUTF16(kTestAddressLine1));

    personal_data_manager_->SaveImportedProfile(autofill_profile);
    waiter.Wait();  // Wait for the completion of the asynchronous operation.

    autofill_profile_edit_controller_ = [AutofillProfileEditTableViewController
        controllerWithProfile:autofill_profile
          personalDataManager:personal_data_manager_];

    // Load the view to force the loading of the model.
    [autofill_profile_edit_controller_ loadViewIfNeeded];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  autofill::PersonalDataManager* personal_data_manager_;
  AutofillProfileEditTableViewController* autofill_profile_edit_controller_;
};

// Default test case of no addresses or credit cards.
TEST_F(AutofillProfileEditTableViewControllerTest, TestInitialization) {
  TableViewModel* model = [autofill_profile_edit_controller_ tableViewModel];

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(10, [model numberOfItemsInSection:0]);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileEditTableViewControllerTest, TestOneProfile) {
  TableViewModel* model = [autofill_profile_edit_controller_ tableViewModel];
  UITableView* tableView = [autofill_profile_edit_controller_ tableView];

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(10, [model numberOfItemsInSection:0]);

  NSIndexPath* path = [NSIndexPath indexPathForRow:0 inSection:0];

  UIView* cell = [autofill_profile_edit_controller_ tableView:tableView
                                        cellForRowAtIndexPath:path];

  NSArray* textFields = FindTextFieldDescendants(cell);
  EXPECT_TRUE([textFields count] > 0);
  UITextField* field = [textFields objectAtIndex:0];
  EXPECT_TRUE([field isKindOfClass:[UITextField class]]);
  EXPECT_TRUE(
      [[field text] isEqualToString:base::SysUTF8ToNSString(kTestFullName)]);

  path = [NSIndexPath indexPathForRow:2 inSection:0];
  cell = [autofill_profile_edit_controller_ tableView:tableView
                                cellForRowAtIndexPath:path];
  textFields = FindTextFieldDescendants(cell);
  EXPECT_TRUE([textFields count] > 0);
  field = [textFields objectAtIndex:0];
  EXPECT_TRUE([field isKindOfClass:[UITextField class]]);
  EXPECT_TRUE([[field text]
      isEqualToString:base::SysUTF8ToNSString(kTestAddressLine1)]);
}

}  // namespace
