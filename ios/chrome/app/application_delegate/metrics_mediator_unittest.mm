// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metrics_mediator.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "base/metrics/histogram_functions.h"
#import "base/scoped_environment_variable_override.h"
#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/metrics/metrics_service.h"
#import "components/password_manager/core/common/browser_assisted_login_type.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"
#import "ios/chrome/app/application_delegate/metrics_mediator_testing.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/network_change_notifier.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface MetricsMediator ()
- (void)setMetricsEnabled:(BOOL)enabled;
- (void)setAppGroupMetricsEnabled:(BOOL)enabled;
- (void)updateMetricsPrefsOnPermissionChange:(BOOL)enabled;
@end

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
- (void)setMetricsEnabled:(BOOL)enabled {
}
- (void)setAppGroupMetricsEnabled:(BOOL)enabled {
}
- (void)updateMetricsPrefsOnPermissionChange:(BOOL)enabled {
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

namespace {

enum class TestEnum {
  kZero = 0,
  kOne = 1,
  kTwo = 2,
  kMaxValue = kTwo,
};

}  // namespace

// Tests that histograms recorded via RecordWidgetUsage have bucket parameters
// consistent with base::UmaHistogramEnumeration without a bucket count mismatch
// DCHECK.
TEST_F(MetricsMediatorTest, WidgetHistogramMatchesEnumerationBuckets) {
  base::HistogramTester tester;

  // Pre-log using UmaHistogramEnumeration to create the histogram with
  // `kMaxValue + 1` buckets.
  base::UmaHistogramEnumeration("Test.EnumerationHistogram", TestEnum::kZero);

  NSString* histogram = @"Test.EnumerationHistogram";
  NSString* keyBucket2 = app_group::HistogramCountKey(histogram, 2);

  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  [sharedDefaults setInteger:1 forKey:keyBucket2];

  // This will DCHECK if RecordWidgetUsage passes an exclusive max different
  // from UmaHistogramEnumeration (which uses kMaxValue + 1).
  metrics_mediator::RecordWidgetUsage(
      {{histogram, static_cast<int>(TestEnum::kMaxValue) + 1}});

  // Verify that the events were emitted.
  tester.ExpectBucketCount("Test.EnumerationHistogram", 0, 1);
  tester.ExpectBucketCount("Test.EnumerationHistogram", 2, 1);
  tester.ExpectTotalCount("Test.EnumerationHistogram", 2);
  EXPECT_EQ([sharedDefaults integerForKey:keyBucket2], 0);

  [sharedDefaults removeObjectForKey:keyBucket2];
}

// Tests that browser logging via base::UmaHistogramEnumeration works when the
// histogram was created first by RecordWidgetUsage (e.g. on cold start).
TEST_F(MetricsMediatorTest, WidgetHistogramMatchesEnumerationBucketsColdStart) {
  base::HistogramTester tester;
  NSString* histogram = @"Test.ColdStartEnumerationHistogram";
  NSString* keyBucket2 = app_group::HistogramCountKey(histogram, 2);

  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  [sharedDefaults setInteger:1 forKey:keyBucket2];

  // RecordWidgetUsage creates the histogram before any browser code logs to it.
  metrics_mediator::RecordWidgetUsage(
      {{histogram, static_cast<int>(TestEnum::kMaxValue) + 1}});

  // Verify that the events were emitted.
  tester.ExpectBucketCount("Test.ColdStartEnumerationHistogram", 2, 1);
  tester.ExpectTotalCount("Test.ColdStartEnumerationHistogram", 1);
  EXPECT_EQ([sharedDefaults integerForKey:keyBucket2], 0);

  // Verify that browser logging works when the histogram was created first by
  // RecordWidgetUsage.
  base::UmaHistogramEnumeration("Test.ColdStartEnumerationHistogram",
                                TestEnum::kOne);
  tester.ExpectBucketCount("Test.ColdStartEnumerationHistogram", 1, 1);
  tester.ExpectTotalCount("Test.ColdStartEnumerationHistogram", 2);

  [sharedDefaults removeObjectForKey:keyBucket2];
}

// Tests that PasswordManager.BrowserAssistedLogin.Type logged in an extension
// is correctly re-emitted by Chrome via RecordWidgetUsage.
TEST_F(MetricsMediatorTest, BrowserAssistedLoginTypeHistogramRecorded) {
  using password_manager::metrics_util::BrowserAssistedLoginType;

  base::HistogramTester tester;

  NSString* keyBucket = app_group::HistogramCountKey(
      @"PasswordManager.BrowserAssistedLogin.Type",
      static_cast<int>(BrowserAssistedLoginType::
                           kPasskeyStoredInGPMFacilitatedThroughIOSUI));

  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  [sharedDefaults setInteger:2 forKey:keyBucket];

  metrics_mediator::RecordWidgetUsage(
      {{@"PasswordManager.BrowserAssistedLogin.Type",
        static_cast<int>(BrowserAssistedLoginType::kMaxValue) + 1}});

  tester.ExpectUniqueSample(
      "PasswordManager.BrowserAssistedLogin.Type",
      BrowserAssistedLoginType::kPasskeyStoredInGPMFacilitatedThroughIOSUI, 2);

  // Verify that all entries in NSUserDefaults have been removed.
  EXPECT_EQ([sharedDefaults integerForKey:keyBucket], 0);

  [sharedDefaults removeObjectForKey:keyBucket];
}

#pragma mark - logLaunchMetrics tests.

// A block that takes as arguments the caller and the arguments from
// UserActivityHandler +handleStartupParameters and returns nothing.
typedef void (^LogLaunchMetricsBlock)(id, const char*, int);

class MetricsMediatorLogLaunchTest : public PlatformTest {
 protected:
  MetricsMediatorLogLaunchTest()
      : profile_(TestProfileIOS::Builder().Build()),
        num_tabs_has_been_called_(FALSE) {}

  void initiateMetricsMediator(BOOL coldStart, int tabCount) {
    num_tabs_swizzle_block_ = [^(id self, int numTab) {
      num_tabs_has_been_called_ = YES;
      // Tests.
      EXPECT_EQ(tabCount, numTab);
    } copy];
    if (coldStart) {
      tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordStartupTabCount:),
          num_tabs_swizzle_block_));
    } else {
      tabs_uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordResumeTabCount:),
          num_tabs_swizzle_block_));
    }
  }

  void TearDown() override {
    for (FakeSceneState* scene_state in connected_scenes_) {
      [scene_state shutdown];
    }
    connected_scenes_ = nil;
    PlatformTest::TearDown();
  }

  void verifySwizzleHasBeenCalled() {
    EXPECT_TRUE(num_tabs_has_been_called_);
  }

  NSArray<FakeSceneState*>* SceneArrayWithCount(int count) {
    NSMutableArray<SceneState*>* scenes = [NSMutableArray array];
    for (int i = 0; i < count; i++) {
      [scenes
          addObject:[[FakeSceneState alloc] initWithProfile:profile_.get()]];
    }
    return [scenes copy];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  NSArray<FakeSceneState*>* connected_scenes_;
  __block BOOL num_tabs_has_been_called_;
  LogLaunchMetricsBlock num_tabs_swizzle_block_;
  std::unique_ptr<ScopedBlockSwizzler> tabs_uma_histogram_swizzler_;
};

