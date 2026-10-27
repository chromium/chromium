// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <cstdint>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/tracing/test_trace_processor_impl.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/perf/perf_app_interface.h"
#import "ios/chrome/test/perf/scroll_jank_metrics_util.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

// Perfetto SQL query joining each `SceneViewController` `Display` slice to its
// flow-linked `CATransactionCommit` and `CADisplayLinkDispatch` slices in the
// `ui` category to extract presentation timestamps and per-frame main-thread
// work.
//
// Why `ScrollJank.DroppedFrames` and raw per-frame `ScrollJank.FrameInterval`
// were replaced (crbug.com/570775933):
// 1. `debug.dropped_frames` in `UIViewControllerWithDisplayTracing` only
//    measures commit overruns within an active `UIUpdateLink` pass. Main-thread
//    runloop stalls between update passes begin a fresh pass with a future
//    `estimatedPresentationTime` and report `dropped_frames = 0`. On ProMotion
//    devices, stalls < 100 ms also overwrite `_currentFramePeriodEstimate`, and
//    recording ~2,400 per-frame zeros dilutes the histogram median (`p50`) to
//    `0`.
// 2. Raw per-frame `FrameInterval` primarily tracks ProMotion refresh-rate
//    step-downs during scroll deceleration (`8.33 ms` at 120 Hz to `16.67 ms`
//    at 60 Hz) and counted same-VSync duplicate slices ~0.002 ms apart rather
//    than scroll jank.
constexpr char kScrollJankDisplayFramesSqlQuery[] =
    "SELECT "
    "  d.ts AS display_ts, "
    "  (COALESCE(dl.dur, 0) + COALESCE(c.dur, 0)) / 1e6 AS "
    "main_thread_work_ms "
    "FROM slice d "
    "LEFT JOIN flow f_cd ON f_cd.slice_in = d.id "
    "LEFT JOIN slice c ON f_cd.slice_out = c.id AND c.name = "
    "'CATransactionCommit' "
    "LEFT JOIN flow f_dc ON f_dc.slice_in = c.id "
    "LEFT JOIN slice dl ON f_dc.slice_out = dl.id AND dl.name = "
    "'CADisplayLinkDispatch' "
    "WHERE d.category = 'ui' AND d.name = 'Display' "
    "  AND EXTRACT_ARG(d.arg_set_id, 'debug.controller') = "
    "'SceneViewController' "
    "ORDER BY d.ts;";

struct ScrollJankSwipeSamples {
  NSMutableArray<NSNumber*>* hitch_time_ratios_ms_per_s = nil;
  NSMutableArray<NSNumber*>* frame_interval_p95s_ms = nil;
  NSMutableArray<NSNumber*>* main_thread_frame_works_ms = nil;
};

// Parses the manual Perfetto trace at `traceFilePathString`, removes the trace
// file, and extracts per-swipe `HitchTimeRatio` (ms/s), `FrameIntervalP95`
// (ms), and `MainThreadFrameWork` (ms) samples using the shared deduplication
// (`>= 4 ms`) and swipe segmentation (`<= 180 ms`, `>= 20` frames) helper.
ScrollJankSwipeSamples ExtractScrollJankMetrics(NSString* traceFilePathString,
                                                size_t maxSwipes = 0) {
  GREYAssertTrue(traceFilePathString.length > 0, @"Trace file path is empty.");
  NSData* traceData = [ChromeEarlGrey traceDataForPath:traceFilePathString];
  GREYAssertTrue(traceData && traceData.length > 0,
                 @"Failed to read trace file contents.");
  GREYAssertTrue([ChromeEarlGrey removeTraceFileAtPath:traceFilePathString],
                 @"Failed to remove manual trace file.");

  auto traceSpan = base::as_chars(base::apple::NSDataToSpan(traceData));
  std::vector<char> rawData(traceSpan.begin(), traceSpan.end());

  base::test::TestTraceProcessorImpl ttp;
  auto status = ttp.ParseTrace(rawData);
  GREYAssertTrue(status.ok(), @"Failed to parse Perfetto trace: %s",
                 std::string(status.message()).c_str());

  auto queryResult = ttp.ExecuteQuery(kScrollJankDisplayFramesSqlQuery);
  GREYAssertTrue(queryResult.ok(), @"Failed to query Display frame slices: %s",
                 queryResult.error().c_str());
  GREYAssertTrue(queryResult.result().size() >= 3,
                 @"Expected at least 2 Display slices, got %zu rows.",
                 queryResult.result().size());

  std::vector<ios_perf::DisplayFrameSample> rawFrames;
  rawFrames.reserve(queryResult.result().size() - 1);
  for (size_t row = 1; row < queryResult.result().size(); ++row) {
    const auto& cols = queryResult.result()[row];
    if (cols.size() < 2) {
      continue;
    }
    int64_t tsNs = 0;
    double workMs = 0.0;
    if (base::StringToInt64(cols[0], &tsNs) &&
        base::StringToDouble(cols[1], &workMs)) {
      rawFrames.push_back({tsNs, workMs});
    }
  }

  const std::vector<ios_perf::SwipeScrollJankMetrics> swipeMetrics =
      ios_perf::ComputeScrollJankSwipeMetrics(rawFrames, maxSwipes);

  ScrollJankSwipeSamples samples;
  samples.hitch_time_ratios_ms_per_s =
      [[NSMutableArray alloc] initWithCapacity:swipeMetrics.size()];
  samples.frame_interval_p95s_ms =
      [[NSMutableArray alloc] initWithCapacity:swipeMetrics.size()];
  samples.main_thread_frame_works_ms =
      [[NSMutableArray alloc] initWithCapacity:swipeMetrics.size()];
  for (const ios_perf::SwipeScrollJankMetrics& swipe : swipeMetrics) {
    [samples.hitch_time_ratios_ms_per_s
        addObject:@(swipe.hitch_time_ratio_ms_per_s)];
    [samples.frame_interval_p95s_ms addObject:@(swipe.frame_interval_p95_ms)];
    [samples.main_thread_frame_works_ms
        addObject:@(swipe.main_thread_frame_work_ms)];
  }
  return samples;
}

