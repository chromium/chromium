// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class AutofillProfileTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  AutofillProfileTableViewControllerTest() {
    TestProfileIOS::Builder builder;
    // Profile import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestProfileIOS by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    // Set circular SyncService dependency to null.
    autofill::PersonalDataManagerFactory::GetForProfile(profile_.get())
        ->SetSyncServiceForTest(nullptr);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<AutofillProfileTableViewController>(
        controller()) settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  void AddProfile(const std::string& name, const std::string& address) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_.get());
    personal_data_manager->address_data_manager()
        .get_alternative_state_name_map_updater_for_testing()
        ->set_local_state_for_testing(local_state());
    personal_data_manager->SetSyncServiceForTest(nullptr);
    autofill::PersonalDataChangedWaiter waiter(*personal_data_manager);

    autofill::AutofillProfile autofill_profile(
        autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
    autofill_profile.SetRawInfo(autofill::NAME_FULL, base::ASCIIToUTF16(name));
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                                base::ASCIIToUTF16(address));
    personal_data_manager->address_data_manager().AddProfile(autofill_profile);
    std::move(waiter).Wait();  // Wait for completion of the async operation.
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
};

// Default test case of no addresses.
TEST_F(AutofillProfileTableViewControllerTest, TestInitialization) {
  LegacyChromeTableViewController* controller =
      LegacyChromeTableViewControllerTest::controller();
  CheckController();

  // Expect only the header section.
  EXPECT_EQ(1, NumberOfSections());
  // Expect header section to contain one row (the address Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Expect subtitle section to contain one row (the address Autofill toggle
  // subtitle).
  EXPECT_NE(nil, [controller.tableViewModel footerForSectionIndex:0]);

  // Check the footer of the first section.
  CheckSectionFooterWithId(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL, 0);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfile) {
  AddProfile("John Doe", "1 Main Street");
  CreateController();
  CheckController();

  // Expect two sections (header, and addresses section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

}  // namespace
