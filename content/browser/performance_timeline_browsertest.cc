// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

class PerformanceTimelineBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  [[nodiscard]] EvalJsResult GetNavigationId(const std::string& name) {
    const char kGetPerformanceEntryTemplate[] = R"(
        (() => {performance.mark($1);
        return performance.getEntriesByName($1)[0].navigationId;})();
    )";
    std::string script = JsReplace(kGetPerformanceEntryTemplate, name);
    return EvalJs(shell(), script);
  }

  void WaitForFrameReady() {
    // We should wait for the main frame's hit-test data to be ready before
    // sending the click event below to avoid flakiness.
    content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
    // Ensure the compositor thread is aware of the mouse events.
    content::MainThreadFrameObserver frame_observer(GetRenderWidgetHost());
    frame_observer.Wait();
  }

  void StartTracing() {
    base::RunLoop wait_for_tracing;
    content::TracingController::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(
            "{\"included_categories\": [\"devtools.timeline\"]}"),
        wait_for_tracing.QuitClosure());
    wait_for_tracing.Run();
  }

  std::string StopTracing() {
    base::RunLoop wait_for_tracing;
    std::string trace_output;
    content::TracingController::GetInstance()->StopTracing(
        content::TracingController::CreateStringEndpoint(
            base::BindLambdaForTesting(
                [&](std::unique_ptr<std::string> trace_str) {
                  trace_output = std::move(*trace_str);
                  wait_for_tracing.Quit();
                })));
    wait_for_tracing.Run();
    return trace_output;
  }

  trace_analyzer::TraceEventVector ExtractTraceEventsByName(
      const std::string& trace_str,
      const std::string& event_name) {
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
        trace_analyzer::TraceAnalyzer::Create(trace_str));
    trace_analyzer::TraceEventVector events;
    auto query = trace_analyzer::Query::EventNameIs(event_name);
    analyzer->FindEvents(query, &events);
    return events;
  }

  content::RenderWidgetHost* GetRenderWidgetHost() {
    EXPECT_TRUE(web_contents());
    return web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost();
  }

  // This method is to get the first UKM entry of a repeated event.
  ukm::mojom::UkmEntryPtr GetFirstEntryValue(std::string_view entry_name) {
    auto merged_entries = ukm_recorder()->GetMergedEntriesByName(entry_name);
    EXPECT_EQ(1ul, merged_entries.size());
    const auto& kv = merged_entries.begin();
    return std::move(kv->second);
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PerformanceTimelineBrowserTest,
                       NoResourceTimingEntryForFileProtocol) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));

  file_path = file_path.Append(GetTestDataFilePath())
                  .AppendASCII(
                      "performance_timeline/"
                      "resource-timing-not-for-file-protocol.html");

  EXPECT_TRUE(NavigateToURL(shell(), GetFileUrlWithQuery(file_path, "")));

  // The test html page references 2 css file. One is present and would be
  // loaded via file protocol and the other is not present and would have load
  // failure. Both should not emit a resource timing entry.
  EXPECT_EQ(
      0, EvalJs(shell(),
                "window.performance.getEntriesByType('resource').filter(e=>e."
                "name.includes('css')).length;"));

  std::string applied_style_color = "rgb(0, 128, 0)";

  // Verify that style.css is fetched by verifying color green is applied.
  EXPECT_EQ(applied_style_color, EvalJs(shell(), "getTextColor()"));

  // If the same page is loaded via http protocol, both the successful load nad
  // failure load should emit a resource timing entry.
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/"
      "resource-timing-not-for-file-protocol.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      2, EvalJs(shell(),
                "window.performance.getEntriesByType('resource').filter(e=>e."
                "name.includes('css')).length;"));

  // Verify that style.css is fetched by verifying color green is applied.
  EXPECT_EQ(applied_style_color, EvalJs(shell(), "getTextColor()"));

  // Verify that style.css that is fetched has its resource timing entry.
  EXPECT_EQ(
      1, EvalJs(shell(),
                "window.performance.getEntriesByType('resource').filter(e=>e."
                "name.includes('resources/style.css')).length;"));

  // Verify that non_exist.css that is not fetched has its resource timing
  // entry.
  EXPECT_EQ(
      1, EvalJs(shell(),
                "window.performance.getEntriesByType('resource').filter(e=>e."
                "name.includes('resources/non_exist_style.css')).length;"));
}

class PerformanceTimelineLCPStartTimePrecisionBrowserTest
    : public PerformanceTimelineBrowserTest {
 protected:
  EvalJsResult GetIsEqualToPrecision() const {
    std::string script =
        content::JsReplace("isEqualToPrecision($1);", getPrecision());
    return EvalJs(shell(), script);
  }

  int32_t getPrecision() const { return precision_; }

 private:
  int32_t precision_ = 10;
};

IN_PROC_BROWSER_TEST_F(PerformanceTimelineLCPStartTimePrecisionBrowserTest,
                       LCPStartTimePrecision) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/lcp-start-time-precision.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  EXPECT_TRUE(GetIsEqualToPrecision().ExtractBool());
}

class PerformanceTimelineNavigationIdBrowserTest
    : public PerformanceTimelineBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PerformanceTimelineBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("--enable-blink-test-features");
  }
};