// Verifies that `samples` contains `expectedSwipeCount` per-swipe values for
// all three metrics and records them under `metricPrefix` in
// `perf_samples.json`.
void RecordScrollJankMetrics(const ScrollJankSwipeSamples& samples,
                             NSString* metricPrefix,
                             NSUInteger expectedSwipeCount) {
  GREYAssertEqual(samples.hitch_time_ratios_ms_per_s.count, expectedSwipeCount,
                  @"Expected %lu HitchTimeRatio samples for %@.",
                  static_cast<unsigned long>(expectedSwipeCount), metricPrefix);
  GREYAssertEqual(samples.frame_interval_p95s_ms.count, expectedSwipeCount,
                  @"Expected %lu FrameIntervalP95 samples for %@.",
                  static_cast<unsigned long>(expectedSwipeCount), metricPrefix);
  GREYAssertEqual(samples.main_thread_frame_works_ms.count, expectedSwipeCount,
                  @"Expected %lu MainThreadFrameWork samples for %@.",
                  static_cast<unsigned long>(expectedSwipeCount), metricPrefix);

  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:samples.hitch_time_ratios_ms_per_s
              forMetric:[NSString
                            stringWithFormat:@"%@.HitchTimeRatio", metricPrefix]
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.scroll"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:samples.frame_interval_p95s_ms
              forMetric:[NSString stringWithFormat:@"%@.FrameIntervalP95",
                                                   metricPrefix]
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.scroll"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:samples.main_thread_frame_works_ms
              forMetric:[NSString stringWithFormat:@"%@.MainThreadFrameWork",
                                                   metricPrefix]
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.scroll"]);
}

// Registers `/scroll_jank.html` on `testServer`, starts the server, loads the
// page, and waits for 5 `requestAnimationFrame` ticks to settle.
void LoadScrollJankTestPage(net::EmbeddedTestServer* testServer) {
  testServer->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/scroll_jank.html") {
          return nullptr;
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/html");
        response->set_content(
            "<!doctype html>\n"
            "<meta name='viewport' content='width=device-width'>\n"
            "<style>\n"
            "  body { margin: 0; font-family: sans-serif; }\n"
            "  .row { height: 120px; border-bottom: 1px solid #ccc; "
            "padding: 16px; }\n"
            "</style>\n"
            "<div id='root'></div>\n"
            "<script>\n"
            "  const root = document.getElementById('root');\n"
            "  for (let i = 0; i < 200; i++) {\n"
            "    const newDiv = document.createElement('div');\n"
            "    newDiv.className = 'row';\n"
            "    newDiv.textContent = 'Item ' + i;\n"
            "    root.appendChild(newDiv);\n"
            "  }\n"
            "  var ready = false;\n"
            "  var frameCount = 0;\n"
            "  function checkReady() {\n"
            "    if (frameCount == 5) {\n"
            "      ready = true;\n"
            "    } else {\n"
            "      frameCount++;\n"
            "      requestAnimationFrame(checkReady);\n"
            "    }\n"
            "  }\n"
            "  requestAnimationFrame(checkReady);\n"
            "</script>");
        return response;
      }));
  GREYAssertTrue(testServer->Start(), @"EmbeddedTestServer failed to start.");

  const GURL scrollURL = testServer->GetURL("/scroll_jank.html");
  [ChromeEarlGrey loadURL:scrollURL];
  [ChromeEarlGrey waitForJavaScriptCondition:@"ready === true"];
}

}  // namespace

