// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/trace_event_analyzer.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_LCPStartTimePrecision DISABLED_LCPStartTimePrecision
#else
#define MAYBE_LCPStartTimePrecision LCPStartTimePrecision
#endif
IN_PROC_BROWSER_TEST_F(PerformanceTimelineLCPStartTimePrecisionBrowserTest,
                       MAYBE_LCPStartTimePrecision) {
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

  const std::string initial_navigation_id =
      GetNavigationId("first_nav").ExtractString();
  // Navigate away and back 3 times. The 1st time is to verify the
  // navigation id is incremented. The 2nd time is to verify that the id is
  // incremented on the same restored document. The 3rd time is to
  // verify the increment does not stop at 2.
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  std::string prev_navigation_id = initial_navigation_id;

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
    std::string curr_navigation_id =
        GetNavigationId("subsequent_nav" + base::NumberToString(i))
            .ExtractString();
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
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL landing_url(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/prefetch.html"));

  EXPECT_TRUE(NavigateToURL(shell(), landing_url));
  Prefetch();
  EXPECT_TRUE(NavigateToURL(shell(), prefetch_url));
  // Navigate to a prefetched url should result in a navigation timing entry
  // with 0 transfer size.
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
  void CheckEntries(const base::Value::List lst,
                    const std::string& initial_navigation_id) const {
    std::string prev_navigation_id = initial_navigation_id;

    for (const auto& i : lst) {
      auto* dict = i.GetIfDict();
      EXPECT_TRUE(dict);
      EXPECT_EQ("", *dict->FindString("name"));
      EXPECT_EQ("back-forward-cache-restoration",
                *dict->FindString("entryType"));

      const std::string* curr_navigation_id = dict->FindString("navigationId");
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

  std::string initial_navigation_id =
      GetNavigationId("initial_navigation_id").ExtractString();
  for (int i = 0; i < num_of_loops; i++) {
    // Navigate away
    ASSERT_TRUE(NavigateToURL(shell(), url2));

    // Verify `rfh` is stored in back/forward cache.
    ASSERT_TRUE(rfh->IsInBackForwardCache());

    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));
  }
  auto result = std::move(GetBackForwardCacheRestorationEntriesByObserver()
                              .ExtractList()
                              .GetList());
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
          .ExtractList();

  int num_event_entres = entry_cnt_and_dropped_entry_cnt.GetList()[0].GetInt();
  EXPECT_EQ(num_event_entres, buffer_size);

  int num_dropped_entries =
      entry_cnt_and_dropped_entry_cnt.GetList()[1].GetInt();
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

  // The max duration and total duration is non-determinstic. We only verify
  // they exist.
  ukm::TestUkmRecorder::EntryHasMetric(
      ukm_entry.get(),
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName);
  ukm::TestUkmRecorder::EntryHasMetric(
      ukm_entry.get(),
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName);
}

}  // namespace content