// Verifies that the log of the number of open tabs is sent and verifies
TEST_F(MetricsMediatorLogLaunchTest,
       logLaunchMetricsWithEnteredBackgroundDate) {
  // Setup.
  BOOL coldStart = YES;
  initiateMetricsMediator(coldStart, 23);
  // 23 tabs across three scenes.
  connected_scenes_ = SceneArrayWithCount(3);
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
  connected_scenes_ = SceneArrayWithCount(5);
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
}

class MetricsMediatorNoFixtureTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    [PreviousSessionInfo sharedInstance].isFirstSessionAfterUpgrade = NO;
    SetFirstSessionAfterDeviceRestoreForTesting(signin::Tribool::kFalse);
  }

  void TearDown() override {
    ResetDeviceRestoreDataForTesting();
    [PreviousSessionInfo sharedInstance].isFirstSessionAfterUpgrade = NO;
    PlatformTest::TearDown();
  }

 protected:
  // Creates and returns a FakeStartupInformation configured for a standard cold
  // start.
  FakeStartupInformation* CreateDefaultStartupInformation() {
    FakeStartupInformation* startup_information =
        [[FakeStartupInformation alloc] init];
    startup_information.isColdStart = YES;
    startup_information.isFirstRun = NO;
    startup_information.launchReason = std::nullopt;
    startup_information.appLaunchTime =
        base::TimeTicks::Now() - base::Seconds(1);
    startup_information.didFinishLaunchingTime =
        startup_information.appLaunchTime;
    startup_information.firstSceneConnectionTime =
        startup_information.appLaunchTime;
    startup_information.preMainDuration = base::Milliseconds(200);
    return startup_information;
  }
};

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
  FakeStartupInformation* startupInformation =
      CreateDefaultStartupInformation();

  id metricKitSubscriber =
      [OCMockObject mockForClass:[MetricKitSubscriber class]];
  [[metricKitSubscriber expect] endExtendedLaunchTask];

  [MetricsMediator logStartupDuration:startupInformation];
  EXPECT_OCMOCK_VERIFY(metricKitSubscriber);
}

