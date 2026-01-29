// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/profile_report_generator_ios.h"

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/enterprise/browser/reporting/report_generation_config.h"
#import "components/enterprise/browser/reporting/report_type.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/cloud_policy_service.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
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
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

constexpr char kFakeEnrollmentToken[] = "enrollment-token";
constexpr char kFakeBrowserClientId[] = "client-id";
constexpr char kFakeAffiliationId[] = "affiliation-id";
constexpr char kFakeProfileId[] = "profile-id";
constexpr char kFakeBrowserDmToken[] = "browser_dm_token";
constexpr char kFakeMachineDomain[] = "example.com";

constexpr char kFakeEmail[] = "foo@example.com";
constexpr char kFakeHostedDomain[] = "example.com";
constexpr char kFakeFullName[] = "Full Name";
constexpr char kFakeGivenName[] = "Full";
constexpr char kFakeLocale[] = "en-US";

std::unique_ptr<KeyedService> CreateProfileIdService(ProfileIOS* profile) {
  return std::make_unique<enterprise::ProfileIdService>(kFakeProfileId);
}

enum class ProfileReporting {
  kEnabled,
  kDisabled,
};

enum class ProfileName {
  kProfileName,
  kEmail,
};

struct TestParams {
  ProfileReporting profile_reporting;
  ProfileName profile_name;
};

}  // namespace

enum class Affiliation {
  kAffiliated,
  kNotAffiliated,
};

