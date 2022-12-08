// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/window_performance.h"
#include <cstdint>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

using test::RunPendingTasks;

namespace {

base::TimeTicks GetTimeOrigin() {
  return base::TimeTicks() + base::Seconds(500);
}

base::TimeTicks GetTimeStamp(int64_t time) {
  return GetTimeOrigin() + base::Milliseconds(time);
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
    monitor->DidProcessTask(base::TimeTicks(),
                            base::TimeTicks() + base::Seconds(1));
  }

  void SimulateSwapPromise(base::TimeTicks timestamp) {
    performance_->ReportEventTimings(frame_counter++, timestamp);
  }

  void SimulateInteractionId(
      PerformanceEventTiming* entry,
      absl::optional<int> key_code,
      absl::optional<PointerId> pointer_id,
      base::TimeTicks event_timestamp = base::TimeTicks(),
      base::TimeTicks presentation_timestamp = base::TimeTicks()) {
    ResponsivenessMetrics::EventTimestamps event_timestamps = {
        event_timestamp, presentation_timestamp};
    performance_->SetInteractionIdAndRecordLatency(entry, key_code, pointer_id,
                                                   event_timestamps);
  }

  void RegisterKeyboardEvent(AtomicString type,
                             base::TimeTicks start_time,
                             base::TimeTicks processing_start,
                             base::TimeTicks processing_end,
                             int key_code) {
    KeyboardEventInit* init = KeyboardEventInit::Create();
    init->setKeyCode(key_code);
    KeyboardEvent* keyboard_event =
        MakeGarbageCollected<KeyboardEvent>(type, init);
    performance_->RegisterEventTiming(*keyboard_event, start_time,
                                      processing_start, processing_end);
  }

  void RegisterPointerEvent(AtomicString type,
                            base::TimeTicks start_time,
                            base::TimeTicks processing_start,
                            base::TimeTicks processing_end,
                            PointerId pointer_id,
                            EventTarget* target = nullptr) {
    PointerEventInit* init = PointerEventInit::Create();
    init->setPointerId(pointer_id);
    PointerEvent* pointer_event = PointerEvent::Create(type, init);
    if (target) {
      pointer_event->SetTarget(target);
    }
    performance_->RegisterEventTiming(*pointer_event, start_time,
                                      processing_start, processing_end);
  }

  PerformanceEventTiming* CreatePerformanceEventTiming(
      const AtomicString& name) {
    return PerformanceEventTiming::Create(name, 0.0, 0.0, 0.0, false, nullptr,
                                          1);
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
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    page_holder_->GetDocument().SetURL(KURL("https://example.com"));

    LocalDOMWindow* window = LocalDOMWindow::From(GetScriptState());
    performance_ = DOMWindowPerformance::performance(*window);
    performance_->SetClocksForTesting(test_task_runner_->GetMockClock(),
                                      test_task_runner_->GetMockTickClock());
    performance_->time_origin_ = GetTimeOrigin();
    // Stop UKM sampling for testing.
    performance_->GetResponsivenessMetrics().StopUkmSamplingForTesting();
  }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(page_holder_->GetDocument().GetFrame());
  }

  ukm::TestUkmRecorder* GetUkmRecorder() {
    return scoped_fake_ukm_recorder_.recorder();
  }

  const base::HistogramTester& GetHistogramTester() const {
    return histogram_tester_;
  }

  void PageVisibilityChanged(base::TimeTicks timestamp) {
    performance_->last_visibility_change_timestamp_ = timestamp;
  }

  uint64_t frame_counter = 1;
  Persistent<WindowPerformance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  ScopedFakeUkmRecorder scoped_fake_ukm_recorder_;
  base::HistogramTester histogram_tester_;
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
  DummyPageHolder another_page(gfx::Size(400, 300));
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
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
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
      WebNavigationParams::CreateWithHTMLBufferForTesting(
          SharedBuffer::Create(), url);
  MockPolicyContainerHost mock_policy_container_host;
  params->policy_container = std::make_unique<WebPolicyContainer>(
      WebPolicyContainerPolicies(),
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
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
  test_task_runner_->FastForwardBy(base::Seconds(2));
  for (int i = 0; i < 8; i++) {
    performance_->mark(GetScriptState(), AtomicString::Number(i), nullptr,
                       exception_state);
  }
  test_task_runner_->FastForwardBy(base::Seconds(2));
  for (int i = 8; i < 17; i++) {
    performance_->mark(GetScriptState(), AtomicString::Number(i), nullptr,
                       exception_state);
  }
  PerformanceEntryVector entries = performance_->getEntries(GetScriptState());
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
  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());

  base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(1.1);
  base::TimeTicks processing_start = GetTimeOrigin() + base::Seconds(3.3);
  base::TimeTicks processing_end = GetTimeOrigin() + base::Seconds(3.8);
  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  base::TimeTicks swap_time = GetTimeOrigin() + base::Seconds(6.0);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(1u, performance_->getBufferedEntriesByType("event").size());

  page_holder_->GetFrame()
      .Loader()
      .GetDocumentLoader()
      ->GetTiming()
      .MarkLoadEventStart();
  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(2u, performance_->getBufferedEntriesByType("event").size());

  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  GetFrame()->DetachDocument();
  EXPECT_FALSE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(3u, performance_->getBufferedEntriesByType("event").size());
}

