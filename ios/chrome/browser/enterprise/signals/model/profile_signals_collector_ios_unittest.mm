// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/profile_signals_collector_ios.h"

#import <memory>

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
constexpr char kFakeProfileEnrollmentDomain[] = "profile.domain.com";
}  // namespace

// Test fixture for ProfileSignalsCollectorIOS. It sets up a fake profile with
// policy management and required dependencies (such as UserCloudPolicyManager,
// ProfileIdService, and ConnectorsService) to test profile-scoped signal
// collection.
class ProfileSignalsCollectorIOSTest : public PlatformTest {
 protected:
  // Configures a test profile with cloud policy management, fake DM tokens,
  // and initializes `collector_` with required profile dependencies.
  void SetUp() override {
    PlatformTest::SetUp();

    // Setup Policy Manager for enrollment domain.
    enterprise_management::PolicyData profile_policy_data;
    profile_policy_data.set_managed_by(kFakeProfileEnrollmentDomain);
    profile_policy_data.set_request_token("test_profile_dm_token");

    auto store = std::make_unique<policy::MockUserCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    store->set_policy_data_for_testing(
        std::make_unique<enterprise_management::PolicyData>(
            std::move(profile_policy_data)));

    auto cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(store), /*extension_install_store=*/nullptr,
            base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.SetUserCloudPolicyManager(std::move(cloud_policy_manager));

    profile_ = std::move(builder).Build();

    // Set up FakeBrowserDMTokenStorage for ConnectorsService.
    fake_dm_token_storage_.SetClientId("test_client_id");
    fake_dm_token_storage_.SetDMToken("test_browser_dm_token");

    collector_ = std::make_unique<ProfileSignalsCollectorIOS>(
        profile_->GetPrefs(), profile_->GetUserCloudPolicyManager(),
        enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile_.get()),
        enterprise_connectors::ConnectorsServiceFactory::GetForProfile(
            profile_.get()));
  }

  base::test::TaskEnvironment task_environment_;
  // Provides fake DM token and client ID for ConnectorsService.
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  std::unique_ptr<TestProfileIOS> profile_;
  // The collector instance being tested.
  std::unique_ptr<ProfileSignalsCollectorIOS> collector_;
};

// Tests that `GetSupportedSignalNames()` returns only `kBrowserContextSignals`
// as the supported signal name on iOS.
TEST_F(ProfileSignalsCollectorIOSTest, GetSupportedSignalNames) {
  const std::unordered_set<device_signals::SignalName> supported_signals =
      collector_->GetSupportedSignalNames();
  EXPECT_EQ(supported_signals.size(), 1u);
  EXPECT_TRUE(supported_signals.find(
                  device_signals::SignalName::kBrowserContextSignals) !=
              supported_signals.end());
}

// Tests that `IsSignalSupported()` returns true for `kBrowserContextSignals`
// and false for unsupported signal types such as `kOsSignals`.
TEST_F(ProfileSignalsCollectorIOSTest, IsSignalSupported) {
  EXPECT_TRUE(collector_->IsSignalSupported(
      device_signals::SignalName::kBrowserContextSignals));
  EXPECT_FALSE(
      collector_->IsSignalSupported(device_signals::SignalName::kOsSignals));
}

// Tests that `GetSignal()` correctly collects default profile signals,
// including standard Safe Browsing protection level, enrollment domain, and
// profile ID, when default preferences are configured.
TEST_F(ProfileSignalsCollectorIOSTest, PopulateProfileSignals_DefaultValues) {
  // Set some prefs to verify they are collected.
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, false);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;

  collector_->GetSignal(device_signals::SignalName::kBrowserContextSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.profile_signals_response.has_value());
  const device_signals::ProfileSignalsResponse& profile_signals =
      response.profile_signals_response.value();

  // Verify Safe Browsing Level.
  EXPECT_EQ(profile_signals.safe_browsing_protection_level,
            safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  // Verify Enrollment Domain.
  ASSERT_TRUE(profile_signals.profile_enrollment_domain.has_value());
  EXPECT_EQ(profile_signals.profile_enrollment_domain.value(),
            kFakeProfileEnrollmentDomain);

  // Verify Profile ID.
  EXPECT_TRUE(profile_signals.profile_id.has_value());

  // Verify other fields are initialized.
  EXPECT_FALSE(profile_signals.built_in_dns_client_enabled);
  EXPECT_FALSE(profile_signals.chrome_remote_desktop_app_blocked);
  EXPECT_FALSE(profile_signals.site_isolation_enabled);
}