// This test case is to verify PerformanceEntry.navigationId gets incremented
// for each back/forward cache restore.
IN_PROC_BROWSER_TEST_F(PerformanceTimelineNavigationIdBrowserTest,
                       BackForwardCacheRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  const int initial_navigation_id = GetNavigationId("first_nav").ExtractInt();
  // Navigate away and back 3 times. The 1st time is to verify the
  // navigation id is incremented. The 2nd time is to verify that the id is
  // incremented on the same restored document. The 3rd time is to
  // verify the increment does not stop at 2.
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  int prev_navigation_id = initial_navigation_id;

  for (int i = 1; i <= 3; i++) {
    // Navigate away
    ASSERT_TRUE(NavigateToURL(shell(), url2));

    // Verify `rfh_a` is stored in back/forward cache in case back/forward cache
    // feature is enabled.
    if (IsBackForwardCacheEnabled()) {
      ASSERT_TRUE(rfh_a->IsInBackForwardCache());
    } else {
      // Verify `rfh_a` is deleted in case back/forward cache feature is
      // disabled.
      ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
    }

    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    // Verify navigation id is re-generated each time in case back/forward
    // cache feature is enabled. Verify navigation id is not changed in case
    // back/forward cache feature is not enabled.
    int curr_navigation_id =
        GetNavigationId("subsequent_nav" + base::NumberToString(i))
            .ExtractInt();
    EXPECT_NE(curr_navigation_id, prev_navigation_id);
    EXPECT_NE(curr_navigation_id, initial_navigation_id);

    prev_navigation_id = curr_navigation_id;
  }
}

class PerformanceTimelinePrefetchTransferSizeBrowserTest
    : public PerformanceTimelineBrowserTest {
 protected:
  EvalJsResult Prefetch() {
    std::string script = R"(
        (() => {
          return addPrefetch();
        })();
    )";
    return EvalJs(shell(), script);
  }
  [[nodiscard]] EvalJsResult GetTransferSize() {
    std::string script = R"(
        (() => {
          return performance.getEntriesByType('navigation')[0].transferSize;
        })();
    )";
    return EvalJs(shell(), script);
  }
};

IN_PROC_BROWSER_TEST_F(PerformanceTimelinePrefetchTransferSizeBrowserTest,
                       PrefetchTransferSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL prefetch_url(
      embedded_test_server()->GetURL("a.com", "/cacheable.html"));
  const GURL landing_url(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/prefetch.html"));

  EXPECT_TRUE(NavigateToURL(shell(), landing_url));
  Prefetch();
  EXPECT_TRUE(NavigateToURL(shell(), prefetch_url));
  // Navigate to a HTTP-cached prefetched url should result in a navigation
  // timing entry with 0 transfer size since the HTTP cache gets used.
  EXPECT_EQ(0, GetTransferSize());
}

class PerformanceTimelineBackForwardCacheRestorationBrowserTest
    : public PerformanceTimelineBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkTestFeatures,
                                    "NavigationId");
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  EvalJsResult GetBackForwardCacheRestorationEntriesByObserver() const {
    std::string script = R"(
      (
        async ()=>Promise.all([entryTypesPromise, typePromise])
      )();
    )";
    return EvalJs(shell(), script);
  }

  EvalJsResult GetDroppedEntriesCount() const {
    std::string script = R"(
      (
        async ()=> {
          let promise =  new Promise(resolve=>{
                new PerformanceObserver((list, observer, options) => {
                  resolve(options['droppedEntriesCount']);
                }).observe({ type: 'back-forward-cache-restoration',
                buffered: true });
              });
          return await promise;
        }
      )();
    )";
    return EvalJs(shell(), script);
  }

  EvalJsResult SetBackForwardCacheRestorationBufferSize(int size) const {
    std::string script = R"(
        internals.setBackForwardCacheRestorationBufferSize($1);
    )";
    script = content::JsReplace(script, size);
    return EvalJs(shell(), script);
  }

  EvalJsResult RegisterPerformanceObservers(int max_size) const {
    std::string script = R"(
            let entryTypesEntries = [];
            var entryTypesPromise =  new Promise(resolve=>{
              new PerformanceObserver((list) => {
                const entries = list.getEntries().filter(
                  e => e.entryType == 'back-forward-cache-restoration').map(
                    e=>e.toJSON());;
                if (entries.length > 0) {
                  entryTypesEntries = entryTypesEntries.concat(entries);
                }
                if(entryTypesEntries.length>=$1){
                  resolve(entryTypesEntries);
                }
              }).observe({ entryTypes: ['back-forward-cache-restoration'] });
            });

            let typeEntries = [];
            var typePromise =  new Promise(resolve=>{
              new PerformanceObserver((list) => {
                const entries = list.getEntries().filter(
                  e => e.entryType == 'back-forward-cache-restoration').map(
                    e=>e.toJSON());
                if (entries.length > 0) {
                  typeEntries = typeEntries.concat(entries);
                }
                if(typeEntries.length>=$1){
                  resolve(typeEntries);
                }
              }).observe({type: 'back-forward-cache-restoration'});
            });
    )";
    script = content::JsReplace(script, max_size);
    return EvalJs(shell(), script);
  }

  // This method checks a list of performance entries of the
  // back-forward-cache-restoration type. Each entry is created when there is
  // a back/forward cache restoration.
  void CheckEntries(const base::ListValue lst,
                    int initial_navigation_id) const {
    int prev_navigation_id = initial_navigation_id;

    for (const auto& i : lst) {
      auto* dict = i.GetIfDict();
      EXPECT_TRUE(dict);
      EXPECT_EQ("", *dict->FindString("name"));
      EXPECT_EQ("back-forward-cache-restoration",
                *dict->FindString("entryType"));

      std::optional<int> curr_navigation_id = dict->FindInt("navigationId");
      // This verifies the navigation id changes each time a back/forward
      // restoration happens.
      EXPECT_NE(prev_navigation_id, *curr_navigation_id);

      prev_navigation_id = *curr_navigation_id;

      EXPECT_LE(dict->FindDouble("pageshowEventStart").value(),
                dict->FindDouble("pageshowEventEnd").value());
    }
  }
};

