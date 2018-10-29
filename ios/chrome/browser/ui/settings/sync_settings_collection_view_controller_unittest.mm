// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync_settings_collection_view_controller.h"

#include <memory>

#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/text_and_error_item.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncSettingsCollectionViewController (ExposedForTesting)
- (int)titleIdForSyncableDataType:(SyncSetupService::SyncableDatatype)datatype;
- (void)onSyncStateChanged;
@end

namespace {

using testing::DefaultValue;
using testing::NiceMock;
using testing::Return;

class SyncSetupServiceMockThatFails : public SyncSetupServiceMock {
 public:
  SyncSetupServiceMockThatFails(browser_sync::ProfileSyncService* sync_service,
                                PrefService* prefs)
      : SyncSetupServiceMock(sync_service, prefs) {}
  bool IsSyncEnabled() const override { return sync_enabled_; }
  void SetSyncEnabled(bool sync_enabled) override {}
  bool IsSyncingAllDataTypes() const override { return sync_all_; }
  void SetSyncingAllDataTypes(bool sync_all) override {}

  static void SetSyncEnabledTestValue(bool sync_enabled) {
    sync_enabled_ = sync_enabled;
  }
  static void SetSyncEverythingTestValue(bool sync_all) {
    sync_all_ = sync_all;
  }

 protected:
  static bool sync_enabled_;
  static bool sync_all_;
};

bool SyncSetupServiceMockThatFails::sync_enabled_ = true;
bool SyncSetupServiceMockThatFails::sync_all_ = true;

class SyncSetupServiceMockThatSucceeds : public SyncSetupServiceMockThatFails {
 public:
  SyncSetupServiceMockThatSucceeds(
      browser_sync::ProfileSyncService* sync_service,
      PrefService* prefs)
      : SyncSetupServiceMockThatFails(sync_service, prefs) {}
  void SetSyncEnabled(bool sync_enabled) override {
    sync_enabled_ = sync_enabled;
  }
  void SetSyncingAllDataTypes(bool sync_all) override { sync_all_ = sync_all; }
};

class SyncSettingsCollectionViewControllerTest
    : public CollectionViewControllerTest {
 public:
  SyncSettingsCollectionViewControllerTest()
      : default_auth_error_(GoogleServiceAuthError::NONE) {}

  static std::unique_ptr<KeyedService> CreateSyncSetupService(
      web::BrowserState* context) {
    ios::ChromeBrowserState* chrome_browser_state =
        ios::ChromeBrowserState::FromBrowserState(context);
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state);
    return std::make_unique<NiceMock<SyncSetupServiceMock>>(
        sync_service, chrome_browser_state->GetPrefs());
  }

  static std::unique_ptr<KeyedService> CreateSucceedingSyncSetupService(
      web::BrowserState* context) {
    ios::ChromeBrowserState* chrome_browser_state =
        ios::ChromeBrowserState::FromBrowserState(context);
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state);
    return std::make_unique<NiceMock<SyncSetupServiceMockThatSucceeds>>(
        sync_service, chrome_browser_state->GetPrefs());
  }

  static std::unique_ptr<KeyedService> CreateFailingSyncSetupService(
      web::BrowserState* context) {
    ios::ChromeBrowserState* chrome_browser_state =
        ios::ChromeBrowserState::FromBrowserState(context);
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state);
    return std::make_unique<NiceMock<SyncSetupServiceMockThatFails>>(
        sync_service, chrome_browser_state->GetPrefs());
  }

  static std::unique_ptr<KeyedService> CreateProfileSyncService(
      web::BrowserState* context) {
    browser_sync::ProfileSyncService::InitParams init_params =
        CreateProfileSyncServiceParamsForTest(
            nullptr, ios::ChromeBrowserState::FromBrowserState(context));
    return std::make_unique<NiceMock<browser_sync::ProfileSyncServiceMock>>(
        &init_params);
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

  void TurnSyncOn() {
    ON_CALL(*mock_sync_setup_service_, IsSyncEnabled())
        .WillByDefault(Return(true));
  }

  void TurnSyncEverythingOn() {
    ON_CALL(*mock_sync_setup_service_, IsSyncingAllDataTypes())
        .WillByDefault(Return(true));
  }

  void SetSyncServiceState(SyncSetupService::SyncServiceState state) {
    ON_CALL(*mock_sync_setup_service_, GetSyncServiceState())
        .WillByDefault(Return(state));
  }

  void TurnSyncPassphraseErrorOn() {
    SetSyncServiceState(SyncSetupService::kSyncServiceNeedsPassphrase);
  }

  void TurnSyncErrorOff() {
    SetSyncServiceState(SyncSetupService::kNoSyncServiceError);
  }

 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&CreateSyncSetupService));
    test_cbs_builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateProfileSyncService));
    test_cbs_builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = test_cbs_builder.Build();
    CollectionViewControllerTest::SetUp();

    DefaultValue<const GoogleServiceAuthError&>::Set(default_auth_error_);
    DefaultValue<syncer::SyncCycleSnapshot>::Set(default_sync_cycle_snapshot_);

    mock_sync_setup_service_ = static_cast<NiceMock<SyncSetupServiceMock>*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    // The other mocked functions of SyncSetupServiceMock return bools, so they
    // will by default return false.   |syncServiceState|, however, returns an
    // enum, and thus always needs its default value set.
    TurnSyncErrorOff();

    mock_profile_sync_service_ =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    ON_CALL(*mock_profile_sync_service_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*mock_profile_sync_service_, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));
    mock_profile_sync_service_->Initialize();
    EXPECT_CALL(*mock_profile_sync_service_, GetPreferredDataTypes())
        .WillRepeatedly(Return(syncer::UserSelectableTypes()));
  }

  CollectionViewController* InstantiateController() override {
    return [[SyncSettingsCollectionViewController alloc]
          initWithBrowserState:chrome_browser_state_.get()
        allowSwitchSyncAccount:YES];
  }

  SyncSettingsCollectionViewController* CreateSyncController() {
    CreateController();
    CheckTitleWithId(IDS_IOS_SYNC_SETTING_TITLE);
    return static_cast<SyncSettingsCollectionViewController*>(controller());
  }

  void CheckErrorIcon(BOOL expectedShouldDisplayError,
                      NSInteger section,
                      NSInteger item) {
    CollectionViewModel* model = [controller() collectionViewModel];
    NSIndexPath* path = [NSIndexPath indexPathForItem:item inSection:section];

    TextAndErrorItem* errorObject = base::mac::ObjCCastStrict<TextAndErrorItem>(
        [model itemAtIndexPath:path]);
    EXPECT_TRUE(errorObject);
    EXPECT_EQ(expectedShouldDisplayError, errorObject.shouldDisplayError);
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Weak, owned by |profile_|.
  NiceMock<SyncSetupServiceMock>* mock_sync_setup_service_;
  // Weak, owned by |profile_|.
  browser_sync::ProfileSyncServiceMock* mock_profile_sync_service_;
  // Default return values for ProfileSyncServiceMock.
  GoogleServiceAuthError default_auth_error_;
  syncer::SyncCycleSnapshot default_sync_cycle_snapshot_;
};

