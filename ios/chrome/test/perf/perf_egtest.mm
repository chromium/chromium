// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

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
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

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

// Measures scroll frame timing and dropped frames across vertical swipe
// gestures using manual Perfetto tracing and in-test SQL extraction.
- (void)testScrollJank {
  self.testServer->RegisterRequestHandler(base::BindRepeating(
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
  GREYAssertTrue(self.testServer->Start(),
                 @"EmbeddedTestServer failed to start.");

  const GURL scrollURL = self.testServer->GetURL("/scroll_jank.html");
  [ChromeEarlGrey loadURL:scrollURL];
  [ChromeEarlGrey waitForJavaScriptCondition:@"ready === true"];

  GREYAssertTrue([ChromeEarlGrey startManualTracingWithCategoryFilter:@"ui"],
                 @"Failed to start manual tracing.");

  static constexpr int kScrollCycles = 4;
  for (int i = 0; i < kScrollCycles; ++i) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  }

  NSString* traceFilePathString = [ChromeEarlGrey stopManualTracing];
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

  static constexpr char kScrollJankSqlQuery[] =
      "SELECT "
      "COALESCE(EXTRACT_ARG(arg_set_id, 'debug.dropped_frames'), 0) "
      "AS dropped_frames, "
      "(ts - LAG(ts) OVER (ORDER BY ts)) / 1e6 "
      "AS frame_interval_ms "
      "FROM slice "
      "WHERE category = 'ui' AND name = 'Display';";
  auto queryResult = ttp.ExecuteQuery(kScrollJankSqlQuery);
  GREYAssertTrue(queryResult.ok(), @"Failed to query Display frame slices: %s",
                 queryResult.error().c_str());
  GREYAssertTrue(queryResult.result().size() >= 3,
                 @"Expected at least 2 Display slices, got %zu rows.",
                 queryResult.result().size());

  // Match `UIViewControllerWithDisplayTracing`'s `kMaxSupportedFramePeriod`
  // (100 ms) so inter-swipe idle pauses are excluded while genuine dropped
  // frame intervals (`(dropped + 1) * 100 ms`) are retained.
  static constexpr double kMaxSupportedFramePeriodMs = 100.0;
  NSMutableArray<NSNumber*>* droppedSamples = [[NSMutableArray alloc] init];
  NSMutableArray<NSNumber*>* intervalSamples = [[NSMutableArray alloc] init];
  for (size_t row = 1; row < queryResult.result().size(); ++row) {
    const auto& cols = queryResult.result()[row];
    if (cols.size() < 2) {
      continue;
    }
    double dropped = 0.0;
    if (base::StringToDouble(cols[0], &dropped)) {
      [droppedSamples addObject:@(dropped)];
    }
    double intervalMs = 0.0;
    if (base::StringToDouble(cols[1], &intervalMs) && intervalMs > 0.0 &&
        intervalMs <= (dropped + 1.0) * kMaxSupportedFramePeriodMs) {
      [intervalSamples addObject:@(intervalMs)];
    }
  }

  GREYAssertTrue(droppedSamples.count > 0 && intervalSamples.count > 0,
                 @"Expected Display frame timing samples during scroll.");
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:droppedSamples
              forMetric:@"ScrollJank.DroppedFrames"
                   unit:@"count_smallerIsBetter"
              benchmark:@"bling.scroll"]);
  chrome_test_util::GREYAssertErrorNil([PerfAppInterface
      recordPerfSamples:intervalSamples
              forMetric:@"ScrollJank.FrameInterval"
                   unit:@"ms_smallerIsBetter"
              benchmark:@"bling.scroll"]);
}

@end
