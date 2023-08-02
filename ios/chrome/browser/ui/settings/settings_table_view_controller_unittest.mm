// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/base/features.h"
#import "components/sync/test/mock_sync_service.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ::testing::NiceMock;
using ::testing::Return;
using web::WebTaskEnvironment;

class SettingsTableViewControllerTest : public ChromeTableViewControllerTest {
 public:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    chrome_browser_state_ = builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    browser_state_ = TestChromeBrowserState::Builder().Build();

    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get()));

    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));

    password_store_mock_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromePasswordStoreFactory::GetForBrowserState(
                chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

    // Make sure there is no pre-existing policy present.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  }

  void TearDown() override {
    // Cleanup any policies left from the test.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];

    [static_cast<SettingsTableViewController*>(controller())
        settingsWillBeDismissed];
    ChromeTableViewControllerTest::TearDown();
  }

  ChromeTableViewController* InstantiateController() override {
    id mockSnackbarCommandHandler =
        OCMProtocolMock(@protocol(SnackbarCommands));

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockSnackbarCommandHandler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    SettingsTableViewController* controller =
        [[SettingsTableViewController alloc]
            initWithBrowser:browser_.get()
                 dispatcher:static_cast<id<ApplicationCommands, BrowserCommands,
                                           BrowsingDataCommands>>(
                                browser_->GetCommandDispatcher())];
    controller.applicationCommandsHandler = HandlerForProtocol(
        browser_->GetCommandDispatcher(), ApplicationCommands);
    controller.snackbarCommandsHandler =
        HandlerForProtocol(browser_->GetCommandDispatcher(), SnackbarCommands);
    return controller;
  }

  void SetupSyncServiceEnabledExpectations() {
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*sync_service_mock_->GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(Return(syncer::UserSelectableTypeSet::All()));
    ON_CALL(*sync_service_mock_, HasSyncConsent()).WillByDefault(Return(true));
  }

  void AddSigninDisabledEnterprisePolicy() {
    NSDictionary* policy = @{
      base::SysUTF8ToNSString(policy::key::kBrowserSignin) : [NSNumber
          numberWithInt:static_cast<int>(BrowserSigninMode::kDisabled)]
    };

    [[NSUserDefaults standardUserDefaults]
        setObject:policy
           forKey:kPolicyLoaderIOSConfigurationKey];
  }

  PrefService* GetLocalState() { return scoped_testing_local_state_.Get(); }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  FakeSystemIdentity* fake_identity_ = nullptr;
  AuthenticationService* auth_service_ = nullptr;
  syncer::MockSyncService* sync_service_mock_ = nullptr;
  scoped_refptr<password_manager::TestPasswordStore> password_store_mock_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  SceneState* scene_state_;

  SettingsTableViewController* controller_ = nullptr;
};

// Verifies that the Sync icon displays the on state when the user has turned
// on sync during sign-in.
TEST_F(SettingsTableViewControllerTest, SyncOn) {
  SetupSyncServiceEnabledExpectations();
  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

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
TEST_F(SettingsTableViewControllerTest, SyncPasswordError) {
  SetupSyncServiceEnabledExpectations();
  // Set missing password error in Sync service.
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));
  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

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

  // Verify that the account item does not hold the error when done through the
  // sync item.
  TableViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(account_items[0]);
  EXPECT_FALSE(identityAccountItem.shouldDisplayError);

  // Check that there is no sign-in promo when there is a sync error.
  ASSERT_FALSE([controller().tableViewModel
      hasSectionForSectionIdentifier:SettingsSectionIdentifier::
                                         SettingsSectionIdentifierSignIn]);
}

// Verifies that the Sync icon displays the off state (and no detail text) when
// the user has completed the sign-in and sync flow then explicitly turned off
// all data types in the Sync settings.
// This case can only happen for pre-MICE users who migrated with MICE.
TEST_F(SettingsTableViewControllerTest,
       DisablesAllSyncSettingsAfterFirstSetup) {
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(Return(syncer::UserSelectableTypeSet()));
  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(true));
  ON_CALL(*sync_service_mock_, HasSyncConsent()).WillByDefault(Return(true));
  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

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
  ASSERT_EQ(nil, sync_item.detailText);
}

// Verifies that the sign-in setting row is removed if sign-in is disabled
// through the "Allow Chrome Sign-in" option.
TEST_F(SettingsTableViewControllerTest, SigninDisabled) {
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  CreateController();
  CheckController();

  ASSERT_FALSE([controller().tableViewModel
      hasSectionForSectionIdentifier:SettingsSectionIdentifier::
                                         SettingsSectionIdentifierSignIn]);
}