// Tests that +logStartupDuration: does not record metrics if not cold start.
TEST_F(MetricsMediatorNoFixtureTest, LogStartupDurationNotColdStart) {
  base::HistogramTester histogram_tester;
  FakeStartupInformation* startup_information =
      CreateDefaultStartupInformation();
  startup_information.isColdStart = NO;

  [MetricsMediator logStartupDuration:startup_information];

  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain.NotPrewarmed",
                                    0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain.ActivePrewarm",
                                    0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.Regular", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.NotPrewarmed", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.ActivePrewarm",
                                    0);
}

// Scoped helper to set or unset the ActivePrewarm environment variable used by
// base::ios::IsApplicationPreWarmed().
class ScopedSetProcessPreWarmed {
 public:
  explicit ScopedSetProcessPreWarmed(bool pre_warmed)
      : scoped_override_(
            pre_warmed
                ? base::ScopedEnvironmentVariableOverride("ActivePrewarm", "1")
                : base::ScopedEnvironmentVariableOverride("ActivePrewarm")) {}

 private:
  base::ScopedEnvironmentVariableOverride scoped_override_;
};

// Parameters for MetricsMediatorStartupTemperatureTest.
struct StartupTemperatureTestCase {
  std::string test_name;
  bool is_pre_warmed = false;
  std::optional<IOSLaunchReason> initial_launch_reason = std::nullopt;
  base::TimeDelta app_launch_delay = base::Seconds(1);
  IOSLaunchReason expected_launch_reason = IOSLaunchReason::kForeground;
  std::string expected_temperature;
  int expected_cold_start_from_main_count = 1;
  int expected_cold_start_pre_main_count = 1;
};

// Test fixture for testing startup duration variants across different startup
// temperatures.
class MetricsMediatorStartupTemperatureTest
    : public MetricsMediatorNoFixtureTest,
      public ::testing::WithParamInterface<StartupTemperatureTestCase> {};

