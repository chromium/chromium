// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"

#import <map>
#import <memory>
#import <string>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager_test_util.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller+testing.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_profile_item.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);

    // Set circular SyncService dependency to null.
    autofill::PersonalDataManagerFactory::GetForProfile(profile_)
        ->SetSyncServiceForTest(nullptr);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<AutofillProfileTableViewController>(
        controller()) settingsWillBeDismissed];
    // Call `LegacyChromeTableViewControllerTest::TearDown()` first to drain the
    // autorelease pool and deallocate the controller while `browser_` and
    // `profile_` are still valid.
    LegacyChromeTableViewControllerTest::TearDown();
    browser_.reset();
    profile_ = nullptr;
  }

  // Helper method to add an autofill::AutofillProfile to the
  // PersonalDataManager. The profile is constructed based on the `data` map,
  // which maps autofill::FieldType keys to their string values.
  void AddProfile(const std::map<autofill::FieldType, std::string>& data) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_);
    personal_data_manager->SetSyncServiceForTest(nullptr);
    autofill::PersonalDataChangedWaiter waiter(*personal_data_manager);

    autofill::AutofillProfile autofill_profile(
        autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
    for (const auto& pair : data) {
      autofill_profile.SetInfo(pair.first, base::ASCIIToUTF16(pair.second),
                               l10n_util::GetLocaleOverride());
    }
    personal_data_manager->address_data_manager().AddProfile(autofill_profile);
    std::move(waiter).Wait();  // Wait for completion of the async operation.
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
};

// Tests that initializing the controller with no addresses adds only the
// switches section.
TEST_F(AutofillProfileTableViewControllerTest, TestInitialization) {
  LegacyChromeTableViewController* controller =
      LegacyChromeTableViewControllerTest::controller();
  CheckController();
  CheckTitleWithId(IDS_AUTOFILL_CONTACT_INFO_TITLE);

  // Expect only the header section.
  EXPECT_EQ(1, NumberOfSections());
  // Expect header section to contain one row (the address Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Expect subtitle section to contain one row (the address Autofill toggle
  // subtitle).
  EXPECT_NE(nil, [controller.tableViewModel footerForSectionIndex:0]);

  // Check the footer of the first section.
  CheckSectionFooterWithId(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL, 0);

  // Check the add button has no menu and is configured with a direct action.
  AutofillProfileTableViewController* viewController =
      base::apple::ObjCCastStrict<AutofillProfileTableViewController>(
          controller);
  UIBarButtonItem* addButton = [viewController addButtonInToolbar];
  EXPECT_EQ(nil, addButton.menu);
  EXPECT_EQ(@selector(handleAddAddress), addButton.action);
  EXPECT_EQ(viewController, addButton.target);
}

// Tests that adding a single address results in an address section.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfile) {
  AddProfile({{autofill::NAME_FULL, "John Doe"},
              {autofill::ADDRESS_HOME_LINE1, "1 Main Street"}});
  CreateController();
  CheckController();

  // Expect two sections (address toggle, addresses).
  EXPECT_EQ(2, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Checks if city is set as the default `detailText` when the
// `ADDRESS_HOME_LINE1` is empty.
TEST_F(AutofillProfileTableViewControllerTest,
       TestDetailTextFallbackValueCity) {
  AddProfile({
      {autofill::ADDRESS_HOME_CITY, "Montreal"},
  });

  CreateController();
  CheckController();

  AutofillProfileItem* item = base::apple::ObjCCastStrict<AutofillProfileItem>(
      GetTableViewItem(/*section=*/1, /*item=*/0));

  EXPECT_NSEQ(@"Montreal", item.detailText);
}

// Checks if city is set as the default `detailText` when the state and zip code
// are not empty.
TEST_F(AutofillProfileTableViewControllerTest,
       TestDetailTextFallbackValuePriorityCheck) {
  AddProfile({
      {autofill::ADDRESS_HOME_CITY, "Montreal"},
      {autofill::ADDRESS_HOME_STATE, "Quebec"},
      {autofill::ADDRESS_HOME_ZIP, "A1B 2C3"},
  });

  CreateController();
  CheckController();

  AutofillProfileItem* item = base::apple::ObjCCastStrict<AutofillProfileItem>(
      GetTableViewItem(/*section=*/1, /*item=*/0));

  EXPECT_NSEQ(@"Montreal", item.detailText);
}

// Checks if country is set as the default `detailText` when the user adds an
// empty address.
TEST_F(AutofillProfileTableViewControllerTest,
       TestDetailTextFallbackValueForEmptyAddress) {
  AddProfile({
      {autofill::ADDRESS_HOME_COUNTRY, "Canada"},
  });

  CreateController();
  CheckController();

  AutofillProfileItem* item = base::apple::ObjCCastStrict<AutofillProfileItem>(
      GetTableViewItem(/*section=*/1, /*item=*/0));

  EXPECT_NSEQ(@"Canada", item.detailText);
}

}  // namespace
