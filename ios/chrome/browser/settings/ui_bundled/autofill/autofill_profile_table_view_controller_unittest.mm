// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/with_feature_override.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager_test_utils.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#import "components/plus_addresses/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller+testing.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_profile_item.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/gtest_support.h"
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
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Set circular SyncService dependency to null.
    autofill::PersonalDataManagerFactory::GetForProfile(profile_.get())
        ->SetSyncServiceForTest(nullptr);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAiCreateEntityDataManager,
         autofill::features::kAutofillAiWithDataSchema,
         autofill::features::kAutofillAiReauthRequired},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<AutofillProfileTableViewController>(
        controller()) settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  void SignIn() {
    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager->AddIdentity(fake_identity);

    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(account_manager_service->GetDefaultIdentity(),
                         signin_metrics::AccessPoint::kStartPage);
  }

  // Helper method to add an autofill::AutofillProfile to the
  // PersonalDataManager. The profile is constructed based on the `data` map,
  // which maps autofill::FieldType keys to their string values.
  void AddProfile(const std::map<autofill::FieldType, std::string>& data) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_.get());
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

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  base::test::ScopedFeatureList feature_list_;
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
  EXPECT_EQ(3, NumberOfSections());
  // Expect header section to contain one row (the address Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Expect subtitle section to contain one row (the address Autofill toggle
  // subtitle).
  EXPECT_NE(nil, [controller.tableViewModel footerForSectionIndex:0]);
  // Expect header section to contain one row (the enhanced Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  EXPECT_EQ(nil, [controller.tableViewModel footerForSectionIndex:1]);
  // Expect header section to contain one row (the user verification toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(2));
  // Expect subtitle section to contain one row (the use verification toggle
  // subtitle).
  EXPECT_NE(nil, [controller.tableViewModel footerForSectionIndex:2]);

  // Check the footer of the first section.
  CheckSectionFooterWithId(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL, 0);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfile) {
  AddProfile({{autofill::NAME_FULL, "John Doe"},
              {autofill::ADDRESS_HOME_LINE1, "1 Main Street"}});
  CreateController();
  CheckController();

  // Expect four sections (address toggle, enhanced autofill, user verification,
  // addresses).
  EXPECT_EQ(4, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(3));
}

// Checks if there is a plus address section when
// `plus_addresses::features::kPlusAddressesEnabled` is enabled.
TEST_F(AutofillProfileTableViewControllerTest, TestPlusAddressSection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      plus_addresses::features::kPlusAddressesEnabled);

  SignIn();

  LegacyChromeTableViewController* controller =
      LegacyChromeTableViewControllerTest::controller();
  CheckController();

  // Expect 4 sections (address toggle, plus address, enhanced autofill, user
  // verification).
  EXPECT_EQ(4, NumberOfSections());
  // Expect plus address section to contain one row.
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  // Expect subtitle section to contain one row.
  EXPECT_NE(nil, [controller.tableViewModel footerForSectionIndex:1]);

  // Check the footer of the sections.
  CheckSectionFooterWithId(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL, 0);
  CheckSectionFooterWithId(IDS_PLUS_ADDRESS_SETTINGS_SUBLABEL, 1);
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
      GetTableViewItem(/*section=*/3, /*item=*/0));

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
      GetTableViewItem(/*section=*/3, /*item=*/0));

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
      GetTableViewItem(/*section=*/3, /*item=*/0));

  EXPECT_NSEQ(@"Canada", item.detailText);
}

// Checks that the enhanced autofill menu item is visible when the feature is
// enabled.
TEST_F(AutofillProfileTableViewControllerTest,
       TestEnhancedAutofillMenuPresent) {
  CreateController();
  CheckController();

  TableViewDetailIconItem* item =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          GetTableViewItem(/*section=*/1, /*item=*/0));

  NSString* text = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);
  EXPECT_NSEQ(text, item.text);
}

// TODO(crbug.com/496456595): Alter this test once YourSavedInfoSettingsPageIos
// is fully rolled out.
class AutofillProfileTableViewControllerTitleTest
    : public base::test::WithFeatureOverride,
      public AutofillProfileTableViewControllerTest {
 public:
  AutofillProfileTableViewControllerTitleTest()
      : base::test::WithFeatureOverride(kYourSavedInfoSettingsPageIos) {}
};

