// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import "base/files/file_path.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/enterprise/browser/reporting/report_request.h"
#import "components/policy/core/common/cloud/cloud_policy_util.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/schema_registry.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector_mock.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

class ReportGeneratorIOSTest : public PlatformTest {
 public:
  ReportGeneratorIOSTest() : generator_(&delegate_factory_) {
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
  }

  ReportGeneratorIOSTest(const ReportGeneratorIOSTest&) = delete;
  ReportGeneratorIOSTest& operator=(const ReportGeneratorIOSTest&) = delete;
  ~ReportGeneratorIOSTest() override = default;

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

  std::vector<std::unique_ptr<ReportRequest>> GenerateRequests() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    base::RunLoop run_loop;
    std::vector<std::unique_ptr<ReportRequest>> reqs;
    generator_.Generate(ReportType::kFull,
                        base::BindLambdaForTesting(
                            [&run_loop, &reqs](ReportRequestQueue requests) {
                              while (!requests.empty()) {
                                reqs.push_back(std::move(requests.front()));
                                requests.pop();
                              }
                              run_loop.Quit();
                            }));
    run_loop.Run();
    VerifyMetrics(reqs);
    return reqs;
  }

  void VerifyMetrics(std::vector<std::unique_ptr<ReportRequest>>& rets) {
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingRequestCount", rets.size(), 1);
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingBasicRequestSize",
        /*basic request size floor to KB*/ 0, 1);
  }

  base::FilePath GetProfilePath() { return profile_->GetStatePath(); }

  const std::string& GetProfileName() { return profile_->GetProfileName(); }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;

  ReportingDelegateFactoryIOS delegate_factory_;
  ReportGenerator generator_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  policy::SchemaRegistry schema_registry_;
  policy::PolicyMap policy_map_;

};

TEST_F(ReportGeneratorIOSTest, GenerateBasicReport) {
  auto requests = GenerateRequests();
  EXPECT_EQ(1u, requests.size());

  // Verify the basic request
  auto* basic_request = requests[0].get();

  EXPECT_NE(std::string(),
            basic_request->GetDeviceReportRequest().computer_name());
  EXPECT_EQ(std::string(),
            basic_request->GetDeviceReportRequest().serial_number());
  EXPECT_EQ(policy::GetBrowserDeviceIdentifier()->SerializePartialAsString(),
            basic_request->GetDeviceReportRequest()
                .browser_device_identifier()
                .SerializePartialAsString());
  EXPECT_NE(std::string(),
            basic_request->GetDeviceReportRequest().device_model());
  EXPECT_NE(std::string(),
            basic_request->GetDeviceReportRequest().brand_name());

  // Verify the OS report
  EXPECT_TRUE(basic_request->GetDeviceReportRequest().has_os_report());
  auto& os_report = basic_request->GetDeviceReportRequest().os_report();
  EXPECT_NE(std::string(), os_report.name());
  EXPECT_NE(std::string(), os_report.arch());
  EXPECT_NE(std::string(), os_report.version());

  // Verify the browser report
  EXPECT_TRUE(basic_request->GetDeviceReportRequest().has_browser_report());
  auto& browser_report =
      basic_request->GetDeviceReportRequest().browser_report();
  EXPECT_NE(std::string(), browser_report.browser_version());
  EXPECT_TRUE(browser_report.has_channel());
  EXPECT_NE(std::string(), browser_report.executable_path());

  // Verify the profile report
  EXPECT_EQ(1, browser_report.chrome_user_profile_infos_size());
  auto profile_info = browser_report.chrome_user_profile_infos(0);
  EXPECT_EQ(GetProfilePath().AsUTF8Unsafe(), profile_info.id());
  EXPECT_EQ(GetProfileName(), profile_info.name());
  EXPECT_TRUE(profile_info.has_is_detail_available());
  EXPECT_TRUE(profile_info.is_detail_available());
  EXPECT_EQ(2, profile_info.chrome_policies_size());
}

}  // namespace enterprise_reporting
