// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_util.h"
#import "components/sync_device_info/fake_device_info_sync_service.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator_delegate.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Test implementation of `CrossDevicePrefTracker`.
class TestCrossDevicePrefTracker
    : public sync_preferences::CrossDevicePrefTracker {
 public:
  TestCrossDevicePrefTracker() = default;
  ~TestCrossDevicePrefTracker() override = default;

  // `KeyedService` overrides.
  void Shutdown() override {}

  // `CrossDevicePrefTracker` overrides.
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  std::vector<sync_preferences::TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    auto it = pref_values_.find(pref_name);
    if (it == pref_values_.end()) {
      return {};
    }

    std::vector<sync_preferences::TimestampedPrefValue> result;
    for (const auto& timestamped_value : it->second) {
      sync_preferences::TimestampedPrefValue copied_value =
          timestamped_value.Clone();
      result.push_back(std::move(copied_value));
    }
    return result;
  }

  std::optional<sync_preferences::TimestampedPrefValue> GetMostRecentValue(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    return std::nullopt;
  }

  // Testing Method for injecting pref values into the tracker.
  void AddSyncedPrefValue(std::string_view pref_name,
                          sync_preferences::TimestampedPrefValue& value) {
    pref_values_[pref_name].push_back(std::move(value));
  }

 private:
  // Testing member. Map containing TimestampedPrefValues mapped to their
  // associated pref's name.
  std::map<std::string_view,
           std::vector<sync_preferences::TimestampedPrefValue>>
      pref_values_;
};

}  // namespace

