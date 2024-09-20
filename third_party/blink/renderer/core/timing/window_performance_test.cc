// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include <cstdint>

#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_timing_details.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
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
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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

  void AddLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ |= PerformanceEntry::kLongTask;
  }

  void RemoveLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ = PerformanceEntry::kInvalid;
  }

  void SimulatePaint() { performance_->OnPaintFinished(); }
  void SimulateResolvePresentationPromise(uint64_t presentation_index,
                                          base::TimeTicks timestamp) {
    viz::FrameTimingDetails presentation_details;
    presentation_details.presentation_feedback.timestamp = timestamp;
    performance_->OnPresentationPromiseResolved(presentation_index,
                                                presentation_details);
  }

  // Only use this function if you don't care about the time difference between
  // paint & frame presented. Otherwise, use SimulatePaint() &
  // SimulateResolvePresentationPromise() separately instead and perform actions
  // in between as needed.
  void SimulatePaintAndResolvePresentationPromise(base::TimeTicks timestamp) {
    SimulatePaint();
    SimulateResolvePresentationPromise(
        performance_->event_presentation_promise_count_, timestamp);
  }

  void SimulateInteractionId(PerformanceEventTiming* entry) {
    ResponsivenessMetrics::EventTimestamps event_timestamps = {
        entry->GetEventTimingReportingInfo()->creation_time, base::TimeTicks(),
        base::TimeTicks(),
        entry->GetEventTimingReportingInfo()->fallback_time.has_value()
            ? entry->GetEventTimingReportingInfo()->fallback_time.value()
            : entry->GetEventTimingReportingInfo()->presentation_time.value()};
    performance_->SetInteractionIdAndRecordLatency(entry, event_timestamps);
  }

  uint64_t RegisterKeyboardEvent(AtomicString type,
                                 base::TimeTicks start_time,
                                 base::TimeTicks processing_start,
                                 base::TimeTicks processing_end,
                                 int key_code) {
    KeyboardEventInit* init = KeyboardEventInit::Create();
    init->setKeyCode(key_code);
    KeyboardEvent* keyboard_event =
        MakeGarbageCollected<KeyboardEvent>(type, init);
    performance_->RegisterEventTiming(*keyboard_event, keyboard_event->target(),
                                      start_time, processing_start,
                                      processing_end);
    return performance_->event_presentation_promise_count_;
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
    performance_->RegisterEventTiming(*pointer_event, pointer_event->target(),
                                      start_time, processing_start,
                                      processing_end);
  }

  PerformanceEventTiming* CreatePerformanceEventTiming(
      const AtomicString& name,
      std::optional<int> key_code,
      std::optional<PointerId> pointer_id,
      base::TimeTicks event_creation_timestamp,
      base::TimeTicks presentation_timestamp) {
    PerformanceEventTiming::EventTimingReportingInfo reporting_info{
        .creation_time = event_creation_timestamp,
        .presentation_time = presentation_timestamp,
        .key_code = key_code,
        .pointer_id = pointer_id};

    return PerformanceEventTiming::Create(
        name, reporting_info, false, nullptr,
        LocalDOMWindow::From(GetScriptState()));
  }

  HeapVector<Member<PerformanceEventTiming>>*
  GetWindowPerformanceEventTimingEntries() {
    return &performance_->event_timing_entries_;
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
    performance_->PageVisibilityChangedWithTimestamp(timestamp);
  }

  test::TaskEnvironment task_environment_;
  Persistent<WindowPerformance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  ScopedFakeUkmRecorder scoped_fake_ukm_recorder_;
  base::HistogramTester histogram_tester_;
};

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

  // Simulate navigation commit.
  GetFrame()->DomWindow()->FrameDestroyed();
}