TEST_F(WindowPerformanceTest, Expose100MsEvents) {
  base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(1);
  base::TimeTicks processing_start = start_time + base::Milliseconds(10);
  base::TimeTicks processing_end = processing_start + base::Milliseconds(10);
  RegisterPointerEvent("mousedown", start_time, processing_start,
                       processing_end, 4);

  base::TimeTicks start_time2 = start_time + base::Microseconds(200);
  RegisterPointerEvent("click", start_time2, processing_start, processing_end,
                       4);

  // The swap time is 100.1 ms after |start_time| but only 99.9 ms after
  // |start_time2|.
  base::TimeTicks swap_time = start_time + base::Microseconds(100100);
  SimulateSwapPromise(swap_time);
  // Only the longer event should have been reported.
  const auto& entries = performance_->getBufferedEntriesByType("event");
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ("mousedown", entries.at(0)->name());
}

TEST_F(WindowPerformanceTest, EventTimingDuration) {
  base::TimeTicks start_time = GetTimeOrigin() + base::Milliseconds(1000);
  base::TimeTicks processing_start = GetTimeOrigin() + base::Milliseconds(1001);
  base::TimeTicks processing_end = GetTimeOrigin() + base::Milliseconds(1002);
  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  base::TimeTicks short_swap_time = GetTimeOrigin() + base::Milliseconds(1003);
  SimulateSwapPromise(short_swap_time);
  EXPECT_EQ(0u, performance_->getBufferedEntriesByType("event").size());

  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  base::TimeTicks long_swap_time = GetTimeOrigin() + base::Milliseconds(2000);
  SimulateSwapPromise(long_swap_time);
  EXPECT_EQ(1u, performance_->getBufferedEntriesByType("event").size());

  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  SimulateSwapPromise(short_swap_time);
  RegisterPointerEvent("click", start_time, processing_start, processing_end,
                       4);
  SimulateSwapPromise(long_swap_time);
  EXPECT_EQ(2u, performance_->getBufferedEntriesByType("event").size());
}

// Test the case where multiple events are registered and then their swap
// promise is resolved.
TEST_F(WindowPerformanceTest, MultipleEventsThenSwap) {
  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(i);
    base::TimeTicks processing_start = start_time + base::Milliseconds(100);
    base::TimeTicks processing_end = start_time + base::Milliseconds(200);
    RegisterPointerEvent("click", start_time, processing_start, processing_end,
                         4);
    EXPECT_EQ(0u, performance_->getBufferedEntriesByType("event").size());
  }
  base::TimeTicks swap_time = GetTimeOrigin() + base::Seconds(num_events);
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
    if (input.event_type == "keydown" || input.event_type == "keypress") {
      RegisterKeyboardEvent(input.event_type, GetTimeOrigin(),
                            GetTimeOrigin() + base::Milliseconds(1),
                            GetTimeOrigin() + base::Milliseconds(2), 4);
    } else {
      RegisterPointerEvent(input.event_type, GetTimeOrigin(),
                           GetTimeOrigin() + base::Milliseconds(1),
                           GetTimeOrigin() + base::Milliseconds(2), 4);
    }
    SimulateSwapPromise(GetTimeOrigin() + base::Milliseconds(3));
    PerformanceEntryVector firstInputs =
        performance_->getEntriesByType(GetScriptState(), "first-input");
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
    RegisterPointerEvent(event, GetTimeOrigin(),
                         GetTimeOrigin() + base::Milliseconds(1),
                         GetTimeOrigin() + base::Milliseconds(2), 4);
    SimulateSwapPromise(GetTimeOrigin() + base::Milliseconds(3));
  }
  ASSERT_EQ(
      1u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  EXPECT_EQ("mousedown",
            performance_->getEntriesByType(GetScriptState(), "first-input")[0]
                ->name());
}

