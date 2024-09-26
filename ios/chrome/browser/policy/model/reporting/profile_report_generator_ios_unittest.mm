// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/profile_report_generator_ios.h"

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "components/enterprise/browser/reporting/report_type.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector_mock.h"
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
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

class ProfileReportGeneratorIOSTest : public PlatformTest {
 public:
  ProfileReportGeneratorIOSTest() : generator_(&delegate_factory_) {
    InitPolicyMap();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.SetPolicyConnector(
        std::make_unique<BrowserStatePolicyConnectorMock>(
            CreateMockPolicyService(), &schema_registry_));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
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

  void InitPolicyMap() {
    policy_map_.Set("kPolicyName1", policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value(base::Value::List()), nullptr);
    policy_map_.Set("kPolicyName2", policy::POLICY_LEVEL_RECOMMENDED,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                    base::Value(true), nullptr);
  }

  FakeSystemIdentity* SignIn() {
    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager->AddIdentity(fake_identity);
    authentication_service_->SignIn(
        fake_identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
    return fake_identity;
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport() {
    const base::FilePath path = profile_->GetStatePath();
    const std::string& name = GetProfileName();
    std::unique_ptr<em::ChromeUserProfileInfo> report =
        generator_.MaybeGenerate(path, name, ReportType::kFull);

    if (!report)
      return nullptr;

    EXPECT_EQ(name, report->name());
    EXPECT_EQ(path.AsUTF8Unsafe(), report->id());
    EXPECT_TRUE(report->is_detail_available());

    return report;
  }

  const std::string& GetProfileName() const {
    return profile_->GetProfileName();
  }

  ReportingDelegateFactoryIOS delegate_factory_;
  ProfileReportGenerator generator_;

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;

  policy::SchemaRegistry schema_registry_;
  policy::PolicyMap policy_map_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
};

TEST_F(ProfileReportGeneratorIOSTest, UnsignedInProfile) {
  auto report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_FALSE(report->has_chrome_signed_in_user());
}

TEST_F(ProfileReportGeneratorIOSTest, SignedInProfile) {
  FakeSystemIdentity* fake_identity = SignIn();
  auto report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(base::SysNSStringToUTF8(fake_identity.userEmail),
            report->chrome_signed_in_user().email());
  EXPECT_EQ(base::SysNSStringToUTF8(fake_identity.gaiaID),
            report->chrome_signed_in_user().obfuscated_gaia_id());
}

TEST_F(ProfileReportGeneratorIOSTest, PoliciesReportedOnlyWhenEnabled) {
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

}  // namespace enterprise_reporting