@interface PerfTestCase : ChromeTestCase
@end

@implementation PerfTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Match production field trial configuration rather than default test
  // behavior.
  config.additional_args.push_back("--enable-field-trial-config");
  // Disable GWP-ASan so random guard page allocations do not skew
  // `Memory.PhysFootprint` measurements across iterations.
  config.additional_args.push_back(
      "--disable-features=GwpAsanMalloc,GwpAsanPartitionAlloc");
  return config;
}

#pragma mark - Tests

// Measures cold startup latency and startup memory footprint across cold
// process relaunches using unbucketed values from StartupInformation.
- (void)testStartup {
  static constexpr int kColdLaunchIterations = 5;
  static constexpr base::TimeDelta kStartupMetricsTimeout = base::Seconds(10);
  // Deferred initialization blocks wait 3 seconds after first scene activation
  // and run sequentially with a 200ms inter-block delay.
  static constexpr base::TimeDelta kStartupMemoryTimeout = base::Seconds(15);
  NSMutableArray<NSNumber*>* preMainSamples = [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* fromMainSamples = [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* sceneConnectSamples =
      [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* pageInsSamples = [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* footprintSamples = [[NSMutableArray alloc] init];

  for (int i = 0; i < kColdLaunchIterations; ++i) {
    AppLaunchConfiguration config = [self appConfigurationForTestCase];
    config.relaunch_policy = ForceRelaunchByKilling;
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:config];

    __block NSDictionary<NSString*, NSNumber*>* timings = nil;
    GREYCondition* waitForColdStartMetric = [GREYCondition
        conditionWithName:@"Wait for ColdStartFromMain metric"
                    block:^BOOL {
                      timings = [PerfAppInterface
                          currentStartupTimingsInMilliseconds];
                      return timings[@"Startup.ColdStartFromMain"] != nil;
                    }];
    GREYAssertTrue(
        [waitForColdStartMetric
            waitWithTimeout:kStartupMetricsTimeout.InSecondsF()],
        @"Timed out waiting for Startup.ColdStartFromMain on iteration %d.", i);

    NSNumber* preMain = timings[@"Startup.ColdStartPreMain"];
    NSNumber* fromMain = timings[@"Startup.ColdStartFromMain"];
    NSNumber* sceneConnect = timings[@"Startup.TimeFromMainToSceneConnection"];
    double pageIns = [PerfAppInterface currentProcessPageIns];
    if (preMain) {
      [preMainSamples addObject:preMain];
    }
    if (fromMain) {
      [fromMainSamples addObject:fromMain];
    }
    if (sceneConnect) {
      [sceneConnectSamples addObject:sceneConnect];
    }
    if (pageIns >= 0.0) {
      [pageInsSamples addObject:@(pageIns)];
    }

    // Avoid eDO polling during the 3-second quiet window before deferred
    // blocks begin executing.
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(3));

    __block double footprintBytes = -1.0;
    GREYCondition* waitForMemoryFootprint = [GREYCondition
        conditionWithName:@"Wait for Startup.MemoryFootprint metric"
                    block:^BOOL {
                      footprintBytes =
                          [PerfAppInterface startupMemoryFootprintBytes];
                      return footprintBytes > 0.0;
                    }];
    GREYAssertTrue(
        [waitForMemoryFootprint
            waitWithTimeout:kStartupMemoryTimeout.InSecondsF()],
        @"Timed out waiting for Startup.MemoryFootprint on iteration %d.", i);
    [footprintSamples addObject:@(footprintBytes)];
  }

  GREYAssertEqual(fromMainSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected ColdStartFromMain samples from all iterations.");
  GREYAssertEqual(preMainSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected ColdStartPreMain samples from all iterations.");
  GREYAssertEqual(
      sceneConnectSamples.count, static_cast<NSUInteger>(kColdLaunchIterations),
      @"Expected TimeFromMainToSceneConnection samples from all iterations.");
  GREYAssertEqual(pageInsSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected ColdStartPageIns samples from all iterations.");
  GREYAssertEqual(footprintSamples.count,
                  static_cast<NSUInteger>(kColdLaunchIterations),
                  @"Expected MemoryFootprint samples from all iterations.");
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:fromMainSamples
              forMetric:@"Startup.ColdStartFromMain"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:preMainSamples
              forMetric:@"Startup.ColdStartPreMain"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:sceneConnectSamples
              forMetric:@"Startup.TimeFromMainToSceneConnection"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:pageInsSamples
              forMetric:@"Startup.ColdStartPageIns"
                   unit:@"count_smallerIsBetter"
              benchmark:@"bling.startup"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:footprintSamples
              forMetric:@"Startup.MemoryFootprint"
                   unit:@"sizeInBytes_smallerIsBetter"
              benchmark:@"bling.startup"]);
}

// Measures browser kernel physical memory footprint across tab open/close
// cycles using TASK_VM_INFO phys_footprint.
- (void)testOpenCloseTab {
  static constexpr int kOpenCloseIterations = 5;
  static constexpr base::TimeDelta kMemorySettleDelay = base::Seconds(1);
  NSMutableArray<NSNumber*>* footprintSamples = [[NSMutableArray alloc] init];

  for (int i = 0; i < kOpenCloseIterations; ++i) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TabGridOverflowMenuButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                            TabGridOverflowMenuCloseAllButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                       IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABSANDGROUPS)]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
        performAction:grey_tap()];

    // Allow transient UI and autorelease allocations to settle before reading
    // kernel phys_footprint.
    base::test::ios::SpinRunLoopWithMinDelay(kMemorySettleDelay);

    double footprintBytes = [PerfAppInterface currentProcessPhysFootprintBytes];
    GREYAssertTrue(footprintBytes > 0.0,
                   @"Expected positive TASK_VM_INFO phys_footprint.");
    [footprintSamples addObject:@(footprintBytes)];
  }

  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:footprintSamples
              forMetric:@"Memory.PhysFootprint"
                   unit:@"sizeInBytes_smallerIsBetter"
              benchmark:@"bling.memory"]);
}

