// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metrics_mediator.h"

#import <Foundation/Foundation.h>

#import "base/test/metrics/histogram_tester.h"
#import "components/metrics/metrics_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"
#import "ios/chrome/app/application_delegate/metrics_mediator_testing.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/network_change_notifier.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Mock class for testing MetricsMediator.
@interface MetricsMediatorMock : MetricsMediator
@property(nonatomic) NSInteger reportingValue;
- (void)reset;
@end

@implementation MetricsMediatorMock
@synthesize reportingValue = _reportingValue;

- (void)reset {
  _reportingValue = -1;
}
- (void)setReporting:(BOOL)enableReporting {
  _reportingValue = enableReporting ? 1 : 0;
}
- (BOOL)areMetricsEnabled {
  return YES;
}

@end

using MetricsMediatorTest = PlatformTest;

// Tests that histograms logged in a widget are correctly re-emitted by Chrome.
TEST_F(MetricsMediatorTest, WidgetHistogramMetricsRecorded) {
  using app_group::HistogramCountKey;

  base::HistogramTester tester;
  NSString* histogram = @"MyHistogram";

  // Simulate 1 event fired in bucket 0, and 2 events fired in bucket 2.
  NSString* keyBucket0 = HistogramCountKey(histogram, 0);
  NSString* keyBucket1 = HistogramCountKey(histogram, 1);
  NSString* keyBucket2 = HistogramCountKey(histogram, 2);

  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  [sharedDefaults setInteger:1 forKey:keyBucket0];
  [sharedDefaults setInteger:2 forKey:keyBucket2];

  const metrics_mediator::HistogramNameCountPair histograms[] = {
      {
          histogram,
          // 3 buckets, to make sure that the first and last buckets are logged.
          3,
      },
  };

  metrics_mediator::RecordWidgetUsage(histograms);

  // Verify that the correct events were emitted.
  tester.ExpectBucketCount("MyHistogram", 0, 1);
  tester.ExpectBucketCount("MyHistogram", 1, 0);
  tester.ExpectBucketCount("MyHistogram", 2, 2);

  // Verify that all entries in NSUserDefaults have been removed.
  EXPECT_EQ(0, [sharedDefaults integerForKey:keyBucket0]);
  EXPECT_EQ(0, [sharedDefaults integerForKey:keyBucket1]);
  EXPECT_EQ(0, [sharedDefaults integerForKey:keyBucket2]);
}

#pragma mark - logLaunchMetrics tests.

// A block that takes as arguments the caller and the arguments from
// UserActivityHandler +handleStartupParameters and returns nothing.
typedef void (^LogLaunchMetricsBlock)(id, const char*, int);

class MetricsMediatorLogLaunchTest : public PlatformTest {
 protected:
  MetricsMediatorLogLaunchTest()
      : profile_(TestProfileIOS::Builder().Build()),
        num_tabs_has_been_called_(FALSE),
        num_ntp_tabs_has_been_called_(FALSE),
        num_live_ntp_tabs_has_been_called_(FALSE) {}

  void initiateMetricsMediator(BOOL coldStart, int tabCount) {
    num_tabs_swizzle_block_ = [^(id self, int numTab) {
      num_tabs_has_been_called_ = YES;
      // Tests.
      EXPECT_EQ(tabCount, numTab);
    } copy];
    num_ntp_tabs_swizzle_block_ = [^(id self, int numTab) {
      num_ntp_tabs_has_been_called_ = YES;
      // Tests.
      EXPECT_EQ(tabCount, numTab);
    } copy];
    num_live_ntp_tabs_swizzle_block_ = [^(id self, int numTab) {
      num_live_ntp_tabs_has_been_called_ = YES;
    } copy];
    if (coldStart) {
      tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordStartupTabCount:),
          num_tabs_swizzle_block_));
      ntp_tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordStartupNTPTabCount:),
          num_ntp_tabs_swizzle_block_));
    } else {
      tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordResumeTabCount:),
          num_tabs_swizzle_block_));
      ntp_tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordResumeNTPTabCount:),
          num_ntp_tabs_swizzle_block_));
      live_ntp_tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordResumeLiveNTPTabCount:),
          num_live_ntp_tabs_swizzle_block_));
    }
  }

  void TearDown() override {
    connected_scenes_ = nil;
    PlatformTest::TearDown();
  }

  void verifySwizzleHasBeenCalled() {
    EXPECT_TRUE(num_tabs_has_been_called_);
    EXPECT_TRUE(num_ntp_tabs_has_been_called_);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  NSArray<FakeSceneState*>* connected_scenes_;
  __block BOOL num_tabs_has_been_called_;
  __block BOOL num_ntp_tabs_has_been_called_;
  __block BOOL num_live_ntp_tabs_has_been_called_;
  LogLaunchMetricsBlock num_tabs_swizzle_block_;
  LogLaunchMetricsBlock num_ntp_tabs_swizzle_block_;
  LogLaunchMetricsBlock num_live_ntp_tabs_swizzle_block_;
  std::unique_ptr<ScopedBlockSwizzler> tabs_uma_histogram_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> ntp_tabs_uma_histogram_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> live_ntp_tabs_uma_histogram_swizzler_;
};

