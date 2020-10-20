// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_double.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

base::TimeTicks GetTimeOrigin() {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(500);
}

}  // namespace

class WindowPerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    ResetPerformance();
  }

  bool ObservingLongTasks() {
    return !PerformanceMonitor::Monitor(performance_->GetExecutionContext())
                ->thresholds_[PerformanceMonitor::kLongTask]
                .is_zero();
  }

  void AddLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ |= PerformanceEntry::kLongTask;
  }

  void RemoveLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ = PerformanceEntry::kInvalid;
  }

  void SimulateDidProcessLongTask() {
    auto* monitor = GetFrame()->GetPerformanceMonitor();
    monitor->WillExecuteScript(GetWindow());
    monitor->DidExecuteScript();
    monitor->DidProcessTask(
        base::TimeTicks(), base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  }

  void SimulateSwapPromise(base::TimeTicks timestamp) {
    performance_->ReportEventTimings(frame_counter++, WebSwapResult::kDidSwap,
                                     timestamp);
  }

  LocalFrame* GetFrame() const { return &page_holder_->GetFrame(); }

  LocalDOMWindow* GetWindow() const { return GetFrame()->DomWindow(); }

  String SanitizedAttribution(ExecutionContext* context,
                              bool has_multiple_contexts,
                              LocalFrame* observer_frame) {
    return WindowPerformance::SanitizedAttribution(
               context, has_multiple_contexts, observer_frame)
        .first;
  }

  void ResetPerformance() {
    page_holder_ = nullptr;
    page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
    page_holder_->GetDocument().SetURL(KURL("https://example.com"));

    LocalDOMWindow* window = LocalDOMWindow::From(GetScriptState());
    performance_ = DOMWindowPerformance::performance(*window);
    performance_->SetClocksForTesting(test_task_runner_->GetMockClock(),
                                      test_task_runner_->GetMockTickClock());
    performance_->time_origin_ = GetTimeOrigin();
  }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(page_holder_->GetDocument().GetFrame());
  }

  uint64_t frame_counter = 1;
  Persistent<WindowPerformance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(WindowPerformanceTest, LongTaskObserverInstrumentation) {
  // Check that we're always observing longtasks
  EXPECT_TRUE(ObservingLongTasks());

  // Adding LongTask observer.
  AddLongTaskObserver();
  EXPECT_TRUE(ObservingLongTasks());

  // Removing LongTask observer doeos not cause us to stop observing. We still
  // observe because entries should still be added to the longtasks buffer.
  RemoveLongTaskObserver();
  EXPECT_TRUE(ObservingLongTasks());
}

TEST_F(WindowPerformanceTest, SanitizedLongTaskName) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("self", SanitizedAttribution(GetWindow(), false, GetFrame()));

  // Unable to attribute, when multiple script execution contents are involved.
  EXPECT_EQ("multiple-contexts",
            SanitizedAttribution(GetWindow(), true, GetFrame()));
}

TEST_F(WindowPerformanceTest, SanitizedLongTaskName_CrossOrigin) {
  // Create another dummy page holder and pretend it is an iframe.
  DummyPageHolder another_page(IntSize(400, 300));
  another_page.GetDocument().SetURL(KURL("https://iframed.com/bar"));

  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("cross-origin-unreachable",
            SanitizedAttribution(another_page.GetFrame().DomWindow(), false,
                                 GetFrame()));
}

// https://crbug.com/706798: Checks that after navigation that have replaced the
// window object, calls to not garbage collected yet WindowPerformance belonging
// to the old window do not cause a crash.
TEST_F(WindowPerformanceTest, NavigateAway) {
  AddLongTaskObserver();
  EXPECT_TRUE(ObservingLongTasks());

  // Simulate navigation commit.
  GetFrame()->DomWindow()->FrameDestroyed();

  // m_performance is still alive, and should not crash when notified.
  SimulateDidProcessLongTask();
}