// Test fixture for SyncedSetUpMediator.
class SyncedSetUpMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    web_state_list_ = browser_.get()->GetWebStateList();
    web_state_ = std::make_unique<web::FakeWebState>();
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  // Sets a profile pref value.
  void SetProfilePref(std::string_view pref_name, base::Value value) {
    PrefService* profile_pref_service = profile_->GetPrefs();
    profile_pref_service->Set(pref_name, value);
  }

  // Sets a local-state pref value.
  void SetLocalStatePref(std::string_view pref_name, base::Value value) {
    PrefService* local_state =
        TestingApplicationContext::GetGlobal()->GetLocalState();
    local_state->Set(pref_name, value);
  }

  // Initializes the mediator.
  void InitializeMediator(bool on_ntp = false) {
    ConfigureWebStateList(on_ntp);
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);

    GURL gurl(on_ntp ? "chrome://newtab" : "https://chromium.org");
    startup_params_ = [[AppStartupParameters alloc]
         initWithExternalURL:gurl
                 completeURL:gurl
             applicationMode:ApplicationModeForTabOpening::NORMAL
        forceApplicationMode:NO];

    snackbar_handler_mock_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));

    mediator_ = [[SyncedSetUpMediator alloc]
            initWithPrefTracker:&pref_tracker_
          authenticationService:authentication_service_
          accountManagerService:account_manager_service_
          deviceInfoSyncService:&device_info_sync_service_
             profilePrefService:profile_->GetPrefs()
                identityManager:identity_manager_
                   webStateList:web_state_list_
              startupParameters:startup_params_
        snackbarCommandsHandler:snackbar_handler_mock_];

    consumer_mock_ = OCMStrictProtocolMock(@protocol(SyncedSetUpConsumer));
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(SyncedSetUpMediatorDelegate));
  }

  // Configures `web_state_list_` with a an active WebState, given whether the
  // visible page should be the NTP.
  void ConfigureWebStateList(bool on_ntp = false) {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kCreateTabHelperOnlyForRealizedWebStates);
    web_state_->SetIsRealized(false);
    if (on_ntp) {
      web_state_->SetVisibleURL(GURL("chrome://newtab"));
      ASSERT_TRUE(IsVisibleURLNewTabPage(web_state_.get()));
    } else {
      web_state_->SetVisibleURL(GURL("https://chromium.org"));
      ASSERT_FALSE(IsVisibleURLNewTabPage(web_state_.get()));
    }
    int index = web_state_list_->InsertWebState(std::move(web_state_));
    ASSERT_NE(index, -1);
    web_state_list_->ActivateWebStateAt(index);
  }

  // Creates a DeviceInfo object.
  std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfoForTesting(
      std::string guid,
      syncer::DeviceInfo::FormFactor form_factor,
      syncer::DeviceInfo::OsType os_type,
      base::Time last_updated_timestamp = base::Time::Now()) {
    return CreateFakeDeviceInfo(guid, "Device Name", std::nullopt,
                                sync_pb::SyncEnums::TYPE_UNSET, os_type,
                                form_factor, "manufacturer", "model",
                                std::string(), last_updated_timestamp);
  }

  // Helper for creating a DeviceInfo object.
  std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
      const std::string& guid,
      const std::string& name = "name",
      const std::optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
          std::nullopt,
      sync_pb::SyncEnums_DeviceType device_type =
          sync_pb::SyncEnums_DeviceType_TYPE_UNSET,
      syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kUnknown,
      syncer::DeviceInfo::FormFactor form_factor =
          syncer::DeviceInfo::FormFactor::kUnknown,
      const std::string& manufacturer_name = "manufacturer",
      const std::string& model_name = "model",
      const std::string& full_hardware_class = std::string(),
      base::Time last_updated_timestamp = base::Time::Now()) {
    return std::make_unique<syncer::DeviceInfo>(
        guid, name, "chrome_version", "user_agent", device_type, os_type,
        form_factor, "device_id", manufacturer_name, model_name,
        full_hardware_class, last_updated_timestamp,
        syncer::DeviceInfoUtil::GetPulseInterval(),
        /*send_tab_to_self_receiving_enabled=*/
        false,
        sync_pb::
            SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
        sharing_info,
        /*paask_info=*/std::nullopt,
        /*fcm_registration_token=*/std::string(),
        /*interested_data_types=*/syncer::DataTypeSet(),
        /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
  }

  // Helper for configuring a TimestampedPrefValue.
  void ConfigureTimestampedPrefValue(
      sync_preferences::TimestampedPrefValue& timestamped_value,
      base::Value value,
      std::string device_sync_cache_guid,
      base::Time last_observed_change_time = base::Time::Now()) {
    timestamped_value.value = value.Clone();
    timestamped_value.last_observed_change_time = last_observed_change_time;
    timestamped_value.device_sync_cache_guid = device_sync_cache_guid;
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestCrossDevicePrefTracker pref_tracker_;
  syncer::FakeDeviceInfoSyncService device_info_sync_service_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  AppStartupParameters* startup_params_;
  SyncedSetUpMediator* mediator_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  std::unique_ptr<web::FakeWebState> web_state_;
  SceneState* scene_state_;
  id snackbar_handler_mock_;
  id consumer_mock_;
  id delegate_mock_;
  FakeSystemIdentity* fake_identity_ = [FakeSystemIdentity fakeIdentity1];
};

// Tests that the delegate is informed that Synced Set Up is running during the
// first run while on the NTP.
TEST_F(SyncedSetUpMediatorTest, TestDelegateInformedOfFirstRunOnNTP) {
  ResetFirstRunSentinel();
  ASSERT_TRUE(IsFirstRun());

  // Add a pref associated with a remote device to the Cross Device Pref
  // Tracker.
  std::string remote_guid = "remote_device";
  sync_preferences::TimestampedPrefValue magic_stack_timestamped_pref_value;
  ConfigureTimestampedPrefValue(magic_stack_timestamped_pref_value,
                                base::Value(true), remote_guid);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      magic_stack_timestamped_pref_value);

  // Add the remote device to the Device Info Tracker.
  device_info_sync_service_.GetDeviceInfoTracker()->Add(
      CreateDeviceInfoForTesting(remote_guid,
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kIOS));

  // Set corresponding pref on the local device.
  SetProfilePref(ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
                 base::Value(false));

  // Synced Set Up was started from the New Tab page.
  bool on_ntp = true;
  InitializeMediator(on_ntp);

  OCMExpect([delegate_mock_ mediatorWillStartPostFirstRunFlow:[OCMArg any]]);
  mediator_.delegate = delegate_mock_;
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that the delegate is informed that Synced Set Up is running during the
// first run while on a URL page.
TEST_F(SyncedSetUpMediatorTest, TestDelegateInformedOfFirstRunOnURLPage) {
  ResetFirstRunSentinel();
  ASSERT_TRUE(IsFirstRun());

  // Add a pref associated with a remote device to the Cross Device Pref
  // Tracker.
  std::string remote_guid = "remote_device";
  sync_preferences::TimestampedPrefValue magic_stack_timestamped_pref_value;
  ConfigureTimestampedPrefValue(magic_stack_timestamped_pref_value,
                                base::Value(true), remote_guid);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      magic_stack_timestamped_pref_value);

  // Add the remote device to the Device Info Tracker.
  device_info_sync_service_.GetDeviceInfoTracker()->Add(
      CreateDeviceInfoForTesting(remote_guid,
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kIOS));

  // Set corresponding pref on the local device.
  SetProfilePref(ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
                 base::Value(false));

  // Synced Set Up was not started from the New Tab page.
  bool on_ntp = false;
  InitializeMediator(on_ntp);

  OCMExpect([delegate_mock_ mediatorWillStartPostFirstRunFlow:[OCMArg any]]);
  mediator_.delegate = delegate_mock_;
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that the delegate is not told to dismiss by a dismissing Snackbar, if
// it was dismissed by another Snackbar appearing in its place.
TEST_F(SyncedSetUpMediatorTest,
       TestMediatorDoesNotFinishIfNewSnackbarPresented) {
  ForceFirstRunRecency(7);
  ASSERT_FALSE(IsFirstRun());

  // Add a pref associated with a remote device to the Cross Device Pref
  // Tracker.
  std::string remote_guid = "remote_device";
  sync_preferences::TimestampedPrefValue omnibox_timestamped_pref_value;
  ConfigureTimestampedPrefValue(omnibox_timestamped_pref_value,
                                base::Value(true), remote_guid);
  pref_tracker_.AddSyncedPrefValue(prefs::kCrossDeviceOmniboxIsInBottomPosition,
                                   omnibox_timestamped_pref_value);

  // Add the remote device to the Device Info Tracker.
  device_info_sync_service_.GetDeviceInfoTracker()->Add(
      CreateDeviceInfoForTesting(remote_guid,
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kIOS));

  // Set corresponding pref on the local device.
  SetLocalStatePref(omnibox::kIsOmniboxInBottomPosition, base::Value(false));

  // Synced Set Up was not started from the New Tab page.
  bool on_ntp = false;
  InitializeMediator(on_ntp);

  OCMReject([delegate_mock_ mediatorWillStartPostFirstRunFlow:[OCMArg any]]);
  OCMExpect([delegate_mock_ mediatorWillStartFromUrlPage:[OCMArg any]]);
  mediator_.delegate = delegate_mock_;
  EXPECT_OCMOCK_VERIFY(delegate_mock_);

  // A Snackbar will dismiss naturally after 4 seconds, then the mediator will
  // inform the delegate it is finished. If a snackbar is dismissed early by the
  // presentation of another Snackbar, the mediator should not inform the
  // delegate that it is finished.
  OCMExpect([snackbar_handler_mock_ showSnackbarMessage:[OCMArg any]]);
  OCMExpect([delegate_mock_ recordSyncedSetUpShown:[OCMArg any]]);
  [mediator_ applyPrefs];
  EXPECT_OCMOCK_VERIFY(snackbar_handler_mock_);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);

  task_environment_.FastForwardBy(base::Seconds(3));

  OCMExpect([snackbar_handler_mock_ showSnackbarMessage:[OCMArg any]]);
  OCMExpect([delegate_mock_ recordSyncedSetUpShown:[OCMArg any]]);
  [mediator_ applyPrefs];
  EXPECT_OCMOCK_VERIFY(snackbar_handler_mock_);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);

  OCMReject([delegate_mock_ syncedSetUpMediatorDidComplete:[OCMArg any]]);
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that the delegate is informed the mediator is finished if the mediator
// attemps to apply prefs and no remote prefs are available.
TEST_F(SyncedSetUpMediatorTest, TestMediatorFinishesWhenNoRemotePrefsToApply) {
  ResetFirstRunSentinel();
  ASSERT_TRUE(IsFirstRun());
  InitializeMediator();

  OCMExpect([delegate_mock_ syncedSetUpMediatorDidComplete:[OCMArg any]]);
  mediator_.delegate = delegate_mock_;
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that the consumer receives the correct generic welcome message and no
// avatar when the user is signed out.
TEST_F(SyncedSetUpMediatorTest, TestConsumerUpdatesOnSignedOutState) {
  InitializeMediator();
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_TITLE);

  OCMExpect([consumer_mock_ setWelcomeMessage:expectedTitle]);
  OCMExpect([consumer_mock_ setAvatarImage:nil]);

  mediator_.consumer = consumer_mock_;

  EXPECT_OCMOCK_VERIFY(consumer_mock_);
}