// Verifies that the log of the number of open tabs is sent and verifies
TEST_F(MetricsMediatorLogLaunchTest,
       logLaunchMetricsWithEnteredBackgroundDate) {
  // Setup.
  BOOL coldStart = YES;
  initiateMetricsMediator(coldStart, 23);
  // 23 tabs across three scenes.
  connected_scenes_ = [FakeSceneState sceneArrayWithCount:3
                                                  profile:profile_.get()];
  [connected_scenes_[0] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:9];
  [connected_scenes_[1] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:9];
  [connected_scenes_[2] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:5];
  // Mark one of the scenes as active.
  connected_scenes_[0].activationLevel = SceneActivationLevelForegroundActive;

  const NSTimeInterval kFirstUserActionTimeout = 30.0;

  id startupInformation =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];
  [[[startupInformation stub] andReturnValue:@(coldStart)] isColdStart];
  [[startupInformation expect]
      expireFirstUserActionRecorderAfterDelay:kFirstUserActionTimeout];

  [[NSUserDefaults standardUserDefaults]
      setObject:[NSDate date]
         forKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  // Action.
  [MetricsMediator logLaunchMetricsWithStartupInformation:startupInformation
                                          connectedScenes:connected_scenes_];

  // Tests.
  NSDate* dateStored = [[NSUserDefaults standardUserDefaults]
      objectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];
  EXPECT_EQ(nil, dateStored);
  verifySwizzleHasBeenCalled();
  EXPECT_OCMOCK_VERIFY(startupInformation);
}

// Verifies that +logLaunchMetrics logs of the number of open tabs and nothing
// more if the background date is not set;
TEST_F(MetricsMediatorLogLaunchTest, logLaunchMetricsNoBackgroundDate) {
  // Setup.
  BOOL coldStart = NO;
  initiateMetricsMediator(coldStart, 32);
  // 32 tabs across five scenes.
  connected_scenes_ = [FakeSceneState sceneArrayWithCount:5
                                                  profile:profile_.get()];
  [connected_scenes_[0] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:8];
  [connected_scenes_[1] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:8];
  // Scene 2 has zero tabs.
  [connected_scenes_[3] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:8];
  [connected_scenes_[4] appendWebStatesWithURL:GURL(kChromeUINewTabURL)
                                         count:8];

  id startupInformation =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformation stub] andReturnValue:@(coldStart)] isColdStart];

  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  // Action.
  [MetricsMediator logLaunchMetricsWithStartupInformation:startupInformation
                                          connectedScenes:connected_scenes_];
  // Tests.
  verifySwizzleHasBeenCalled();
  EXPECT_TRUE(num_live_ntp_tabs_has_been_called_);
}

using MetricsMediatorNoFixtureTest = PlatformTest;

// Tests that +logDateInUserDefaults logs the date in UserDefaults.
TEST_F(MetricsMediatorNoFixtureTest, logDateInUserDefaultsTest) {
  // Setup.
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  NSDate* lastAppClose = [[NSUserDefaults standardUserDefaults]
      objectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  ASSERT_EQ(nil, lastAppClose);

  // Action.
  [MetricsMediator logDateInUserDefaults];

  // Setup.
  lastAppClose = [[NSUserDefaults standardUserDefaults]
      objectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];
  EXPECT_NE(nil, lastAppClose);
}

// Tests that +logStartupDuration: calls
// +endExtendedLaunchTask on cold start.
TEST_F(MetricsMediatorNoFixtureTest, endExtendedLaunchTaskOnColdStart) {
  id startupInformation =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformation stub] andReturnValue:@YES] isColdStart];

  base::TimeTicks time = base::TimeTicks();
  [[[startupInformation stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:(void*)&time];
  }] appLaunchTime];

  [[[startupInformation stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:(void*)&time];
  }] didFinishLaunchingTime];

  [[[startupInformation stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:(void*)&time];
  }] firstSceneConnectionTime];

  id metricKitSubscriber =
      [OCMockObject mockForClass:[MetricKitSubscriber class]];
  [[metricKitSubscriber expect] endExtendedLaunchTask];

  [MetricsMediator logStartupDuration:startupInformation];
  EXPECT_OCMOCK_VERIFY(metricKitSubscriber);
}

// Tests that +logStartupDuration: does not call
// +endExtendedLaunchTask on warm start.
TEST_F(MetricsMediatorNoFixtureTest, endExtendedLaunchTaskOnWarmStart) {
  id startupInformation =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformation stub] andReturnValue:@NO] isColdStart];

  id metricKitSubscriber =
      [OCMockObject mockForClass:[MetricKitSubscriber class]];
  [[metricKitSubscriber reject] endExtendedLaunchTask];

  [MetricsMediator logStartupDuration:startupInformation];
  EXPECT_OCMOCK_VERIFY(metricKitSubscriber);
}