// Checks that WindowPerformance object and its fields (like PerformanceTiming)
// function correctly after transition to another document in the same window.
// This happens when a page opens a new window and it navigates to a same-origin
// document.
TEST(PerformanceLifetimeTest, SurviveContextSwitch) {
  test::TaskEnvironment task_environment;
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
      WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
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
  PerformanceEntryVector entries =
      performance_->getEntriesByType(performance_entry_names::kMark);
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
  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  base::TimeTicks presentation_time = GetTimeOrigin() + base::Seconds(6.0);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(1u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  page_holder_->GetFrame()
      .Loader()
      .GetDocumentLoader()
      ->GetTiming()
      .MarkLoadEventStart();
  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(2u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  GetFrame()->DetachDocument();
  EXPECT_FALSE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(3u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());
}

TEST_F(WindowPerformanceTest, Expose100MsEvents) {
  base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(1);
  base::TimeTicks processing_start = start_time + base::Milliseconds(10);
  base::TimeTicks processing_end = processing_start + base::Milliseconds(10);
  RegisterPointerEvent(event_type_names::kMousedown, start_time,
                       processing_start, processing_end, 4);

  base::TimeTicks start_time2 = start_time + base::Microseconds(200);
  RegisterPointerEvent(event_type_names::kClick, start_time2, processing_start,
                       processing_end, 4);

  // The presentation time is 100.1 ms after |start_time| but only 99.9 ms after
  // |start_time2|.
  base::TimeTicks presentation_time = start_time + base::Microseconds(100100);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  // Only the longer event should have been reported.
  const auto& entries =
      performance_->getBufferedEntriesByType(performance_entry_names::kEvent);
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(event_type_names::kMousedown, entries.at(0)->name());
}

TEST_F(WindowPerformanceTest, EventTimingDuration) {
  base::TimeTicks start_time = GetTimeOrigin() + base::Milliseconds(1000);
  base::TimeTicks processing_start = GetTimeOrigin() + base::Milliseconds(1001);
  base::TimeTicks processing_end = GetTimeOrigin() + base::Milliseconds(1002);
  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  base::TimeTicks short_presentation_time =
      GetTimeOrigin() + base::Milliseconds(1003);
  SimulatePaintAndResolvePresentationPromise(short_presentation_time);
  EXPECT_EQ(0u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  base::TimeTicks long_presentation_time =
      GetTimeOrigin() + base::Milliseconds(2000);
  SimulatePaintAndResolvePresentationPromise(long_presentation_time);
  EXPECT_EQ(1u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(short_presentation_time);
  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(long_presentation_time);
  EXPECT_EQ(2u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());
}

// Test the case where multiple events are registered and then their
// presentation promise is resolved.
TEST_F(WindowPerformanceTest, MultipleEventsThenPresent) {
  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(i);
    base::TimeTicks processing_start = start_time + base::Milliseconds(100);
    base::TimeTicks processing_end = start_time + base::Milliseconds(200);
    RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                         processing_end, 4);
    EXPECT_EQ(
        0u,
        performance_->getBufferedEntriesByType(performance_entry_names::kEvent)
            .size());
  }
  base::TimeTicks presentation_time =
      GetTimeOrigin() + base::Seconds(num_events);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(
      num_events,
      performance_->getBufferedEntriesByType(performance_entry_names::kEvent)
          .size());
}

// Test the case where commit finish timestamps are recorded on all pending
// EventTimings.
TEST_F(WindowPerformanceTest,
       CommitFinishTimeRecordedOnAllPendingEventTimings) {
  size_t num_events = 3;
  for (size_t i = 0; i < num_events; ++i) {
    base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(i);
    base::TimeTicks processing_start = start_time + base::Milliseconds(100);
    base::TimeTicks processing_end = start_time + base::Milliseconds(200);
    RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                         processing_end, 4);
  }
  auto* event_timing_entries = GetWindowPerformanceEventTimingEntries();
  EXPECT_EQ(event_timing_entries->size(), 3u);
  for (const auto event_data : *event_timing_entries) {
    EXPECT_FALSE(event_data->GetEventTimingReportingInfo()
                     ->commit_finish_time.has_value());
  }
  base::TimeTicks commit_finish_time = GetTimeOrigin() + base::Seconds(2);
  performance_->SetCommitFinishTimeStampForPendingEvents(commit_finish_time);
  for (const auto event_data : *event_timing_entries) {
    EXPECT_TRUE(event_data->GetEventTimingReportingInfo()
                    ->commit_finish_time.has_value());
    EXPECT_EQ(
        event_data->GetEventTimingReportingInfo()->commit_finish_time.value(),
        commit_finish_time);
  }
}

// Test the case where a new commit finish timestamps does not affect previous
// EventTiming who has already seen a commit finish.
TEST_F(WindowPerformanceTest, NewCommitNotOverwritePreviousEventTimings) {
  base::TimeTicks start_time = GetTimeOrigin() + base::Seconds(1);
  base::TimeTicks processing_start = start_time + base::Milliseconds(100);
  base::TimeTicks processing_end = start_time + base::Milliseconds(200);
  RegisterPointerEvent(event_type_names::kClick, start_time, processing_start,
                       processing_end, 4);
  base::TimeTicks commit_finish_time = GetTimeOrigin() + base::Seconds(2);
  performance_->SetCommitFinishTimeStampForPendingEvents(commit_finish_time);
  auto* event_timing_entries = GetWindowPerformanceEventTimingEntries();
  EXPECT_EQ(event_timing_entries->size(), 1u);
  EXPECT_EQ(event_timing_entries->at(0)
                ->GetEventTimingReportingInfo()
                ->commit_finish_time,
            commit_finish_time);
  // Set a new commit finish timestamp.
  base::TimeTicks commit_finish_time_1 = commit_finish_time + base::Seconds(1);
  performance_->SetCommitFinishTimeStampForPendingEvents(commit_finish_time_1);
  EXPECT_EQ(event_timing_entries->at(0)
                ->GetEventTimingReportingInfo()
                ->commit_finish_time,
            commit_finish_time);
  EXPECT_NE(event_timing_entries->at(0)
                ->GetEventTimingReportingInfo()
                ->commit_finish_time,
            commit_finish_time_1);
}

// Test for existence of 'first-input' given different types of first events.
TEST_F(WindowPerformanceTest, FirstInput) {
  struct {
    AtomicString event_type;
    bool should_report;
  } inputs[] = {{event_type_names::kClick, true},
                {event_type_names::kKeydown, true},
                {event_type_names::kKeypress, false},
                {event_type_names::kPointerdown, false},
                {event_type_names::kMousedown, true},
                {event_type_names::kMouseover, false}};
  for (const auto& input : inputs) {
    // first-input does not have a |duration| threshold so use close values.
    if (input.event_type == event_type_names::kKeydown ||
        input.event_type == event_type_names::kKeypress) {
      RegisterKeyboardEvent(input.event_type, GetTimeOrigin(),
                            GetTimeOrigin() + base::Milliseconds(1),
                            GetTimeOrigin() + base::Milliseconds(2), 4);
    } else {
      RegisterPointerEvent(input.event_type, GetTimeOrigin(),
                           GetTimeOrigin() + base::Milliseconds(1),
                           GetTimeOrigin() + base::Milliseconds(2), 4);
    }
    SimulatePaintAndResolvePresentationPromise(GetTimeOrigin() +
                                               base::Milliseconds(3));
    PerformanceEntryVector firstInputs =
        performance_->getEntriesByType(performance_entry_names::kFirstInput);
    EXPECT_GE(1u, firstInputs.size());
    EXPECT_EQ(input.should_report, firstInputs.size() == 1u);
    ResetPerformance();
  }
}

// Test that the 'first-input' is populated after some irrelevant events are
// ignored.
TEST_F(WindowPerformanceTest, FirstInputAfterIgnored) {
  AtomicString several_events[] = {event_type_names::kMouseover,
                                   event_type_names::kMousedown,
                                   event_type_names::kPointerup};
  for (const auto& event : several_events) {
    RegisterPointerEvent(event, GetTimeOrigin(),
                         GetTimeOrigin() + base::Milliseconds(1),
                         GetTimeOrigin() + base::Milliseconds(2), 4);
    SimulatePaintAndResolvePresentationPromise(GetTimeOrigin() +
                                               base::Milliseconds(3));
  }
  ASSERT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  EXPECT_EQ(
      event_type_names::kMousedown,
      performance_->getEntriesByType(performance_entry_names::kFirstInput)[0]
          ->name());
}

// Test that pointerdown followed by pointerup works as a 'firstInput'.
TEST_F(WindowPerformanceTest, FirstPointerUp) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks presentation_time = GetTimeStamp(3);
  RegisterPointerEvent(event_type_names::kPointerdown, start_time,
                       processing_start, processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  RegisterPointerEvent(event_type_names::kPointerup, start_time,
                       processing_start, processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  // The name of the entry should be event_type_names::kPointerdown.
  EXPECT_EQ(1u, performance_
                    ->getEntriesByName(event_type_names::kPointerdown,
                                       performance_entry_names::kFirstInput)
                    .size());
}

// When the pointerdown is optimized out, the mousedown works as a
// 'first-input'.
TEST_F(WindowPerformanceTest, PointerdownOptimizedOut) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks presentation_time = GetTimeStamp(3);
  RegisterPointerEvent(event_type_names::kMousedown, start_time,
                       processing_start, processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  // The name of the entry should be event_type_names::kMousedown.
  EXPECT_EQ(1u, performance_
                    ->getEntriesByName(event_type_names::kMousedown,
                                       performance_entry_names::kFirstInput)
                    .size());
}

// Test that pointerdown followed by mousedown, pointerup works as a
// 'first-input'.
TEST_F(WindowPerformanceTest, PointerdownOnDesktop) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks presentation_time = GetTimeStamp(3);
  RegisterPointerEvent(event_type_names::kPointerdown, start_time,
                       processing_start, processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  RegisterPointerEvent(event_type_names::kMousedown, start_time,
                       processing_start, processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  RegisterPointerEvent(event_type_names::kPointerup, start_time,
                       processing_start, processing_end, 4);
  SimulatePaintAndResolvePresentationPromise(presentation_time);
  EXPECT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  // The name of the entry should be event_type_names::kPointerdown.
  EXPECT_EQ(1u, performance_
                    ->getEntriesByName(event_type_names::kPointerdown,
                                       performance_entry_names::kFirstInput)
                    .size());
}

TEST_F(WindowPerformanceTest, OneKeyboardInteraction) {
  base::TimeTicks keydown_timestamp = GetTimeStamp(0);
  // Keydown
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks presentation_time_keydown = GetTimeStamp(5);
  int key_code = 2;
  RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                        processing_start_keydown, processing_end_keydown,
                        key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);
  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks presentation_time_keyup = GetTimeStamp(10);
  RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                        processing_start_keyup, processing_end_keyup, key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);

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
  base::TimeTicks presentation_time_keydown = GetTimeStamp(5);
  int key_code = 2;
  RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                        processing_start_keydown, processing_end_keydown,
                        key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

  // Second Keydown
  keydown_timestamp = GetTimeStamp(1);
  processing_start_keydown = GetTimeStamp(2);
  processing_end_keydown = GetTimeStamp(3);
  presentation_time_keydown = GetTimeStamp(7);
  RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                        processing_start_keydown, processing_end_keydown,
                        key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

  // Third Keydown
  keydown_timestamp = GetTimeStamp(2);
  processing_start_keydown = GetTimeStamp(3);
  processing_end_keydown = GetTimeStamp(5);
  presentation_time_keydown = GetTimeStamp(9);
  RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                        processing_start_keydown, processing_end_keydown,
                        key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks presentation_time_keyup = GetTimeStamp(13);
  RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                        processing_start_keyup, processing_end_keyup, key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);

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
    auto* entry = entries[i].get();
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
  base::TimeTicks presentation_time_keydown = GetTimeStamp(5);
  int first_key_code = 2;
  RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                        processing_start_keydown, processing_end_keydown,
                        first_key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

  // Press the second key.
  processing_start_keydown = GetTimeStamp(2);
  processing_end_keydown = GetTimeStamp(3);
  presentation_time_keydown = GetTimeStamp(7);
  int second_key_code = 4;
  RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                        processing_start_keydown, processing_end_keydown,
                        second_key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

  // Release the first key.
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks presentation_time_keyup = GetTimeStamp(13);
  RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                        processing_start_keyup, processing_end_keyup,
                        first_key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);

  // Release the second key.
  keyup_timestamp = GetTimeStamp(5);
  processing_start_keyup = GetTimeStamp(5);
  processing_end_keyup = GetTimeStamp(6);
  presentation_time_keyup = GetTimeStamp(20);
  RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                        processing_start_keyup, processing_end_keyup,
                        second_key_code);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);

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
    auto* entry = entries[i].get();
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