// Test that pointerdown followed by pointerup works as a 'firstInput'.
TEST_F(WindowPerformanceTest, FirstPointerUp) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks swap_time = GetTimeStamp(3);
  RegisterPointerEvent("pointerdown", start_time, processing_start,
                       processing_end, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(
      0u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  RegisterPointerEvent("pointerup", start_time, processing_start,
                       processing_end, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(
      1u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  // The name of the entry should be "pointerdown".
  EXPECT_EQ(
      1u, performance_
              ->getEntriesByName(GetScriptState(), "pointerdown", "first-input")
              .size());
}

// When the pointerdown is optimized out, the mousedown works as a
// 'first-input'.
TEST_F(WindowPerformanceTest, PointerdownOptimizedOut) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks swap_time = GetTimeStamp(3);
  RegisterPointerEvent("mousedown", start_time, processing_start,
                       processing_end, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(
      1u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  // The name of the entry should be "pointerdown".
  EXPECT_EQ(1u,
            performance_
                ->getEntriesByName(GetScriptState(), "mousedown", "first-input")
                .size());
}

// Test that pointerdown followed by mousedown, pointerup works as a
// 'first-input'.
TEST_F(WindowPerformanceTest, PointerdownOnDesktop) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks swap_time = GetTimeStamp(3);
  RegisterPointerEvent("pointerdown", start_time, processing_start,
                       processing_end, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(
      0u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  RegisterPointerEvent("mousedown", start_time, processing_start,
                       processing_end, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(
      0u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  RegisterPointerEvent("pointerup", start_time, processing_start,
                       processing_end, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(
      1u,
      performance_->getEntriesByType(GetScriptState(), "first-input").size());
  // The name of the entry should be "pointerdown".
  EXPECT_EQ(
      1u, performance_
              ->getEntriesByName(GetScriptState(), "pointerdown", "first-input")
              .size());
}

TEST_F(WindowPerformanceTest, OneKeyboardInteraction) {
  base::TimeTicks keydown_timestamp = GetTimeStamp(0);
  // Keydown
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  int key_code = 2;
  RegisterKeyboardEvent("keydown", keydown_timestamp, processing_start_keydown,
                        processing_end_keydown, key_code);
  SimulateSwapPromise(swap_time_keydown);
  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(10);
  RegisterKeyboardEvent("keyup", keyup_timestamp, processing_start_keyup,
                        processing_end_keyup, key_code);
  SimulateSwapPromise(swap_time_keyup);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* ukm_entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 7);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      10);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 0);

  // Check UMA recording.
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes", 1);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Keyboard", 1);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.TapOrClick", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Drag", 0);
}

TEST_F(WindowPerformanceTest, HoldingDownAKey) {
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  base::TimeTicks keydown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  int key_code = 2;
  RegisterKeyboardEvent("keydown", keydown_timestamp, processing_start_keydown,
                        processing_end_keydown, key_code);
  SimulateSwapPromise(swap_time_keydown);

  // Second Keydown
  keydown_timestamp = GetTimeStamp(1);
  processing_start_keydown = GetTimeStamp(2);
  processing_end_keydown = GetTimeStamp(3);
  swap_time_keydown = GetTimeStamp(7);
  RegisterKeyboardEvent("keydown", keydown_timestamp, processing_start_keydown,
                        processing_end_keydown, key_code);
  SimulateSwapPromise(swap_time_keydown);

  // Third Keydown
  keydown_timestamp = GetTimeStamp(2);
  processing_start_keydown = GetTimeStamp(3);
  processing_end_keydown = GetTimeStamp(5);
  swap_time_keydown = GetTimeStamp(9);
  RegisterKeyboardEvent("keydown", keydown_timestamp, processing_start_keydown,
                        processing_end_keydown, key_code);
  SimulateSwapPromise(swap_time_keydown);

  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(13);
  RegisterKeyboardEvent("keyup", keyup_timestamp, processing_start_keyup,
                        processing_end_keyup, key_code);
  SimulateSwapPromise(swap_time_keyup);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(3u, entries.size());
  std::vector<std::pair<int, int>> expected_durations;
  expected_durations.emplace_back(std::make_pair(5, 5));
  expected_durations.emplace_back(std::make_pair(6, 6));
  expected_durations.emplace_back(std::make_pair(10, 11));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    auto* entry = entries[i];
    GetUkmRecorder()->ExpectEntryMetric(
        entry,
        ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
        expected_durations[i].first);
    GetUkmRecorder()->ExpectEntryMetric(
        entry,
        ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
        expected_durations[i].second);
    GetUkmRecorder()->ExpectEntryMetric(
        entry,
        ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 0);
  }

  // Check UMA recording.
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes", 3);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Keyboard", 3);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.TapOrClick", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Drag", 0);
}

TEST_F(WindowPerformanceTest, PressMultipleKeys) {
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  // Press the first key.
  base::TimeTicks keydown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  int first_key_code = 2;
  RegisterKeyboardEvent("keydown", keydown_timestamp, processing_start_keydown,
                        processing_end_keydown, first_key_code);
  SimulateSwapPromise(swap_time_keydown);

  // Press the second key.
  processing_start_keydown = GetTimeStamp(2);
  processing_end_keydown = GetTimeStamp(3);
  swap_time_keydown = GetTimeStamp(7);
  int second_key_code = 4;
  RegisterKeyboardEvent("keydown", keydown_timestamp, processing_start_keydown,
                        processing_end_keydown, second_key_code);
  SimulateSwapPromise(swap_time_keydown);

  // Release the first key.
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(13);
  RegisterKeyboardEvent("keyup", keyup_timestamp, processing_start_keyup,
                        processing_end_keyup, first_key_code);
  SimulateSwapPromise(swap_time_keyup);

  // Release the second key.
  keyup_timestamp = GetTimeStamp(5);
  processing_start_keyup = GetTimeStamp(5);
  processing_end_keyup = GetTimeStamp(6);
  swap_time_keyup = GetTimeStamp(20);
  RegisterKeyboardEvent("keyup", keyup_timestamp, processing_start_keyup,
                        processing_end_keyup, second_key_code);
  SimulateSwapPromise(swap_time_keyup);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(2u, entries.size());
  std::vector<std::pair<int, int>> expected_durations;
  expected_durations.emplace_back(std::make_pair(10, 13));
  expected_durations.emplace_back(std::make_pair(15, 20));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    auto* entry = entries[i];
    GetUkmRecorder()->ExpectEntryMetric(
        entry,
        ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
        expected_durations[i].first);
    GetUkmRecorder()->ExpectEntryMetric(
        entry,
        ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
        expected_durations[i].second);
    GetUkmRecorder()->ExpectEntryMetric(
        entry,
        ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 0);
  }
}

TEST_F(WindowPerformanceTest, TapOrClick) {
  // Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent("pointerdown", pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);
  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks swap_time_pointerup = GetTimeStamp(10);
  RegisterPointerEvent("pointerup", pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks swap_time_click = GetTimeStamp(20);
  RegisterPointerEvent("click", click_timestamp, processing_start_click,
                       processing_end_click, pointer_id);
  SimulateSwapPromise(swap_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* ukm_entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 7);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      17);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 1);

  // Check UMA recording.
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes", 1);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Keyboard", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.TapOrClick", 1);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Drag", 0);
}

TEST_F(WindowPerformanceTest, PageVisibilityChanged) {
  // Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent("pointerdown", pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);

  // The page visibility gets changed.
  PageVisibilityChanged(GetTimeStamp(18));

  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks swap_time_pointerup = GetTimeStamp(20);
  RegisterPointerEvent("pointerup", pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks swap_time_click = GetTimeStamp(20);
  RegisterPointerEvent("click", click_timestamp, processing_start_click,
                       processing_end_click, pointer_id);
  SimulateSwapPromise(swap_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* ukm_entry = entries[0];
  // The event duration of pointerdown is 5ms. Because the page visibility was
  // changed after the pointerup, click were created, the event durations of
  // them are 3ms, 3ms. The maximum event duration is 5ms. The total event
  // duration is 9ms.
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 5);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      9);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 1);
}