IN_PROC_BROWSER_TEST_F(
    PerformanceTimelineBackForwardCacheRestorationBrowserTest,
    Create) {
  if (!IsBackForwardCacheEnabled())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  RenderFrameHostImplWrapper rfh(current_frame_host());

  int buffer_size = 10;
  int num_of_loops = 12;

  SetBackForwardCacheRestorationBufferSize(buffer_size);
  RegisterPerformanceObservers(num_of_loops);

  int initial_navigation_id =
      GetNavigationId("initial_navigation_id").ExtractInt();
  for (int i = 0; i < num_of_loops; i++) {
    // Navigate away
    ASSERT_TRUE(NavigateToURL(shell(), url2));

    // Verify `rfh` is stored in back/forward cache.
    ASSERT_TRUE(rfh->IsInBackForwardCache());

    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));
  }
  auto result =
      GetBackForwardCacheRestorationEntriesByObserver().TakeValue().TakeList();
  CheckEntries(std::move(result[0]).TakeList(), initial_navigation_id);
  CheckEntries(std::move(result[1]).TakeList(), initial_navigation_id);

  // Size of back forward restoration buffer is smaller than the number of back
  // forward restoration instances expected by 2. Therefore the
  // droppedEntriesCount is expected to be 2.
  EXPECT_EQ(2, GetDroppedEntriesCount().ExtractInt());
}

class PerformanceEventTimingBrowserTest
    : public PerformanceTimelineBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PerformanceTimelineBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("--enable-blink-test-features");
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  content::EvalJsResult setEventTimingBufferSize(int size) const {
    std::string script = R"(
        internals.setEventTimingBufferSize($1);
        internals.stopResponsivenessMetricsUkmSampling();
    )";
    script = content::JsReplace(script, size);
    return EvalJs(web_contents()->GetPrimaryMainFrame(), script);
  }
};

IN_PROC_BROWSER_TEST_F(PerformanceEventTimingBrowserTest,
                       RecordingContinuesWhenBufferIsFull) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/event_timing.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WaitForFrameReady();

  int buffer_size = 2;
  setEventTimingBufferSize(buffer_size);

  StartTracing();

  // Expect UKM entry Responsiveness_UserInteraction be recorded.
  base::RunLoop ukm_loop1;
  ukm_recorder()->SetOnAddEntryCallback(
      ukm::builders::Responsiveness_UserInteraction::kEntryName,
      ukm_loop1.QuitClosure());

  // Registers 3 listeners for mouseup, pointerup, click respectively. All the 3
  // listener callbacks have to be invoked for the test to proceed. In this way,
  // we know at least 3 events occurred. The buffer size is set to 2.
  ASSERT_TRUE(EvalJs(web_contents(), "registerEventListeners()").ExtractBool());

  // Simulate a click which will trigger multiple events, pointerenter,
  // pointerover, mouseover,pointerdown, mousedown, mouseup, pointerup, click.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Wait for the listener callbacks to be invoked.
  ASSERT_TRUE(EvalJs(web_contents(), "waitForEvent()").ExtractBool());

  ukm_loop1.Run();

  // Retrieve the number of entries and the number of dropped entries with a
  // performance observer with the init option buffer being true. The number of
  // entries should be the size of the buffer.
  auto entry_cnt_and_dropped_entry_cnt =
      EvalJs(web_contents(), " getEntriesCntAndDroppedEntriesCnt()")
          .TakeValue()
          .TakeList();

  int num_event_entres = entry_cnt_and_dropped_entry_cnt[0].GetInt();
  EXPECT_EQ(num_event_entres, buffer_size);

  int num_dropped_entries = entry_cnt_and_dropped_entry_cnt[1].GetInt();
  EXPECT_GE(num_dropped_entries, 1);

  // Verify that at least buffer_size+1 events are emitted to tracing.
  auto trace_str = StopTracing();

  auto events = ExtractTraceEventsByName(trace_str, "EventTiming");
  EXPECT_GE(events.size(), 3ul);

  // Verify UKM entry Responsiveness_UserInteraction is recorded. This is a
  // repeated event but as there is only 1 interacton we expect only 1 record.
  auto ukm_entry = GetFirstEntryValue(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);

  // Verify the event type is TapOrClick, the integer representation of which
  // is 1.
  ukm::TestUkmRecorder::ExpectEntryMetric(
      ukm_entry.get(),
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 1);

  // The max duration is non-determinstic. We only verify it exists.
  ukm::TestUkmRecorder::EntryHasMetric(
      ukm_entry.get(),
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName);
}

