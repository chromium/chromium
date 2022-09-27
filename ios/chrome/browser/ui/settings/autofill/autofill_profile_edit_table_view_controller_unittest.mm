// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"

#import <memory>

#import "base/feature_list.h"
#import "base/guid.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char16_t kTestHonorificPrefix[] = u"";
const char16_t kTestFullName[] = u"That Guy John";
const char16_t kTestCompany[] = u"Awesome Inc.";
const char16_t kTestAddressLine1[] = u"Some person's garage";
const char16_t kTestAddressLine2[] = u"Near the lake";
const char16_t kTestCity[] = u"Springfield";
const char16_t kTestState[] = u"IL";
const char16_t kTestZip[] = u"55123";
const char16_t kTestCountryCode[] = u"US";
const char16_t kTestCountry[] = u"United States";
const char16_t kTestPhone[] = u"16502530000";
const char16_t kTestEmail[] = u"test@email.com";

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
    test_cbs_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillUseAlternativeStateNameMap)) {
      personal_data_manager_->personal_data_manager_cleaner_for_testing()
          ->alternative_state_name_map_updater_for_testing()
          ->set_local_state_for_testing(local_state_.Get());
    }
    personal_data_manager_->OnSyncServiceInitialized(nullptr);
    PersonalDataManagerFinishedProfileTasksWaiter waiter(
        personal_data_manager_);

    std::string guid = base::GenerateGUID();

    autofill::AutofillProfile autofill_profile;
    autofill_profile =
        autofill::AutofillProfile(guid, "https://www.example.com/");
    autofill_profile.SetRawInfo(autofill::NAME_HONORIFIC_PREFIX,
                                kTestHonorificPrefix);
    autofill_profile.SetRawInfo(autofill::NAME_FULL, kTestFullName);
    autofill_profile.SetRawInfo(autofill::COMPANY_NAME, kTestCompany);
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                                kTestAddressLine1);
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_LINE2,
                                kTestAddressLine2);
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_CITY, kTestCity);
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_STATE, kTestState);
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, kTestZip);
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY,
                                kTestCountryCode);
    autofill_profile.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER, kTestPhone);
    autofill_profile.SetRawInfo(autofill::EMAIL_ADDRESS, kTestEmail);
    personal_data_manager_->SaveImportedProfile(autofill_profile);
    waiter.Wait();  // Wait for the completion of the asynchronous operation.

    autofill_profile_edit_controller_ = [AutofillProfileEditTableViewController
        controllerWithProfile:autofill_profile
          personalDataManager:personal_data_manager_];

    // Load the view to force the loading of the model.
    [autofill_profile_edit_controller_ loadViewIfNeeded];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  autofill::PersonalDataManager* personal_data_manager_;
  AutofillProfileEditTableViewController* autofill_profile_edit_controller_;
};

// Default test case of no addresses or credit cards.
TEST_F(AutofillProfileEditTableViewControllerTest, TestInitialization) {
  TableViewModel* model = [autofill_profile_edit_controller_ tableViewModel];
  int rowCnt =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHonorificPrefixes)
          ? 11
          : 10;

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(rowCnt, [model numberOfItemsInSection:0]);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileEditTableViewControllerTest, TestOneProfile) {
  TableViewModel* model = [autofill_profile_edit_controller_ tableViewModel];
  UITableView* tableView = [autofill_profile_edit_controller_ tableView];

  std::vector<const char16_t*> expected_values = {
      kTestFullName, kTestCompany, kTestAddressLine1, kTestAddressLine2,
      kTestCity,     kTestState,   kTestZip,          kTestCountry,
      kTestPhone,    kTestEmail};
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
    expected_values.insert(expected_values.begin(), kTestHonorificPrefix);
  }

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(expected_values.size(), (size_t)[model numberOfItemsInSection:0]);

  for (size_t row = 0; row < expected_values.size(); row++) {
    NSIndexPath* path = [NSIndexPath indexPathForRow:row inSection:0];
    UIView* cell = [autofill_profile_edit_controller_ tableView:tableView
                                          cellForRowAtIndexPath:path];
    NSArray* textFields = FindTextFieldDescendants(cell);
    EXPECT_TRUE([textFields count] > 0);
    UITextField* field = [textFields objectAtIndex:0];
    EXPECT_TRUE([field isKindOfClass:[UITextField class]]);
    EXPECT_TRUE([[field text]
        isEqualToString:base::SysUTF16ToNSString(expected_values[row])]);
  }
}

}  // namespace