TEST_F(WindowPerformanceTest, Drag) {
  // Pointerdown
  base::TimeTicks pointerdwon_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent("pointerdown", pointerdwon_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);
  // Notify drag.
  performance_->NotifyPotentialDrag(pointer_id);
  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks swap_time_pointerup = GetTimeStamp(10);
  RegisterPointerEvent("pointerup", pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks swap_time_click = GetTimeStamp(20);
  RegisterPointerEvent("click", click_timestamp, processing_start_click,
                       processing_end_click, pointer_id);
  SimulateSwapPromise(swap_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* ukm_entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 7);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      17);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 2);

  // Check UMA recording.
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes", 1);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Keyboard", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.TapOrClick", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Drag", 1);
}

TEST_F(WindowPerformanceTest, Scroll) {
  // Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  PointerId pointer_id = 5;
  RegisterPointerEvent("pointerdown", pointerdown_timestamp,
                       processing_start_keydown, processing_end_keydown,
                       pointer_id);
  SimulateSwapPromise(swap_time_keydown);
  // Pointercancel
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(10);
  RegisterPointerEvent("pointercancel", pointerup_timestamp,
                       processing_start_keyup, processing_end_keyup,
                       pointer_id);
  SimulateSwapPromise(swap_time_keyup);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Check UMA recording.
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Keyboard", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.TapOrClick", 0);
  GetHistogramTester().ExpectTotalCount(
      "Blink.Responsiveness.UserInteraction.MaxEventDuration.Drag", 0);
}

TEST_F(WindowPerformanceTest, TouchesWithoutClick) {
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  // First Pointerdown
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent("pointerdown", pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);

  // Second Pointerdown
  pointerdown_timestamp = GetTimeStamp(6);
  processing_start_pointerdown = GetTimeStamp(7);
  processing_end_pointerdown = GetTimeStamp(8);
  swap_time_pointerdown = GetTimeStamp(15);
  RegisterPointerEvent("pointerdown", pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(WindowPerformanceTest, ElementTimingTraceEvent) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  // |element| needs to be non-null to prevent a crash.
  performance_->AddElementTiming(
      "image-paint", "url", gfx::RectF(10, 20, 30, 40), GetTimeStamp(2000),
      GetTimeStamp(1000), "identifier", gfx::Size(200, 300), "id",
      /*element*/ page_holder_->GetDocument().documentElement());
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("PerformanceElementTiming");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);
  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");
  std::string* element_type = arg_dict.FindString("elementType");
  ASSERT_TRUE(element_type);
  EXPECT_EQ(*element_type, "image-paint");
  EXPECT_EQ(arg_dict.FindInt("loadTime").value_or(-1), 1000);
  EXPECT_EQ(arg_dict.FindInt("renderTime").value_or(-1), 2000);
  EXPECT_EQ(arg_dict.FindDouble("rectLeft").value_or(-1), 10);
  EXPECT_EQ(arg_dict.FindDouble("rectTop").value_or(-1), 20);
  EXPECT_EQ(arg_dict.FindDouble("rectWidth").value_or(-1), 30);
  EXPECT_EQ(arg_dict.FindDouble("rectHeight").value_or(-1), 40);
  std::string* identifier = arg_dict.FindString("identifier");
  ASSERT_TRUE(identifier);
  EXPECT_EQ(*identifier, "identifier");
  EXPECT_EQ(arg_dict.FindInt("naturalWidth").value_or(-1), 200);
  EXPECT_EQ(arg_dict.FindInt("naturalHeight").value_or(-1), 300);
  std::string* element_id = arg_dict.FindString("elementId");
  ASSERT_TRUE(element_id);
  EXPECT_EQ(*element_id, "id");
  std::string* url = arg_dict.FindString("url");
  ASSERT_TRUE(url);
  EXPECT_EQ(*url, "url");
}