class ProfileReportGeneratorIOSTest
    : public PlatformTest,
      public testing::WithParamInterface<TestParams> {
 public:
  ProfileReportGeneratorIOSTest() : generator_(&delegate_factory_) {}

  void TearDown() override {
    GetApplicationContext()
        ->GetBrowserPolicyConnector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(/*manager=*/nullptr);
  }

  void Init(Affiliation affiliation) {
    base::flat_map<base::test::FeatureRef, bool> feature_states;
    feature_states[enterprise_reporting::kCloudProfileReporting] =
        IsProfileReportingEnabled();
    feature_states[enterprise_reporting::kUseEmailAsProfileName] =
        ShouldUseEmailAsProfileName();
    feature_list_.InitWithFeatureStates(feature_states);

    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kEnableChromeBrowserCloudManagement);

    if (affiliation == Affiliation::kAffiliated) {
      InitDeviceAffiliation();
    }

    InitPolicyMap();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        enterprise::ProfileIdServiceFactoryIOS::GetInstance(),
        base::BindRepeating(&CreateProfileIdService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    if (affiliation == Affiliation::kAffiliated) {
      InitProfileAffiliation();
    }
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
        CreateMockPolicyService(), &schema_registry_, policy_store_.get()));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

  ProfileReportGeneratorIOSTest(const ProfileReportGeneratorIOSTest&) = delete;
  ProfileReportGeneratorIOSTest& operator=(
      const ProfileReportGeneratorIOSTest&) = delete;
  ~ProfileReportGeneratorIOSTest() override = default;

  std::unique_ptr<policy::MockPolicyService> CreateMockPolicyService() {
    auto policy_service = std::make_unique<policy::MockPolicyService>();

    ON_CALL(*policy_service.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));

    return policy_service;
  }

  bool IsProfileReportingEnabled() const {
    return GetParam().profile_reporting == ProfileReporting::kEnabled;
  }

  bool ShouldUseEmailAsProfileName() const {
    return GetParam().profile_name == ProfileName::kEmail;
  }

  void InitPolicyMap() {
    policy_map_.Set("kPolicyName1", policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value(base::ListValue()), nullptr);
    policy_map_.Set("kPolicyName2", policy::POLICY_LEVEL_RECOMMENDED,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                    base::Value(true), nullptr);
  }

  void InitDeviceAffiliation() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kFakeMachineDomain);
    policy_data->add_device_affiliation_ids(kFakeAffiliationId);
    policy_data->set_state(em::PolicyData::ACTIVE);

    browser_dm_token_storage_ =
        std::make_unique<policy::FakeBrowserDMTokenStorage>();
    browser_dm_token_storage_->SetEnrollmentToken(kFakeEnrollmentToken);
    browser_dm_token_storage_->SetClientId(kFakeBrowserClientId);
    browser_dm_token_storage_->EnableStorage(true);
    browser_dm_token_storage_->SetDMToken(kFakeBrowserDmToken);
    policy::BrowserDMTokenStorage::SetForTesting(
        browser_dm_token_storage_.get());

    auto machine_store =
        std::make_unique<policy::MachineLevelUserCloudPolicyStore>(
            policy::DMToken::CreateValidToken(kFakeBrowserDmToken),
            std::string(), base::FilePath(), base::FilePath(), base::FilePath(),
            base::FilePath(),
            policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
            scoped_refptr<base::SequencedTaskRunner>());
    machine_store->set_policy_data_for_testing(std::move(policy_data));

    machine_policy_manager_ =
        std::make_unique<policy::MachineLevelUserCloudPolicyManager>(
            std::move(machine_store), /*extension_install_store=*/nullptr,
            /*external_data_manager=*/nullptr,
            /*policy_dir=*/base::FilePath(),
            scoped_refptr<base::SequencedTaskRunner>(),
            network::TestNetworkConnectionTracker::CreateGetter());

    auto client = std::make_unique<policy::CloudPolicyClient>(
        /*service=*/nullptr, /*url_laoder_factory=*/nullptr,
        policy::CloudPolicyClient::DeviceDMTokenCallback());
    client->SetupRegistration(kFakeBrowserDmToken, "client-id",
                              {kFakeAffiliationId});
    machine_policy_manager_->core()->ConnectForTesting(
        /*service=*/nullptr, std::move(client));

    GetApplicationContext()
        ->GetBrowserPolicyConnector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(
            machine_policy_manager_.get());
  }

  void InitProfileAffiliation() {
    auto policy_data = std::make_unique<em::PolicyData>();
    policy_data->add_user_affiliation_ids(kFakeAffiliationId);
    policy_data->set_policy_type(
        policy::dm_protocol::GetChromeUserPolicyType());
    policy_data->set_state(em::PolicyData::ACTIVE);

    policy_store_ = std::make_unique<policy::MockCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    policy_store_->SetPolicy(std::move(policy_data));
  }

  AccountInfo SignIn() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, kFakeEmail, signin::ConsentLevel::kSignin);
    signin::SimulateSuccessfulFetchOfAccountInfo(
        identity_manager, account_info.account_id, account_info.email,
        account_info.gaia, kFakeHostedDomain, kFakeFullName, kFakeGivenName,
        kFakeLocale, "");
    return account_info;
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport() {
    const base::FilePath path = profile_->GetStatePath();
    base::test::TestFuture<std::unique_ptr<em::ChromeUserProfileInfo>>
        test_future;
    generator_.MaybeGenerate(path, ReportType::kFull,
                             SecuritySignalsMode::kSignalsAttached,
                             test_future.GetCallback());
    auto report = test_future.Take();

    if (!report) {
      return nullptr;
    }

    EXPECT_EQ(GetProfileName(), report->name());
    EXPECT_EQ(path.AsUTF8Unsafe(), report->id());
    EXPECT_TRUE(report->is_detail_available());

    return report;
  }

  std::string GetProfileName() const {
    if (ShouldUseEmailAsProfileName()) {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);
      if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        return std::string();
      }
      return kFakeFullName;
    }
    return profile_->GetProfileName();
  }

  ReportingDelegateFactoryIOS delegate_factory_;
  ProfileReportGenerator generator_;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine command_line_;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<policy::MockCloudPolicyStore> policy_store_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;

  policy::SchemaRegistry schema_registry_;
  policy::PolicyMap policy_map_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> browser_dm_token_storage_;
  std::unique_ptr<policy::MachineLevelUserCloudPolicyManager>
      machine_policy_manager_;
};