// Test a real world scenario, where keydown got presented first but its
// callback got invoked later than keyup's due to multi processes & threading
// overhead.
TEST_F(WindowPerformanceTest, KeyupFinishLastButCallbackInvokedFirst) {
  // Arbitrary keycode picked for testing from
  // https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/keyCode#value_of_keycode
  int digit_1_key_code = 0x31;

  // Keydown
  base::TimeTicks keydown_timestamp = GetTimeStamp(0);
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(5);
  base::TimeTicks presentation_time_keydown = GetTimeStamp(7);
  const uint64_t presentation_index_keydown = RegisterKeyboardEvent(
      event_type_names::kKeydown, keydown_timestamp, processing_start_keydown,
      processing_end_keydown, digit_1_key_code);

  SimulatePaint();

  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(6);
  base::TimeTicks processing_end_keyup = GetTimeStamp(7);
  base::TimeTicks presentation_promise_break_time_keyup = GetTimeStamp(8);
  const uint64_t presentation_index_keyup = RegisterKeyboardEvent(
      event_type_names::kKeyup, keyup_timestamp, processing_start_keyup,
      processing_end_keyup, digit_1_key_code);

  // keyup resolved without a paint, due to no damage.
  SimulateResolvePresentationPromise(presentation_index_keyup,
                                     presentation_promise_break_time_keyup);
  SimulateResolvePresentationPromise(presentation_index_keydown,
                                     presentation_time_keydown);

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
      8);
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

