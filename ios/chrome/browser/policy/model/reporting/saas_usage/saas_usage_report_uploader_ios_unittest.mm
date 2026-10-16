// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_report_uploader_ios.h"

#import <memory>
#import <string>

#import "base/functional/callback_helpers.h"
#import "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#import "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "ios/chrome/browser/policy/model/reporting/real_time_event_uploader_ios_test_base.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

using ::testing::_;

struct SaasUsageReportUploaderIOSTestParam {
  std::string test_name;
  bool is_browser_managed;
  bool is_profile_managed;
  bool is_affiliated;
  bool is_profile_report_uploader;
  bool create_reporting_client;
  std::string expected_dm_token;
  bool expected_per_profile;
  bool expect_report_upload;
};

::chrome::cros::reporting::proto::SaasUsageReportEvent BuildReportEvent() {
  ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
  auto* domain_metrics = report.add_domain_metrics();
  domain_metrics->set_domain("example.com");
  domain_metrics->set_visit_count(1);
  domain_metrics->set_start_time_millis(1000);
  domain_metrics->set_end_time_millis(2000);
  domain_metrics->add_encryption_protocols("TLS 1.3");
  return report;
}

}  // namespace

class SaasUsageReportUploaderIOSParamTest
    : public RealTimeEventUploaderIOSTestBase,
      public testing::WithParamInterface<SaasUsageReportUploaderIOSTestParam> {
};

TEST_P(SaasUsageReportUploaderIOSParamTest, UploadReport) {
  const auto& param = GetParam();
  SetBrowserManaged(param.is_browser_managed, param.is_affiliated);
  ProfileIOS* profile = CreatePersonalProfile(
      /*create_reporting_client=*/!param.is_profile_managed &&
      param.create_reporting_client);
  if (param.is_profile_managed) {
    profile = CreateManagedProfile([FakeSystemIdentity fakeManagedIdentity],
                                   param.is_affiliated,
                                   param.create_reporting_client);
  }

  if (param.expect_report_upload) {
    EXPECT_CALL(*GetMockClient(profile),
                ReportSaasUsageEvent(_, param.expected_per_profile,
                                     param.expected_dm_token, _));
  } else if (param.create_reporting_client) {
    EXPECT_CALL(*GetMockClient(profile), ReportSaasUsageEvent(_, _, _, _))
        .Times(0);
  }

  std::unique_ptr<SaasUsageReportUploaderIOS> uploader;
  if (param.is_profile_report_uploader) {
    uploader = std::make_unique<SaasUsageReportUploaderIOS>(profile);
  } else {
    uploader = std::make_unique<SaasUsageReportUploaderIOS>();
  }
  uploader->UploadReport(BuildReportEvent(), base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SaasUsageReportUploaderIOSParamTest,
    testing::Values(
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadBrowserReport_UnmanagedProfile",
            .is_browser_managed = true,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .create_reporting_client = true,
            .expected_dm_token = "browser_dm_token",
            .expected_per_profile = false,
            .expect_report_upload = true},
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadBrowserReport_ManagedProfile",
            .is_browser_managed = true,
            .is_profile_managed = true,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .create_reporting_client = true,
            .expected_dm_token = "browser_dm_token",
            .expected_per_profile = false,
            .expect_report_upload = true},
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadProfileReport_Unaffiliated",
            .is_browser_managed = true,
            .is_profile_managed = true,
            .is_affiliated = false,
            .is_profile_report_uploader = true,
            .create_reporting_client = true,
            .expected_dm_token =
                RealTimeEventUploaderIOSTestBase::kTestUserDmToken,
            .expected_per_profile = true,
            .expect_report_upload = true},
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadProfileReport_Affiliated",
            .is_browser_managed = true,
            .is_profile_managed = true,
            .is_affiliated = true,
            .is_profile_report_uploader = true,
            .create_reporting_client = true,
            .expected_dm_token = "browser_dm_token",
            .expected_per_profile = false,
            .expect_report_upload = true},
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadBrowserReport_NoReportingClient",
            .is_browser_managed = true,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .create_reporting_client = false,
            .expected_dm_token = "",
            .expected_per_profile = false,
            .expect_report_upload = false},
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadBrowserReport_NoDMToken",
            .is_browser_managed = false,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = false,
            .create_reporting_client = true,
            .expected_dm_token = "",
            .expected_per_profile = false,
            .expect_report_upload = false},
        SaasUsageReportUploaderIOSTestParam{
            .test_name = "UploadProfileReport_Unmanaged",
            .is_browser_managed = true,
            .is_profile_managed = false,
            .is_affiliated = false,
            .is_profile_report_uploader = true,
            .create_reporting_client = true,
            .expected_dm_token = "",
            .expected_per_profile = false,
            .expect_report_upload = false}),
    [](const testing::TestParamInfo<
        SaasUsageReportUploaderIOSParamTest::ParamType>& info) {
      return info.param.test_name;
    });