TEST_F(WindowPerformanceTest, EventTimingTraceEvents) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(1);
  base::TimeTicks processing_start = start_time + base::Milliseconds(5);
  base::TimeTicks processing_end = processing_start + base::Milliseconds(10);
  RegisterPointerEvent("pointerdown", start_time, processing_start,
                       processing_end, 4, GetWindow()->document());

  base::TimeTicks swap_time = processing_end + base::Milliseconds(10);
  SimulateSwapPromise(swap_time);

  base::TimeTicks start_time2 = start_time + base::Milliseconds(100);
  base::TimeTicks processing_start2 = start_time2 + base::Milliseconds(5);
  base::TimeTicks processing_end2 = processing_start2 + base::Milliseconds(10);
  RegisterPointerEvent("pointerup", start_time2, processing_start2,
                       processing_end2, 4, GetWindow()->document());

  base::TimeTicks start_time3 = start_time2;
  base::TimeTicks processing_start3 = processing_end2;
  base::TimeTicks processing_end3 = processing_start3 + base::Milliseconds(10);
  RegisterPointerEvent("click", start_time3, processing_start3, processing_end3,
                       4, GetWindow()->document());

  base::TimeTicks swap_time2 = processing_end3 + base::Milliseconds(5);
  SimulateSwapPromise(swap_time2);

  // Only the longer event should have been reported.
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("EventTiming");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(6u, events.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ("devtools.timeline", events[i]->category);

  // Items in the trace events list is ordered chronologically, that is -- trace
  // event with smaller timestamp comes eairlier.
  //
  // --Timestamps--
  // pointerdown_begin: 1000ms
  const trace_analyzer::TraceEvent* pointerdown_begin = events[0];
  // pointerdown_end: 1025ms
  const trace_analyzer::TraceEvent* pointerdown_end = events[1];
  // pointerup_begin: 1100ms
  const trace_analyzer::TraceEvent* pointerup_begin = events[2];
  // click_begin: 1100ms
  const trace_analyzer::TraceEvent* click_begin = events[3];
  // pointerup_end: 1130ms
  const trace_analyzer::TraceEvent* pointerup_end = events[4];
  // click_end: 1130ms
  const trace_analyzer::TraceEvent* click_end = events[5];

  // pointerdown
  ASSERT_TRUE(pointerdown_begin->HasDictArg("data"));
  base::Value::Dict arg_dict = pointerdown_begin->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("interactionId").value_or(-1), 0);
  std::string* event_name = arg_dict.FindString("type");
  ASSERT_TRUE(event_name);
  EXPECT_EQ(*event_name, "pointerdown");
  std::string* frame_trace_value = arg_dict.FindString("frame");
  EXPECT_EQ(*frame_trace_value, ToTraceValue(GetFrame()));
  EXPECT_EQ(arg_dict.FindInt("nodeId"),
            DOMNodeIds::IdForNode(GetWindow()->document()));
  EXPECT_EQ(pointerdown_begin->id, pointerdown_end->id);
  EXPECT_LT(pointerdown_begin->timestamp, pointerdown_end->timestamp);
  ASSERT_FALSE(pointerdown_end->HasDictArg("data"));

  // pointerup
  ASSERT_TRUE(pointerup_begin->HasDictArg("data"));
  arg_dict = pointerup_begin->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("interactionId").value_or(-1), 0);
  event_name = arg_dict.FindString("type");
  ASSERT_TRUE(event_name);
  EXPECT_EQ(*event_name, "pointerup");
  frame_trace_value = arg_dict.FindString("frame");
  EXPECT_EQ(*frame_trace_value, ToTraceValue(GetFrame()));
  EXPECT_EQ(arg_dict.FindInt("nodeId"),
            DOMNodeIds::IdForNode(GetWindow()->document()));
  EXPECT_EQ(pointerup_begin->id, pointerup_end->id);
  EXPECT_LT(pointerup_begin->timestamp, pointerup_end->timestamp);
  ASSERT_FALSE(pointerup_end->HasDictArg("data"));

  // click
  ASSERT_TRUE(click_begin->HasDictArg("data"));
  arg_dict = click_begin->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("interactionId").value_or(-1), 0);
  event_name = arg_dict.FindString("type");
  ASSERT_TRUE(event_name);
  EXPECT_EQ(*event_name, "click");
  frame_trace_value = arg_dict.FindString("frame");
  EXPECT_EQ(*frame_trace_value, ToTraceValue(GetFrame()));
  EXPECT_EQ(arg_dict.FindInt("nodeId"),
            DOMNodeIds::IdForNode(GetWindow()->document()));
  EXPECT_EQ(click_begin->id, click_end->id);
  EXPECT_LT(click_begin->timestamp, click_end->timestamp);
  ASSERT_FALSE(click_end->HasDictArg("data"));
}

TEST_F(WindowPerformanceTest, InteractionID) {
  // Keyboard with max duration 25, total duration 40.
  PerformanceEventTiming* keydown_entry =
      CreatePerformanceEventTiming(event_type_names::kKeydown);
  SimulateInteractionId(keydown_entry, 1, absl::nullopt, GetTimeStamp(100),
                        GetTimeStamp(120));
  PerformanceEventTiming* keyup_entry =
      CreatePerformanceEventTiming(event_type_names::kKeyup);
  SimulateInteractionId(keyup_entry, 1, absl::nullopt, GetTimeStamp(115),
                        GetTimeStamp(140));
  EXPECT_EQ(keydown_entry->interactionId(), keyup_entry->interactionId());
  EXPECT_GT(keydown_entry->interactionId(), 0u);

  // Tap or Click with max duration 70, total duration 90.
  PointerId pointer_id_1 = 10;
  PerformanceEventTiming* pointerdown_entry =
      CreatePerformanceEventTiming(event_type_names::kPointerdown);
  SimulateInteractionId(pointerdown_entry, absl::nullopt, pointer_id_1,
                        GetTimeStamp(100), GetTimeStamp(120));
  PerformanceEventTiming* pointerup_entry =
      CreatePerformanceEventTiming(event_type_names::kPointerup);
  SimulateInteractionId(pointerup_entry, absl::nullopt, pointer_id_1,
                        GetTimeStamp(130), GetTimeStamp(150));
  PerformanceEventTiming* click_entry =
      CreatePerformanceEventTiming(event_type_names::kClick);
  SimulateInteractionId(click_entry, absl::nullopt, pointer_id_1,
                        GetTimeStamp(130), GetTimeStamp(200));
  EXPECT_GT(pointerdown_entry->interactionId(), 0u);
  EXPECT_EQ(pointerdown_entry->interactionId(),
            pointerup_entry->interactionId());
  EXPECT_EQ(pointerup_entry->interactionId(), click_entry->interactionId());

  // Drag with max duration 50, total duration 80.
  PointerId pointer_id_2 = 20;
  pointerdown_entry =
      CreatePerformanceEventTiming(event_type_names::kPointerdown);
  SimulateInteractionId(pointerdown_entry, absl::nullopt, pointer_id_2,
                        GetTimeStamp(150), GetTimeStamp(200));
  performance_->NotifyPotentialDrag(20);
  pointerup_entry = CreatePerformanceEventTiming(event_type_names::kPointerup);
  SimulateInteractionId(pointerup_entry, absl::nullopt, pointer_id_2,
                        GetTimeStamp(200), GetTimeStamp(230));
  EXPECT_GT(pointerdown_entry->interactionId(), 0u);
  EXPECT_EQ(pointerdown_entry->interactionId(),
            pointerup_entry->interactionId());

  // Scroll should not be reported in ukm.
  pointerdown_entry =
      CreatePerformanceEventTiming(event_type_names::kPointerdown);
  SimulateInteractionId(pointerdown_entry, absl::nullopt, pointer_id_2,
                        GetTimeStamp(300), GetTimeStamp(315));
  PerformanceEventTiming* pointercancel_entry =
      CreatePerformanceEventTiming(event_type_names::kPointercancel);
  SimulateInteractionId(pointercancel_entry, absl::nullopt, pointer_id_2,
                        GetTimeStamp(310), GetTimeStamp(330));
  EXPECT_EQ(pointerdown_entry->interactionId(), 0u);
  EXPECT_EQ(pointercancel_entry->interactionId(), 0u);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM values.
  struct {
    int max_duration;
    int total_duration;
    UserInteractionType type;
  } expected_ukm[] = {{25, 40, UserInteractionType::kKeyboard},
                      {70, 90, UserInteractionType::kTapOrClick},
                      {50, 80, UserInteractionType::kDrag}};
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(3u, entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    const ukm::mojom::UkmEntry* ukm_entry = entries[i];
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
        expected_ukm[i].max_duration);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
        expected_ukm[i].total_duration);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName,
        static_cast<int>(expected_ukm[i].type));
  }
}

