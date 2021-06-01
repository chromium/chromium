// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#import <UIKit/UIKit.h>

#include "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#import "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<syncer::MockSyncService>>();
}

PrefService* SetPrefService() {
  TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
  PrefRegistrySimple* registry = prefs->registry();
  registry->RegisterBooleanPref(autofill::prefs::kAutofillWalletImportEnabled,
                                true);
  return prefs;
}
}  // namespace

class ManageSyncSettingsMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    FakeChromeIdentity* identity =
        [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                       gaiaID:@"foo1ID"
                                         name:@"Fake Foo 1"];
    identity_service()->AddIdentity(identity);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();

    consumer_ = [[ManageSyncSettingsTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
    [consumer_ loadModel];

    pref_service_ = SetPrefService();

    // Sign a fake identity into Chrome.
    authentication_service_ =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    authentication_service_->SignIn(identity);

    sync_setup_service_mock_ = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()));
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(browser_state_.get()));

    mediator_ = [[ManageSyncSettingsMediator alloc]
        initWithSyncService:sync_service_mock_
            userPrefService:pref_service_];
    mediator_.syncSetupService = sync_setup_service_mock_;
    mediator_.authService = authentication_service_;
    mediator_.consumer = consumer_;
  }

  void SetupSyncServiceInitializedExpectations() {
    ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, IsSyncingAllDataTypes())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  }

  void SetupSyncDisabledExpectations() {
    ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
        .WillByDefault(Return(false));
    ON_CALL(*sync_setup_service_mock_, IsSyncingAllDataTypes())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
        .WillByDefault(Return(false));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::DISABLED));
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;

  syncer::MockSyncService* sync_service_mock_;
  SyncSetupServiceMock* sync_setup_service_mock_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  ManageSyncSettingsMediator* mediator_ = nullptr;
  ManageSyncSettingsTableViewController* consumer_ = nullptr;
  PrefService* pref_service_ = nullptr;
  AuthenticationService* authentication_service_ = nullptr;
};

// Tests for Advanced Settings items.

// Tests that encryption is not accessible when Sync settings have not been
// confirmed.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceSetupNotCommitted) {
  SetupSyncDisabledExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncSettingsNotConfirmed));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item only available in
  // |signin::kMobileIdentityConsistency|.
  ASSERT_FALSE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SignOutSectionIdentifier]);

  // Encryption item is disabled.
  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_FALSE(encryption_item.enabled);
}

// Tests that encryption is not accessible when Sync is disabled by the
// administrator.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledByAdministrator) {
  SetupSyncDisabledExpectations();
  ON_CALL(*sync_service_mock_, GetDisableReasons())
      .WillByDefault(
          Return(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item only available in
  // |signin::kMobileIdentityConsistency|.
  ASSERT_FALSE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SignOutSectionIdentifier]);

  // Encryption item is disabled.
  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_FALSE(encryption_item.enabled);
}

// Tests that encryption is accessible when there is a Sync error due to a
// missing passphrase, but Sync has otherwise been enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledNeedsPassphrase) {
  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncServiceNeedsPassphrase));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_TRUE(encryption_item.enabled);
}

// Tests that encryption is accessible when Sync is enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceEnabledWithEncryption) {
  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_TRUE(encryption_item.enabled);

  ASSERT_FALSE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncErrorsSectionIdentifier]);
}

// Tests that "Turn off Sync" is hidden when Sync is disabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledWithTurnOffSync) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);

  SetupSyncDisabledExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item is shown.
  NSArray* sign_out_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SignOutSectionIdentifier];
  ASSERT_EQ(0UL, sign_out_items.count);
}

// Tests that "Turn off Sync" is accessible when Sync is enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceEnabledWithTurnOffSync) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);

  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item is shown.
  NSArray* sign_out_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SignOutSectionIdentifier];
  ASSERT_EQ(1UL, sign_out_items.count);
}

// Tests that a Sync error that occurs after the user has loaded the Settings
// page once will update the full page.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceSuccessThenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);

  SetupSyncServiceInitializedExpectations();
  EXPECT_CALL(*sync_service_mock_, GetDisableReasons())
      .WillOnce(Return(syncer::MockSyncService::DisableReasonSet()))
      .WillOnce(Return(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY));

  // Loads the Sync page once in success state.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];
  // Loads the Sync page again in disabled state.
  [mediator_ onSyncStateChanged];

  ASSERT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];
  ASSERT_EQ(1UL, error_items.count);
}

// Tests that Sync errors display a single error message when loaded one after
// the other.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceMultipleErrors) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);

  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncServiceNeedsPassphrase));
  EXPECT_CALL(*sync_service_mock_, GetDisableReasons())
      .WillOnce(Return(syncer::MockSyncService::DisableReasonSet()))
      .WillOnce(Return(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY));

  // Loads the Sync page once in the disabled by enterprise policy error state.
  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];
  // Loads the Sync page again in the needs encryption passphrase error state.
  [mediator_ onSyncStateChanged];

  ASSERT_TRUE([mediator_.consumer.tableViewModel
      hasSectionForSectionIdentifier:SyncSettingsSectionIdentifier::
                                         SyncErrorsSectionIdentifier]);
  NSArray* error_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SyncErrorsSectionIdentifier];
  ASSERT_EQ(1UL, error_items.count);
  SettingsImageDetailTextItem* error_item =
      static_cast<SettingsImageDetailTextItem*>(error_items[0]);
  ASSERT_NSEQ(
      error_item.detailText,
      l10n_util::GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENTER_PASSPHRASE_TO_START_SYNC));
}

// Tests that "Turn off Sync" item transition from disabled to enabled goes from
// hiding to showing the item.
TEST_F(ManageSyncSettingsMediatorTest,
       SyncServiceSetupTransitionForTurnOffSync) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);

  // Set Sync disabled expectations.
  SetupSyncDisabledExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncSettingsNotConfirmed));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  // "Turn off Sync" item is hidden.
  NSArray* hidden_sign_out_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SignOutSectionIdentifier];
  ASSERT_EQ(0UL, hidden_sign_out_items.count);

  // Set Sync enabled expectations.
  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));

  // Loads the Sync page again in enabled state.
  [mediator_ onSyncStateChanged];

  // "Turn off Sync" item is shown.
  NSArray* shown_sign_out_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       SignOutSectionIdentifier];
  ASSERT_EQ(1UL, shown_sign_out_items.count);
}
