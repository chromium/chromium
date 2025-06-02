// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/plus_addresses/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync/test/test_sync_user_settings.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_client.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ::testing::NiceMock;
using ::testing::Return;
using web::WebTaskEnvironment;

namespace {

using variations::SyntheticTrialRegistry;
using variations::UIStringOverrider;
using variations::VariationsService;
using variations::VariationsServiceClient;

// TODO(crbug.com/40742801): Remove when fake VariationsServiceClient created.
// TODO(crbug.com/377275759): Check if TestVariationsServiceClient and
// ScopedVariationsService can be consolidated with implementations elsewhere.
class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
};

// Creates a VariationsService and sets it as the TestingApplicationContext's
// VariationService for the life of the instance.
class ScopedVariationsService {
 public:
  ScopedVariationsService() {
    EXPECT_EQ(nullptr,
              TestingApplicationContext::GetGlobal()->GetVariationsService());
    synthetic_trial_registry_ = std::make_unique<SyntheticTrialRegistry>();
    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(false, false);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        TestingApplicationContext::GetGlobal()->GetLocalState(),
        enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    variations_service_ = VariationsService::Create(
        std::make_unique<TestVariationsServiceClient>(),
        TestingApplicationContext::GetGlobal()->GetLocalState(),
        metrics_state_manager_.get(),
        /*disable_network_switch=*/"dummy-disable-background-switch",
        UIStringOverrider(),
        network::TestNetworkConnectionTracker::CreateGetter(),
        synthetic_trial_registry_.get());
    TestingApplicationContext::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  ~ScopedVariationsService() {
    EXPECT_EQ(variations_service_.get(),
              TestingApplicationContext::GetGlobal()->GetVariationsService());
    TestingApplicationContext::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
  }

  VariationsService* Get() { return variations_service_.get(); }

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<VariationsService> variations_service_;
  std::unique_ptr<SyntheticTrialRegistry> synthetic_trial_registry_;
};

}  // namespace

class SettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();

    scoped_variations_service_.Get()->OverrideStoredPermanentCountry("us");

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(PhotosServiceFactory::GetInstance(),
                              PhotosServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    // Prepare mocks for PushNotificationClient dependency
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser_.get());

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    password_store_mock_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::kUnknown);
    sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

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
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    // Create mock command handlers. These are just for initializing the view
    // controller; because the handlers are local to this methdd, they will not
    // exist during tests, so if the tests call any commands they will fail.
    id mock_application_handler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mock_settings_handler = OCMProtocolMock(@protocol(SettingsCommands));
    id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
    mock_popup_menu_handler_ = OCMProtocolMock(@protocol(PopupMenuCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_handler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_handler
                             forProtocol:@protocol(SettingsCommands)];
    [dispatcher startDispatchingToTarget:mock_snackbar_handler
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mock_popup_menu_handler_
                             forProtocol:@protocol(PopupMenuCommands)];

    SettingsTableViewController* controller =
        [[SettingsTableViewController alloc]
                     initWithBrowser:browser_.get()
            hasDefaultBrowserBlueDot:has_default_browser_blue_dot_];
    controller.applicationHandler =
        HandlerForProtocol(dispatcher, ApplicationCommands);
    controller.settingsHandler =
        HandlerForProtocol(dispatcher, SettingsCommands);
    controller.snackbarHandler =
        HandlerForProtocol(dispatcher, SnackbarCommands);
    return controller;
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

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  void VerifyDefaultBrowwserBlueDot(bool has_default_browser_blue_dot) {
    has_default_browser_blue_dot_ = has_default_browser_blue_dot;
    CreateController();
    CheckController();

    NSArray<TableViewItem*>* default_section_items =
        [controller().tableViewModel
            itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                             SettingsSectionIdentifierDefaults];

    TableViewDetailIconItem* default_browser_item =
        static_cast<TableViewDetailIconItem*>(default_section_items[0]);

    EXPECT_EQ(has_default_browser_blue_dot,
              BadgeType::kNotificationDot == default_browser_item.badgeType);
  }

 protected:
  // Needed for test profile created by TestProfileIOS().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  ScopedVariationsService scoped_variations_service_;
  TestProfileManagerIOS profile_manager_;

  FakeSystemIdentity* fake_identity_ = nullptr;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
  raw_ptr<syncer::TestSyncService> sync_service_ = nullptr;
  scoped_refptr<password_manager::TestPasswordStore> password_store_mock_;

  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;

  BOOL has_default_browser_blue_dot_ = false;
  id<PopupMenuCommands> mock_popup_menu_handler_;
};

// Verifies that the sign-in setting row is removed if sign-in is disabled
// through the "Allow Chrome Sign-in" option.
TEST_F(SettingsTableViewControllerTest, SigninDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  CreateController();
  CheckController();

  ASSERT_FALSE([controller().tableViewModel
      hasSectionForSectionIdentifier:SettingsSectionIdentifier::
                                         SettingsSectionIdentifierSignIn]);
}