class InteractionIdTest : public WindowPerformanceTest {
 public:
  struct EventForInteraction {
    EventForInteraction(
        const AtomicString& name,
        absl::optional<int> key_code,
        absl::optional<PointerId> pointer_id,
        base::TimeTicks event_timestamp = base::TimeTicks(),
        base::TimeTicks presentation_timestamp = base::TimeTicks())
        : name_(name),
          key_code_(key_code),
          pointer_id_(pointer_id),
          event_timestamp_(event_timestamp),
          presentation_timestamp_(presentation_timestamp) {}

    AtomicString name_;
    absl::optional<int> key_code_;
    absl::optional<PointerId> pointer_id_;
    base::TimeTicks event_timestamp_;
    base::TimeTicks presentation_timestamp_;
  };

  struct ExpectedUkmValue {
    int max_duration_;
    int total_duration_;
    UserInteractionType interaction_type_;
  };

  std::vector<uint32_t> SimulateInteractionIds(
      const std::vector<EventForInteraction>& events) {
    // Store the entries first and record interactionIds at the end.
    HeapVector<Member<PerformanceEventTiming>> entries;
    for (const auto& event : events) {
      PerformanceEventTiming* entry = CreatePerformanceEventTiming(event.name_);
      SimulateInteractionId(entry, event.key_code_, event.pointer_id_,
                            event.event_timestamp_,
                            event.presentation_timestamp_);
      entries.push_back(entry);
    }
    std::vector<uint32_t> interaction_ids;
    for (const auto& entry : entries) {
      interaction_ids.push_back(entry->interactionId());
    }
    return interaction_ids;
  }

  void CheckUKMValues(const std::vector<ExpectedUkmValue>& expected_ukms) {
    // Flush UKM logging mojo request.
    RunPendingTasks();

    auto entries = GetUkmRecorder()->GetEntriesByName(
        ukm::builders::Responsiveness_UserInteraction::kEntryName);
    EXPECT_EQ(expected_ukms.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      const ukm::mojom::UkmEntry* ukm_entry = entries[i];
      GetUkmRecorder()->ExpectEntryMetric(
          ukm_entry,
          ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
          expected_ukms[i].max_duration_);
      GetUkmRecorder()->ExpectEntryMetric(
          ukm_entry,
          ukm::builders::Responsiveness_UserInteraction::
              kTotalEventDurationName,
          expected_ukms[i].total_duration_);
      GetUkmRecorder()->ExpectEntryMetric(
          ukm_entry,
          ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName,
          static_cast<int>(expected_ukms[i].interaction_type_));
    }
  }
};

// Tests English typing.
TEST_F(InteractionIdTest, InputOutsideComposition) {
  // Insert "a" with a max duration of 50 and total of 50.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 65, absl::nullopt, GetTimeStamp(100),
       GetTimeStamp(150)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(120), GetTimeStamp(220)},
      {event_type_names::kKeyup, 65, absl::nullopt, GetTimeStamp(130),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_GT(ids1[0], 0u) << "Keydown interactionId was nonzero";
  EXPECT_EQ(ids1[1], 0u) << "Input interactionId was zero";
  EXPECT_EQ(ids1[0], ids1[2]) << "Keydown and keyup interactionId match";

  // Insert "3" with a max duration of 40 and total of 60.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 53, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(220)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(220), GetTimeStamp(320)},
      {event_type_names::kKeyup, 53, absl::nullopt, GetTimeStamp(250),
       GetTimeStamp(290)}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_GT(ids2[0], 0u) << "Second keydown has nonzero interactionId";
  EXPECT_EQ(ids2[1], 0u) << "Second input interactionId was zero";
  EXPECT_EQ(ids2[0], ids2[2]) << "Second keydown and keyup interactionId match";
  EXPECT_NE(ids1[0], ids2[0])
      << "First and second keydown have different interactionId";

  // Backspace with max duration of 25 and total of 25.
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kKeydown, 8, absl::nullopt, GetTimeStamp(300),
       GetTimeStamp(320)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(300), GetTimeStamp(400)},
      {event_type_names::kKeyup, 8, absl::nullopt, GetTimeStamp(300),
       GetTimeStamp(325)}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);
  EXPECT_GT(ids3[0], 0u) << "Third keydown has nonzero interactionId";
  EXPECT_EQ(ids3[1], 0u) << "Third input interactionId was zero";
  EXPECT_EQ(ids3[0], ids3[2]) << "Third keydown and keyup interactionId match";
  EXPECT_NE(ids1[0], ids3[0])
      << "First and third keydown have different interactionId";
  EXPECT_NE(ids2[0], ids3[0])
      << "Second and third keydown have different interactionId";

  CheckUKMValues({{50, 50, UserInteractionType::kKeyboard},
                  {40, 60, UserInteractionType::kKeyboard},
                  {25, 25, UserInteractionType::kKeyboard}});
}