// Tests that +logStartupDuration: records the expected histogram variants for
// each startup temperature.
TEST_P(MetricsMediatorStartupTemperatureTest, LogStartupDuration) {
  const StartupTemperatureTestCase& test_case = GetParam();
  ScopedSetProcessPreWarmed scoped_pre_warmed(test_case.is_pre_warmed);

  base::HistogramTester histogram_tester;
  FakeStartupInformation* startup_information =
      CreateDefaultStartupInformation();
  startup_information.launchReason = test_case.initial_launch_reason;
  startup_information.appLaunchTime =
      base::TimeTicks::Now() - test_case.app_launch_delay;
  startup_information.didFinishLaunchingTime =
      startup_information.appLaunchTime;
  startup_information.firstSceneConnectionTime =
      startup_information.appLaunchTime;

  [MetricsMediator logStartupDuration:startup_information];

  ASSERT_TRUE(startup_information.launchReason.has_value());
  EXPECT_EQ(*startup_information.launchReason,
            test_case.expected_launch_reason);
  histogram_tester.ExpectUniqueSample("Startup.IOSLaunchReason.Foregrounded",
                                      test_case.expected_launch_reason, 1);

  histogram_tester.ExpectTotalCount(
      "Startup.IOSColdStartType.ForegroundLaunchesOnly",
      test_case.expected_cold_start_from_main_count);
  histogram_tester.ExpectTotalCount(
      "Startup.ColdStartFromMain",
      test_case.expected_cold_start_from_main_count);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {"Startup.ColdStartFromMain.", test_case.expected_temperature}),
      test_case.expected_cold_start_from_main_count);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Startup.ColdStartFromMain.",
                    test_case.expected_temperature, ".Regular"}),
      test_case.expected_cold_start_from_main_count);

  histogram_tester.ExpectTotalCount(
      "Startup.TimeFromMainToDidFinishLaunchingCall",
      test_case.expected_cold_start_from_main_count);
  histogram_tester.ExpectTotalCount(
      "Startup.TimeFromMainToSceneConnection",
      test_case.expected_cold_start_from_main_count);

  histogram_tester.ExpectTotalCount(
      "Startup.ColdStartPreMain", test_case.expected_cold_start_pre_main_count);
  histogram_tester.ExpectTotalCount(
      "Startup.ColdStartPreMain.Regular",
      test_case.expected_cold_start_pre_main_count);

  // Verify other temperatures are not recorded for ColdStartFromMain, and no
  // temperature variants are ever recorded for ColdStartPreMain.
  for (const std::string& temp : {"NotPrewarmed", "ActivePrewarm"}) {
    histogram_tester.ExpectTotalCount(
        base::StrCat({"Startup.ColdStartPreMain.", temp}), 0);
    if (temp != test_case.expected_temperature) {
      histogram_tester.ExpectTotalCount(
          base::StrCat({"Startup.ColdStartFromMain.", temp}), 0);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MetricsMediatorStartupTemperatureTest,
    ::testing::Values(
        StartupTemperatureTestCase{
            .test_name = "NotPrewarmed",
            .is_pre_warmed = false,
            .initial_launch_reason = std::nullopt,
            .expected_launch_reason = IOSLaunchReason::kForeground,
            .expected_temperature = "NotPrewarmed",
            .expected_cold_start_from_main_count = 1,
            .expected_cold_start_pre_main_count = 1,
        },
        StartupTemperatureTestCase{
            .test_name = "ActivePrewarm",
            .is_pre_warmed = true,
            .initial_launch_reason = std::nullopt,
            .expected_launch_reason = IOSLaunchReason::kPreWarming,
            .expected_temperature = "ActivePrewarm",
            .expected_cold_start_from_main_count = 1,
            .expected_cold_start_pre_main_count = 0,
        },
        StartupTemperatureTestCase{
            .test_name = "LaunchedInBackground",
            .is_pre_warmed = false,
            .initial_launch_reason = IOSLaunchReason::kBackgroundRefresh,
            .expected_launch_reason = IOSLaunchReason::kBackgroundRefresh,
            .expected_temperature = "NotPrewarmed",
            .expected_cold_start_from_main_count = 0,
            .expected_cold_start_pre_main_count = 0,
        },
        StartupTemperatureTestCase{
            .test_name = "LaunchedInBackgroundPrecedence",
            .is_pre_warmed = true,
            .initial_launch_reason = IOSLaunchReason::kBackgroundRefresh,
            .expected_launch_reason = IOSLaunchReason::kBackgroundRefresh,
            .expected_temperature = "ActivePrewarm",
            .expected_cold_start_from_main_count = 0,
            .expected_cold_start_pre_main_count = 0,
        },
        StartupTemperatureTestCase{
            .test_name = "Suspicious",
            .is_pre_warmed = false,
            .initial_launch_reason = std::nullopt,
            .app_launch_delay = base::Seconds(31),
            .expected_launch_reason = IOSLaunchReason::kSuspicious,
            .expected_temperature = "NotPrewarmed",
            .expected_cold_start_from_main_count = 0,
            .expected_cold_start_pre_main_count = 0,
        },
        StartupTemperatureTestCase{
            .test_name = "SuspiciousPrewarm",
            .is_pre_warmed = true,
            .initial_launch_reason = std::nullopt,
            .app_launch_delay = base::Seconds(31),
            .expected_launch_reason = IOSLaunchReason::kSuspicious,
            .expected_temperature = "ActivePrewarm",
            .expected_cold_start_from_main_count = 0,
            .expected_cold_start_pre_main_count = 0,
        }),
    [](const ::testing::TestParamInfo<StartupTemperatureTestCase>& info) {
      return info.param.test_name;
    });

// Parameters for MetricsMediatorColdStartTypeTest.
struct ColdStartTypeTestCase {
  std::string test_name;
  bool is_first_run = false;
  bool is_first_session_after_upgrade = false;
  signin::Tribool device_restore = signin::Tribool::kFalse;
  std::string expected_cold_start_type;
};

// Test fixture for testing startup duration sub-variants across different cold
// start types.
class MetricsMediatorColdStartTypeTest
    : public MetricsMediatorNoFixtureTest,
      public ::testing::WithParamInterface<ColdStartTypeTestCase> {};

// Tests that +logStartupDuration: records the expected ColdStartType
// sub-variants.
TEST_P(MetricsMediatorColdStartTypeTest, LogStartupDuration) {
  const ColdStartTypeTestCase& test_case = GetParam();
  ScopedSetProcessPreWarmed scoped_pre_warmed(false);
  SetFirstSessionAfterDeviceRestoreForTesting(test_case.device_restore);
  [PreviousSessionInfo sharedInstance].isFirstSessionAfterUpgrade =
      test_case.is_first_session_after_upgrade;

  base::HistogramTester histogram_tester;
  FakeStartupInformation* startup_information =
      CreateDefaultStartupInformation();
  startup_information.isFirstRun = test_case.is_first_run;

  [MetricsMediator logStartupDuration:startup_information];

  histogram_tester.ExpectTotalCount(
      "Startup.IOSColdStartType.ForegroundLaunchesOnly", 1);
  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain", 1);
  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain.NotPrewarmed",
                                    1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Startup.ColdStartFromMain.NotPrewarmed.",
                    test_case.expected_cold_start_type}),
      1);

  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain", 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {"Startup.ColdStartPreMain.", test_case.expected_cold_start_type}),
      1);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.NotPrewarmed", 0);

  // Verify other cold start types are not recorded.
  for (const std::string& type :
       {"Regular", "FirstRun", "AfterChromeUpgrade", "AfterDeviceRestore",
        "AfterDeviceRestoreAndChromeUpgrade", "UnknownDeviceRestore",
        "UnknownDeviceRestoreAndChromeUpgrade"}) {
    if (type != test_case.expected_cold_start_type) {
      histogram_tester.ExpectTotalCount(
          base::StrCat({"Startup.ColdStartFromMain.NotPrewarmed.", type}), 0);
      histogram_tester.ExpectTotalCount(
          base::StrCat({"Startup.ColdStartPreMain.", type}), 0);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MetricsMediatorColdStartTypeTest,
    ::testing::Values(
        ColdStartTypeTestCase{
            .test_name = "Regular",
            .is_first_run = false,
            .is_first_session_after_upgrade = false,
            .device_restore = signin::Tribool::kFalse,
            .expected_cold_start_type = "Regular",
        },
        ColdStartTypeTestCase{
            .test_name = "FirstRun",
            .is_first_run = true,
            .is_first_session_after_upgrade = false,
            .device_restore = signin::Tribool::kFalse,
            .expected_cold_start_type = "FirstRun",
        },
        ColdStartTypeTestCase{
            .test_name = "AfterChromeUpgrade",
            .is_first_run = false,
            .is_first_session_after_upgrade = true,
            .device_restore = signin::Tribool::kFalse,
            .expected_cold_start_type = "AfterChromeUpgrade",
        },
        ColdStartTypeTestCase{
            .test_name = "AfterDeviceRestore",
            .is_first_run = false,
            .is_first_session_after_upgrade = false,
            .device_restore = signin::Tribool::kTrue,
            .expected_cold_start_type = "AfterDeviceRestore",
        },
        ColdStartTypeTestCase{
            .test_name = "AfterDeviceRestoreAndChromeUpgrade",
            .is_first_run = false,
            .is_first_session_after_upgrade = true,
            .device_restore = signin::Tribool::kTrue,
            .expected_cold_start_type = "AfterDeviceRestoreAndChromeUpgrade",
        },
        ColdStartTypeTestCase{
            .test_name = "UnknownDeviceRestore",
            .is_first_run = false,
            .is_first_session_after_upgrade = false,
            .device_restore = signin::Tribool::kUnknown,
            .expected_cold_start_type = "UnknownDeviceRestore",
        },
        ColdStartTypeTestCase{
            .test_name = "UnknownDeviceRestoreAndChromeUpgrade",
            .is_first_run = false,
            .is_first_session_after_upgrade = true,
            .device_restore = signin::Tribool::kUnknown,
            .expected_cold_start_type = "UnknownDeviceRestoreAndChromeUpgrade",
        }),
    [](const ::testing::TestParamInfo<ColdStartTypeTestCase>& info) {
      return info.param.test_name;
    });

// Tests that +logStartupDuration: does not record Startup.ColdStartPreMain
// when preMainDuration is zero or not positive.
TEST_F(MetricsMediatorNoFixtureTest, LogStartupDurationZeroPreMainDuration) {
  ScopedSetProcessPreWarmed scoped_pre_warmed(false);
  base::HistogramTester histogram_tester;
  FakeStartupInformation* startup_information =
      CreateDefaultStartupInformation();
  startup_information.preMainDuration = base::TimeDelta();

  [MetricsMediator logStartupDuration:startup_information];

  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain", 1);
  histogram_tester.ExpectTotalCount("Startup.ColdStartFromMain.NotPrewarmed",
                                    1);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.Regular", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.NotPrewarmed", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.ActivePrewarm",
                                    0);
}