// Tests that the consumer receives a personalized welcome message and an avatar
// when the user is signed in.
TEST_F(SyncedSetUpMediatorTest, TestConsumerUpdatesOnSignedInState) {
  InitializeMediator();
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity_);
  authentication_service_->SignIn(fake_identity_,
                                  signin_metrics::AccessPoint::kUnknown);

  NSString* expectedTitle = l10n_util::GetNSStringF(
      IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_WITH_USER_NAME_TITLE,
      base::SysNSStringToUTF16(fake_identity_.userGivenName));
  OCMExpect([consumer_mock_ setWelcomeMessage:expectedTitle]);
  OCMExpect([consumer_mock_ setAvatarImage:[OCMArg isNotNil]]);

  mediator_.consumer = consumer_mock_;

  EXPECT_OCMOCK_VERIFY(consumer_mock_);
}

// Tests that prefs on the local device are updated after the mediator applies
// prefs.
TEST_F(SyncedSetUpMediatorTest, TestPrefsChangeOnApply) {
  ResetFirstRunSentinel();
  ASSERT_TRUE(IsFirstRun());

  // Add a pref associated with a remote device to the Cross Device Pref
  // Tracker.
  std::string remote_guid = "remote_device";
  sync_preferences::TimestampedPrefValue magic_stack_timestamped_pref_value;
  ConfigureTimestampedPrefValue(magic_stack_timestamped_pref_value,
                                base::Value(true), remote_guid);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      magic_stack_timestamped_pref_value);

  // Add the remote device to the Device Info Tracker.
  device_info_sync_service_.GetDeviceInfoTracker()->Add(
      CreateDeviceInfoForTesting(remote_guid,
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kIOS));

  // Set corresponding pref on the local device.
  SetProfilePref(ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
                 base::Value(false));

  ASSERT_FALSE(profile_->GetPrefs()
                   ->GetValue(ntp_tiles::prefs::kMagicStackHomeModuleEnabled)
                   .GetBool());

  // Synced Set Up was started from the New Tab page.
  bool on_ntp = true;
  InitializeMediator(on_ntp);

  OCMExpect([delegate_mock_ mediatorWillStartPostFirstRunFlow:[OCMArg any]]);
  mediator_.delegate = delegate_mock_;
  EXPECT_OCMOCK_VERIFY(delegate_mock_);

  // Expect that the pref is changed.
  [mediator_ applyPrefs];
  EXPECT_EQ(profile_->GetPrefs()->GetValue(
                ntp_tiles::prefs::kMagicStackHomeModuleEnabled),
            base::Value(true));
}

