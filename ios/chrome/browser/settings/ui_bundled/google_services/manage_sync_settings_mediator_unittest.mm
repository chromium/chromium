// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_mediator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/sync_switch_item.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/features.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class ManageSyncSettingsMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_system_identity_);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState* context) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    authentication_service->SignIn(fake_system_identity_,
                                   signin_metrics::AccessPoint::kUnknown);
  }

  // Creates the mediator for a given sync state.
  void CreateManageSyncSettingsMediator() {
    ASSERT_FALSE(mediator_);
    ASSERT_FALSE(consumer_);
    consumer_ = [[ManageSyncSettingsTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
    [consumer_ loadModel];
    mediator_ = [[ManageSyncSettingsMediator alloc]
          initWithSyncService:sync_service_
              identityManager:IdentityManagerFactory::GetForProfile(
                                  profile_.get())
        authenticationService:AuthenticationServiceFactory::GetForProfile(
                                  profile_.get())
        accountManagerService:ChromeAccountManagerServiceFactory::GetForProfile(
                                  profile_.get())
                  prefService:profile_->GetPrefs()];
    mediator_.consumer = consumer_;
  }

  void CreateManageSyncSettingsMediator(
      BOOL isEEAAccount) {
    CreateManageSyncSettingsMediator();
    mediator_.isEEAAccount = isEEAAccount;
  }

 protected:
  // Needed for test profile created by TestProfileIOS().
  web::WebTaskEnvironment task_environment_;

  // Needed for the initialization of authentication service.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  base::test::ScopedFeatureList feature_list_;

  raw_ptr<syncer::TestSyncService> sync_service_;
  std::unique_ptr<TestProfileIOS> profile_;

  ManageSyncSettingsMediator* mediator_ = nullptr;
  ManageSyncSettingsTableViewController* consumer_ = nullptr;

  FakeSystemIdentity* fake_system_identity_ =
      [FakeSystemIdentity fakeIdentity1];
};

// Tests that account types for a signed in account are showing correctly.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckAccountSwitchItemsForSignedInAccount) {
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get account switches.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncDataTypeSectionIdentifier];

  for (TableViewItem* item in items) {
    // Check OpenTabsDataTypeItemType does not exist as it is merged and handled
    // by Hitstory type.
    EXPECT_FALSE(item.type == OpenTabsDataTypeItemType);
  }
}

// Tests that the sign out item exists in the ManageAndSignOutSectionIdentifier
// for a signed in account along with manage accounts items.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckSignOutSectionItemsForSignedInAccount) {
  feature_list_.InitAndDisableFeature(kIOSManageAccountStorage);
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:ManageAndSignOutSectionIdentifier];

  ASSERT_GE([items count], 2u);
  EXPECT_EQ(ManageGoogleAccountItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[0]).type);
  EXPECT_EQ(ManageAccountsItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[1]).type);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[0]).text);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[1]).text);

  // The "Sign out" item only exists in this section if
  // kSeparateProfilesForManagedAccounts is disabled.
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    ASSERT_EQ([items count], 3u);
    EXPECT_EQ(SignOutItemType,
              base::apple::ObjCCastStrict<TableViewItem>(items[2]).type);
    EXPECT_NSEQ(
        l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM),
        base::apple::ObjCCastStrict<TableViewTextItem>(items[2]).text);
  } else {
    ASSERT_EQ([items count], 2u);
  }
}

// Tests the signout section items when manage storage is enabled.
TEST_F(ManageSyncSettingsMediatorTest,
       CheckSignOutSectionItemsForSignedInNotSyncingAccountWithStorage) {
  feature_list_.InitAndEnableFeature(kIOSManageAccountStorage);
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  // Loads the Sync page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:ManageAndSignOutSectionIdentifier];

  ASSERT_GE([items count], 3u);
  EXPECT_EQ(ManageGoogleAccountItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[0]).type);
  EXPECT_EQ(ManageAccountStorageType,
            base::apple::ObjCCastStrict<TableViewItem>(items[1]).type);
  EXPECT_EQ(ManageAccountsItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[2]).type);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[0]).text);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_STORAGE_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[1]).text);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM),
              base::apple::ObjCCastStrict<TableViewTextItem>(items[2]).text);

  // The "Sign out" item only exists in this section if
  // kSeparateProfilesForManagedAccounts is disabled.
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    ASSERT_EQ([items count], 4u);
    EXPECT_EQ(SignOutItemType,
              base::apple::ObjCCastStrict<TableViewItem>(items[3]).type);
    EXPECT_NSEQ(
        l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM),
        base::apple::ObjCCastStrict<TableViewTextItem>(items[3]).text);
  } else {
    ASSERT_EQ([items count], 3u);
  }
}

// Tests that a persistent auth error is displayed as a text button at the top
// of the page for a signed in account.
TEST_F(ManageSyncSettingsMediatorTest, TestAuthErrorForSignedInAccount) {
  feature_list_.InitAndEnableFeature(switches::kEnableIdentityInAuthError);
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_->SetPersistentAuthError();

  // Loads the account settings page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  EXPECT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];

  EXPECT_EQ(2UL, error_items.count);
  EXPECT_NSEQ(
      base::apple::ObjCCastStrict<SettingsImageDetailTextItem>(error_items[0])
          .detailText,
      l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_MESSAGE));
  EXPECT_NSEQ(
      base::apple::ObjCCastStrict<TableViewTextItem>(error_items[1]).text,
      l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_BUTTON));
}