// Measures Speedometer 3.1 benchmark score served from third_party/speedometer.
- (void)testSpeedometer {
  if (!self.testServer->Started()) {
    self.testServer->ServeFilesFromSourceDirectory(
        "third_party/speedometer/v3.1");
    GREYAssertTrue(self.testServer->Start(),
                   @"EmbeddedTestServer failed to start.");
  }

  const GURL speedometerURL =
      self.testServer->GetURL("/index.html?startAutomatically=true");
  [ChromeEarlGrey loadURL:speedometerURL];

  NSString* const kGetSpeedometerScoreScript =
      @"(function() {"
      @"  var el = document.getElementById('result-number');"
      @"  return el ? el.textContent.trim() : '';"
      @"})()";

  __block std::string scoreString;
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for Speedometer to complete"
                  block:^BOOL {
                    base::Value result = [ChromeEarlGrey
                        evaluateJavaScript:kGetSpeedometerScoreScript];
                    if (result.is_string() && !result.GetString().empty()) {
                      scoreString = result.GetString();
                      return YES;
                    }
                    return NO;
                  }];

  // Delay initial JS polling so benchmark execution isn't perturbed by main
  // thread evaluation during the core run.
  static constexpr base::TimeDelta kInitialDelay = base::Seconds(45);
  static constexpr base::TimeDelta kPollInterval = base::Seconds(15);
  static constexpr base::TimeDelta kTimeout = base::Minutes(10);

  base::test::ios::SpinRunLoopWithMinDelay(kInitialDelay);
  GREYAssertTrue(
      [condition waitWithTimeout:(kTimeout - kInitialDelay).InSecondsF()
                    pollInterval:kPollInterval.InSecondsF()],
      @"Speedometer failed to finish within timeout.");

  double score = 0.0;
  GREYAssertTrue(base::StringToDouble(scoreString, &score) && score > 0.0,
                 @"Speedometer score must be a positive number.");

  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:@[ @(score) ]
              forMetric:@"Speedometer3.TotalScore"
                   unit:@"unitless_biggerIsBetter"
              benchmark:@"bling.speedometer"]);
}

