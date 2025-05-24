// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

#import "base/json/json_reader.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestProfileDmToken[] = "profile_dm_token";
constexpr char kTestBrowserDmToken[] = "browser_dm_token";
constexpr char kTestClientId[] = "client_id";
constexpr char kTestProfileEmail[] = "test@example.com";
constexpr char kTestProfileDomain[] = "example.com";
constexpr char kTestMachineDomain[] = "machine.com";

class ConnectorsServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    // Setup required to access a profile DM token.
    enterprise_management::PolicyData profile_policy_data;
    profile_policy_data.set_request_token(kTestProfileDmToken);

    auto store = std::make_unique<policy::MockUserCloudPolicyStore>();
    store->set_policy_data_for_testing(
        std::make_unique<enterprise_management::PolicyData>(
            std::move(profile_policy_data)));

    auto cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(store), base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    auto profile_builder = TestProfileIOS::Builder();
    profile_builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_builder.SetUserCloudPolicyManager(std::move(cloud_policy_manager));
    profile_ = std::move(profile_builder).Build();

    // Setup required to access a profile DM token.
    fake_browser_dm_token_storage_.SetDMToken(kTestBrowserDmToken);
    fake_browser_dm_token_storage_.SetClientId(kTestClientId);

    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kTestMachineDomain);

    auto machine_store =
        std::make_unique<policy::MachineLevelUserCloudPolicyStore>(
            policy::DMToken::CreateValidToken(kTestBrowserDmToken),
            std::string(), base::FilePath(), base::FilePath(), base::FilePath(),
            base::FilePath(), scoped_refptr<base::SequencedTaskRunner>());
    machine_store->set_policy_data_for_testing(std::move(policy_data));

    manager_ = std::make_unique<policy::MachineLevelUserCloudPolicyManager>(
        std::move(machine_store), /*external_data_manager=*/nullptr,
        /*policy_dir=*/base::FilePath(),
        scoped_refptr<base::SequencedTaskRunner>(),
        network::TestNetworkConnectionTracker::CreateGetter());

    GetApplicationContext()
        ->GetBrowserPolicyConnector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(manager_.get());
  }

  TestProfileIOS* profile() { return profile_.get(); }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  void MakePrimaryAccountAvailable(const std::string& email) {
    signin::MakePrimaryAccountAvailable(identity_manager(), email,
                                        signin::ConsentLevel::kSignin);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
  std::unique_ptr<policy::MachineLevelUserCloudPolicyManager> manager_;
};

}  // namespace

TEST_F(ConnectorsServiceTest, GetPrefs) {
  ConnectorsService connectors_service{profile()};
  const ConnectorsService const_connectors_service{profile()};

  PrefService* prefs = connectors_service.GetPrefs();
  const PrefService* const_prefs = const_connectors_service.GetPrefs();

  ASSERT_TRUE(prefs);
  ASSERT_TRUE(const_prefs);
  ASSERT_EQ(prefs, const_prefs);
}

TEST_F(ConnectorsServiceTest, GetProfileDmToken) {
  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_USER);
  ConnectorsService connectors_service{profile()};

  auto profile_dm_token =
      connectors_service.GetDmToken(kEnterpriseRealTimeUrlCheckScope);
  ASSERT_TRUE(profile_dm_token.has_value());
  ASSERT_EQ(profile_dm_token->value, kTestProfileDmToken);
  ASSERT_EQ(profile_dm_token->scope, policy::POLICY_SCOPE_USER);
}

TEST_F(ConnectorsServiceTest, GetBrowserDmToken) {
  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_MACHINE);
  ConnectorsService connectors_service{profile()};

  auto browser_dm_token =
      connectors_service.GetDmToken(kEnterpriseRealTimeUrlCheckScope);
  ASSERT_TRUE(browser_dm_token.has_value());
  ASSERT_EQ(browser_dm_token->value, kTestBrowserDmToken);
  ASSERT_EQ(browser_dm_token->scope, policy::POLICY_SCOPE_MACHINE);

  ASSERT_TRUE(connectors_service.GetBrowserDmToken());
  ASSERT_EQ(*connectors_service.GetBrowserDmToken(), kTestBrowserDmToken);
}

TEST_F(ConnectorsServiceTest, ConnectorsEnabled) {
  ASSERT_TRUE(
      ConnectorsServiceFactory::GetForProfile(profile())->ConnectorsEnabled());
  ASSERT_FALSE(ConnectorsServiceFactory::GetForProfile(
                   profile()->GetOffTheRecordProfile())
                   ->ConnectorsEnabled());
  ASSERT_TRUE(ConnectorsService(profile()).ConnectorsEnabled());
  ASSERT_FALSE(ConnectorsService(profile()->GetOffTheRecordProfile())
                   .ConnectorsEnabled());
}

TEST_F(ConnectorsServiceTest, RealTimeUrlCheck) {
  auto service = ConnectorsService(profile());

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kPolicyDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);

  profile()->GetPrefs()->SetInteger(
      kEnterpriseRealTimeUrlCheckMode,
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_MACHINE);
  ASSERT_TRUE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(*service.GetDMTokenForRealTimeUrlCheck(), kTestBrowserDmToken);
  ASSERT_EQ(
      service.GetAppliedRealTimeUrlCheck(),
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_USER);
  ASSERT_TRUE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(*service.GetDMTokenForRealTimeUrlCheck(), kTestProfileDmToken);
  ASSERT_EQ(
      service.GetAppliedRealTimeUrlCheck(),
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
}