class LongAnimationFrameStyleDurationBrowserTest
    : public PerformanceTimelineBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PerformanceTimelineBrowserTest::SetUpCommandLine(command_line);
    // Enable the experimental style duration feature
    command_line->AppendSwitchASCII("enable-blink-features",
                                    "LongAnimationFrameStyleDuration");
  }

  void StartStyleTracing() {
    base::test::TestFuture<void> future;
    TracingController::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(
            "{\"included_categories\": [\"blink\", \"blink_style\", "
            "\"devtools.timeline\", \"benchmark\"]}"),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  std::string StopStyleTracing() {
    base::test::TestFuture<std::unique_ptr<std::string>> future;
    TracingController::GetInstance()->StopTracing(
        TracingController::CreateStringEndpoint(future.GetCallback()));
    return std::move(*future.Get());
  }

  struct TraceStyleResult {
    double total_duration_ms = 0.0;
    size_t event_count = 0;
  };

  // Sum up durations of all style duration events in the trace.
  // This gives us the ground truth for style recalculation time.
  TraceStyleResult GetStyleDurationFromTrace(const std::string& trace_str) {
    TraceStyleResult result;
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
        trace_analyzer::TraceAnalyzer::Create(trace_str));
    if (!analyzer) {
      result.total_duration_ms = -1.0;
      return result;
    }

    // Associate begin and end events to get durations.
    analyzer->AssociateBeginEndEvents();

    trace_analyzer::TraceEventVector events;
    trace_analyzer::Query query =
        trace_analyzer::Query::EventNameIs("UpdateLayoutTree");
    analyzer->FindEvents(query, &events);

    double total_duration_us = 0.0;
    for (const trace_analyzer::TraceEvent* event : events) {
      total_duration_us += event->duration;
    }
    // Convert microseconds to milliseconds.
    result.total_duration_ms = total_duration_us / 1000.0;
    result.event_count = events.size();
    return result;
  }

  struct TraceLayoutResult {
    double total_duration_ms = 0.0;
    size_t event_count = 0;
  };

  // Sum up durations of all Layout events in the trace.
  // This gives us the ground truth for layout time.
  TraceLayoutResult GetLayoutDurationFromTrace(const std::string& trace_str) {
    TraceLayoutResult result;
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
        trace_analyzer::TraceAnalyzer::Create(trace_str));
    if (!analyzer) {
      result.total_duration_ms = -1.0;
      return result;
    }

    // Associate begin and end events to get durations.
    analyzer->AssociateBeginEndEvents();

    trace_analyzer::TraceEventVector events;
    // The trace event name is "LocalFrameView::layout" as emitted by
    // LocalFrameView::UpdateLayout in the blink,benchmark category.
    // This event has a slightly wider scope than "Layout" from
    // devtools.timeline and more closely matches the probe::UpdateLayout scope.
    trace_analyzer::Query query =
        trace_analyzer::Query::EventNameIs("LocalFrameView::layout");
    analyzer->FindEvents(query, &events);

    double total_duration_us = 0.0;
    for (const trace_analyzer::TraceEvent* event : events) {
      total_duration_us += event->duration;
    }
    // Convert microseconds to milliseconds.
    result.total_duration_ms = total_duration_us / 1000.0;
    result.event_count = events.size();
    return result;
  }
};

// Test that styleDuration is properly captured during ResizeObserver callbacks.
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       ResizeObserverStyleDuration) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareResizeObserverTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runResizeObserverTest()");

  // Phase 3: Stop tracing and get the trace data.
  [[maybe_unused]] std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  ASSERT_TRUE(dict.FindBool("resizeObserverFired").value());

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  // On slow Android emulators, LoAF entries may not be captured reliably.
  // Skip the test rather than fail flakily.
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error;
#endif

  EXPECT_TRUE(dict.FindBool("hasStyleDuration").value());

  double style_duration = dict.FindDouble("styleDuration").value();
  double total_forced_style_duration =
      dict.FindDouble("totalForcedStyleDuration").value();
  double duration = dict.FindDouble("duration").value();

  EXPECT_GE(style_duration, 0.0);
  EXPECT_GE(total_forced_style_duration, 0.0);
  EXPECT_GT(duration, 0.0);
  EXPECT_LE(style_duration, duration);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // On Android emulators and ChromeOS, timing discrepancies between the LoAF
  // API and tracing may be too large to reliably compare. Skip the
  // tolerance-based assertions on these platforms.
  TraceStyleResult trace_result = GetStyleDurationFromTrace(trace_str);
  double trace_style_duration = trace_result.total_duration_ms;
  double api_total_style = style_duration + total_forced_style_duration;

  double max_value = std::max(api_total_style, trace_style_duration);
  // Use 20% tolerance to account for timing measurement differences.
  double tolerance_ms = std::max(15.0, max_value * 0.2);

  // For the lower bound, LoAF may report less than trace due to the 5ms script
  // threshold.
  constexpr double kLoafThresholdMs = 5.0;
  double lower_tolerance_ms =
      tolerance_ms +
      (kLoafThresholdMs * static_cast<double>(trace_result.event_count));

  EXPECT_LE(api_total_style, trace_style_duration + tolerance_ms);
  EXPECT_GE(api_total_style, trace_style_duration - lower_tolerance_ms);
#endif
}