// Tests that +logStartupDuration: does not record Startup.ColdStartPreMain
// when preMainDuration exceeds the 30s upper bound (e.g., due to a forward
// wall-clock jump).
TEST_F(MetricsMediatorNoFixtureTest,
       LogStartupDurationExcessivePreMainDuration) {
  ScopedSetProcessPreWarmed scoped_pre_warmed(false);
  base::HistogramTester histogram_tester;
  FakeStartupInformation* startup_information =
      CreateDefaultStartupInformation();
  startup_information.preMainDuration = base::Seconds(31);

  [MetricsMediator logStartupDuration:startup_information];

  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain", 0);
  histogram_tester.ExpectTotalCount("Startup.ColdStartPreMain.Regular", 0);
}

// Tests that +logStartupDuration: does not call
// +endExtendedLaunchTask on warm start.
TEST_F(MetricsMediatorNoFixtureTest, endExtendedLaunchTaskOnWarmStart) {
  FakeStartupInformation* startupInformation =
      CreateDefaultStartupInformation();
  startupInformation.isColdStart = NO;

  id metricKitSubscriber =
      [OCMockObject mockForClass:[MetricKitSubscriber class]];
  [[metricKitSubscriber reject] endExtendedLaunchTask];

  [MetricsMediator logStartupDuration:startupInformation];
  EXPECT_OCMOCK_VERIFY(metricKitSubscriber);
}