class SaasUsageReportUploaderIOSTest : public RealTimeEventUploaderIOSTestBase {
};

TEST_F(SaasUsageReportUploaderIOSTest, UploadBrowserReport_MultiProfile) {
  SetBrowserManaged(true);

  ProfileIOS* personal_profile =
      CreatePersonalProfile(/*create_reporting_client=*/true);
  ProfileIOS* managed_profile =
      CreateManagedProfile([FakeSystemIdentity fakeManagedIdentity],
                           /*is_affiliated=*/false,
                           /*create_reporting_client=*/true);

  // `SaasUsageReportUploaderIOS` picks an arbitrary loaded profile from
  // `GetLoadedProfiles()` for browser-level reporting. Verify that the report
  // is uploaded exactly once across the loaded profiles without relying on the
  // order returned by `GetLoadedProfiles()`.
  int report_count = 0;
  auto on_report =
      [&report_count](
          auto, bool, const std::string&,
          base::OnceCallback<void(policy::CloudPolicyClient::Result)>
              callback) {
        ++report_count;
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      };

  EXPECT_CALL(
      *GetMockClient(personal_profile),
      ReportSaasUsageEvent(_, /*per_profile=*/false, "browser_dm_token", _))
      .Times(testing::AtMost(1))
      .WillRepeatedly(on_report);
  EXPECT_CALL(
      *GetMockClient(managed_profile),
      ReportSaasUsageEvent(_, /*per_profile=*/false, "browser_dm_token", _))
      .Times(testing::AtMost(1))
      .WillRepeatedly(on_report);

  auto uploader = std::make_unique<SaasUsageReportUploaderIOS>();
  uploader->UploadReport(BuildReportEvent(), base::DoNothing());
  EXPECT_EQ(report_count, 1);
}

TEST_F(SaasUsageReportUploaderIOSTest, UploadProfileReport_MultiProfile) {
  CreatePersonalProfile(/*create_reporting_client=*/false);
  ProfileIOS* managed_profile1 =
      CreateManagedProfile([FakeSystemIdentity fakeManagedIdentity],
                           /*is_affiliated=*/false,
                           /*create_reporting_client=*/true, "user_dm_token_1");
  ProfileIOS* managed_profile2 = CreateManagedProfile(
      [FakeSystemIdentity identityWithEmail:@"bar@foo.com"],
      /*is_affiliated=*/false,
      /*create_reporting_client=*/true, "user_dm_token_2");

  auto* mock_client1 = GetMockClient(managed_profile1);
  auto* mock_client2 = GetMockClient(managed_profile2);

  EXPECT_CALL(*mock_client1, ReportSaasUsageEvent(_, /*per_profile=*/true,
                                                  "user_dm_token_1", _));
  EXPECT_CALL(*mock_client2, ReportSaasUsageEvent(_, _, _, _)).Times(0);

  auto uploader =
      std::make_unique<SaasUsageReportUploaderIOS>(managed_profile1);
  uploader->UploadReport(BuildReportEvent(), base::DoNothing());
}

}  // namespace enterprise_reporting
