// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#import "components/signin/ios/browser/features.h"
#import "components/sync/driver/mock_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::NiceMock;
using ::testing::Return;
using web::WebTaskEnvironment;

namespace {
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<syncer::MockSyncService>>();
}
}  // namespace

class SettingsTableViewControllerMICETest
    : public ChromeTableViewControllerTest {
 public:
  void SetUp() override {
    scoped_feature_.InitAndEnableFeature(signin::kMobileIdentityConsistency);
    ChromeTableViewControllerTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = builder.Build();

    WebStateList* web_state_list = nullptr;
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list);

    sync_setup_service_mock_ = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        ProfileSyncServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));

    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));

    password_store_mock_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromePasswordStoreFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    chrome_browser_state_.get(),
                    base::BindRepeating(&password_manager::BuildPasswordStore<
                                        web::BrowserState,
                                        password_manager::TestPasswordStore>))
                .get()));

    fake_identity_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                                    gaiaID:@"foo1ID"
                                                      name:@"Fake Foo 1"];
  }

  void TearDown() override {
    [static_cast<SettingsTableViewController*>(controller())
        settingsWillBeDismissed];
    ChromeTableViewControllerTest::TearDown();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[SettingsTableViewController alloc]
        initWithBrowser:browser_.get()
             dispatcher:static_cast<id<ApplicationCommands, BrowserCommands,
                                       BrowsingDataCommands>>(
                            browser_->GetCommandDispatcher())];
  }

  void SetupSyncServiceEnabledExpectations() {
    ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, IsSyncingAllDataTypes())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(Return(syncer::UserSelectableTypeSet::All()));
    ON_CALL(*sync_service_mock_, IsAuthenticatedAccountPrimary())
        .WillByDefault(Return(true));
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList scoped_feature_;

  FakeChromeIdentity* fake_identity_ = nullptr;
  AuthenticationServiceFake* auth_service_ = nullptr;
  syncer::MockSyncService* sync_service_mock_ = nullptr;
  SyncSetupServiceMock* sync_setup_service_mock_ = nullptr;
  scoped_refptr<password_manager::TestPasswordStore> password_store_mock_ =
      nullptr;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestBrowser> browser_;

  SettingsTableViewController* controller_ = nullptr;
};

// Verifies that the Sync icon displays the on state when the user has turned
// on sync during sign-in.
TEST_F(SettingsTableViewControllerMICETest, SyncOn) {
  SetupSyncServiceEnabledExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));
  auth_service_->SignIn(fake_identity_);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(3U, account_items.count);

  TableViewDetailIconItem* sync_item =
      static_cast<TableViewDetailIconItem*>(account_items[1]);
  ASSERT_NSEQ(sync_item.text,
              l10n_util::GetNSString(IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE));
  ASSERT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON), sync_item.detailText);
  ASSERT_EQ(UILayoutConstraintAxisHorizontal,
            sync_item.textLayoutConstraintAxis);
}

// Verifies that the Sync icon displays the sync password error when the user
// has turned on sync during sign-in, but not entered an existing encryption
// password.
TEST_F(SettingsTableViewControllerMICETest, SyncPasswordError) {
  SetupSyncServiceEnabledExpectations();
  // Set missing password error in Sync service.
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncServiceNeedsPassphrase));
  auth_service_->SignIn(fake_identity_);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(3U, account_items.count);

  TableViewDetailIconItem* sync_item =
      static_cast<TableViewDetailIconItem*>(account_items[1]);
  ASSERT_NSEQ(sync_item.text,
              l10n_util::GetNSString(IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE));
  ASSERT_NSEQ(sync_item.detailText,
              l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION));
  ASSERT_EQ(UILayoutConstraintAxisVertical, sync_item.textLayoutConstraintAxis);

  // Check that there is no sign-in promo when there is a sync error.
  NSArray* identity_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierSignIn];
  ASSERT_EQ(0U, identity_items.count);
}

// Verifies that the Sync icon displays the off state when the user has
// completed the sign-in and sync flow then explcitly turned off the Sync
// setting.
TEST_F(SettingsTableViewControllerMICETest, TurnsSyncOffAfterFirstSetup) {
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));
  ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
      .WillByDefault(Return(false));
  auth_service_->SignIn(fake_identity_);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(3U, account_items.count);

  TableViewDetailIconItem* sync_item =
      static_cast<TableViewDetailIconItem*>(account_items[1]);
  ASSERT_NSEQ(l10n_util::GetNSString(IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE),
              sync_item.text);
  ASSERT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              sync_item.detailText);
}

// Verifies that the Sync icon displays the off state when the user has
// completed the sign-in and sync flow then explcitly turned off all data types
// in the Sync settings.
TEST_F(SettingsTableViewControllerMICETest,
       DisablesAllSyncSettingsAfterFirstSetup) {
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(Return(syncer::UserSelectableTypeSet()));
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));
  ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
      .WillByDefault(Return(true));
  auth_service_->SignIn(fake_identity_);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(3U, account_items.count);

  TableViewDetailIconItem* sync_item =
      static_cast<TableViewDetailIconItem*>(account_items[1]);
  ASSERT_NSEQ(l10n_util::GetNSString(IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE),
              sync_item.text);
  ASSERT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              sync_item.detailText);
}
