// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/reporting/profile_report_generator_ios.h"
#import "components/enterprise/browser/reporting/report_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/schema_registry.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector_mock.h"
#import "ios/chrome/browser/policy/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

const base::FilePath kProfilePath = base::FilePath("/fake/profile/default");
const std::string kAccount = "fake_account";

}  // namespace

class ProfileReportGeneratorIOSTest : public PlatformTest {
 public:
  ProfileReportGeneratorIOSTest() : generator_(&delegate_factory_) {
    TestChromeBrowserState::Builder builder;
    builder.SetPath(kProfilePath);
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    InitMockPolicyService();
    builder.SetPolicyConnector(
        std::make_unique<BrowserStatePolicyConnectorMock>(
            std::move(policy_service_), &schema_registry_));

    InitPolicyMap();

    scoped_browser_state_manager_ =
        std::make_unique<IOSChromeScopedTestingChromeBrowserStateManager>(
            std::make_unique<TestChromeBrowserStateManager>(builder.Build()));

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        GetBrowserState(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    authentication_service_ =
        AuthenticationServiceFactory::GetForBrowserState(GetBrowserState());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            GetBrowserState());
  }

  ProfileReportGeneratorIOSTest(const ProfileReportGeneratorIOSTest&) = delete;
  ProfileReportGeneratorIOSTest& operator=(
      const ProfileReportGeneratorIOSTest&) = delete;
  ~ProfileReportGeneratorIOSTest() override = default;

  void InitMockPolicyService() {
    policy_service_ = std::make_unique<policy::MockPolicyService>();

    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  void InitPolicyMap() {
    policy_map_.Set("kPolicyName1", policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value(base::Value::List()), nullptr);
    policy_map_.Set("kPolicyName2", policy::POLICY_LEVEL_RECOMMENDED,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                    base::Value(true), nullptr);
  }

  void SignIn() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentities(
        @[ base::SysUTF8ToNSString(kAccount) ]);
    id<SystemIdentity> identity =
        account_manager_service_->GetDefaultIdentity();
    authentication_service_->SignIn(identity);
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport() {
    std::unique_ptr<em::ChromeUserProfileInfo> report =
        generator_.MaybeGenerate(kProfilePath,
                                 kProfilePath.BaseName().AsUTF8Unsafe(),
                                 ReportType::kFull);

    if (!report)
      return nullptr;

    EXPECT_EQ(kProfilePath.BaseName().AsUTF8Unsafe(), report->name());
    EXPECT_EQ(kProfilePath.AsUTF8Unsafe(), report->id());
    EXPECT_TRUE(report->is_detail_available());

    return report;
  }

  ChromeBrowserState* GetBrowserState() {
    return GetApplicationContext()
        ->GetChromeBrowserStateManager()
        ->GetLastUsedBrowserState();
  }

  ReportingDelegateFactoryIOS delegate_factory_;
  ProfileReportGenerator generator_;

 private:
  web::WebTaskEnvironment task_environment_;

  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::SchemaRegistry schema_registry_;
  policy::PolicyMap policy_map_;
  std::unique_ptr<IOSChromeScopedTestingChromeBrowserStateManager>
      scoped_browser_state_manager_;
  AuthenticationService* authentication_service_;
  ChromeAccountManagerService* account_manager_service_;
};

TEST_F(ProfileReportGeneratorIOSTest, UnsignedInProfile) {
  auto report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_FALSE(report->has_chrome_signed_in_user());
}

TEST_F(ProfileReportGeneratorIOSTest, SignedInProfile) {
  SignIn();
  auto report = GenerateReport();
  ASSERT_TRUE(report);
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(kAccount + "@gmail.com", report->chrome_signed_in_user().email());
  EXPECT_EQ(kAccount + "_hashID",
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