// Tests Japanese on Mac.
TEST_F(InteractionIdTest, CompositionSingleKeydown) {
  // Insert "a" with a duration of 20.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(100),
       GetTimeStamp(200)},
      {event_type_names::kCompositionstart, absl::nullopt, absl::nullopt},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(120), GetTimeStamp(140)},
      {event_type_names::kKeyup, 65, absl::nullopt, GetTimeStamp(120),
       GetTimeStamp(220)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_EQ(ids1[0], 0u) << "Keydown interactionId was zero";
  EXPECT_EQ(ids1[1], 0u) << "Compositionstart interactionId was zero";
  EXPECT_GT(ids1[2], 0u) << "Input interactionId was nonzero";
  EXPECT_EQ(ids1[3], 0u) << "Keyup interactionId was zero";

  // Insert "b" and finish composition with a duration of 30.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(230), GetTimeStamp(260)},
      {event_type_names::kKeyup, 66, absl::nullopt, GetTimeStamp(270),
       GetTimeStamp(370)},
      {event_type_names::kCompositionend, absl::nullopt, absl::nullopt}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_EQ(ids2[0], 0u) << "Second keydown interactionId was zero";
  EXPECT_GT(ids2[1], 0u) << "Second input interactionId was nonzero";
  EXPECT_EQ(ids2[2], 0u) << "Second keyup interactionId was zero";
  EXPECT_EQ(ids2[3], 0u) << "Compositionend interactionId was zero";
  EXPECT_NE(ids1[2], ids2[1])
      << "First and second inputs have different interactionIds";

  CheckUKMValues({{20, 20, UserInteractionType::kKeyboard},
                  {30, 30, UserInteractionType::kKeyboard}});
}

// Tests Chinese on Mac. Windows is similar, but has more keyups inside the
// composition.
TEST_F(InteractionIdTest, CompositionToFinalInput) {
  // Insert "a" with a duration of 25.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(100),
       GetTimeStamp(190)},
      {event_type_names::kCompositionstart, absl::nullopt, absl::nullopt},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(100), GetTimeStamp(125)},
      {event_type_names::kKeyup, 65, absl::nullopt, GetTimeStamp(110),
       GetTimeStamp(190)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_GT(ids1[2], 0u) << "First input nonzero";

  // Insert "b" with a duration of 35.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(290)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(220), GetTimeStamp(255)},
      {event_type_names::kKeyup, 66, absl::nullopt, GetTimeStamp(210),
       GetTimeStamp(290)}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_GT(ids2[1], 0u) << "Second input nonzero";
  EXPECT_NE(ids1[2], ids2[1])
      << "First and second input have different interactionIds";

  // Select a composed input and finish, with a duration of 140.
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(300), GetTimeStamp(440)},
      {event_type_names::kCompositionend, absl::nullopt, absl::nullopt}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);
  EXPECT_EQ(ids3[1], 0u) << "Compositionend has zero interactionId";
  EXPECT_GT(ids3[0], 0u) << "Third input has nonzero interactionId";
  EXPECT_NE(ids1[2], ids3[0])
      << "First and third inputs have different interactionIds";
  EXPECT_NE(ids2[1], ids3[0])
      << "Second and third inputs have different interactionIds";

  CheckUKMValues({{25, 25, UserInteractionType::kKeyboard},
                  {35, 35, UserInteractionType::kKeyboard},
                  {140, 140, UserInteractionType::kKeyboard}});
}

// Tests Chinese on Windows.
TEST_F(InteractionIdTest, CompositionToFinalInputMultipleKeyUps) {
  // Insert "a" with a duration of 66.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(100)},
      {event_type_names::kCompositionstart, absl::nullopt, absl::nullopt},
      {event_type_names::kInput, absl::nullopt, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(66)},
      {event_type_names::kKeyup, 229, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(100)},
      {event_type_names::kKeyup, 65, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(100)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_GT(ids1[2], 0u) << "First input nonzero";
  EXPECT_EQ(ids1[3], 0u) << "First keyup has zero interactionId";
  EXPECT_EQ(ids1[4], 0u) << "Second keyup has zero interactionId";

  // Insert "b" with a duration of 51.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(200), GetTimeStamp(251)},
      {event_type_names::kKeyup, 229, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kKeyup, 66, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_GT(ids2[1], 0u) << "Second input nonzero";
  EXPECT_NE(ids1[2], ids2[1])
      << "First and second input have different interactionIds";
  EXPECT_EQ(ids2[2], 0u) << "Third keyup has zero interactionId";
  EXPECT_EQ(ids2[3], 0u) << "Fourth keyup has zero interactionId";

  // Select a composed input and finish, with duration of 85.
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(300), GetTimeStamp(385)},
      {event_type_names::kCompositionend, absl::nullopt, absl::nullopt}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);
  EXPECT_GT(ids3[0], 0u) << "Third input has nonzero interactionId";
  EXPECT_NE(ids1[2], ids3[0])
      << "First and third inputs have different interactionIds";
  EXPECT_NE(ids2[1], ids3[0])
      << "Second and third inputs have different interactionIds";

  CheckUKMValues({{66, 66, UserInteractionType::kKeyboard},
                  {51, 51, UserInteractionType::kKeyboard},
                  {85, 85, UserInteractionType::kKeyboard}});
}