// Tests that `GetSignal()` collects the Enhanced Safe Browsing protection level
// when both Safe Browsing and Enhanced Safe Browsing preferences are enabled.
TEST_F(ProfileSignalsCollectorIOSTest,
       PopulateProfileSignals_EnhancedProtection) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;

  collector_->GetSignal(device_signals::SignalName::kBrowserContextSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.profile_signals_response.has_value());
  const device_signals::ProfileSignalsResponse& profile_signals =
      response.profile_signals_response.value();

  EXPECT_EQ(profile_signals.safe_browsing_protection_level,
            safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
}

// Tests that `GetSignal()` collects the `NO_SAFE_BROWSING` protection level
// when the Safe Browsing preference is disabled.
TEST_F(ProfileSignalsCollectorIOSTest, PopulateProfileSignals_NoSafeBrowsing) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;

  collector_->GetSignal(device_signals::SignalName::kBrowserContextSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.profile_signals_response.has_value());
  const device_signals::ProfileSignalsResponse& profile_signals =
      response.profile_signals_response.value();

  EXPECT_EQ(profile_signals.safe_browsing_protection_level,
            safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
}

// Tests that `GetSignal()` correctly collects enterprise connectors signals,
// including the real-time URL check mode and security event reporting
// providers.
TEST_F(ProfileSignalsCollectorIOSTest,
       PopulateProfileSignals_ConnectorsService) {
  // Set prefs to enable real-time URL check and reporting.
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
                    enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
                        REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  prefs->SetInteger(enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
                    policy::PolicyScope::POLICY_SCOPE_USER);

  // Enable reporting.
  prefs->SetInteger(enterprise_connectors::kOnSecurityEventScopePref,
                    policy::PolicyScope::POLICY_SCOPE_USER);
  enterprise_connectors::test::SetOnSecurityEventReporting(prefs,
                                                           /*enabled=*/true);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;

  collector_->GetSignal(device_signals::SignalName::kBrowserContextSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.profile_signals_response.has_value());
  const device_signals::ProfileSignalsResponse& profile_signals =
      response.profile_signals_response.value();

  EXPECT_EQ(profile_signals.realtime_url_check_mode,
            enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
                REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  // Verify reporting provider names.
  EXPECT_EQ(profile_signals.security_event_providers,
            std::vector<std::string>({"google"}));
}

// Tests that `GetSignal()` correctly collects all profile signals
// simultaneously when Safe Browsing, Enhanced Safe Browsing, and Connectors
// Service reporting preferences are active.
TEST_F(ProfileSignalsCollectorIOSTest, PopulateProfileSignals_AllActive) {
  PrefService* prefs = profile_->GetPrefs();

  // 1. Enable Enhanced Safe Browsing
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  // 2. Configure Connectors Service
  prefs->SetInteger(enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
                    enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
                        REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  prefs->SetInteger(enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
                    policy::PolicyScope::POLICY_SCOPE_USER);
  prefs->SetInteger(enterprise_connectors::kOnSecurityEventScopePref,
                    policy::PolicyScope::POLICY_SCOPE_USER);
  enterprise_connectors::test::SetOnSecurityEventReporting(prefs,
                                                           /*enabled=*/true);

  device_signals::SignalsAggregationRequest request;
  device_signals::SignalsAggregationResponse response;
  base::RunLoop run_loop;

  collector_->GetSignal(device_signals::SignalName::kBrowserContextSignals,
                        device_signals::UserPermission::kGranted, request,
                        response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.profile_signals_response.has_value());
  const device_signals::ProfileSignalsResponse& profile_signals =
      response.profile_signals_response.value();

  // Verify all signals are collected correctly simultaneously
  EXPECT_EQ(profile_signals.safe_browsing_protection_level,
            safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  ASSERT_TRUE(profile_signals.profile_enrollment_domain.has_value());
  EXPECT_EQ(profile_signals.profile_enrollment_domain.value(),
            kFakeProfileEnrollmentDomain);

  EXPECT_TRUE(profile_signals.profile_id.has_value());

  EXPECT_EQ(profile_signals.realtime_url_check_mode,
            enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
                REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  EXPECT_EQ(profile_signals.security_event_providers,
            std::vector<std::string>({"google"}));
}
