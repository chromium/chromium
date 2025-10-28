// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/sync_device_info/fake_device_info_sync_service.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Test implementation of `CrossDevicePrefTracker` that does nothing.
class TestCrossDevicePrefTracker
    : public sync_preferences::CrossDevicePrefTracker {
 public:
  TestCrossDevicePrefTracker() = default;
  ~TestCrossDevicePrefTracker() override = default;

  // `KeyedService` overrides
  void Shutdown() override {}

  // `CrossDevicePrefTracker` overrides
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  std::vector<sync_preferences::TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    return {};
  }

  std::optional<sync_preferences::TimestampedPrefValue> GetMostRecentValue(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    return std::nullopt;
  }
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

    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);

    GURL gurl("http://www.google.com");
    startup_params_ = [[AppStartupParameters alloc]
         initWithExternalURL:gurl
                 completeURL:gurl
             applicationMode:ApplicationModeForTabOpening::NORMAL
        forceApplicationMode:NO];

    mediator_ = [[SyncedSetUpMediator alloc]
          initWithPrefTracker:&pref_tracker_
        authenticationService:authentication_service_
        accountManagerService:account_manager_service_
        deviceInfoSyncService:&device_info_sync_service_
           profilePrefService:profile_->GetPrefs()
            startupParameters:startup_params_
              identityManager:identity_manager_];

    consumer_mock_ = OCMStrictProtocolMock(@protocol(SyncedSetUpConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  TestCrossDevicePrefTracker pref_tracker_;
  syncer::FakeDeviceInfoSyncService device_info_sync_service_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  AppStartupParameters* startup_params_;
  SyncedSetUpMediator* mediator_;
  id consumer_mock_;
  FakeSystemIdentity* fake_identity_ = [FakeSystemIdentity fakeIdentity1];
};

// Tests that the consumer receives the correct generic welcome message and no
// avatar when the user is signed out.
TEST_F(SyncedSetUpMediatorTest, TestConsumerUpdatesOnSignedOutState) {
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

// Tests that the consumer is updated when the primary account changes from
// signed-out to signed-in.
TEST_F(SyncedSetUpMediatorTest, TestConsumerUpdatesOnSignIn) {
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