TEST_F(WindowPerformanceTest, TapOrClick) {
  // Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks presentation_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerdown);
  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks presentation_time_pointerup = GetTimeStamp(10);
  RegisterPointerEvent(event_type_names::kPointerup, pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks presentation_time_click = GetTimeStamp(20);
  RegisterPointerEvent(event_type_names::kClick, click_timestamp,
                       processing_start_click, processing_end_click,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_click);

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
  base::TimeTicks presentation_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerdown);

  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  RegisterPointerEvent(event_type_names::kPointerup, pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks presentation_time_pointerup_and_click = GetTimeStamp(20);
  RegisterPointerEvent(event_type_names::kClick, click_timestamp,
                       processing_start_click, processing_end_click,
                       pointer_id);

  performance_->GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden, true);
  PageVisibilityChanged(GetTimeStamp(18));

  SimulatePaintAndResolvePresentationPromise(
      presentation_time_pointerup_and_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* ukm_entry = entries[0];
  // The event duration of pointerdown is 5ms, all the way to presentation.
  // The event duration of pointerup is processingEnd 6 - event
  // creation time 3 = 3.
  // The event duration of click is page visibility change time 16 - 13 = 3.
  // So the max duration should be 5 ms.
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 5);
  // The total duration should be 9ms, which is the sum of time from time 0 of
  // pointer down creation time to the processingEnd of pointer up 6ms +
  // duration of click which is 16-13 = 3ms.
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      9);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 1);

  EXPECT_EQ(1ul, performance_->interactionCount());
}

TEST_F(WindowPerformanceTest, Drag) {
  // Pointerdown
  base::TimeTicks pointerdwon_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks presentation_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdwon_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerdown);
  // Notify drag.
  performance_->NotifyPotentialDrag(pointer_id);
  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks presentation_time_pointerup = GetTimeStamp(10);
  RegisterPointerEvent(event_type_names::kPointerup, pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks presentation_time_click = GetTimeStamp(20);
  RegisterPointerEvent(event_type_names::kClick, click_timestamp,
                       processing_start_click, processing_end_click,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_click);

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
  base::TimeTicks presentation_time_keydown = GetTimeStamp(5);
  PointerId pointer_id = 5;
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdown_timestamp,
                       processing_start_keydown, processing_end_keydown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);
  // Pointercancel
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks presentation_time_keyup = GetTimeStamp(10);
  RegisterPointerEvent(event_type_names::kPointercancel, pointerup_timestamp,
                       processing_start_keyup, processing_end_keyup,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);

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
  base::TimeTicks presentation_time_pointerdown = GetTimeStamp(5);
  PointerId pointer_id = 4;
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerdown);

  // Second Pointerdown
  pointerdown_timestamp = GetTimeStamp(6);
  processing_start_pointerdown = GetTimeStamp(7);
  processing_end_pointerdown = GetTimeStamp(8);
  presentation_time_pointerdown = GetTimeStamp(15);
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerdown);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

#if BUILDFLAG(IS_MAC)
//  Test artificial pointerup and click on MacOS fall back to use processingEnd
//  as event duration ending time.
//  See crbug.com/1321819
TEST_F(WindowPerformanceTest, ArtificialPointerupOrClick) {
  // Arbitrary pointerId picked for testing
  PointerId pointer_id = 4;

  // Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks presentation_time_pointerdown = GetTimeStamp(3);
  RegisterPointerEvent(event_type_names::kPointerdown, pointerdown_timestamp,
                       processing_start_pointerdown, processing_end_pointerdown,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerdown);
  // Artificial Pointerup
  base::TimeTicks pointerup_timestamp = pointerdown_timestamp;
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks presentation_time_pointerup = GetTimeStamp(10);
  RegisterPointerEvent(event_type_names::kPointerup, pointerup_timestamp,
                       processing_start_pointerup, processing_end_pointerup,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_pointerup);
  // Artificial Click
  base::TimeTicks click_timestamp = pointerup_timestamp;
  base::TimeTicks processing_start_click = GetTimeStamp(11);
  base::TimeTicks processing_end_click = GetTimeStamp(12);
  base::TimeTicks presentation_time_click = GetTimeStamp(20);
  RegisterPointerEvent(event_type_names::kClick, click_timestamp,
                       processing_start_click, processing_end_click,
                       pointer_id);
  SimulatePaintAndResolvePresentationPromise(presentation_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* ukm_entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 12);
  GetUkmRecorder()->ExpectEntryMetric(
      ukm_entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      12);
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
#endif  // BUILDFLAG(IS_MAC)

// The trace_analyzer does not work on platforms on which the migration of
// tracing into Perfetto has not completed.
TEST_F(WindowPerformanceTest, PerformanceMarkTraceEvent) {
  v8::HandleScope handle_scope(GetScriptState()->GetIsolate());
  v8::Local<v8::Context> context = GetScriptState()->GetContext();
  v8::Context::Scope context_scope(context);
  DummyExceptionStateForTesting exception_state;

  using trace_analyzer::Query;
  trace_analyzer::Start("*");

  performance_->mark(GetScriptState(), AtomicString("test_trace"), nullptr,
                     exception_state);

  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;

  Query q = Query::EventNameIs("test_trace");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());

  EXPECT_EQ("blink.user_timing", events[0]->category);

  ASSERT_TRUE(events[0]->HasDictArg("data"));

  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");

  std::optional<double> start_time = arg_dict.FindDouble("startTime");
  ASSERT_TRUE(start_time.has_value());

  // The navigationId should be recorded if performance.mark is executed by a
  // document.
  std::string* navigation_id = arg_dict.FindString("navigationId");
  ASSERT_TRUE(navigation_id);
}