// Checks that WindowPerformance object and its fields (like PerformanceTiming)
// function correctly after transition to another document in the same window.
// This happens when a page opens a new window and it navigates to a same-origin
// document.
TEST(PerformanceLifetimeTest, SurviveContextSwitch) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  // Emulate a new window inheriting the origin for its initial empty document
  // from its opener. This is necessary to ensure window reuse below, as that
  // only happens when origins match.
  KURL url("http://example.com");
  page_holder->GetFrame()
      .DomWindow()
      ->GetSecurityContext()
      .SetSecurityOriginForTesting(SecurityOrigin::Create(KURL(url)));

  WindowPerformance* perf =
      DOMWindowPerformance::performance(*page_holder->GetFrame().DomWindow());
  PerformanceTiming* timing = perf->timing();

  auto* document_loader = page_holder->GetFrame().Loader().GetDocumentLoader();
  ASSERT_TRUE(document_loader);
  document_loader->GetTiming().SetNavigationStart(base::TimeTicks::Now());

  EXPECT_EQ(page_holder->GetFrame().DomWindow(), perf->DomWindow());
  EXPECT_EQ(page_holder->GetFrame().DomWindow(), timing->DomWindow());
  auto navigation_start = timing->navigationStart();
  EXPECT_NE(0U, navigation_start);

  // Simulate changing the document while keeping the window.
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url);
  page_holder->GetFrame().Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(perf, DOMWindowPerformance::performance(
                      *page_holder->GetFrame().DomWindow()));
  EXPECT_EQ(timing, perf->timing());
  EXPECT_EQ(page_holder->GetFrame().DomWindow(), perf->DomWindow());
  EXPECT_EQ(page_holder->GetFrame().DomWindow(), timing->DomWindow());
  EXPECT_LE(navigation_start, timing->navigationStart());
}

// Make sure the output entries with the same timestamps follow the insertion
// order. (http://crbug.com/767560)
TEST_F(WindowPerformanceTest, EnsureEntryListOrder) {
  // Need to have an active V8 context for ScriptValues to operate.
  v8::HandleScope handle_scope(GetScriptState()->GetIsolate());
  v8::Local<v8::Context> context = GetScriptState()->GetContext();
  v8::Context::Scope context_scope(context);
  auto initial_offset =
      test_task_runner_->NowTicks().since_origin().InSecondsF();
  test_task_runner_->FastForwardBy(GetTimeOrigin() - base::TimeTicks());

  DummyExceptionStateForTesting exception_state;
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  for (int i = 0; i < 8; i++) {
    performance_->mark(GetScriptState(), AtomicString::Number(i), nullptr,
                       exception_state);
  }
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  for (int i = 8; i < 17; i++) {
    performance_->mark(GetScriptState(), AtomicString::Number(i), nullptr,
                       exception_state);
  }
  PerformanceEntryVector entries = performance_->getEntries();
  EXPECT_EQ(17U, entries.size());
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(AtomicString::Number(i), entries[i]->name());
    EXPECT_NEAR(2000, entries[i]->startTime() - initial_offset, 0.005);
  }
  for (int i = 8; i < 17; i++) {
    EXPECT_EQ(AtomicString::Number(i), entries[i]->name());
    EXPECT_NEAR(4000, entries[i]->startTime() - initial_offset, 0.005);
  }
}

TEST_F(WindowPerformanceTest, EventTimingEntryBuffering) {
  ScopedEventTimingForTest event_timing(true);
  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());

  base::TimeTicks start_time =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(1.1);
  base::TimeTicks processing_start =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(3.3);
  base::TimeTicks processing_end =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(3.8);
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr);
  base::TimeTicks swap_time =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(6.0);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(1u, performance_->getBufferedEntriesByType("event").size());

  page_holder_->GetFrame()
      .Loader()
      .GetDocumentLoader()
      ->GetTiming()
      .MarkLoadEventStart();
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, true, nullptr);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(2u, performance_->getBufferedEntriesByType("event").size());

  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  GetFrame()->DetachDocument();
  EXPECT_FALSE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(3u, performance_->getBufferedEntriesByType("event").size());
}

TEST_F(WindowPerformanceTest, Expose100MsEvents) {
  ScopedEventTimingForTest event_timing(true);
  base::TimeTicks start_time =
      GetTimeOrigin() + base::TimeDelta::FromSeconds(1);
  base::TimeTicks processing_start =
      start_time + base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks processing_end =
      processing_start + base::TimeDelta::FromMilliseconds(10);
  performance_->RegisterEventTiming("mousedown", start_time, processing_start,
                                    processing_end, false, nullptr);

  base::TimeTicks start_time2 =
      start_time + base::TimeDelta::FromMicroseconds(200);
  performance_->RegisterEventTiming("click", start_time2, processing_start,
                                    processing_end, false, nullptr);

  // The swap time is 100.1 ms after |start_time| but only 99.9 ms after
  // |start_time2|.
  base::TimeTicks swap_time =
      start_time + base::TimeDelta::FromMicroseconds(100100);
  SimulateSwapPromise(swap_time);
  // Only the longer event should have been reported.
  const auto& entries = performance_->getBufferedEntriesByType("event");
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ("mousedown", entries.at(0)->name());
}

