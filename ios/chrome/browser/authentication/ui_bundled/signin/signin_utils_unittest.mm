// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/version.h"
#import "components/feature_engagement/public/configuration.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

void ExpectNextShowTimeInRange(base::Time next_show_time) {
  base::Time lo = base::Time::Now() - base::Days(13);
  base::Time hi = base::Time::Now();
  EXPECT_GE(next_show_time, lo);
  EXPECT_LE(next_show_time, hi);
}

class SigninUtilsTest : public PlatformTest {
 public:
  SigninUtilsTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    feature_list_.InitAndDisableFeature(
        switches::kFullscreenSignInPromoUseDate);
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
    sync_service_ = SyncServiceFactory::GetForProfile(profile_);
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_);
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_));
    time_in_past_ = base::Time::Now();
    task_environment_.FastForwardBy(base::Days(2));
  }

  void TearDown() override {
    mock_tracker_ = nullptr;
    identity_manager_ = nullptr;
    sync_service_ = nullptr;
    account_manager_service_ = nullptr;
    profile_ = nullptr;
    NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
    [standardDefaults removeObjectForKey:kDisplayedSSORecallForMajorVersionKey];
    [standardDefaults removeObjectForKey:kLastShownAccountGaiaIdVersionKey];
    [standardDefaults removeObjectForKey:kSigninPromoViewDisplayCountKey];
    [standardDefaults synchronize];
    PlatformTest::TearDown();
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterProfilePrefs(registry.get());
    return prefs;
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  PrefService* GetProfilePrefs() { return profile_->GetPrefs(); }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  raw_ptr<ChromeAccountManagerService> account_manager_service_ = nullptr;
  base::Time time_in_past_;
};

// Should not show the sign-in upgrade for the first time, after FRE.
TEST_F(SigninUtilsTest, TestWillNotDisplayNoLastShownTime) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
  ExpectNextShowTimeInRange(GetLocalState()->GetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset));
}

// Should not show the sign-in upgrade for the first time, after FRE.
TEST_F(SigninUtilsTest, TestWillRecordLastShowTimeAgain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");

  GetLocalState()->SetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset,
      base::Time::Now() + base::Days(10));

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
  ExpectNextShowTimeInRange(GetLocalState()->GetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset));
}

// Should not show the sign-in upgrade if next show time is not reached.
TEST_F(SigninUtilsTest, TestWillNotDisplayNextShowTimeNotReached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "100"}});
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");

  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
  ExpectNextShowTimeInRange(GetLocalState()->GetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset));
}

// Should show the sign-in upgrade if next show time is reached.
TEST_F(SigninUtilsTest, TestWillDisplayNextShowTimeReached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");

  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  base::Time next_show_time = GetLocalState()->GetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset);
  task_environment_.FastForwardBy(base::Days(100));

  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
  EXPECT_EQ(GetLocalState()->GetTime(
                prefs::kSigninStartupPromoLastShownTimeWithRandomOffset),
            next_show_time);
}