TEST_F(WindowPerformanceTest, ElementTimingTraceEvent) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  // |element| needs to be non-null to prevent a crash.
  performance_->AddElementTiming(
      AtomicString("image-paint"), "url", gfx::RectF(10, 20, 30, 40),
      GetTimeStamp(2000), GetTimeStamp(1000), AtomicString("identifier"),
      gfx::Size(200, 300), AtomicString("id"),
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
  RegisterPointerEvent(event_type_names::kPointerdown, start_time,
                       processing_start, processing_end, 4,
                       GetWindow()->document());

  base::TimeTicks presentation_time = processing_end + base::Milliseconds(10);
  SimulatePaintAndResolvePresentationPromise(presentation_time);

  base::TimeTicks start_time2 = start_time + base::Milliseconds(100);
  base::TimeTicks processing_start2 = start_time2 + base::Milliseconds(5);
  base::TimeTicks processing_end2 = processing_start2 + base::Milliseconds(10);
  RegisterPointerEvent(event_type_names::kPointerup, start_time2,
                       processing_start2, processing_end2, 4,
                       GetWindow()->document());

  base::TimeTicks start_time3 = start_time2;
  base::TimeTicks processing_start3 = processing_end2;
  base::TimeTicks processing_end3 = processing_start3 + base::Milliseconds(10);
  RegisterPointerEvent(event_type_names::kClick, start_time3, processing_start3,
                       processing_end3, 4, GetWindow()->document());

  base::TimeTicks presentation_time2 = processing_end3 + base::Milliseconds(5);
  SimulatePaintAndResolvePresentationPromise(presentation_time2);

  // Only the longer event should have been reported.
  auto analyzer = trace_analyzer::Stop();
  analyzer->AssociateAsyncBeginEndEvents();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("EventTiming") &&
            Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(3u, events.size());
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ("devtools.timeline", events[i]->category);
  }

  // Items in the trace events list is ordered chronologically, that is -- trace
  // event with smaller timestamp comes earlier.
  //
  // --Timestamps--
  // pointerdown_begin: 1000ms (pointerdown end: 1025ms)
  const trace_analyzer::TraceEvent* pointerdown_begin = events[0];
  // pointerup_begin: 1100ms (pointerup end: 1130ms)
  const trace_analyzer::TraceEvent* pointerup_begin = events[1];
  // click_begin: 1100ms (click end 1130ms)
  const trace_analyzer::TraceEvent* click_begin = events[2];

  // pointerdown
  ASSERT_TRUE(pointerdown_begin->HasDictArg("data"));
  base::Value::Dict arg_dict = pointerdown_begin->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("interactionId").value_or(-1), 0);
  std::string* event_name = arg_dict.FindString("type");
  ASSERT_TRUE(event_name);
  EXPECT_EQ(*event_name, "pointerdown");
  std::string* frame_trace_value = arg_dict.FindString("frame");
  EXPECT_EQ(String(*frame_trace_value), GetFrameIdForTracing(GetFrame()));
  EXPECT_EQ(arg_dict.FindInt("nodeId"),
            DOMNodeIds::IdForNode(GetWindow()->document()));
  ASSERT_TRUE(pointerdown_begin->has_other_event());
  EXPECT_EQ(base::ClampRound(pointerdown_begin->GetAbsTimeToOtherEvent()),
            25000);
  EXPECT_FALSE(pointerdown_begin->other_event->HasDictArg("data"));

  // pointerup
  ASSERT_TRUE(pointerup_begin->HasDictArg("data"));
  arg_dict = pointerup_begin->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("interactionId").value_or(-1), 0);
  event_name = arg_dict.FindString("type");
  ASSERT_TRUE(event_name);
  EXPECT_EQ(*event_name, "pointerup");
  frame_trace_value = arg_dict.FindString("frame");
  EXPECT_EQ(String(*frame_trace_value), GetFrameIdForTracing(GetFrame()));
  EXPECT_EQ(arg_dict.FindInt("nodeId"),
            DOMNodeIds::IdForNode(GetWindow()->document()));
  ASSERT_TRUE(pointerup_begin->has_other_event());
  EXPECT_EQ(base::ClampRound(pointerup_begin->GetAbsTimeToOtherEvent()), 30000);
  EXPECT_FALSE(pointerup_begin->other_event->HasDictArg("data"));

  // click
  ASSERT_TRUE(click_begin->HasDictArg("data"));
  arg_dict = click_begin->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("interactionId").value_or(-1), 0);
  event_name = arg_dict.FindString("type");
  ASSERT_TRUE(event_name);
  EXPECT_EQ(*event_name, "click");
  frame_trace_value = arg_dict.FindString("frame");
  EXPECT_EQ(String(*frame_trace_value), GetFrameIdForTracing(GetFrame()));
  EXPECT_EQ(arg_dict.FindInt("nodeId"),
            DOMNodeIds::IdForNode(GetWindow()->document()));
  ASSERT_TRUE(click_begin->has_other_event());
  EXPECT_EQ(base::ClampRound(click_begin->GetAbsTimeToOtherEvent()), 30000);
  EXPECT_FALSE(click_begin->other_event->HasDictArg("data"));
}