// Verifies that for a signed-in user, the account section shows 2 items: the
// one with the name/email, and the "Google Services" one.
TEST_F(SettingsTableViewControllerTest, AccountSectionIfSignedIn) {
  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kUnknown);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

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
  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kUnknown);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_->GetUserSettings()->SetPassphraseRequired();

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);

  // Verify that the account item is in an error state.
  TableViewAccountItem* identityAccountItem =
      base::apple::ObjCCast<TableViewAccountItem>(account_items[0]);
  EXPECT_TRUE(identityAccountItem.shouldDisplayError);
}

// Verifies that the error is removed from the model when the Account Storage
// error is resolved. Triggers the model update by firing a Sync State change.
TEST_F(SettingsTableViewControllerTest, ClearAccountStorageErrorWhenResolved) {
  // Set account error to resolve.
  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kUnknown);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
  const char kSyncPassphrase[] = "passphrase";
  sync_service_->GetUserSettings()->SetPassphraseRequired(kSyncPassphrase);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);

  // Verify that the account item is in an error state.
  TableViewAccountItem* identityAccountItem =
      base::apple::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem.shouldDisplayError);

  // Resolve the account error.
  sync_service_->GetUserSettings()->SetDecryptionPassphrase(kSyncPassphrase);

  // Verify that the account item is not in an error state when the error was
  // resolved and the data model reloaded.
  [controller() loadModel];
  account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);
  identityAccountItem =
      base::apple::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem != nil);
  EXPECT_FALSE(identityAccountItem.shouldDisplayError);
}

// Verifies that when eligible the account item model doesn't have the Account
// Storage error when there is no error.
TEST_F(SettingsTableViewControllerTest, DontHoldAccountErrorWhenNoError) {
  // Set no account error state.
  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kUnknown);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);

  CreateController();
  CheckController();

  NSArray* account_items = [controller().tableViewModel
      itemsInSectionWithIdentifier:SettingsSectionIdentifier::
                                       SettingsSectionIdentifierAccount];
  ASSERT_NE(0U, account_items.count);

  // Verify that the account item is not in an error state.
  TableViewAccountItem* identityAccountItem =
      base::apple::ObjCCast<TableViewAccountItem>(account_items[0]);
  ASSERT_TRUE(identityAccountItem != nil);
  EXPECT_FALSE(identityAccountItem.shouldDisplayError);
}

// Verifies that if the Save to Photos flag is enabled and Save to Photos is
// supported, then there is a Downloads Settings item in the expected section.
TEST_F(SettingsTableViewControllerTest, HasDownloadsMenuItem) {
  base::test::ScopedFeatureList features;

  CreateController();
  CheckController();

  EXPECT_TRUE([controller().tableViewModel
      hasItemForItemType:SettingsItemTypeDownloadsSettings
       sectionIdentifier:SettingsSectionIdentifierInfo]);
}

// Verifies that the default browser blue dot is displayed when indicated.
TEST_F(SettingsTableViewControllerTest, TestHasDefaultBrowserBlueDot) {
  VerifyDefaultBrowwserBlueDot(true);
}

// Verifies that the default browser blue dot is not displayed when indicated.
TEST_F(SettingsTableViewControllerTest, TestHasNoDefaultBrowserBlueDot) {
  VerifyDefaultBrowwserBlueDot(false);
}

// Verifies that blue dot will be updated when default browser settings are
// viewed while blue dot was showing.
TEST_F(SettingsTableViewControllerTest,
       TestUpdateToolsMenuBlueDotVisibilityCalled) {
  has_default_browser_blue_dot_ = true;
  CreateController();
  CheckController();

  // Create a navigation controller to avoid hitting the CHECK.
  [[maybe_unused]] UINavigationController* nav_controller =
      [[UINavigationController alloc] initWithRootViewController:controller()];

  OCMExpect([mock_popup_menu_handler_ updateToolsMenuBlueDotVisibility]);

  // Tap on the default browser settings.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];

  EXPECT_OCMOCK_VERIFY((id)mock_popup_menu_handler_);
}

// Verifies that blue dot will not be updated when default browser settings
// are viewed with no blue dot showing.
TEST_F(SettingsTableViewControllerTest,
       TestUpdateToolsMenuBlueDotVisibilityNotCalled) {
  has_default_browser_blue_dot_ = false;
  CreateController();
  CheckController();

  OCMReject([mock_popup_menu_handler_ updateToolsMenuBlueDotVisibility]);

  // Create a navigation controller to avoid hitting the CHECK.
  [[maybe_unused]] UINavigationController* nav_controller =
      [[UINavigationController alloc] initWithRootViewController:controller()];

  // Tap on the default browser settings.
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];

  EXPECT_OCMOCK_VERIFY((id)mock_popup_menu_handler_);
}

// Tests that updating search engines does not cause a crash.
// See crbug.com/408017580 for more details.
TEST_F(SettingsTableViewControllerTest, SearchEngineChnagedDoesntCrash) {
  // Make sure the controller is initialized without creating the view/model.
  CreateControllerWithoutView();

  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile_.get());

  // Force the search engines to load and alert their observer (the settings
  // table view controller). This should not crash, even though the view
  // controller has not loaded its model yet.
  template_url_service->Load();
}
