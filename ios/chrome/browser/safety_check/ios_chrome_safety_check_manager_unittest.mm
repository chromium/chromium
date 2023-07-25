// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"

#import <vector>

#import "base/memory/scoped_refptr.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_utils.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/upgrade/upgrade_recommended_details.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class IOSChromeSafetyCheckManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* registry = pref_service_->registry();
    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, false);
    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);

    TestChromeBrowserState::Builder builder;

    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));

    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    browser_state_ = builder.Build();

    password_check_manager_ =
        IOSChromePasswordCheckManagerFactory::GetForBrowserState(
            browser_state_.get());

    safety_check_manager_ = std::make_unique<IOSChromeSafetyCheckManager>(
        pref_service_.get(), password_check_manager_,
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void TearDown() override { safety_check_manager_->Shutdown(); }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

std::vector<password_manager::CredentialUIEntry>
CreateCredentialsListWithNoInsecurePasswords() {
  password_manager::PasswordForm password_form;
  password_form.url = GURL("http://accounts.google.com/a/LoginAuth");
  password_form.username_value = u"test@testmail.com";
  password_form.password_value = u"test1";

  return {password_manager::CredentialUIEntry(password_form)};
}

// Returns app upgrade details for an up-to-date application.
UpgradeRecommendedDetails UpdatedAppDetails() {
  UpgradeRecommendedDetails details;

  details.is_up_to_date = true;

  // Within Omaha, when the app is up-to-date, `next_version` and `upgrade_url`
  // are empty.
  details.next_version = "";
  details.upgrade_url = GURL();

  return details;
}

// Returns app upgrade details for an outdated application.
UpgradeRecommendedDetails OutdatedAppDetails() {
  UpgradeRecommendedDetails details;

  details.is_up_to_date = false;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");

  return details;
}

}  // namespace

// Tests the Safe Browsing Check state is `kSafe` when Safe Browsing is enabled,
// but not managed.
TEST_F(IOSChromeSafetyCheckManagerTest, SafeBrowsingEnabledReturnsSafeState) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);
}

// Tests the Safe Browsing Check state is `kUnsafe` when Safe Browsing is
// disabled, and not managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingDisabledReturnsUnsafeState) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kUnsafe);
}

// Tests the Safe Browsing Check state is `kManaged` when Safe Browsing is
// enabled, and managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingManagedAndEnabledReturnsManagedState) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled, base::Value(true));

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kManaged);
}

// Tests the Safe Browsing Check state is `kManaged` when Safe Browsing is
// disabled, and managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingManagedAndDisabledReturnsManagedState) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled,
                                base::Value(false));

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kManaged);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kRunning.
TEST_F(IOSChromeSafetyCheckManagerTest, ConvertsPasswordCheckStateRunning) {
  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kRunning, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kRunning);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kRunning, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kRunning);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kNoPasswords.
TEST_F(IOSChromeSafetyCheckManagerTest, ConvertsPasswordCheckStateNoPasswords) {
  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kNoPasswords, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDisabled);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kNoPasswords, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDisabled);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kSignedOut when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateSignedOutWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kSignedOut);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kSignedOut when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateSignedOutWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kSignedOut);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kSignedOut);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOffline when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOfflineWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOffline when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOfflineWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kQuotaLimit when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateQuotaLimitWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kQuotaLimit when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateQuotaLimitWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOther when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOtherWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOther when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOtherWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kCanceled when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateCanceledWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kCanceled when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateCanceledWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kIdle when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateIdleWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kSafe);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kIdle when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateIdleWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kSafe);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kSafe);
}

// Tests an Omaha response that exceeds `kOmahaNetworkWaitTime` wait time is
// properly handled.
TEST_F(IOSChromeSafetyCheckManagerTest, HandlesExpiredOmahaResponse) {
  // Starting the Omaha check sets the Update Chrome check state to running.
  safety_check_manager_->StartOmahaCheckForTesting();
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  // Even 1s before `kOmahaNetworkWaitTime` is met, the check state should still
  // be running.
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime - base::Seconds(1));
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  // Once `kOmahaNetworkWaitTime` is met, the current Omaha request should be
  // considered an Omaha error.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOmahaError);
}

// Tests a valid, app-up-to-date Omaha response is properly handled.
TEST_F(IOSChromeSafetyCheckManagerTest, HandlesOmahaResponseAppIsUpToDate) {
  safety_check_manager_->StartOmahaCheckForTesting();
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);

  safety_check_manager_->HandleOmahaResponseForTesting(UpdatedAppDetails());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kUpToDate);

  // Once `kOmahaNetworkWaitTime` elapses, nothing should happen, because the
  // response was received before `kOmahaNetworkWaitTime` was met.
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kUpToDate);
}

// Tests a valid, app-outdated Omaha response is properly handled.
TEST_F(IOSChromeSafetyCheckManagerTest, HandlesOmahaResponseAppOutdated) {
  safety_check_manager_->StartOmahaCheckForTesting();
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);

  safety_check_manager_->HandleOmahaResponseForTesting(OutdatedAppDetails());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOutOfDate);

  // Once `kOmahaNetworkWaitTime` elapses, nothing should happen, because the
  // response was received before `kOmahaNetworkWaitTime` was met.
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOutOfDate);
}