TEST_F(WindowPerformanceTest, SlowInteractionToNextPaintTraceEvents) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");

  constexpr int kKeyCode = 2;

  // Short, untraced keyboard event.
  {
    // Keydown.
    base::TimeTicks keydown_timestamp = GetTimeStamp(0);
    base::TimeTicks processing_start_keydown = GetTimeStamp(1);
    base::TimeTicks processing_end_keydown = GetTimeStamp(2);
    base::TimeTicks presentation_time_keydown = GetTimeStamp(20);
    RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                          processing_start_keydown, processing_end_keydown,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

    // Keyup.
    base::TimeTicks keyup_timestamp = GetTimeStamp(10);
    base::TimeTicks processing_start_keyup = GetTimeStamp(15);
    base::TimeTicks processing_end_keyup = GetTimeStamp(50);
    base::TimeTicks presentation_time_keyup = GetTimeStamp(110);
    RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                          processing_start_keyup, processing_end_keyup,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);
  }

  // Single long event.
  {
    // Keydown (quick).
    base::TimeTicks keydown_timestamp = GetTimeStamp(200);
    base::TimeTicks processing_start_keydown = GetTimeStamp(201);
    base::TimeTicks processing_end_keydown = GetTimeStamp(202);
    base::TimeTicks presentation_time_keydown = GetTimeStamp(220);
    RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                          processing_start_keydown, processing_end_keydown,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

    // Keyup (start = 210, dur = 101ms).
    base::TimeTicks keyup_timestamp = GetTimeStamp(210);
    base::TimeTicks processing_start_keyup = GetTimeStamp(215);
    base::TimeTicks processing_end_keyup = GetTimeStamp(250);
    base::TimeTicks presentation_time_keyup = GetTimeStamp(311);
    RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                          processing_start_keyup, processing_end_keyup,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);
  }

  // Overlapping events.
  {
    // Keydown (quick).
    base::TimeTicks keydown_timestamp = GetTimeStamp(1000);
    base::TimeTicks processing_start_keydown = GetTimeStamp(1001);
    base::TimeTicks processing_end_keydown = GetTimeStamp(1002);
    base::TimeTicks presentation_time_keydown = GetTimeStamp(1010);
    RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp,
                          processing_start_keydown, processing_end_keydown,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keydown);

    // Keyup (start = 1020, dur = 1000ms).
    base::TimeTicks keyup_timestamp = GetTimeStamp(1020);
    base::TimeTicks processing_start_keyup = GetTimeStamp(1030);
    base::TimeTicks processing_end_keyup = GetTimeStamp(1040);
    base::TimeTicks presentation_time_keyup = GetTimeStamp(2020);
    RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp,
                          processing_start_keyup, processing_end_keyup,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keyup);

    // Keydown (quick).
    base::TimeTicks keydown_timestamp2 = GetTimeStamp(1000);
    base::TimeTicks processing_start_keydown2 = GetTimeStamp(1001);
    base::TimeTicks processing_end_keydown2 = GetTimeStamp(1002);
    base::TimeTicks presentation_time_keydown2 = GetTimeStamp(1010);
    RegisterKeyboardEvent(event_type_names::kKeydown, keydown_timestamp2,
                          processing_start_keydown2, processing_end_keydown2,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keydown2);

    // Keyup (start = 1800, dur = 600ms).
    base::TimeTicks keyup_timestamp2 = GetTimeStamp(1800);
    base::TimeTicks processing_start_keyup2 = GetTimeStamp(1802);
    base::TimeTicks processing_end_keyup2 = GetTimeStamp(1810);
    base::TimeTicks presentation_time_keyup2 = GetTimeStamp(2400);
    RegisterKeyboardEvent(event_type_names::kKeyup, keyup_timestamp2,
                          processing_start_keyup2, processing_end_keyup2,
                          kKeyCode);
    SimulatePaintAndResolvePresentationPromise(presentation_time_keyup2);
  }

  auto analyzer = trace_analyzer::Stop();
  analyzer->AssociateAsyncBeginEndEvents();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("SlowInteractionToNextPaint") &&
            Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  analyzer->FindEvents(q, &events);

  ASSERT_EQ(3u, events.size());

  ASSERT_TRUE(events[0]->has_other_event());
  EXPECT_EQ(events[0]->category, "latency");
  EXPECT_EQ(base::ClampRound(events[0]->GetAbsTimeToOtherEvent()), 101000);

  ASSERT_TRUE(events[1]->has_other_event());
  EXPECT_EQ(events[1]->category, "latency");
  EXPECT_EQ(base::ClampRound(events[1]->GetAbsTimeToOtherEvent()), 1000000);

  ASSERT_TRUE(events[2]->has_other_event());
  EXPECT_EQ(events[2]->category, "latency");
  EXPECT_EQ(base::ClampRound(events[2]->GetAbsTimeToOtherEvent()), 600000);
}

TEST_F(WindowPerformanceTest, InteractionID) {
  // Keyboard with max duration 25, total duration 40.
  PerformanceEventTiming* keydown_entry =
      CreatePerformanceEventTiming(event_type_names::kKeydown, 1, std::nullopt,
                                   GetTimeStamp(100), GetTimeStamp(120));
  SimulateInteractionId(keydown_entry);
  PerformanceEventTiming* keyup_entry =
      CreatePerformanceEventTiming(event_type_names::kKeyup, 1, std::nullopt,
                                   GetTimeStamp(115), GetTimeStamp(140));
  SimulateInteractionId(keyup_entry);
  EXPECT_EQ(keydown_entry->interactionId(), keyup_entry->interactionId());
  EXPECT_GT(keydown_entry->interactionId(), 0u);

  // Tap or Click with max duration 70, total duration 90.
  PointerId pointer_id_1 = 10;
  PerformanceEventTiming* pointerdown_entry = CreatePerformanceEventTiming(
      event_type_names::kPointerdown, std::nullopt, pointer_id_1,
      GetTimeStamp(100), GetTimeStamp(120));
  SimulateInteractionId(pointerdown_entry);
  PerformanceEventTiming* pointerup_entry = CreatePerformanceEventTiming(
      event_type_names::kPointerup, std::nullopt, pointer_id_1,
      GetTimeStamp(130), GetTimeStamp(150));
  SimulateInteractionId(pointerup_entry);
  PerformanceEventTiming* click_entry = CreatePerformanceEventTiming(
      event_type_names::kClick, std::nullopt, pointer_id_1, GetTimeStamp(130),
      GetTimeStamp(200));
  SimulateInteractionId(click_entry);
  EXPECT_GT(pointerdown_entry->interactionId(), 0u);
  EXPECT_EQ(pointerdown_entry->interactionId(),
            pointerup_entry->interactionId());
  EXPECT_EQ(pointerup_entry->interactionId(), click_entry->interactionId());

  // Drag with max duration 50, total duration 80.
  PointerId pointer_id_2 = 20;
  pointerdown_entry = CreatePerformanceEventTiming(
      event_type_names::kPointerdown, std::nullopt, pointer_id_2,
      GetTimeStamp(150), GetTimeStamp(200));
  SimulateInteractionId(pointerdown_entry);
  performance_->NotifyPotentialDrag(20);
  pointerup_entry = CreatePerformanceEventTiming(
      event_type_names::kPointerup, std::nullopt, pointer_id_2,
      GetTimeStamp(200), GetTimeStamp(230));
  SimulateInteractionId(pointerup_entry);
  EXPECT_GT(pointerdown_entry->interactionId(), 0u);
  EXPECT_EQ(pointerdown_entry->interactionId(),
            pointerup_entry->interactionId());

  // Scroll should not be reported in ukm.
  pointerdown_entry = CreatePerformanceEventTiming(
      event_type_names::kPointerdown, std::nullopt, pointer_id_2,
      GetTimeStamp(300), GetTimeStamp(315));
  SimulateInteractionId(pointerdown_entry);
  PerformanceEventTiming* pointercancel_entry = CreatePerformanceEventTiming(
      event_type_names::kPointercancel, std::nullopt, pointer_id_2,
      GetTimeStamp(310), GetTimeStamp(330));
  SimulateInteractionId(pointercancel_entry);
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
        std::optional<int> key_code,
        std::optional<PointerId> pointer_id,
        base::TimeTicks event_timestamp = base::TimeTicks(),
        base::TimeTicks presentation_timestamp = base::TimeTicks())
        : name_(name),
          key_code_(key_code),
          pointer_id_(pointer_id),
          event_timestamp_(event_timestamp),
          presentation_timestamp_(presentation_timestamp) {}

    AtomicString name_;
    std::optional<int> key_code_;
    std::optional<PointerId> pointer_id_;
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
      PerformanceEventTiming* entry = CreatePerformanceEventTiming(
          event.name_, event.key_code_, event.pointer_id_,
          event.event_timestamp_, event.presentation_timestamp_);
      SimulateInteractionId(entry);
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
      {event_type_names::kKeydown, 65, std::nullopt, GetTimeStamp(100),
       GetTimeStamp(150)},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(120),
       GetTimeStamp(220)},
      {event_type_names::kKeyup, 65, std::nullopt, GetTimeStamp(130),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_GT(ids1[0], 0u) << "Keydown interactionId was nonzero";
  EXPECT_EQ(ids1[1], 0u) << "Input interactionId was zero";
  EXPECT_EQ(ids1[0], ids1[2]) << "Keydown and keyup interactionId match";

  // Insert "3" with a max duration of 40 and total of 60.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 53, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(220)},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(220),
       GetTimeStamp(320)},
      {event_type_names::kKeyup, 53, std::nullopt, GetTimeStamp(250),
       GetTimeStamp(290)}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_GT(ids2[0], 0u) << "Second keydown has nonzero interactionId";
  EXPECT_EQ(ids2[1], 0u) << "Second input interactionId was zero";
  EXPECT_EQ(ids2[0], ids2[2]) << "Second keydown and keyup interactionId match";
  EXPECT_NE(ids1[0], ids2[0])
      << "First and second keydown have different interactionId";

  // Backspace with max duration of 25 and total of 25.
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kKeydown, 8, std::nullopt, GetTimeStamp(300),
       GetTimeStamp(320)},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(300),
       GetTimeStamp(400)},
      {event_type_names::kKeyup, 8, std::nullopt, GetTimeStamp(300),
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
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(100),
       GetTimeStamp(200)},
      {event_type_names::kCompositionstart, std::nullopt, std::nullopt},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(120),
       GetTimeStamp(140)},
      {event_type_names::kKeyup, 65, std::nullopt, GetTimeStamp(120),
       GetTimeStamp(220)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);

  // Insert "b" and finish composition with a duration of 30.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(230),
       GetTimeStamp(260)},
      {event_type_names::kKeyup, 66, std::nullopt, GetTimeStamp(270),
       GetTimeStamp(370)},
      {event_type_names::kCompositionend, std::nullopt, std::nullopt}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);

  performance_->GetResponsivenessMetrics().FlushAllEventsForTesting();

  EXPECT_GT(ids1[0], 0u) << "Keydown interactionId was nonzero";
  EXPECT_EQ(ids1[1], 0u) << "Compositionstart interactionId was zero";
  EXPECT_GT(ids1[3], 0u) << "Input interactionId was nonzero";
  EXPECT_GT(ids1[4], 0u) << "Keyup interactionId was nonzero";
  EXPECT_EQ(ids1[0], ids1[3])
      << "Keydown and Input have the same interactionIds";

  EXPECT_GT(ids2[0], 0u) << "Second keydown interactionId was nonzero";
  EXPECT_GT(ids2[2], 0u) << "Second input interactionId was nonzero";
  EXPECT_GT(ids2[3], 0u) << "Second keyup interactionId was non zero";
  EXPECT_EQ(ids2[4], 0u) << "Compositionend interactionId was zero";
  EXPECT_EQ(ids2[0], ids2[2])
      << "Keydown and Input have the same interactionIds";
  EXPECT_NE(ids1[3], ids2[2])
      << "First and second inputs have different interactionIds";

  CheckUKMValues({{100, 120, UserInteractionType::kKeyboard},
                  {100, 170, UserInteractionType::kKeyboard}});
}

