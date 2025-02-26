// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

namespace enterprise_connectors {

class IOSRealtimeReportingClientTest : public PlatformTest {
 public:
  IOSRealtimeReportingClientTest() {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  // Add local state to test ApplicationContext. Required by
  // TestProfileManagerIOS.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
};

// Tests that the GetProfileUserName() returns the expected value.
TEST_F(IOSRealtimeReportingClientTest, ReturnsProfileUserName) {
  IOSRealtimeReportingClient client(profile_);
  ASSERT_EQ(client.GetProfileUserName(), std::string());
}

}  // namespace enterprise_connectors