class MetricsMediatorDeferralTest : public PlatformTest {
 private:
  base::test::TaskEnvironment task_environment_;
};

// Test that MetricKit registration is deferred on startup when feature flag is
// enabled.
TEST_F(MetricsMediatorDeferralTest, DeferMetricKitRegistrationOnStartup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMetrickitDeferRegistration);

  id subscriber_mock = OCMStrictClassMock([MetricKitSubscriber class]);
  OCMStub([subscriber_mock sharedInstance]).andReturn(subscriber_mock);

  MetricsMediatorMock* mediator = [[MetricsMediatorMock alloc] init];
  [mediator updateMetricsStateBasedOnPrefsUserTriggered:NO];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  OCMExpect([subscriber_mock setEnabled:YES]);
  [mediator registerMetricKitSubscriberIfNeeded];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  [subscriber_mock stopMocking];
}

// Test that MetricKit registration is deferred on startup even for
// user-triggered preference updates, but propagates immediately once
// initialized.
TEST_F(MetricsMediatorDeferralTest,
       MetricKitRegistrationDeferralAndPostInitUserTrigger) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMetrickitDeferRegistration);

  id subscriber_mock = OCMStrictClassMock([MetricKitSubscriber class]);
  OCMStub([subscriber_mock sharedInstance]).andReturn(subscriber_mock);

  MetricsMediatorMock* mediator = [[MetricsMediatorMock alloc] init];
  // On startup prior to registration, user-triggered update does not
  // immediately setEnabled.
  [mediator updateMetricsStateBasedOnPrefsUserTriggered:YES];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  // When deferred task runs, registration happens.
  OCMExpect([subscriber_mock setEnabled:YES]);
  [mediator registerMetricKitSubscriberIfNeeded];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  // Subsequent user-triggered preference updates after initialization update
  // immediately.
  OCMExpect([subscriber_mock setEnabled:YES]);
  [mediator updateMetricsStateBasedOnPrefsUserTriggered:YES];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  [subscriber_mock stopMocking];
}