// Tests that the consumer is updated when the primary account changes from
// signed-out to signed-in.
TEST_F(SyncedSetUpMediatorTest, TestConsumerUpdatesOnSignIn) {
  InitializeMediator();
  NSString* signedOutTitle =
      l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_TITLE);

  OCMExpect([consumer_mock_ setWelcomeMessage:signedOutTitle]);
  OCMExpect([consumer_mock_ setAvatarImage:nil]);

  mediator_.consumer = consumer_mock_;

  EXPECT_OCMOCK_VERIFY(consumer_mock_);

  NSString* signedInTitle = l10n_util::GetNSStringF(
      IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_WITH_USER_NAME_TITLE,
      base::SysNSStringToUTF16(fake_identity_.userGivenName));
  OCMExpect([consumer_mock_ setWelcomeMessage:signedInTitle]);
  OCMExpect([consumer_mock_ setAvatarImage:[OCMArg isNotNil]]);

  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity_);
  authentication_service_->SignIn(fake_identity_,
                                  signin_metrics::AccessPoint::kUnknown);

  EXPECT_OCMOCK_VERIFY(consumer_mock_);
}

// Tests that the consumer is updated when account info (like an avatar) is
// fetched/updated after the user is already signed in.
TEST_F(SyncedSetUpMediatorTest, TestConsumerUpdatesOnAccountInfoUpdated) {
  InitializeMediator();
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(fake_identity_);
  authentication_service_->SignIn(fake_identity_,
                                  signin_metrics::AccessPoint::kUnknown);

  NSString* signedInTitle = l10n_util::GetNSStringF(
      IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_WITH_USER_NAME_TITLE,
      base::SysNSStringToUTF16(fake_identity_.userGivenName));
  OCMExpect([consumer_mock_ setWelcomeMessage:signedInTitle]);
  OCMExpect([consumer_mock_ setAvatarImage:[OCMArg isNotNil]]);
  mediator_.consumer = consumer_mock_;

  EXPECT_OCMOCK_VERIFY(consumer_mock_);

  OCMExpect([consumer_mock_ setWelcomeMessage:signedInTitle]);
  OCMExpect([consumer_mock_ setAvatarImage:[OCMArg isNotNil]]);

  system_identity_manager->FireIdentityUpdatedNotification(fake_identity_);

  EXPECT_OCMOCK_VERIFY(consumer_mock_);
}