// Measures per-swipe scroll jank metrics (`HitchTimeRatio`, `FrameIntervalP95`,
// and `MainThreadFrameWork`) across 4 vertical up/down swipe cycles (8 swipes
// per surface) on both web content (`ScrollJank.*`) and the 30-tab
// Chrome-native Tab Grid (`ScrollJank.TabGrid.*`).
- (void)testScrollJank {
  static constexpr int kScrollCycles = 4;
  static constexpr NSUInteger kExpectedSwipesPerSurface = kScrollCycles * 2;
  static constexpr int kTabGridTabCount = 30;

  LoadScrollJankTestPage(self.testServer);

  GREYAssertTrue([ChromeEarlGrey startManualTracingWithCategoryFilter:@"ui"],
                 @"Failed to start manual tracing for web scroll.");
  for (int i = 0; i < kScrollCycles; ++i) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  }
  ScrollJankSwipeSamples webSamples = ExtractScrollJankMetrics(
      [ChromeEarlGrey stopManualTracing], kExpectedSwipesPerSurface);
  RecordScrollJankMetrics(webSamples, @"ScrollJank", kExpectedSwipesPerSurface);

  // Measure Chrome-native Tab Grid scroll with 30 open tabs so browser
  // main-thread UICollectionView cell layout and rendering work is exercised.
  // The 30 background tabs never load, so their grid cells have no snapshots.
  // That keeps the measurement stable but doesn't cover snapshot decoding. A
  // follow-up could load real pages.
  [PerfAppInterface openBackgroundNewTabs:kTabGridTabCount];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertTrue([ChromeEarlGrey startManualTracingWithCategoryFilter:@"ui"],
                 @"Failed to start manual tracing for Tab Grid scroll.");
  for (int i = 0; i < kScrollCycles; ++i) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  }
  ScrollJankSwipeSamples tabGridSamples = ExtractScrollJankMetrics(
      [ChromeEarlGrey stopManualTracing], kExpectedSwipesPerSurface);
  RecordScrollJankMetrics(tabGridSamples, @"ScrollJank.TabGrid",
                          kExpectedSwipesPerSurface);
}