TEST_F(WindowPerformanceTest, EventTimingDuration) {
  ScopedEventTimingForTest event_timing(true);

  base::TimeTicks start_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1000);
  base::TimeTicks processing_start =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1001);
  base::TimeTicks processing_end =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1002);
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr);
  base::TimeTicks short_swap_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1003);
  SimulateSwapPromise(short_swap_time);
  EXPECT_EQ(0u, performance_->getBufferedEntriesByType("event").size());

  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, true, nullptr);
  base::TimeTicks long_swap_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2000);
  SimulateSwapPromise(long_swap_time);
  EXPECT_EQ(1u, performance_->getBufferedEntriesByType("event").size());

  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, true, nullptr);
  SimulateSwapPromise(short_swap_time);
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr);
  SimulateSwapPromise(long_swap_time);
  EXPECT_EQ(2u, performance_->getBufferedEntriesByType("event").size());
}

// Test the case where multiple events are registered and then their swap
// promise is resolved.
TEST_F(WindowPerformanceTest, MultipleEventsThenSwap) {
  ScopedEventTimingForTest event_timing(true);

  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    base::TimeTicks start_time =
        GetTimeOrigin() + base::TimeDelta::FromSeconds(i);
    base::TimeTicks processing_start =
        start_time + base::TimeDelta::FromMilliseconds(100);
    base::TimeTicks processing_end =
        start_time + base::TimeDelta::FromMilliseconds(200);
    performance_->RegisterEventTiming("click", start_time, processing_start,
                                      processing_end, false, nullptr);
    EXPECT_EQ(0u, performance_->getBufferedEntriesByType("event").size());
  }
  base::TimeTicks swap_time =
      GetTimeOrigin() + base::TimeDelta::FromSeconds(num_events);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(num_events, performance_->getBufferedEntriesByType("event").size());
}

// Test for existence of 'first-input' given different types of first events.
TEST_F(WindowPerformanceTest, FirstInput) {
  struct {
    AtomicString event_type;
    bool should_report;
  } inputs[] = {{"click", true},     {"keydown", true},
                {"keypress", false}, {"pointerdown", false},
                {"mousedown", true}, {"mouseover", false}};
  for (const auto& input : inputs) {
    // first-input does not have a |duration| threshold so use close values.
    performance_->RegisterEventTiming(
        input.event_type, GetTimeOrigin(),
        GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1),
        GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2), false, nullptr);
    SimulateSwapPromise(GetTimeOrigin() + base::TimeDelta::FromMilliseconds(3));
    PerformanceEntryVector firstInputs =
        performance_->getEntriesByType("first-input");
    EXPECT_GE(1u, firstInputs.size());
    EXPECT_EQ(input.should_report, firstInputs.size() == 1u);
    ResetPerformance();
  }
}

// Test that the 'first-input' is populated after some irrelevant events are
// ignored.
TEST_F(WindowPerformanceTest, FirstInputAfterIgnored) {
  AtomicString several_events[] = {"mouseover", "mousedown", "pointerup"};
  for (const auto& event : several_events) {
    performance_->RegisterEventTiming(
        event, GetTimeOrigin(),
        GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1),
        GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2), false, nullptr);
    SimulateSwapPromise(GetTimeOrigin() + base::TimeDelta::FromMilliseconds(3));
  }
  ASSERT_EQ(1u, performance_->getEntriesByType("first-input").size());
  EXPECT_EQ("mousedown",
            performance_->getEntriesByType("first-input")[0]->name());
}

// Test that pointerdown followed by pointerup works as a 'firstInput'.
TEST_F(WindowPerformanceTest, FirstPointerUp) {
  base::TimeTicks start_time = GetTimeOrigin();
  base::TimeTicks processing_start =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1);
  base::TimeTicks processing_end =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2);
  base::TimeTicks swap_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(3);
  performance_->RegisterEventTiming("pointerdown", start_time, processing_start,
                                    processing_end, false, nullptr);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(0u, performance_->getEntriesByType("first-input").size());
  performance_->RegisterEventTiming("pointerup", start_time, processing_start,
                                    processing_end, false, nullptr);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(1u, performance_->getEntriesByType("first-input").size());
  // The name of the entry should be "pointerdown".
  EXPECT_EQ(
      1u, performance_->getEntriesByName("pointerdown", "first-input").size());
}

}  // namespace blink
