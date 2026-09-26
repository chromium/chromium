// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class MainControllerTest : public PlatformTest {
 protected:
  MainControllerTest() { main_controller_ = [[MainController alloc] init]; }

  ~MainControllerTest() override { main_controller_ = nil; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  MainController* main_controller_;
};

// Tests that when `maybeSetLaunchReason:` is called with `kForeground`,
// `launchReason` is latched to `kForeground`, `isLaunchedInBackground` is NO,
// and `Startup.IOSLaunchReason` is recorded once. Subsequent background events
// do not overwrite `launchReason` or record additional samples.
TEST_F(MainControllerTest, TestForegroundLaunchBackgroundEventLater) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE([main_controller_ launchReason].has_value());
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectTotalCount("Startup.IOSLaunchReason", 0);

  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kForeground];
  ASSERT_TRUE([main_controller_ launchReason].has_value());
  EXPECT_EQ(*[main_controller_ launchReason], IOSLaunchReason::kForeground);
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason",
                                      IOSLaunchReason::kForeground, 1);

  // Subsequent background events should not overwrite the foreground launch
  // state or emit another sample.
  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kBackgroundRefresh];
  ASSERT_TRUE([main_controller_ launchReason].has_value());
  EXPECT_EQ(*[main_controller_ launchReason], IOSLaunchReason::kForeground);
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason",
                                      IOSLaunchReason::kForeground, 1);
}

// Tests that when `maybeSetLaunchReason:` is called with `kPreWarming`,
// `launchReason` is latched to `kPreWarming`, `isLaunchedInBackground` returns
// NO, and `Startup.IOSLaunchReason` records `kPreWarming` once. Subsequent
// calls do not overwrite `kPreWarming`.
TEST_F(MainControllerTest, TestPreWarmingLaunchReasonLatches) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE([main_controller_ launchReason].has_value());
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);

  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kPreWarming];
  ASSERT_TRUE([main_controller_ launchReason].has_value());
  EXPECT_EQ(*[main_controller_ launchReason], IOSLaunchReason::kPreWarming);
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason",
                                      IOSLaunchReason::kPreWarming, 1);

  // Subsequent foreground or background calls do not overwrite kPreWarming or
  // record additional samples.
  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kForeground];
  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kBackgroundRefresh];
  EXPECT_EQ(*[main_controller_ launchReason], IOSLaunchReason::kPreWarming);
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason",
                                      IOSLaunchReason::kPreWarming, 1);
}

// Tests that when a background refresh occurs before foregrounding, the
// reported launch reason is latched to `kBackgroundRefresh`,
// `isLaunchedInBackground` returns YES, and the histogram records
// `kBackgroundRefresh` once. Subsequent foregrounding does not overwrite this
// reason.
TEST_F(MainControllerTest, TestBackgroundRefreshFollowedByForeground) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE([main_controller_ launchReason].has_value());
  EXPECT_FALSE([main_controller_ isLaunchedInBackground]);

  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kBackgroundRefresh];
  ASSERT_TRUE([main_controller_ launchReason].has_value());
  EXPECT_EQ(*[main_controller_ launchReason],
            IOSLaunchReason::kBackgroundRefresh);
  EXPECT_TRUE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason",
                                      IOSLaunchReason::kBackgroundRefresh, 1);

  // Subsequent foregrounding must not overwrite kBackgroundRefresh or record
  // additional samples.
  [main_controller_ maybeSetLaunchReason:IOSLaunchReason::kForeground];
  ASSERT_TRUE([main_controller_ launchReason].has_value());
  EXPECT_EQ(*[main_controller_ launchReason],
            IOSLaunchReason::kBackgroundRefresh);
  EXPECT_TRUE([main_controller_ isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason",
                                      IOSLaunchReason::kBackgroundRefresh, 1);
}

// Tests that when the app is woken to handle events for a background URL
// session, the reported launch reason is `kBackgroundURLSession`,
// `isLaunchedInBackground` returns YES, and the histogram records
// `kBackgroundURLSession`. Subsequent foregrounding does not overwrite this
// reason.
TEST_F(MainControllerTest, TestBackgroundURLSessionFollowedByForeground) {
  base::HistogramTester histogram_tester;
  MainApplicationDelegate* app_delegate =
      [[MainApplicationDelegate alloc] init];
  MainController* main_controller = app_delegate.mainController;

  // iOS wakes the app to handle background URL session events.
  base::test::TestFuture<void> completion;
  auto* completion_ptr = &completion;
  [app_delegate application:[UIApplication sharedApplication]
      handleEventsForBackgroundURLSession:@"test_session"
                        completionHandler:^{
                          completion_ptr->SetValue();
                        }];

  EXPECT_TRUE(completion.Wait());
  ASSERT_TRUE([main_controller launchReason].has_value());
  EXPECT_EQ(*[main_controller launchReason],
            IOSLaunchReason::kBackgroundURLSession);
  EXPECT_TRUE([main_controller isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample(
      "Startup.IOSLaunchReason", IOSLaunchReason::kBackgroundURLSession, 1);

  // Subsequent foregrounding does not overwrite kBackgroundURLSession.
  [main_controller maybeSetLaunchReason:IOSLaunchReason::kForeground];
  ASSERT_TRUE([main_controller launchReason].has_value());
  EXPECT_EQ(*[main_controller launchReason],
            IOSLaunchReason::kBackgroundURLSession);
  EXPECT_TRUE([main_controller isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample(
      "Startup.IOSLaunchReason", IOSLaunchReason::kBackgroundURLSession, 1);
}

// Tests that when the app is woken to handle a background remote push
// notification, the reported launch reason is `kBackgroundSilentNotification`,
// `isLaunchedInBackground` returns YES, and the histogram records
// `kBackgroundSilentNotification`. Subsequent foregrounding does not overwrite
// this reason.
TEST_F(MainControllerTest, TestRemoteNotificationFollowedByForeground) {
  base::HistogramTester histogram_tester;
  MainApplicationDelegate* app_delegate =
      [[MainApplicationDelegate alloc] init];
  MainController* main_controller = app_delegate.mainController;

  // iOS wakes the app for a background remote notification.
  base::test::TestFuture<UIBackgroundFetchResult> fetch_result;
  auto* fetch_result_ptr = &fetch_result;
  [app_delegate application:[UIApplication sharedApplication]
      didReceiveRemoteNotification:@{}
      fetchCompletionHandler:^(UIBackgroundFetchResult result) {
        fetch_result_ptr->SetValue(result);
      }];

  EXPECT_EQ(fetch_result.Get(), UIBackgroundFetchResultNoData);
  ASSERT_TRUE([main_controller launchReason].has_value());
  EXPECT_EQ(*[main_controller launchReason],
            IOSLaunchReason::kBackgroundSilentNotification);
  EXPECT_TRUE([main_controller isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample(
      "Startup.IOSLaunchReason", IOSLaunchReason::kBackgroundSilentNotification,
      1);

  // Subsequent foregrounding does not overwrite kBackgroundSilentNotification.
  [main_controller maybeSetLaunchReason:IOSLaunchReason::kForeground];
  ASSERT_TRUE([main_controller launchReason].has_value());
  EXPECT_EQ(*[main_controller launchReason],
            IOSLaunchReason::kBackgroundSilentNotification);
  EXPECT_TRUE([main_controller isLaunchedInBackground]);
  histogram_tester.ExpectUniqueSample(
      "Startup.IOSLaunchReason", IOSLaunchReason::kBackgroundSilentNotification,
      1);
}

}  // namespace