// Verifies that for a signed-in non-syncing user with
// kReplaceSyncPromosWithSignInPromos disabled, the account section shows 3
// items: the one with the name/email, the "Sync off" one, and the "Google
// Services" one.
TEST_F(SettingsTableViewControllerTest,
       AccountSectionIfSignedInNonSyncing_SyncToSigninDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(false));
  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(3U, account_items.count);

  auto* account_item = static_cast<TableViewAccountItem*>(account_items[0]);
  auto* sync_item = static_cast<TableViewDetailIconItem*>(account_items[1]);
  auto* google_services_item =
      static_cast<TableViewDetailIconItem*>(account_items[2]);
  EXPECT_NSEQ(fake_identity_.userFullName, account_item.text);
  EXPECT_NSEQ(fake_identity_.userEmail, account_item.detailText);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE),
              sync_item.text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              sync_item.detailText);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE),
              google_services_item.text);
  EXPECT_NSEQ(nil, google_services_item.detailText);
}

// Verifies that for a signed-in non-syncing user with
// kReplaceSyncPromosWithSignInPromos enabled, the account section shows 2
// items: the one with the name/email, and the "Google Services" one.
TEST_F(SettingsTableViewControllerTest,
       AccountSectionIfSignedInNonSyncing_SyncToSigninEnabled) {
  base::test::ScopedFeatureList features(
      syncer::kReplaceSyncPromosWithSignInPromos);

  ON_CALL(*sync_service_mock_->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(false));
  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(2U, account_items.count);

  auto* account_item = static_cast<TableViewAccountItem*>(account_items[0]);
  auto* google_services_item =
      static_cast<TableViewDetailIconItem*>(account_items[1]);
  EXPECT_NSEQ(fake_identity_.userFullName, account_item.text);
  EXPECT_NSEQ(fake_identity_.userEmail, account_item.detailText);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE),
              google_services_item.text);
  EXPECT_NSEQ(nil, google_services_item.detailText);
}

// Verifies that the sign-in setting item is replaced by the managed sign-in
// item if sign-in is disabled by policy.
TEST_F(SettingsTableViewControllerTest, SigninDisabledByPolicy) {
  AddSigninDisabledEnterprisePolicy();
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));
  CreateController();
  CheckController();

  NSArray* signin_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierSignIn];
  ASSERT_EQ(1U, signin_items.count);

  TableViewInfoButtonItem* signin_item =
      static_cast<TableViewInfoButtonItem*>(signin_items[0]);
  ASSERT_NSEQ(signin_item.text,
              l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE));
  ASSERT_NSEQ(signin_item.statusText,
              l10n_util::GetNSString(IDS_IOS_SETTING_OFF));
}

// Verifies that when eligible the account item model holds the Account Storage
// error.
TEST_F(SettingsTableViewControllerTest, HoldAccountStorageErrorWhenEligible) {
  // Set account error.
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);

  // Verify that the account item is in an error state.
  TableViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(account_items[0]);
  EXPECT_TRUE(identityAccountItem.shouldDisplayError);
}

// Verifies that the error is removed from the model when the Account Storage
// error is resolved. Triggers the model update by firing a Sync State change.
TEST_F(SettingsTableViewControllerTest, ClearAccountStorageErrorWhenResolved) {
  // Set account error to resolve.
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);

  // Verify that the account item is in an error state.
  TableViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem.shouldDisplayError);

  // Resolve the account error.
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::kNone));

  // Verify that the account item is not in an error state when the error was
  // resolved and the data model reloaded.
  [controller() loadModel];
  account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);
  identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem != nil);
  EXPECT_FALSE(identityAccountItem.shouldDisplayError);
}

// Verifies that when ineligible the account item model doesn't hold the Account
// Storage error.
TEST_F(SettingsTableViewControllerTest, DontHoldAccountErrorWhenIneligible) {
  // Enable Sync to make the account item ineligible to indicate errors.
  SetupSyncServiceEnabledExpectations();

  // Set account error that would be in the model when eligible.
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_EQ(3U, account_items.count);

  // Verify that the account item is not in an error state.
  TableViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem != nil);
  EXPECT_FALSE(identityAccountItem.shouldDisplayError);
}

// Verifies that when eligible the account item model doesn't have the Account
// Storage error when there is no error.
TEST_F(SettingsTableViewControllerTest, DontHoldAccountErrorWhenNoError) {
  // Set no account error state.
  ON_CALL(*sync_service_mock_, GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::kNone));

  auth_service_->SignIn(fake_identity_,
                        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);

  // Verify that the account item is not in an error state.
  TableViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem != nil);
  EXPECT_FALSE(identityAccountItem.shouldDisplayError);
}