// Tests Chinese on Mac. Windows is similar, but has more keyups inside the
// composition.
TEST_F(InteractionIdTest, CompositionToFinalInput) {
  // Insert "a" with a duration of 25.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(100),
       GetTimeStamp(190)},
      {event_type_names::kCompositionstart, std::nullopt, std::nullopt},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(100),
       GetTimeStamp(125)},
      {event_type_names::kKeyup, 65, std::nullopt, GetTimeStamp(110),
       GetTimeStamp(190)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);
  EXPECT_GT(ids1[3], 0u) << "First input nonzero";

  // Insert "b" with a duration of 35.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(290)},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(220),
       GetTimeStamp(255)},
      {event_type_names::kKeyup, 66, std::nullopt, GetTimeStamp(210),
       GetTimeStamp(290)}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);
  EXPECT_GT(ids2[2], 0u) << "Second input nonzero";
  EXPECT_NE(ids1[3], ids2[2])
      << "First and second input have different interactionIds";

  // Select a composed input and finish, with a duration of 140.
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(300),
       GetTimeStamp(440)},
      {event_type_names::kCompositionend, std::nullopt, std::nullopt}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);
  EXPECT_EQ(ids3[2], 0u) << "Compositionend has zero interactionId";
  EXPECT_GT(ids3[1], 0u) << "Third input has nonzero interactionId";
  EXPECT_NE(ids1[3], ids3[1])
      << "First and third inputs have different interactionIds";
  EXPECT_NE(ids2[2], ids3[1])
      << "Second and third inputs have different interactionIds";

  performance_->GetResponsivenessMetrics().FlushAllEventsForTesting();

  CheckUKMValues({{90, 90, UserInteractionType::kKeyboard},
                  {90, 90, UserInteractionType::kKeyboard},
                  {140, 140, UserInteractionType::kKeyboard}});
}