// Tests Android smart suggestions (similar to Android Chinese).
TEST_F(InteractionIdTest, SmartSuggestion) {
  // Insert "A" with a duration of 9.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(16)},
      {event_type_names::kCompositionstart, absl::nullopt, absl::nullopt},
      {event_type_names::kInput, absl::nullopt, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(9)},
      {event_type_names::kKeyup, 229, absl::nullopt, GetTimeStamp(0),
       GetTimeStamp(16)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_GT(ids1[2], 0u) << "First input nonzero";

  // Compose to "At" with a duration of 14.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(100), GetTimeStamp(114)},
      {event_type_names::kCompositionend, absl::nullopt, absl::nullopt}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_GT(ids2[0], 0u) << "Second input nonzero";
  EXPECT_NE(ids1[2], ids2[1])
      << "First and second input have different interactionIds";

  // Add "the". No composition so need to consider the keydown and keyup.
  // Max duration of 43 and total duration of 70
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kKeydown, 229, absl::nullopt, GetTimeStamp(200),
       GetTimeStamp(243)},
      {event_type_names::kInput, absl::nullopt, absl::nullopt,
       GetTimeStamp(200), GetTimeStamp(300)},
      {event_type_names::kKeyup, 229, absl::nullopt, GetTimeStamp(235),
       GetTimeStamp(270)}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);
  EXPECT_GT(ids3[0], 0u) << "Keydown nonzero";
  EXPECT_EQ(ids3[0], ids3[2]) << "Keydown and keyup have some id";
  EXPECT_EQ(ids3[1], 0u) << "Third input has zero id";

  CheckUKMValues({{9, 9, UserInteractionType::kKeyboard},
                  {14, 14, UserInteractionType::kKeyboard},
                  {43, 70, UserInteractionType::kKeyboard}});
}

TEST_F(InteractionIdTest, TapWithoutClick) {
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerdown, absl::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(140)},
      {event_type_names::kPointerup, absl::nullopt, 1, GetTimeStamp(120),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_GT(ids[0], 0u) << "Nonzero interaction id";
  EXPECT_EQ(ids[0], ids[1])
      << "Pointerdown and pointerup have same interaction id";
  // No UKM value, since we are waiting for click.
  RunPendingTasks();
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(entries.size(), 0u);

  // After a wait, we should see the UKM.
  test::RunDelayedTasks(base::Seconds(1));
  CheckUKMValues({{40, 50, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, PointerupClick) {
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerup, absl::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(140)},
      {event_type_names::kClick, absl::nullopt, 1, GetTimeStamp(120),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_GT(ids[0], 0u) << "Nonzero interaction id";
  EXPECT_EQ(ids[0], ids[1]) << "Pointerup and click have same interaction id";
  // Flush UKM logging mojo request.
  RunPendingTasks();
  CheckUKMValues({{40, 50, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, JustClick) {
  // Hitting enter on a keyboard may cause just a trusted click event.
  std::vector<EventForInteraction> events = {
      {event_type_names::kClick, absl::nullopt, -1, GetTimeStamp(120),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_GT(ids[0], 0u) << "Nonzero interaction id";
  // Flush UKM logging mojo request.
  RunPendingTasks();
  CheckUKMValues({{30, 30, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, PointerdownClick) {
  // Contextmenus may cause us to only see pointerdown and click (no pointerup).
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerdown, absl::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(140)},
      {event_type_names::kClick, absl::nullopt, 1, GetTimeStamp(120),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_GT(ids[0], 0u) << "Nonzero interaction id";
  EXPECT_EQ(ids[0], ids[1]) << "Pointerdown and click have same interaction id";
  // Flush UKM logging mojo request.
  RunPendingTasks();
  CheckUKMValues({{40, 50, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, MultiTouch) {
  // In multitouch, we report an interaction per pointerId. We do not see
  // clicks.
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerdown, absl::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(110)},
      {event_type_names::kPointerdown, absl::nullopt, 2, GetTimeStamp(120),
       GetTimeStamp(140)},
      {event_type_names::kPointerup, absl::nullopt, 2, GetTimeStamp(200),
       GetTimeStamp(230)},
      {event_type_names::kPointerup, absl::nullopt, 1, GetTimeStamp(200),
       GetTimeStamp(250)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  for (uint32_t id : ids) {
    EXPECT_GT(id, 0u);
  }
  // Interaction ids should match by PointerId.
  EXPECT_EQ(ids[0], ids[3]);
  EXPECT_EQ(ids[1], ids[2]);
  // After a wait, flush UKM logging mojo request.
  test::RunDelayedTasks(base::Seconds(1));
  CheckUKMValues({{50, 60, UserInteractionType::kTapOrClick},
                  {30, 50, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, ClickIncorrectPointerId) {
  // On mobile, in cases where touchstart is skipped, click does not get the
  // correct pointerId. See crbug.com/1264930 for more details.
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerup, absl::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(130)},
      {event_type_names::kClick, absl::nullopt, 0, GetTimeStamp(120),
       GetTimeStamp(160)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_GT(ids[0], 0u) << "Nonzero interaction id";
  EXPECT_EQ(ids[0], ids[1]) << "Pointerup and click have same interaction id";
  // Flush UKM logging mojo request.
  RunPendingTasks();
  CheckUKMValues({{40, 60, UserInteractionType::kTapOrClick}});
}

}  // namespace blink