TEST_F(SyncSettingsCollectionViewControllerTest, TestModel) {
  TurnSyncEverythingOn();
  TurnSyncOn();
  SyncSettingsCollectionViewController* sync_controller =
      CreateSyncController();

  EXPECT_EQ(3, NumberOfSections());

  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // There is one item per data type, except for unified consent that is
  // unsupported by the old UI. In addition, there are two extra items, one
  // for "Sync Everything" and another for Autofill wallet import.
  constexpr int expected_number_of_items =
      SyncSetupService::kNumberOfSyncableDatatypes - 1 + 2;
  EXPECT_EQ(expected_number_of_items, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));

  SyncSwitchItem* syncItem = GetCollectionViewItem(0, 0);
  EXPECT_EQ(syncItem.isOn, YES);
  EXPECT_NSEQ(syncItem.text,
              l10n_util::GetNSString(IDS_IOS_SYNC_SETTING_TITLE));
  EXPECT_NSEQ(
      syncItem.detailText,
      l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE));

  SyncSwitchItem* syncEverythingItem = GetCollectionViewItem(1, 0);
  EXPECT_EQ(syncEverythingItem.isOn, YES);
  EXPECT_NSEQ(syncEverythingItem.text,
              l10n_util::GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE));

  int item = 1;
  for (int i = 0; i < SyncSetupService::kNumberOfSyncableDatatypes; i++) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(i);
    if (dataType == SyncSetupService::kSyncUserEvent) {
      // Old UI without unified consent doesn't support user event data type.
      continue;
    }
    SyncSwitchItem* syncDataTypeItem = GetCollectionViewItem(1, item++);
    EXPECT_NSEQ(syncDataTypeItem.text,
                l10n_util::GetNSString(
                    [sync_controller titleIdForSyncableDataType:dataType]));
  }

  SyncSwitchItem* autofillWalletImportItem = GetCollectionViewItem(1, item);
  NSString* title = l10n_util::GetNSString(
      IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
  EXPECT_NSEQ(autofillWalletImportItem.text, title);

  TextAndErrorItem* encryptionItem = GetCollectionViewItem(2, 0);
  EXPECT_NSEQ(encryptionItem.text,
              l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE));
  CheckErrorIcon(NO, 2, 0);
}

TEST_F(SyncSettingsCollectionViewControllerTest, TestEnabledCellsSyncOff) {
  CreateSyncController();

  for (int item = 0; item < NumberOfItemsInSection(1); ++item) {
    SyncSwitchItem* object = GetCollectionViewItem(1, item);
    EXPECT_FALSE(object.enabled);
  }

  TextAndErrorItem* encryptionItem = GetCollectionViewItem(2, 0);
  EXPECT_FALSE(encryptionItem.enabled);
}