// Tests Chinese on Windows.
TEST_F(InteractionIdTest, CompositionToFinalInputMultipleKeyUps) {
  // Insert "a" with a duration of 66.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(100)},
      {event_type_names::kCompositionstart, std::nullopt, std::nullopt},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(66)},
      {event_type_names::kKeyup, 229, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(100)},
      {event_type_names::kKeyup, 65, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(100)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);

  // Insert "b" with a duration of 51.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(251)},
      {event_type_names::kKeyup, 229, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kKeyup, 66, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);

  // Select a composed input and finish, with duration of 85.
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(300),
       GetTimeStamp(385)},
      {event_type_names::kCompositionend, std::nullopt, std::nullopt}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);

  performance_->GetResponsivenessMetrics().FlushAllEventsForTesting();
  EXPECT_GT(ids1[3], 0u) << "First input nonzero";
  EXPECT_GT(ids1[4], 0u) << "First keyup has nonzero interactionId";
  EXPECT_GT(ids1[5], 0u) << "Second keyup has nonzero interactionId";

  EXPECT_GT(ids2[2], 0u) << "Second input nonzero";
  EXPECT_NE(ids1[3], ids2[2])
      << "First and second input have different interactionIds";
  EXPECT_GT(ids2[3], 0u) << "Third keyup has nonzero interactionId";
  EXPECT_GT(ids2[4], 0u) << "Fourth keyup has nonzero interactionId";

  EXPECT_GT(ids3[1], 0u) << "Third input has nonzero interactionId";
  EXPECT_NE(ids1[3], ids3[1])
      << "First and third inputs have different interactionIds";
  EXPECT_NE(ids2[2], ids3[1])
      << "Second and third inputs have different interactionIds";
  CheckUKMValues({{100, 100, UserInteractionType::kKeyboard},
                  {100, 100, UserInteractionType::kKeyboard},
                  {85, 85, UserInteractionType::kKeyboard}});
}

// Tests Android smart suggestions (similar to Android Chinese).
TEST_F(InteractionIdTest, SmartSuggestion) {
  // Insert "A" with a duration of 9.
  std::vector<EventForInteraction> events1 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(16)},
      {event_type_names::kCompositionstart, std::nullopt, std::nullopt},
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(9)},
      {event_type_names::kKeyup, 229, std::nullopt, GetTimeStamp(0),
       GetTimeStamp(16)}};
  std::vector<uint32_t> ids1 = SimulateInteractionIds(events1);

  // Compose to "At" with a duration of 14.
  std::vector<EventForInteraction> events2 = {
      {event_type_names::kCompositionupdate, std::nullopt, std::nullopt},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(100),
       GetTimeStamp(114)},
      {event_type_names::kCompositionend, std::nullopt, std::nullopt}};
  std::vector<uint32_t> ids2 = SimulateInteractionIds(events2);

  // Add "the". No composition so need to consider the keydown and keyup.
  // Max duration of 43 and total duration of 70
  std::vector<EventForInteraction> events3 = {
      {event_type_names::kKeydown, 229, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(243)},
      {event_type_names::kInput, std::nullopt, std::nullopt, GetTimeStamp(200),
       GetTimeStamp(300)},
      {event_type_names::kKeyup, 229, std::nullopt, GetTimeStamp(235),
       GetTimeStamp(270)}};
  std::vector<uint32_t> ids3 = SimulateInteractionIds(events3);

  performance_->GetResponsivenessMetrics().FlushAllEventsForTesting();
  EXPECT_GT(ids1[3], 0u) << "First input nonzero";
  EXPECT_EQ(ids1[0], ids1[3]) << "Keydown and input have the same id";
  EXPECT_EQ(ids1[0], ids1[3]) << "Keydown and keyup have the same id";

  EXPECT_GT(ids2[1], 0u) << "Second input nonzero";
  EXPECT_NE(ids1[3], ids2[1])
      << "First and second input have different interactionIds";
  EXPECT_GT(ids3[0], 0u) << "Keydown nonzero";
  EXPECT_EQ(ids3[0], ids3[2]) << "Keydown and keyup have some id";
  EXPECT_EQ(ids3[1], 0u) << "Third input has zero id";

  CheckUKMValues({{16, 16, UserInteractionType::kKeyboard},
                  {14, 14, UserInteractionType::kKeyboard},
                  {43, 70, UserInteractionType::kKeyboard}});
}

TEST_F(InteractionIdTest, TapWithoutClick) {
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerdown, std::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(140)},
      {event_type_names::kPointerup, std::nullopt, 1, GetTimeStamp(120),
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
      {event_type_names::kPointerup, std::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(140)},
      {event_type_names::kClick, std::nullopt, 1, GetTimeStamp(120),
       GetTimeStamp(150)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_EQ(ids[0], 0u) << "Orphan pointerup gets interaction id of zero";
  EXPECT_GT(ids[1], 0u) << "Nonzero interaction id for click";
  // Flush UKM logging mojo request.
  RunPendingTasks();
  CheckUKMValues({{30, 30, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, JustClick) {
  // Hitting enter on a keyboard may cause just a trusted click event.
  std::vector<EventForInteraction> events = {
      {event_type_names::kClick, std::nullopt, 0, GetTimeStamp(120),
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
      {event_type_names::kPointerdown, std::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(140)},
      {event_type_names::kClick, std::nullopt, 1, GetTimeStamp(120),
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
      {event_type_names::kPointerdown, std::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(110)},
      {event_type_names::kPointerdown, std::nullopt, 2, GetTimeStamp(120),
       GetTimeStamp(140)},
      {event_type_names::kPointerup, std::nullopt, 2, GetTimeStamp(200),
       GetTimeStamp(230)},
      {event_type_names::kPointerup, std::nullopt, 1, GetTimeStamp(200),
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
  CheckUKMValues({{30, 50, UserInteractionType::kTapOrClick},
                  {50, 60, UserInteractionType::kTapOrClick}});
}

TEST_F(InteractionIdTest, ClickIncorrectPointerId) {
  // On mobile, in cases where touchstart is skipped, click does not get the
  // correct pointerId. See crbug.com/1264930 for more details.
  // TODO crbug.com/359679950: remove this test and event timing workaround
  // since crbug.com/1264930 has been fixed.
  std::vector<EventForInteraction> events = {
      {event_type_names::kPointerup, std::nullopt, 1, GetTimeStamp(100),
       GetTimeStamp(130)},
      {event_type_names::kClick, std::nullopt, 0, GetTimeStamp(120),
       GetTimeStamp(160)}};
  std::vector<uint32_t> ids = SimulateInteractionIds(events);
  EXPECT_EQ(ids[0], 0u) << "Orphan pointerup gets interaction id of zero";
  EXPECT_GT(ids[1], 0u) << "Nonzero interaction id for click";
  // Flush UKM logging mojo request.
  RunPendingTasks();
  CheckUKMValues({{40, 40, UserInteractionType::kTapOrClick}});
}

}  // namespace blink