// Test that styleDuration is properly captured across multiple ResizeObserver
// iterations.
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       MultipleStyleIterations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareMultipleIterationsTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runMultipleIterationsTest()");

  // Phase 3: Stop tracing and get the trace data.
  [[maybe_unused]] std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  int iteration_count = dict.FindInt("iterationCount").value();
  ASSERT_GE(iteration_count, 1);

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  // On slow Android emulators, LoAF entries may not be captured reliably.
  // Skip the test rather than fail flakily.
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error;
#endif

  EXPECT_TRUE(dict.FindBool("hasStyleDuration").value());

  double style_duration = dict.FindDouble("styleDuration").value();
  double total_forced_style_duration =
      dict.FindDouble("totalForcedStyleDuration").value();
  double duration = dict.FindDouble("duration").value();

  EXPECT_GE(style_duration, 0.0);
  EXPECT_GE(total_forced_style_duration, 0.0);
  EXPECT_GT(duration, 0.0);
  EXPECT_LE(style_duration, duration);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // On Android emulators and ChromeOS, timing discrepancies between the LoAF
  // API and tracing may be too large to reliably compare. Skip the
  // tolerance-based assertions on these platforms.
  TraceStyleResult trace_result = GetStyleDurationFromTrace(trace_str);
  double trace_style_duration = trace_result.total_duration_ms;
  double api_total_style = style_duration + total_forced_style_duration;

  double max_value = std::max(api_total_style, trace_style_duration);
  // Use 20% tolerance to account for timing measurement differences.
  double tolerance_ms = std::max(15.0, max_value * 0.2);

  // For the lower bound, LoAF may report less than trace due to the 5ms script
  // threshold.
  constexpr double kLoafThresholdMs = 5.0;
  double lower_tolerance_ms =
      tolerance_ms +
      (kLoafThresholdMs * static_cast<double>(trace_result.event_count));

  EXPECT_LE(api_total_style, trace_style_duration + tolerance_ms);
  EXPECT_GE(api_total_style, trace_style_duration - lower_tolerance_ms);
#endif
}

// Test that forced style during script execution is properly captured in
// forcedStyleDuration, separate from the entry's styleDuration.
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       ForcedStyleSeparation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareForcedStyleTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runForcedStyleTest()");

  // Phase 3: Stop tracing and get the trace data.
  [[maybe_unused]] std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  // On slow Android emulators, LoAF entries may not be captured reliably.
  // Skip the test rather than fail flakily.
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error << ", hasLoafEntry: "
                      << dict.FindBool("hasLoafEntry").value_or(false)
                      << ", scriptCount: "
                      << dict.FindInt("scriptCount").value_or(-1);
#endif

  EXPECT_TRUE(dict.FindBool("hasStyleDuration").value());
  EXPECT_TRUE(dict.FindBool("hasForcedStyleDuration").value());

  double entry_style_duration = dict.FindDouble("entryStyleDuration").value();
  EXPECT_GE(entry_style_duration, 0.0);

  double script_forced_style_duration =
      dict.FindDouble("scriptForcedStyleDuration").value();
  EXPECT_GE(script_forced_style_duration, 0.0);

  double script_forced_style_and_layout_duration =
      dict.FindDouble("scriptForcedStyleAndLayoutDuration").value();
  EXPECT_GE(script_forced_style_and_layout_duration,
            script_forced_style_duration);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // The trace should capture the same style recalc events.
  // script_forced_style_duration should be close to trace_style_duration
  // because the test only forces style during script execution, not during
  // render phase.
  TraceStyleResult trace_result = GetStyleDurationFromTrace(trace_str);
  double trace_style_duration = trace_result.total_duration_ms;

  // The forced style during script should be the majority of trace style.
  // entry_style_duration should be small since we're forcing style inside
  // the script.
  double max_value =
      std::max(script_forced_style_duration, trace_style_duration);
  double tolerance_ms = std::max(15.0, max_value * 0.2);

  constexpr double kLoafThresholdMs = 5.0;
  double lower_tolerance_ms =
      tolerance_ms +
      (kLoafThresholdMs * static_cast<double>(trace_result.event_count));

  EXPECT_LE(script_forced_style_duration, trace_style_duration + tolerance_ms);
  EXPECT_GE(script_forced_style_duration,
            trace_style_duration - lower_tolerance_ms);

  // entry_style_duration should be small since we're forcing style inside
  // the script. Allow some tolerance for any incidental style work.
  EXPECT_LE(entry_style_duration, 30.0)
      << "Render-phase style should be minimal in this test";
#else
  (void)trace_str;
#endif
}