TEST_P(ProfileReportGeneratorIOSTest, UnsignedInProfile) {
  Init(Affiliation::kNotAffiliated);
  auto report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_FALSE(report->has_chrome_signed_in_user());
}

TEST_P(ProfileReportGeneratorIOSTest, SignedInProfile) {
  Init(Affiliation::kNotAffiliated);
  AccountInfo fake_identity = SignIn();
  auto report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(fake_identity.email, report->chrome_signed_in_user().email());
  EXPECT_EQ(fake_identity.gaia.ToString(),
            report->chrome_signed_in_user().obfuscated_gaia_id());
  EXPECT_EQ(GetProfileName(), report->name());
  EXPECT_NE(std::string(), report->name());
}

TEST_P(ProfileReportGeneratorIOSTest, PoliciesReportedOnlyWhenEnabled) {
  Init(Affiliation::kNotAffiliated);
  // Policies are reported by default.
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_EQ(2, report->chrome_policies_size());

  // Make sure policies are no longer reported when `set_policies_enabled` is
  // set to false.
  generator_.set_policies_enabled(false);
  report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_EQ(0, report->chrome_policies_size());

  // Make sure policies are once again being reported after setting
  // `set_policies_enabled` back to true.
  generator_.set_policies_enabled(true);
  report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_EQ(2, report->chrome_policies_size());
}

TEST_P(ProfileReportGeneratorIOSTest, ProfileId) {
  Init(Affiliation::kNotAffiliated);
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();
  if (IsProfileReportingEnabled()) {
    EXPECT_EQ(kFakeProfileId, report->profile_id());
  } else {
    EXPECT_EQ(std::string(), report->profile_id());
  }
}

TEST_P(ProfileReportGeneratorIOSTest, ProfileName) {
  Init(Affiliation::kNotAffiliated);
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();
  EXPECT_EQ(GetProfileName(), report->name());
}

TEST_P(ProfileReportGeneratorIOSTest, NotAffiliated) {
  Init(Affiliation::kNotAffiliated);
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();
  if (IsProfileReportingEnabled()) {
    EXPECT_TRUE(report->has_affiliation());
    EXPECT_FALSE(report->affiliation().is_affiliated());
    EXPECT_EQ(em::AffiliationState_UnaffiliationReason_USER_UNMANAGED,
              report->affiliation().unaffiliation_reason());
  } else {
    EXPECT_FALSE(report->has_affiliation());
  }
}

TEST_P(ProfileReportGeneratorIOSTest, Affiliated) {
  Init(Affiliation::kAffiliated);
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();
  if (IsProfileReportingEnabled()) {
    EXPECT_TRUE(report->has_affiliation());
    EXPECT_TRUE(report->affiliation().is_affiliated());
    EXPECT_FALSE(report->affiliation().has_unaffiliation_reason());
    EXPECT_EQ(em::AffiliationState_UnaffiliationReason_UNKNOWN,
              report->affiliation().unaffiliation_reason());
  } else {
    EXPECT_FALSE(report->has_affiliation());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileReportGeneratorIOSTest,
    testing::Values(
        TestParams{ProfileReporting::kDisabled, ProfileName::kProfileName},
        TestParams{ProfileReporting::kEnabled, ProfileName::kProfileName},
        TestParams{ProfileReporting::kDisabled, ProfileName::kEmail}),
    [](const testing::TestParamInfo<TestParams>& info) {
      return base::StringPrintf(
          "%s_%s",
          info.param.profile_reporting == ProfileReporting::kEnabled
              ? "ProfileReporting"
              : "NoProfileReporting",
          info.param.profile_name == ProfileName::kEmail ? "UseEmail"
                                                         : "NoUseEmail");
    });

}  // namespace enterprise_reporting