// Verifies that injecting a 70 ms main-thread stall in both `NSTimer` and
// `CADisplayLink` modes moves `HitchTimeRatio` (> 100 ms/s), `FrameIntervalP95`
// (> 50 ms), and `MainThreadFrameWork` (noticeably above baseline) on both the
// web surface and the 30-tab Chrome-native Tab Grid without recording samples
// to `perf_samples.json`.
//
// Disabled by default so CI skips it (`+[ChromeTestCase testInvocations]`
// filters `DISABLED_` tests unless `RUN_DISABLED_EARL_GREY_TESTS` is set).
// Run manually on simulator or device with:
//   TEST_RUNNER_RUN_DISABLED_EARL_GREY_TESTS=1 xcodebuild test-without-building
//     -project out/build/all.xcodeproj
//     -scheme ios_chrome_perf_benchmarks_eg2tests_module
//     -destination 'platform=iOS,id=<UDID>'
//     CONFIGURATION_BUILD_DIR="$(pwd)/out/Debug-iphoneos"
//     -only-testing:ios_chrome_perf_benchmarks_eg2tests_module/PerfTestCase/DISABLED_testScrollJankStallSanityCheck
- (void)DISABLED_testScrollJankStallSanityCheck {
  static constexpr int kTabGridTabCount = 30;
  static constexpr int kStallIntervalMs = 120;
  static constexpr int kStallStride = 4;
  static constexpr int kStallDurationMs = 70;
  static constexpr double kExpectedMinHitchTimeRatioMsPerSec = 100.0;
  static constexpr double kExpectedMinP95FrameIntervalMs = 50.0;
  static constexpr double kExpectedMinMainThreadFrameWorkMs = 5.0;

  LoadScrollJankTestPage(self.testServer);

  auto verifyStalledSurface = ^(id<GREYMatcher> scrollMatcher,
                                NSString* surfaceName) {
    // 1. NSTimer stall mode (inter-pass runloop stalls between UIUpdateLink
    // passes).
    GREYAssertTrue([ChromeEarlGrey startManualTracingWithCategoryFilter:@"ui"],
                   @"Failed to start manual tracing for %@ NSTimer stall.",
                   surfaceName);
    [PerfAppInterface startMainThreadStallWithIntervalMs:kStallIntervalMs
                                              durationMs:kStallDurationMs];
    [[EarlGrey selectElementWithMatcher:scrollMatcher]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
    [[EarlGrey selectElementWithMatcher:scrollMatcher]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
    [PerfAppInterface stopMainThreadStall];

    ScrollJankSwipeSamples timerSamples =
        ExtractScrollJankMetrics([ChromeEarlGrey stopManualTracing]);
    GREYAssertTrue(timerSamples.hitch_time_ratios_ms_per_s.count >= 1,
                   @"Expected per-swipe samples for %@ NSTimer stall.",
                   surfaceName);
    for (NSNumber* htr in timerSamples.hitch_time_ratios_ms_per_s) {
      GREYAssertTrue(htr.doubleValue > kExpectedMinHitchTimeRatioMsPerSec,
                     @"%@ NSTimer stall expected HitchTimeRatio > %.1f ms/s, "
                     @"got %.2f",
                     surfaceName, kExpectedMinHitchTimeRatioMsPerSec,
                     htr.doubleValue);
    }
    for (NSNumber* p95 in timerSamples.frame_interval_p95s_ms) {
      GREYAssertTrue(p95.doubleValue > kExpectedMinP95FrameIntervalMs,
                     @"%@ NSTimer stall expected FrameIntervalP95 > %.1f ms, "
                     @"got %.2f",
                     surfaceName, kExpectedMinP95FrameIntervalMs,
                     p95.doubleValue);
    }
    NSLog(@"[ScrollJankSanity] %@ NSTimer stall: HTR=%@ P95=%@ Work=%@",
          surfaceName, timerSamples.hitch_time_ratios_ms_per_s,
          timerSamples.frame_interval_p95s_ms,
          timerSamples.main_thread_frame_works_ms);

    // 2. CADisplayLink stall mode (in-pass stalls inside CADisplayLinkDispatch,
    // moving HitchTimeRatio, FrameIntervalP95, and MainThreadFrameWork).
    GREYAssertTrue([ChromeEarlGrey startManualTracingWithCategoryFilter:@"ui"],
                   @"Failed to start manual tracing for %@ CADisplayLink "
                   @"stall.",
                   surfaceName);
    [PerfAppInterface startCADisplayLinkStallWithStride:kStallStride
                                             durationMs:kStallDurationMs];
    [[EarlGrey selectElementWithMatcher:scrollMatcher]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
    [[EarlGrey selectElementWithMatcher:scrollMatcher]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
    [PerfAppInterface stopMainThreadStall];

    ScrollJankSwipeSamples dlSamples =
        ExtractScrollJankMetrics([ChromeEarlGrey stopManualTracing]);
    GREYAssertTrue(dlSamples.hitch_time_ratios_ms_per_s.count >= 1,
                   @"Expected per-swipe samples for %@ CADisplayLink stall.",
                   surfaceName);
    for (NSNumber* htr in dlSamples.hitch_time_ratios_ms_per_s) {
      GREYAssertTrue(htr.doubleValue > kExpectedMinHitchTimeRatioMsPerSec,
                     @"%@ CADisplayLink stall expected HitchTimeRatio > %.1f "
                     @"ms/s, got %.2f",
                     surfaceName, kExpectedMinHitchTimeRatioMsPerSec,
                     htr.doubleValue);
    }
    for (NSNumber* p95 in dlSamples.frame_interval_p95s_ms) {
      GREYAssertTrue(p95.doubleValue > kExpectedMinP95FrameIntervalMs,
                     @"%@ CADisplayLink stall expected FrameIntervalP95 > %.1f "
                     @"ms, got %.2f",
                     surfaceName, kExpectedMinP95FrameIntervalMs,
                     p95.doubleValue);
    }
    for (NSNumber* work in dlSamples.main_thread_frame_works_ms) {
      GREYAssertTrue(work.doubleValue > kExpectedMinMainThreadFrameWorkMs,
                     @"%@ CADisplayLink stall expected MainThreadFrameWork > "
                     @"%.1f ms, got %.2f",
                     surfaceName, kExpectedMinMainThreadFrameWorkMs,
                     work.doubleValue);
    }
    NSLog(@"[ScrollJankSanity] %@ CADisplayLink stall: HTR=%@ P95=%@ Work=%@",
          surfaceName, dlSamples.hitch_time_ratios_ms_per_s,
          dlSamples.frame_interval_p95s_ms,
          dlSamples.main_thread_frame_works_ms);
  };

  verifyStalledSurface(chrome_test_util::WebStateScrollViewMatcher(), @"Web");

  [PerfAppInterface openBackgroundNewTabs:kTabGridTabCount];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];

  verifyStalledSurface(chrome_test_util::RegularTabGrid(), @"TabGrid");
}

@end