TEST_F(SyncSettingsCollectionViewControllerTest,
       TestEnabledCellsSyncEverythingOn) {
  TurnSyncOn();
  TurnSyncEverythingOn();
  CreateSyncController();

  SyncSwitchItem* syncEverythingItem = GetCollectionViewItem(1, 0);
  EXPECT_TRUE(syncEverythingItem.enabled);
  for (int item = 1; item <= SyncSetupService::kNumberOfSyncableDatatypes;
       ++item) {
    SyncSwitchItem* object = GetCollectionViewItem(1, item);
    EXPECT_FALSE(object.enabled);
  }

  SyncSwitchItem* autofillWalletImportItem =
      GetCollectionViewItem(1, NumberOfItemsInSection(1) - 1);
  EXPECT_FALSE(autofillWalletImportItem.enabled);

  TextAndErrorItem* encryptionItem = GetCollectionViewItem(2, 0);
  EXPECT_TRUE(encryptionItem.enabled);
}

TEST_F(SyncSettingsCollectionViewControllerTest, TestAutofillWalletImportOff) {
  autofill::prefs::SetPaymentsIntegrationEnabled(
      chrome_browser_state_->GetPrefs(), false);

  TurnSyncOn();
  TurnSyncEverythingOn();
  CreateSyncController();

  SyncSwitchItem* autofillWalletImportItem =
      GetCollectionViewItem(1, NumberOfItemsInSection(1) - 1);
  EXPECT_FALSE(autofillWalletImportItem.isOn);
}

TEST_F(SyncSettingsCollectionViewControllerTest, TestAutofillWalletImportOn) {
  autofill::prefs::SetPaymentsIntegrationEnabled(
      chrome_browser_state_->GetPrefs(), true);

  TurnSyncOn();
  TurnSyncEverythingOn();
  CreateSyncController();

  SyncSwitchItem* autofillWalletImportItem =
      GetCollectionViewItem(1, NumberOfItemsInSection(1) - 1);
  EXPECT_TRUE(autofillWalletImportItem.isOn);
}

TEST_F(SyncSettingsCollectionViewControllerTest, TestShouldDisplayError) {
  SyncSettingsCollectionViewController* sync_controller =
      CreateSyncController();
  EXPECT_FALSE([sync_controller shouldDisplaySyncError]);
  EXPECT_FALSE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_FALSE([sync_controller shouldDisplayEncryptionError]);

  SetSyncServiceState(SyncSetupService::kSyncServiceSignInNeedsUpdate);
  EXPECT_TRUE([sync_controller shouldDisplaySyncError]);
  EXPECT_TRUE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_FALSE([sync_controller shouldDisplayEncryptionError]);

  SetSyncServiceState(SyncSetupService::kSyncServiceCouldNotConnect);
  EXPECT_TRUE([sync_controller shouldDisplaySyncError]);
  EXPECT_TRUE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_FALSE([sync_controller shouldDisplayEncryptionError]);

  SetSyncServiceState(SyncSetupService::kSyncServiceServiceUnavailable);
  EXPECT_TRUE([sync_controller shouldDisplaySyncError]);
  EXPECT_TRUE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_FALSE([sync_controller shouldDisplayEncryptionError]);

  SetSyncServiceState(SyncSetupService::kSyncServiceNeedsPassphrase);
  EXPECT_TRUE([sync_controller shouldDisplaySyncError]);
  EXPECT_FALSE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_TRUE([sync_controller shouldDisplayEncryptionError]);

  SetSyncServiceState(SyncSetupService::kSyncServiceUnrecoverableError);
  EXPECT_TRUE([sync_controller shouldDisplaySyncError]);
  EXPECT_TRUE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_FALSE([sync_controller shouldDisplayEncryptionError]);

  SetSyncServiceState(SyncSetupService::kNoSyncServiceError);
  EXPECT_FALSE([sync_controller shouldDisplaySyncError]);
  EXPECT_FALSE([sync_controller shouldDisableSettingsOnSyncError]);
  EXPECT_FALSE([sync_controller shouldDisplayEncryptionError]);
}

TEST_F(SyncSettingsCollectionViewControllerTest,
       TestSyncStateChangedWithEncryptionError) {
  TurnSyncOn();
  TurnSyncEverythingOn();

  SyncSettingsCollectionViewController* sync_controller =
      CreateSyncController();

  TextAndErrorItem* encryptionItem = GetCollectionViewItem(2, 0);
  EXPECT_NSEQ(encryptionItem.text,
              l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE));
  CheckErrorIcon(NO, 2, 0);

  TurnSyncPassphraseErrorOn();
  [sync_controller onSyncStateChanged];
  encryptionItem = GetCollectionViewItem(3, 0);
  EXPECT_NSEQ(encryptionItem.text,
              l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE));
  CheckErrorIcon(YES, 3, 0);

  TurnSyncErrorOff();
  [sync_controller onSyncStateChanged];
  encryptionItem = GetCollectionViewItem(2, 0);
  EXPECT_NSEQ(encryptionItem.text,
              l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE));
  CheckErrorIcon(NO, 2, 0);
}

}  // namespace
