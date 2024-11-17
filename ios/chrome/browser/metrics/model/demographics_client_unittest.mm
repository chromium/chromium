// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/demographics_client.h"

#import "base/time/default_clock.h"
#import "base/time/default_tick_clock.h"
#import "base/time/time.h"
#import "components/network_time/network_time_tracker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace metrics {

class DemographicsClientTest : public PlatformTest {
 public:
  DemographicsClientTest() {
    profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
};

TEST_F(DemographicsClientTest, GetNetworkTime) {
  // Set initial time of the network clock.
  base::TimeDelta resolution = base::Milliseconds(17);
  base::TimeDelta latency = base::Milliseconds(50);
  base::DefaultClock clock;
  base::DefaultTickClock tick_clock;
  GetApplicationContext()->GetNetworkTimeTracker()->UpdateNetworkTime(
      clock.Now() - latency / 2, resolution, latency, tick_clock.NowTicks());

  // Verify network clock gives non-null time.
  DemographicsClient demographic_client;
  EXPECT_FALSE(demographic_client.GetNetworkTime().is_null());
}

TEST_F(DemographicsClientTest, GetSyncService) {
  DemographicsClient demographic_client;
  // Verify if it possible to retrieve the instance of the SyncService
  // associated with the ProfileIOS.
  EXPECT_TRUE(demographic_client.GetSyncService());
}

TEST_F(DemographicsClientTest, GetNumberOfProfilesOnDisk) {
  DemographicsClient demographic_client;
  // On ProfileIOS was created and registered with the ProfileManager,
  // check the client returns the correct value.
  EXPECT_EQ(1, demographic_client.GetNumberOfProfilesOnDisk());
}

}  // namespace metrics