// Tests that Sync errors display as a text button at the top of the page for a
// signed in account.
TEST_F(ManageSyncSettingsMediatorTest, TestSyncErrorsForSignedInAccount) {
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_->SetPassphraseRequired();

  // Loads the account settings page.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  EXPECT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];

  EXPECT_EQ(2UL, error_items.count);
  EXPECT_NSEQ(
      base::apple::ObjCCastStrict<SettingsImageDetailTextItem>(error_items[0])
          .detailText,
      l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_MESSAGE));
  EXPECT_NSEQ(
      base::apple::ObjCCastStrict<TableViewTextItem>(error_items[1]).text,
      l10n_util::GetNSString(
          IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON));
}

// Tests the account state transition on sign out.
// This test to ensure the UI does not crash on sign out because of a missing
// section in that state. Reference bug crbug.com/1456446.
TEST_F(ManageSyncSettingsMediatorTest, TestAccountStateTransitionOnSignOut) {
  // Create mediator with a signed-in account.
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Verify the sign out section exists.
  ASSERT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         ManageAndSignOutSectionIdentifier]);
  // Verify the number of section shown in the kSignedIn state.
  const int expected_num_sections =
      AreSeparateProfilesForManagedAccountsEnabled() ? 4 : 3;
  ASSERT_EQ(expected_num_sections,
            [mediator_.consumer.tableViewModel numberOfSections]);

  sync_service_->SetSignedOut();

  // Sign out.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignOut(signin_metrics::ProfileSignout::kTest, nil);

  // Reload the Sync page.
  [mediator_ onSyncStateChanged];

  // Expected sections from the previous kSignedIn state should be showing and
  // no new sections are added in the kSignedOut state.
  EXPECT_EQ(expected_num_sections,
            [mediator_.consumer.tableViewModel numberOfSections]);
}

// Test that the GoogleActivityControlsItem is visible when the
// LinkedServicesSettings flags is disabled.
TEST_F(ManageSyncSettingsMediatorTest, TestGoogleActivityControlsItem) {
  // Disable the LinkedServicesSettings flag.
  feature_list_.InitAndDisableFeature(kLinkedServicesSettingIos);

  // Create mediator with a signed-in account.
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  EXPECT_EQ(GoogleActivityControlsItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[1]).type);
}

// Test that the PersonalizeGoogleServices is visible when the
// LinkedServicesSettings flags is disabled.
TEST_F(ManageSyncSettingsMediatorTest, TestPersonalizeGoogleServicesItem) {
  // Enable the LinkedServicesSettings flag.
  feature_list_.InitAndEnableFeature(kLinkedServicesSettingIos);

  // Create mediator with a signed-in account.
  CreateManageSyncSettingsMediator();
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  EXPECT_EQ(PersonalizeGoogleServicesItemType,
            base::apple::ObjCCastStrict<TableViewItem>(items[1]).type);
}

// Test that the PersonalizeGoogleServices item open the Personalized Google
// Services settings for EEA users.
TEST_F(ManageSyncSettingsMediatorTest, TestPersonalizeGoogleServicesItemEEA) {
  // Enable the LinkedServicesSettings flag.
  feature_list_.InitAndEnableFeature(kLinkedServicesSettingIos);

  // Create mediator with a signed-in account.
  CreateManageSyncSettingsMediator(true);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Create mock command handler
  id mockCommandHandler =
      OCMProtocolMock(@protocol(ManageSyncSettingsCommandHandler));
  mediator_.commandHandler = mockCommandHandler;
  OCMExpect([mockCommandHandler openPersonalizeGoogleServices]);

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  // Test item behavior for EEA users.
  [mediator_ didSelectItem:items[1] cellRect:CGRectZero];

  EXPECT_OCMOCK_VERIFY(mockCommandHandler);
}

// Test that the PersonalizeGoogleServices item open the Web and App activity
// settings for non EEA users.
TEST_F(ManageSyncSettingsMediatorTest,
       TestPersonalizeGoogleServicesItemNonEEA) {
  // Enable the LinkedServicesSettings flag.
  feature_list_.InitAndEnableFeature(kLinkedServicesSettingIos);

  // Create mediator with a signed-in account.
  CreateManageSyncSettingsMediator(false);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // Create mock command handler
  id mockCommandHandler =
      OCMProtocolMock(@protocol(ManageSyncSettingsCommandHandler));
  mediator_.commandHandler = mockCommandHandler;
  OCMExpect([mockCommandHandler openWebAppActivityDialog]);

  // Get section items.
  NSArray* items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  // Test item behavior for EEA users.
  [mediator_ didSelectItem:items[1] cellRect:CGRectZero];

  EXPECT_OCMOCK_VERIFY(mockCommandHandler);
}
