// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"

#import "base/memory/raw_ptr.h"
#import "base/test/gmock_move_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#import "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;

namespace enterprise_connectors {
inline constexpr char kUploadSuccess[] =
    "Enterprise.ReportingEventUploadSuccess";
inline constexpr char kUploadFailure[] =
    "Enterprise.ReportingEventUploadFailure";

class IOSRealtimeReportingClientTest
    : public PlatformTest,
      public testing::WithParamInterface<bool> {
 public:
  IOSRealtimeReportingClientTest() {}
  void SetUp() override {
    PlatformTest::SetUp();

    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    // Set a mock cloud policy client.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    reporting_client_ =
        IOSRealtimeReportingClientFactory::GetForProfile(profile_);
  }

 protected:
  // Indicates if the event reported is browser or profile based.
  bool is_profile_reporting() { return GetParam(); }

  // Set up the cloudPolicyClient based on if the it's profile based or browser
  // based.
  void SetCloudPolicyClient(bool per_profile) {
    per_profile ? reporting_client_->SetProfileCloudPolicyClientForTesting(
                      client_.get())
                : reporting_client_->SetBrowserCloudPolicyClientForTesting(
                      client_.get());
  }

  web::WebTaskEnvironment task_environment_;
  // Add local state to test ApplicationContext. Required by
  // TestProfileManagerIOS.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  raw_ptr<IOSRealtimeReportingClient> reporting_client_;
  base::HistogramTester histogram_;
  policy::CloudPolicyClient::ResultCallback upload_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that uploading events succeed using the dictionary mapping the events.
TEST_P(IOSRealtimeReportingClientTest, TestDeprecatedUmaEventUploadSucceeds) {
  SetCloudPolicyClient(is_profile_reporting());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  base::Value::Dict event;

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEventReport(_, _, _))
      .WillOnce([&](bool include_device_info, base::Value::Dict&& report,
                    policy::CloudPolicyClient::ResultCallback callback) {
        upload_callback_ = std::move(callback);
        run_loop.Quit();
      });
  reporting_client_->ReportRealtimeEvent(kKeyLoginEvent, std::move(settings),
                                         std::move(event));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));

  histogram_.ExpectUniqueSample(kUploadSuccess,
                                EnterpriseReportingEventType::kLoginEvent, 1);
  histogram_.ExpectTotalCount(kUploadFailure, 0);
}

// Tests that uploading events succeed using the new reporting events proto.
TEST_P(IOSRealtimeReportingClientTest, TestUmaEventUploadSucceeds) {
  base::test::ScopedFeatureList scoped_feature_list(
      policy::kUploadRealtimeReportingEventsUsingProto);

  SetCloudPolicyClient(is_profile_reporting());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  ::chrome::cros::reporting::proto::Event login_event;
  login_event.mutable_login_event()->set_url("login/url");

  EXPECT_EQ(
      EnterpriseReportingEventType::kLoginEvent,
      enterprise_connectors::GetUmaEnumFromEventCase(login_event.event_case()));
  EXPECT_EQ(::chrome::cros::reporting::proto::Event::EventCase::kLoginEvent,
            login_event.event_case());

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEvent(_, _, _))
      .WillOnce(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          });
  reporting_client_->ReportEvent(std::move(login_event), std::move(settings));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));

  histogram_.ExpectUniqueSample(kUploadSuccess,
                                EnterpriseReportingEventType::kLoginEvent, 1);
  histogram_.ExpectTotalCount(kUploadFailure, 0);
}

// Tests that uploading events fails using the dictionary mapping the events.
TEST_P(IOSRealtimeReportingClientTest, TestDeprecatedUmaEventUploadFails) {
  SetCloudPolicyClient(is_profile_reporting());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  base::Value::Dict event;

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEventReport(_, _, _))
      .WillOnce([&](bool include_device_info, base::Value::Dict&& report,
                    policy::CloudPolicyClient::ResultCallback callback) {
        upload_callback_ = std::move(callback);
        run_loop.Quit();
      });
  reporting_client_->ReportRealtimeEvent(kKeyLoginEvent, std::move(settings),
                                         std::move(event));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_REQUEST_FAILED));

  histogram_.ExpectUniqueSample(kUploadFailure,
                                EnterpriseReportingEventType::kLoginEvent, 1);
  histogram_.ExpectTotalCount(kUploadSuccess, 0);
}

// Tests that uploading events fail using the new reporting events proto.
TEST_P(IOSRealtimeReportingClientTest, TestUmaEventUploadFails) {
  base::test::ScopedFeatureList scoped_feature_list(
      policy::kUploadRealtimeReportingEventsUsingProto);
  SetCloudPolicyClient(is_profile_reporting());

  ReportingSettings settings;
  settings.per_profile = is_profile_reporting();
  ::chrome::cros::reporting::proto::Event login_event;
  login_event.mutable_login_event()->set_url("login/url");

  EXPECT_EQ(
      EnterpriseReportingEventType::kLoginEvent,
      enterprise_connectors::GetUmaEnumFromEventCase(login_event.event_case()));
  EXPECT_EQ(::chrome::cros::reporting::proto::Event::EventCase::kLoginEvent,
            login_event.event_case());

  base::RunLoop run_loop;
  EXPECT_CALL(*client_.get(), UploadSecurityEvent(_, _, _))
      .WillOnce(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest&& request,
              policy::CloudPolicyClient::ResultCallback callback) {
            upload_callback_ = std::move(callback);
            run_loop.Quit();
          });
  reporting_client_->ReportEvent(std::move(login_event), std::move(settings));
  run_loop.Run();

  ASSERT_TRUE(upload_callback_);
  std::move(upload_callback_)
      .Run(policy::CloudPolicyClient::Result(policy::DM_STATUS_REQUEST_FAILED));

  histogram_.ExpectUniqueSample(kUploadFailure,
                                EnterpriseReportingEventType::kLoginEvent, 1);
  histogram_.ExpectTotalCount(kUploadSuccess, 0);
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    IOSRealtimeReportingClientTest,
    /* is_profile_reporting */ testing::Bool());

// Tests that all events names are included.
TEST_F(IOSRealtimeReportingClientTest,
       TestEventNameToUmaEnumMapIncludesAllEvents) {
  std::set<std::string> all_reporting_events;
  all_reporting_events.insert(kAllReportingEnabledEvents.begin(),
                              kAllReportingEnabledEvents.end());
  all_reporting_events.insert(kAllReportingOptInEvents.begin(),
                              kAllReportingOptInEvents.end());

  EXPECT_EQ(all_reporting_events.size(), kEventNameToUmaEnumMap.size());
  for (std::string eventName : all_reporting_events) {
    EXPECT_TRUE(kEventNameToUmaEnumMap.contains(eventName));
  }
}

// Tests that unknown event names are mapped to "non-existent-event-name".
TEST_F(IOSRealtimeReportingClientTest,
       TestUnknownEventNameMapsTokUnknownEvent) {
  EXPECT_EQ(GetUmaEnumFromEventName("non-existent-event-name"),
            EnterpriseReportingEventType::kUnknownEvent);
}

// Tests that the GetProfileUserName() returns the expected value.
TEST_F(IOSRealtimeReportingClientTest, ReturnsProfileUserName) {
  IOSRealtimeReportingClient client(profile_);
  ASSERT_EQ(client.GetProfileUserName(), std::string());
}

}  // namespace enterprise_connectors