// Test that MetricKit registration happens immediately when in safe mode.
TEST_F(MetricsMediatorDeferralTest, ImmediateMetricKitRegistrationInSafeMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMetrickitDeferRegistration);

  id safe_mode_mock = OCMClassMock([SafeModeCoordinator class]);
  OCMStub([safe_mode_mock shouldStart]).andReturn(YES);

  id subscriber_mock = OCMStrictClassMock([MetricKitSubscriber class]);
  OCMStub([subscriber_mock sharedInstance]).andReturn(subscriber_mock);
  OCMExpect([subscriber_mock setEnabled:YES]);

  MetricsMediatorMock* mediator = [[MetricsMediatorMock alloc] init];
  [mediator updateMetricsStateBasedOnPrefsUserTriggered:NO];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  [subscriber_mock stopMocking];
  [safe_mode_mock stopMocking];
}

// Test that MetricKit registration happens immediately when the deferred
// registration feature flag is disabled.
TEST_F(MetricsMediatorDeferralTest,
       ImmediateMetricKitRegistrationWhenFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMetrickitDeferRegistration);

  id subscriber_mock = OCMStrictClassMock([MetricKitSubscriber class]);
  OCMStub([subscriber_mock sharedInstance]).andReturn(subscriber_mock);
  OCMExpect([subscriber_mock setEnabled:YES]);

  MetricsMediatorMock* mediator = [[MetricsMediatorMock alloc] init];
  [mediator updateMetricsStateBasedOnPrefsUserTriggered:NO];
  EXPECT_OCMOCK_VERIFY(subscriber_mock);

  [subscriber_mock stopMocking];
}