// Test that layoutDuration is properly captured during ResizeObserver
// callbacks.
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       ResizeObserverLayoutDuration) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareResizeObserverLayoutTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runResizeObserverLayoutTest()");

  // Phase 3: Stop tracing and get the trace data.
  std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  ASSERT_TRUE(dict.FindBool("resizeObserverFired").value());

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  // On slow Android emulators, LoAF entries may not be captured reliably.
  // Skip the test rather than fail flakily.
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error;
#endif

  EXPECT_TRUE(dict.FindBool("hasLayoutDuration").value());

  double layout_duration = dict.FindDouble("layoutDuration").value();
  double total_forced_layout_duration =
      dict.FindDouble("totalForcedLayoutDuration").value();
  double total_forced_style_duration =
      dict.FindDouble("totalForcedStyleDuration").value();
  double total_forced_style_and_layout_duration =
      dict.FindDouble("totalForcedStyleAndLayoutDuration").value();
  double duration = dict.FindDouble("duration").value();

  EXPECT_GE(layout_duration, 0.0);
  EXPECT_GE(total_forced_layout_duration, 0.0);
  EXPECT_GT(duration, 0.0);
  EXPECT_LE(layout_duration, duration);

  // Verify that styleDuration + layoutDuration <= duration (both are subsets of
  // the total frame time)
  double style_duration = dict.FindDouble("styleDuration").value();
  EXPECT_GE(style_duration, 0.0);
  EXPECT_LE(style_duration + layout_duration, duration);

  // Verify the key relationship: the sum of forced style and forced layout
  // should approximately equal forced style+layout (within tolerance).
  double sum_forced =
      total_forced_style_duration + total_forced_layout_duration;
  EXPECT_NEAR(sum_forced, total_forced_style_and_layout_duration, 1.0);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // On Android emulators and ChromeOS, timing discrepancies between the LoAF
  // API and tracing may be too large to reliably compare. Skip the
  // tolerance-based assertions on these platforms.
  TraceLayoutResult trace_result = GetLayoutDurationFromTrace(trace_str);
  double trace_layout_duration = trace_result.total_duration_ms;
  double api_total_layout = layout_duration + total_forced_layout_duration;

  // The probe::UpdateLayout scope is wider than the LocalFrameView::layout
  // trace event. The probe includes setup work before the trace begins and
  // cleanup work after the trace ends. This adds approximately 10ms of
  // overhead per layout event. Account for this with a per-event tolerance.
  constexpr double kProbeOverheadPerEventMs = 10.0;
  double probe_overhead_ms =
      kProbeOverheadPerEventMs * static_cast<double>(trace_result.event_count);

  double max_value = std::max(api_total_layout, trace_layout_duration);
  // Use 20% tolerance plus the per-event probe overhead.
  double tolerance_ms = std::max(15.0, max_value * 0.2) + probe_overhead_ms;

  // For the lower bound, LoAF may report less than trace due to the 5ms script
  // threshold.
  constexpr double kLoafThresholdMs = 5.0;
  double lower_tolerance_ms =
      tolerance_ms +
      (kLoafThresholdMs * static_cast<double>(trace_result.event_count));

  EXPECT_LE(api_total_layout, trace_layout_duration + tolerance_ms)
      << "API layout (" << api_total_layout << "ms) vs trace ("
      << trace_layout_duration << "ms, event_count=" << trace_result.event_count
      << ")";
  EXPECT_GE(api_total_layout, trace_layout_duration - lower_tolerance_ms)
      << "API layout (" << api_total_layout << "ms) vs trace ("
      << trace_layout_duration << "ms, event_count=" << trace_result.event_count
      << ")";
#else
  // Suppress unused variable warning on platforms where we skip trace
  // comparison.
  (void)trace_str;
#endif
}

// Test that forced layout during script execution is properly captured in
// forcedLayoutDuration, separate from the entry's layoutDuration.
// TODO(crbug.com/490039788): Disabled due to flakiness on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ForcedLayoutSeparation DISABLED_ForcedLayoutSeparation
#else
#define MAYBE_ForcedLayoutSeparation ForcedLayoutSeparation
#endif
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       MAYBE_ForcedLayoutSeparation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareForcedLayoutTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runForcedLayoutTest()");

  // Phase 3: Stop tracing and get the trace data.
  std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  // On slow Android emulators, LoAF entries may not be captured reliably.
  // Skip the test rather than fail flakily.
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error << ", hasLoafEntry: "
                      << dict.FindBool("hasLoafEntry").value_or(false)
                      << ", scriptCount: "
                      << dict.FindInt("scriptCount").value_or(-1);
#endif

  EXPECT_TRUE(dict.FindBool("hasLayoutDuration").value());
  EXPECT_TRUE(dict.FindBool("hasForcedLayoutDuration").value());

  double entry_layout_duration = dict.FindDouble("entryLayoutDuration").value();
  EXPECT_GE(entry_layout_duration, 0.0);

  double script_forced_layout_duration =
      dict.FindDouble("scriptForcedLayoutDuration").value();
  EXPECT_GE(script_forced_layout_duration, 0.0);

  double script_forced_style_and_layout_duration =
      dict.FindDouble("scriptForcedStyleAndLayoutDuration").value();
  EXPECT_GE(script_forced_style_and_layout_duration,
            script_forced_layout_duration);

  // Verify the key relationship: forcedStyleAndLayoutDuration should equal
  // forcedStyleDuration + forcedLayoutDuration (within floating point
  // tolerance)
  double script_forced_style_duration =
      dict.FindDouble("scriptForcedStyleDuration").value();
  EXPECT_GE(script_forced_style_duration, 0.0);

  // The sum of forcedStyleDuration + forcedLayoutDuration should approximately
  // equal forcedStyleAndLayoutDuration. Use a small tolerance for floating
  // point comparisons.
  double sum_style_layout =
      script_forced_style_duration + script_forced_layout_duration;
  EXPECT_NEAR(sum_style_layout, script_forced_style_and_layout_duration, 1.0);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // For forced layout, the trace should capture the same layout events.
  // The script_forced_layout_duration should be close to trace_layout_duration
  // because the test only forces layout during script execution, not during
  // render phase.
  TraceLayoutResult trace_result = GetLayoutDurationFromTrace(trace_str);
  double trace_layout_duration = trace_result.total_duration_ms;

  // Also get the JS-measured layout time for comparison.
  double js_measured_layout_time =
      dict.FindDouble("measuredForcedLayoutTime").value_or(-1.0);

  // Verify trace captured some layout events.
  EXPECT_GT(trace_result.event_count, 0u)
      << "Trace should capture at least one layout event";
  EXPECT_GT(trace_layout_duration, 0.0)
      << "Trace layout duration should be positive";

  // The API's forcedLayoutDuration should be close to trace.
  // Allow tolerance for timing differences between probe scope and trace event.
  double tolerance_ms = std::max(15.0, trace_layout_duration * 0.2);
  EXPECT_GE(script_forced_layout_duration, trace_layout_duration - tolerance_ms)
      << "API forced layout (" << script_forced_layout_duration
      << "ms) should be >= trace (" << trace_layout_duration
      << "ms) minus tolerance (" << tolerance_ms
      << "ms), js_measured=" << js_measured_layout_time << "ms";

  // The API's forcedStyleAndLayoutDuration should be <= JS measurement
  // since the JS measurement includes JavaScript overhead (forEach loops,
  // property access, etc.) while the API only measures style/layout time.
  EXPECT_LE(script_forced_style_and_layout_duration,
            js_measured_layout_time + tolerance_ms)
      << "API forcedStyleAndLayout (" << script_forced_style_and_layout_duration
      << "ms) should be <= JS measured (" << js_measured_layout_time
      << "ms) plus tolerance (" << tolerance_ms << "ms)"
      << ", trace=" << trace_layout_duration << "ms";

  // entry_layout_duration should be small since we're not doing layout during
  // render phase. Allow some tolerance for any incidental layout work.
  EXPECT_LE(entry_layout_duration, 30.0)
      << "Render-phase layout should be minimal in this test";