TEST_F(ConnectorsServiceTest, RealTimeUrlCheck_OffTheRecord) {
  auto service = ConnectorsService(profile()->GetOffTheRecordProfile());

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kConnectorsDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);

  profile()->GetPrefs()->SetInteger(
      kEnterpriseRealTimeUrlCheckMode,
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_MACHINE);
  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kConnectorsDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);

  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_USER);
  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetDMTokenForRealTimeUrlCheck().error(),
            ConnectorsServiceBase::NoDMTokenForRealTimeUrlCheckReason::
                kConnectorsDisabled);
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);
}

TEST_F(ConnectorsServiceTest, ReportingSettings) {
  auto service = ConnectorsService(profile());

  EXPECT_FALSE(service.GetReportingSettings());
  EXPECT_TRUE(service.GetReportingServiceProviderNames().empty());

  test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/true);

  auto settings = service.GetReportingSettings();
  EXPECT_TRUE(settings.has_value());
  EXPECT_FALSE(settings->per_profile);
  EXPECT_EQ(settings->dm_token, kTestBrowserDmToken);
  EXPECT_EQ(settings->enabled_event_names,
            std::set<std::string>(kAllReportingEnabledEvents.begin(),
                                  kAllReportingEnabledEvents.end()));
  EXPECT_TRUE(settings->enabled_opt_in_events.empty());
  auto provider_names = service.GetReportingServiceProviderNames();
  EXPECT_EQ(provider_names, std::vector<std::string>({"google"}));

  test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), /*enabled=*/true, /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{}, /*machine_scope=*/false);

  settings = service.GetReportingSettings();
  EXPECT_TRUE(settings.has_value());
  EXPECT_TRUE(settings->per_profile);
  EXPECT_EQ(settings->dm_token, kTestProfileDmToken);
  EXPECT_EQ(settings->enabled_event_names,
            std::set<std::string>(kAllReportingEnabledEvents.begin(),
                                  kAllReportingEnabledEvents.end()));
  EXPECT_TRUE(settings->enabled_opt_in_events.empty());
  provider_names = service.GetReportingServiceProviderNames();
  EXPECT_EQ(provider_names, std::vector<std::string>({"google"}));
}

TEST_F(ConnectorsServiceTest, ReportingSettings_OffTheRecord) {
  auto service = ConnectorsService(profile()->GetOffTheRecordProfile());

  EXPECT_FALSE(service.GetReportingSettings());
  EXPECT_TRUE(service.GetReportingServiceProviderNames().empty());

  test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/true);

  EXPECT_FALSE(service.GetReportingSettings());
  EXPECT_TRUE(service.GetReportingServiceProviderNames().empty());

  test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), /*enabled=*/true, /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{}, /*machine_scope=*/false);

  EXPECT_FALSE(service.GetReportingSettings());
  EXPECT_TRUE(service.GetReportingServiceProviderNames().empty());
}

TEST_F(ConnectorsServiceTest, GetManagementDomain_UrlFilteringEnabled) {
  auto service = ConnectorsService(profile());

  ASSERT_EQ(service.GetManagementDomain(), std::string());

  base::test::ScopedFeatureList feature(kIOSEnterpriseRealtimeUrlFiltering);
  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_USER);

  MakePrimaryAccountAvailable(kTestProfileEmail);

  ASSERT_EQ(service.GetManagementDomain(), kTestProfileDomain);

  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_MACHINE);

  ASSERT_EQ(service.GetManagementDomain(), kTestMachineDomain);
}

TEST_F(ConnectorsServiceTest, GetManagementDomain_EventReportingEnabled) {
  auto service = ConnectorsService(profile());

  ASSERT_EQ(service.GetManagementDomain(), std::string());

  base::test::ScopedFeatureList feature(kEnterpriseRealtimeEventReportingOnIOS);
  profile()->GetPrefs()->SetInteger(kOnSecurityEventScopePref,
                                    policy::POLICY_SCOPE_USER);

  MakePrimaryAccountAvailable(kTestProfileEmail);

  ASSERT_EQ(service.GetManagementDomain(), kTestProfileDomain);

  profile()->GetPrefs()->SetInteger(kOnSecurityEventScopePref,
                                    policy::POLICY_SCOPE_MACHINE);

  ASSERT_EQ(service.GetManagementDomain(), kTestMachineDomain);
}

TEST_F(ConnectorsServiceTest, GetManagementDomain_MachinePolicyHasPrecedence) {
  auto service = ConnectorsService(profile());

  ASSERT_EQ(service.GetManagementDomain(), std::string());

  base::test::ScopedFeatureList feature;
  feature.InitWithFeatures(
      /*enabled_features=*/{kEnterpriseRealtimeEventReportingOnIOS,
                            kIOSEnterpriseRealtimeUrlFiltering},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetInteger(kOnSecurityEventScopePref,
                                    policy::POLICY_SCOPE_USER);
  profile()->GetPrefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                                    policy::POLICY_SCOPE_MACHINE);

  MakePrimaryAccountAvailable(kTestProfileEmail);

  ASSERT_EQ(service.GetManagementDomain(), kTestMachineDomain);
}

TEST_F(ConnectorsServiceTest, GetManagementDomain_OffTheRecord) {
  auto service = ConnectorsService(profile()->GetOffTheRecordProfile());

  ASSERT_EQ(service.GetManagementDomain(), std::string());
}

}  // namespace enterprise_connectors
