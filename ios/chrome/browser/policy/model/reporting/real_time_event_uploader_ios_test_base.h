// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REAL_TIME_EVENT_UPLOADER_IOS_TEST_BASE_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REAL_TIME_EVENT_UPLOADER_IOS_TEST_BASE_H_

#import <memory>
#import <string>

#import "base/functional/callback.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/schema_registry.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ProfileIOS;

namespace policy {
class MachineLevelUserCloudPolicyManager;
}  // namespace policy

namespace enterprise_reporting {

// Mock implementation of IOSRealtimeReportingClient for testing.
class MockIOSRealtimeReportingClient
    : public enterprise_connectors::IOSRealtimeReportingClient {
 public:
  explicit MockIOSRealtimeReportingClient(ProfileIOS* profile);
  ~MockIOSRealtimeReportingClient() override;

  MOCK_METHOD(void,
              ReportSaasUsageEvent,
              (::chrome::cros::reporting::proto::Event event,
               bool per_profile,
               const std::string& dm_token,
               base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                   upload_callback),
              (override));

  MOCK_METHOD(void,
              ReportBrowserLaunchEvent,
              (::chrome::cros::reporting::proto::Event event,
               bool per_profile,
               const std::string& dm_token,
               base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                   upload_callback),
              (override));
};

// Base class for unit tests of real-time event uploaders on iOS.
// Provides helpers for creating managed profiles and mocking reporting clients.
class RealTimeEventUploaderIOSTestBase : public PlatformTest {
 public:
  static constexpr char kTestDmToken[] = "browser_dm_token";

  RealTimeEventUploaderIOSTestBase();
  ~RealTimeEventUploaderIOSTestBase() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  // Configures the machine to be managed with the given DM token.
  void SetBrowserManaged(bool is_managed,
                         bool set_affiliation = false,
                         const std::string& dm_token = kTestDmToken);

  // Creates a testing profile with optional management and affiliation.
  ProfileIOS* CreateProfile(const std::string& name,
                            bool is_managed,
                            bool is_affiliated,
                            bool create_reporting_client);

  // Returns the mock reporting client for the given profile.
  MockIOSRealtimeReportingClient* GetMockClient(ProfileIOS* profile);

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  policy::SchemaRegistry schema_registry_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
  std::unique_ptr<policy::MachineLevelUserCloudPolicyManager>
      machine_policy_manager_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_REAL_TIME_EVENT_UPLOADER_IOS_TEST_BASE_H_