// Should not show the sign-in upgrade twice on the same version.
TEST_F(SigninUtilsTest, TestWillNotDisplaySameVersion) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMinorVersion) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_1_1("1.1");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_1));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayTwoMinorVersions) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_1_2("1.2");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_2));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMajorVersion) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_2_0("2.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_2_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Should show the sign-in upgrade a second time, 2 version after.
TEST_F(SigninUtilsTest, TestWillDisplayTwoMajorVersions) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(profile_, version_3_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Expected: should not show the sign-in upgrade when the fullscreen sign-in
// promo manager migration is disabled.
TEST_F(SigninUtilsTest, TestWillShowTwoTimesOnlyLegacy) {
  // Disable the fullscreen sign-in promo manager migration.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kFullscreenSigninPromoManagerMigration,
                                    false);
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_3_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_5_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowTwoTimesOnly) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");

  // Mock sign-in fullscreen promo FET feature with 2 triggers.
  int interaction_count = 2;
  std::vector<std::pair<feature_engagement::EventConfig, int>> event_list;
  event_list.emplace_back(
      feature_engagement::EventConfig(
          feature_engagement::events::kIOSSigninFullscreenPromoTrigger,
          feature_engagement::Comparator(feature_engagement::ANY, 0),
          feature_engagement::kMaxStoragePeriod,
          feature_engagement::kMaxStoragePeriod),
      interaction_count);
  EXPECT_CALL(*mock_tracker_,
              ListEvents(testing::Ref(
                  feature_engagement::kIPHiOSPromoSigninFullscreenFeature)))
      .WillRepeatedly(testing::Return(event_list));

  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_3_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_5_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Add new account.
// Expected: should show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowForNewAccountAdded) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_3_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(profile_, version_5_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Add new account.
// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Remove previous account.
// Expected: should not show the sign-in upgrade when the fullscreen sign-in
// promo manager migration is disabled.
TEST_F(SigninUtilsTest, TestWillNotShowWithAccountRemovedLegacy) {
  // Disable the fullscreen sign-in promo manager migration.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kFullscreenSigninPromoManagerMigration,
                                    false);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_3_0);
  fake_system_identity_manager()->ForgetIdentity(fake_identity,
                                                 base::DoNothing());
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_5_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Add new account.
// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Remove previous account.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillNotShowWithAccountRemoved) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_3_0);
  fake_system_identity_manager()->ForgetIdentity(fake_identity,
                                                 base::DoNothing());

  // Mock sign-in fullscreen promo FET feature with 2 triggers.
  int interaction_count = 2;
  std::vector<std::pair<feature_engagement::EventConfig, int>> event_list;
  event_list.emplace_back(
      feature_engagement::EventConfig(
          feature_engagement::events::kIOSSigninFullscreenPromoTrigger,
          feature_engagement::Comparator(feature_engagement::ANY, 0),
          feature_engagement::kMaxStoragePeriod,
          feature_engagement::kMaxStoragePeriod),
      interaction_count);
  EXPECT_CALL(*mock_tracker_,
              ListEvents(testing::Ref(
                  feature_engagement::kIPHiOSPromoSigninFullscreenFeature)))
      .WillRepeatedly(testing::Return(event_list));

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_5_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 4.0.
// Add an account.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillNotShowNewAccountUntilTwoVersion) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_4_0("4.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_3_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_4_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Show the sign-in upgrade on version 1.0.
// Move to version 2.0.
// Add an account.
// Expected: should not show the sign-in upgrade (only display every 2
// versions).
TEST_F(SigninUtilsTest, TestWillNotShowNewAccountUntilTwoVersionBis) {
  const base::Version version_1_0("1.0");
  const base::Version version_2_0("2.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_2_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Should not show the sign-in upgrade for first run after post restore.
TEST_F(SigninUtilsTest, TestWillNotShowIfFirstRunAfterPostRestore) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  const base::Version version_1_0("1.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  task_environment_.FastForwardBy(base::Days(100));
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  ASSERT_TRUE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));

  AccountInfo accountInfo =
      AccountInfo::Builder(GaiaId("gaia"), "foo@bar.com").Build();
  StorePreRestoreIdentity(GetProfilePrefs(), accountInfo,
                          /*history_sync_enabled=*/false);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
}

// Should not show the sign-in upgrade if sign-in is disabled by policy.
TEST_F(SigninUtilsTest, TestWillNotShowIfDisabledByPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  const base::Version version_1_0("1.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  task_environment_.FastForwardBy(base::Days(100));
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
}

// Should show if the user is signed-in without history opt-in.
TEST_F(SigninUtilsTest, TestWillShowIfSignedInWithoutHistoryOptIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_);
  authentication_service->SignIn(identity,
                                 signin_metrics::AccessPoint::kStartPage);

  const base::Version version_1_0("1.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  // Using task_environment_.FastForwardBy() causes this test to crash due to
  // sync internal logic.
  GetLocalState()->SetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset, time_in_past_);

  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
  EXPECT_FALSE(
      GetLocalState()
          ->GetTime(prefs::kSigninStartupPromoLastShownTimeWithRandomOffset)
          .is_null());
}

// Should not show if the user is signed-in with history opt-in.
TEST_F(SigninUtilsTest, TestWillNotShowIfSignedInWithHistoryOptIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kFullscreenSignInPromoUseDate, {{"interval", "1"}});
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_);
  authentication_service->SignIn(identity,
                                 signin_metrics::AccessPoint::kStartPage);
  const base::Version version_1_0("1.0");
  signin::RecordFullscreenSigninPromoStarted(
      identity_manager_, account_manager_service_, version_1_0);
  // Using task_environment_.FastForwardBy() causes this test to crash due to
  // sync internal logic.
  GetLocalState()->SetTime(
      prefs::kSigninStartupPromoLastShownTimeWithRandomOffset, time_in_past_);

  syncer::SyncUserSettings* sync_user_settings =
      sync_service_->GetUserSettings();
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                      true);
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kTabs, true);

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(profile_, version_1_0));
}

}  // namespace