// Tests the title of the view controller when the feature is enabled/disabled.
TEST_P(AutofillProfileTableViewControllerTitleTest, Title) {
  CreateController();
  CheckController();

  CheckTitleWithId(IsParamFeatureEnabled() ? IDS_AUTOFILL_CONTACT_INFO_TITLE
                                           : IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    AutofillProfileTableViewControllerTitleTest);

// Tests that deleting an Autofill AI entity logs the Deleted metric.
TEST_F(AutofillProfileTableViewControllerTest, DeleteAIEntity) {
  autofill::EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
  autofill::EntityInstance instance =
      autofill::test::GetVehicleEntityInstance();
  entity_data_manager->AddOrUpdateEntityInstance(instance);

  // Wait for it to be added.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager->GetEntityInstance(instance.guid())
            .has_value();
      }));

  CreateController();
  CheckController();

  NSIndexPath* target_path = nil;
  for (NSInteger section = 0; section < NumberOfSections(); ++section) {
    for (NSInteger row = 0; row < NumberOfItemsInSection(section); ++row) {
      TableViewItem* item = GetTableViewItem(section, row);
      if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
        target_path = [NSIndexPath indexPathForRow:row inSection:section];
        break;
      }
    }
    if (target_path) {
      break;
    }
  }

  ASSERT_TRUE(target_path != nil);

  base::HistogramTester histogram_tester;

  [base::apple::ObjCCastStrict<AutofillProfileTableViewController>(controller())
      willDeleteItemsAtIndexPaths:@[ target_path ]];

  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityDeletedFromSettings",
                                      autofill::EntityTypeName::kVehicle, 1);
}

// Tests that a server wallet entity is grayed out (opacity 0.5) and
// non-interactable (userInteractionEnabled = NO) in editing mode, and returns
// to full opacity and interactable when editing is disabled.
TEST_F(AutofillProfileTableViewControllerTest,
       TestServerWalletEntityOpacityAndInteractionInEditing) {
  autofill::EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());

  // Add a server wallet vehicle entity.
  autofill::EntityInstance wallet_entity =
      autofill::test::GetVehicleEntityInstance(
          {.guid = "00000000-0000-4000-8000-200000000001",
           .record_type = autofill::EntityInstance::RecordType::kServerWallet});

  // Verify local instance is server wallet item.
  ASSERT_TRUE(wallet_entity.IsServerInstance());

  entity_data_manager->AddOrUpdateEntityInstance(wallet_entity);

  // Wait for the entity to be added to the manager.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, true, ^{
        return entity_data_manager->GetEntityInstance(wallet_entity.guid())
            .has_value();
      }));

  // Verify cache entry.
  auto stored_instance =
      entity_data_manager->GetEntityInstance(wallet_entity.guid());
  ASSERT_TRUE(stored_instance.has_value());
  ASSERT_TRUE(stored_instance->IsServerInstance());

  CreateController();
  CheckController();

  AutofillProfileTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillProfileTableViewController>(
          controller());

  // Find the index path of the server wallet entity item.
  NSIndexPath* target_path = nil;
  for (NSInteger section = 0; section < NumberOfSections(); ++section) {
    for (NSInteger row = 0; row < NumberOfItemsInSection(section); ++row) {
      TableViewItem* item = GetTableViewItem(section, row);
      if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
        AutofillAIEntityItem* aiItem =
            base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
        if (aiItem.guid == wallet_entity.guid()) {
          target_path = [NSIndexPath indexPathForRow:row inSection:section];
          EXPECT_TRUE(aiItem.isServerWalletItem);
          break;
        }
      }
    }
    if (target_path) {
      break;
    }
  }

  ASSERT_TRUE(target_path != nil);

  // Initially, not in editing mode: load cell via data source and verify it is
  // fully opaque and interactive.
  EXPECT_FALSE(view_controller.tableView.editing);
  UITableViewCell* cellNonEditing =
      [view_controller tableView:view_controller.tableView
           cellForRowAtIndexPath:target_path];
  ASSERT_TRUE(cellNonEditing != nil);
  EXPECT_EQ(1.0, cellNonEditing.contentView.alpha);
  EXPECT_TRUE(cellNonEditing.userInteractionEnabled);

  // Put table view in editing mode.
  [view_controller setEditing:YES animated:NO];

  // Load cell via data source in editing mode and verify it is grayed out
  // and non-interactable.
  EXPECT_TRUE(view_controller.tableView.editing);
  UITableViewCell* cellEditing =
      [view_controller tableView:view_controller.tableView
           cellForRowAtIndexPath:target_path];
  ASSERT_TRUE(cellEditing != nil);
  EXPECT_EQ(0.5, cellEditing.contentView.alpha);
  EXPECT_FALSE(cellEditing.userInteractionEnabled);

  // Take table view out of editing mode.
  [view_controller setEditing:NO animated:NO];

  // Load cell via data source again and verify it returns to normal state.
  EXPECT_FALSE(view_controller.tableView.editing);
  UITableViewCell* cellNonEditing2 =
      [view_controller tableView:view_controller.tableView
           cellForRowAtIndexPath:target_path];
  ASSERT_TRUE(cellNonEditing2 != nil);
  EXPECT_EQ(1.0, cellNonEditing2.contentView.alpha);
  EXPECT_TRUE(cellNonEditing2.userInteractionEnabled);
}

}  // namespace