#else
  (void)trace_str;
#endif
}

// Test that container queries produce measurable style and layout durations.
// Container queries cause interleaved style+layout passes: the container must
// be laid out to determine its size, then children are re-styled based on
// container query conditions, then re-laid out. Both styleDuration and
// layoutDuration should capture this work.
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       ContainerQueryStyleAndLayoutDuration) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareContainerQueryTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runContainerQueryTest()");

  // Phase 3: Stop tracing and get the trace data.
  [[maybe_unused]] std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error;
#endif

  EXPECT_TRUE(dict.FindBool("hasStyleDuration").value());
  EXPECT_TRUE(dict.FindBool("hasLayoutDuration").value());

  double style_duration = dict.FindDouble("styleDuration").value();
  double layout_duration = dict.FindDouble("layoutDuration").value();
  double duration = dict.FindDouble("duration").value();

  EXPECT_GE(style_duration, 0.0);
  EXPECT_GE(layout_duration, 0.0);
  EXPECT_GT(duration, 0.0);

  // styleDuration + layoutDuration should not exceed total frame duration.
  EXPECT_LE(style_duration + layout_duration, duration);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Verify that trace events for both style and layout were emitted.
  TraceStyleResult trace_style = GetStyleDurationFromTrace(trace_str);
  TraceLayoutResult trace_layout = GetLayoutDurationFromTrace(trace_str);

  EXPECT_GT(trace_style.event_count, 0u)
      << "Trace should capture style recalc events from container queries";
  EXPECT_GT(trace_layout.event_count, 0u)
      << "Trace should capture layout events from container queries";

  // With container queries, the UpdateLayoutTree trace event only captures the
  // initial style pass, while LocalFrameView::layout includes both pure layout
  // AND interleaved container query style recalc. The API now correctly
  // separates style from layout (subtracting container query style time from
  // layout). So we compare the combined total (style + layout) from the API
  // against the combined total from trace events.
  double total_forced_style =
      dict.FindDouble("totalForcedStyleDuration").value();
  double total_forced_layout =
      dict.FindDouble("totalForcedLayoutDuration").value();
  double api_total = style_duration + layout_duration + total_forced_style +
                     total_forced_layout;
  double trace_total =
      trace_style.total_duration_ms + trace_layout.total_duration_ms;

  size_t total_event_count = trace_style.event_count + trace_layout.event_count;
  constexpr double kProbeOverheadPerEventMs = 10.0;
  double probe_overhead_ms =
      kProbeOverheadPerEventMs * static_cast<double>(total_event_count);
  double max_total = std::max(api_total, trace_total);
  double tolerance_ms = std::max(15.0, max_total * 0.2) + probe_overhead_ms;

  constexpr double kLoafThresholdMs = 5.0;
  double lower_tolerance_ms =
      tolerance_ms +
      (kLoafThresholdMs * static_cast<double>(total_event_count));

  EXPECT_LE(api_total, trace_total + tolerance_ms)
      << "API total style+layout (" << api_total << "ms) vs trace total ("
      << trace_total << "ms)";
  EXPECT_GE(api_total, trace_total - lower_tolerance_ms)
      << "API total style+layout (" << api_total << "ms) vs trace total ("
      << trace_total << "ms)";
#endif
}

// Test that forced style+layout with container queries during script execution
// is properly captured. When script resizes a container and reads offsetHeight,
// the browser must synchronously evaluate container queries, re-style children,
// and re-layout. The forced durations should reflect both the style and layout
// components separately.
IN_PROC_BROWSER_TEST_F(LongAnimationFrameStyleDurationBrowserTest,
                       ForcedContainerQuerySeparation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/performance_timeline/long_animation_frame_style_duration.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Phase 1: test setup before starting tracing.
  auto prepare_result = EvalJs(shell(), "prepareForcedContainerQueryTest()");
  ASSERT_TRUE(prepare_result.is_ok());
  ASSERT_TRUE(prepare_result.is_dict());

  const base::DictValue& prepare_dict = prepare_result.ExtractDict();
  ASSERT_TRUE(prepare_dict.FindBool("ready").value());

  // Phase 2: Start tracing and run the actual test.
  StartStyleTracing();

  auto result = EvalJs(shell(), "runForcedContainerQueryTest()");

  // Phase 3: Stop tracing and get the trace data.
  [[maybe_unused]] std::string trace_str = StopStyleTracing();

  ASSERT_TRUE(result.is_ok());
  ASSERT_TRUE(result.is_dict());

  const base::DictValue& dict = result.ExtractDict();

  const std::string* error = dict.FindString("error");
#if BUILDFLAG(IS_ANDROID)
  if (error) {
    return;
  }
#else
  ASSERT_FALSE(error) << "Test failed: " << *error << ", hasLoafEntry: "
                      << dict.FindBool("hasLoafEntry").value_or(false)
                      << ", scriptCount: "
                      << dict.FindInt("scriptCount").value_or(-1);
#endif

  EXPECT_TRUE(dict.FindBool("hasStyleDuration").value());
  EXPECT_TRUE(dict.FindBool("hasLayoutDuration").value());
  EXPECT_TRUE(dict.FindBool("hasForcedStyleDuration").value());
  EXPECT_TRUE(dict.FindBool("hasForcedLayoutDuration").value());

  double entry_style_duration = dict.FindDouble("entryStyleDuration").value();
  double entry_layout_duration = dict.FindDouble("entryLayoutDuration").value();
  double script_forced_style =
      dict.FindDouble("scriptForcedStyleDuration").value();
  double script_forced_layout =
      dict.FindDouble("scriptForcedLayoutDuration").value();
  double script_forced_style_and_layout =
      dict.FindDouble("scriptForcedStyleAndLayoutDuration").value();
  double duration = dict.FindDouble("duration").value();

  EXPECT_GE(entry_style_duration, 0.0);
  EXPECT_GE(entry_layout_duration, 0.0);
  EXPECT_GE(script_forced_style, 0.0);
  EXPECT_GE(script_forced_layout, 0.0);
  EXPECT_GE(script_forced_style_and_layout, 0.0);
  EXPECT_GT(duration, 0.0);

  // Container queries force both style and layout, so both forced durations
  // should be present.
  EXPECT_GE(script_forced_style_and_layout, script_forced_style);
  EXPECT_GE(script_forced_style_and_layout, script_forced_layout);

  // The sum of forced style + forced layout should approximately equal
  // forcedStyleAndLayoutDuration.
  double sum_forced = script_forced_style + script_forced_layout;
  EXPECT_NEAR(sum_forced, script_forced_style_and_layout, 1.0)
      << "forcedStyle (" << script_forced_style << "ms) + forcedLayout ("
      << script_forced_layout << "ms) = " << sum_forced
      << "ms should ≈ forcedStyleAndLayout (" << script_forced_style_and_layout
      << "ms)";

  // All durations should fit within the total frame duration.
  EXPECT_LE(entry_style_duration + entry_layout_duration, duration);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Verify trace captured interleaved style and layout events from
  // container query evaluation.
  TraceStyleResult trace_style = GetStyleDurationFromTrace(trace_str);
  TraceLayoutResult trace_layout = GetLayoutDurationFromTrace(trace_str);

  EXPECT_GT(trace_style.event_count, 0u)
      << "Trace should capture style events from forced container queries";
  EXPECT_GT(trace_layout.event_count, 0u)
      << "Trace should capture layout events from forced container queries";

  // With container queries, the trace events don't cleanly separate style from
  // layout: UpdateLayoutTree only captures the initial style pass, while
  // LocalFrameView::layout includes both pure layout and interleaved container
  // query style recalc. The API correctly separates them via the
  // probe::RecalculateStyle fired during container query style recalc.
  //
  // Compare the combined totals: API (style + layout) vs trace (style +
  // layout).
  double api_total = script_forced_style + script_forced_layout;
  double trace_total =
      trace_style.total_duration_ms + trace_layout.total_duration_ms;

  size_t total_event_count = trace_style.event_count + trace_layout.event_count;
  constexpr double kProbeOverheadPerEventMs = 10.0;
  double probe_overhead_ms =
      kProbeOverheadPerEventMs * static_cast<double>(total_event_count);
  double max_total = std::max(api_total, trace_total);
  double tolerance_ms = std::max(15.0, max_total * 0.2) + probe_overhead_ms;

  constexpr double kLoafThresholdMs = 5.0;
  double lower_tolerance_ms =
      tolerance_ms +
      (kLoafThresholdMs * static_cast<double>(total_event_count));

  EXPECT_LE(api_total, trace_total + tolerance_ms)
      << "API forced style+layout (" << api_total << "ms) vs trace total ("
      << trace_total << "ms)";
  EXPECT_GE(api_total, trace_total - lower_tolerance_ms)
      << "API forced style+layout (" << api_total << "ms) vs trace total ("
      << trace_total << "ms)";

  // The JS-measured time should be >= the API's combined forced duration
  // (since JS measurement includes overhead).
  double js_measured = dict.FindDouble("measuredForcedTime").value_or(-1.0);
  EXPECT_LE(script_forced_style_and_layout, js_measured + tolerance_ms)
      << "API forcedStyleAndLayout (" << script_forced_style_and_layout
      << "ms) should be <= JS measured (" << js_measured << "ms) + tolerance";
#endif
}

}  // namespace content
