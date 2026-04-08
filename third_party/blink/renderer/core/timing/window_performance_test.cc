// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include <array>
#include <cstdint>
#include <type_traits>

#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "base/test/trace_test_utils.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_timing_details.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/events/composition_event.h"
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
#include "third_party/blink/renderer/core/timing/event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace blink {

using test::RunPendingTasks;

namespace {

MATCHER(IsInteraction, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  return arg->interactionId() > 0u;
}

MATCHER_P(SameInteractionAs, other, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<other_type>>();
  return arg->interactionId() > 0u && other->interactionId() > 0u &&
         arg->interactionId() == other->interactionId();
}

MATCHER_P(DifferentInteractionFrom, other, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<other_type>>();
  return arg->interactionId() > 0u && other->interactionId() > 0u &&
         arg->interactionId() != other->interactionId();
}

MATCHER(IsPainted, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  return arg->HasKnownEndTime() &&
         !arg->GetEventTimingReportingInfo()->presentation_time.is_null() &&
         arg->GetEndTime() ==
             arg->GetEventTimingReportingInfo()->presentation_time;
}

MATCHER(IsFallbackPainted, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  bool ok =
      arg->HasKnownEndTime() &&
      !arg->GetEventTimingReportingInfo()->fallback_time.is_null() &&
      arg->GetEndTime() == arg->GetEventTimingReportingInfo()->fallback_time;
  if (!arg->GetEventTimingReportingInfo()->presentation_time.is_null()) {
    ok = ok && (arg->GetEndTime() <
                arg->GetEventTimingReportingInfo()->presentation_time);
  }
  return ok;
}

MATCHER_P(SamePaintAs, other, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<other_type>>();
  return arg->HasKnownEndTime() &&
         !arg->GetEventTimingReportingInfo()->presentation_time.is_null() &&
         arg->GetEndTime() ==
             arg->GetEventTimingReportingInfo()->presentation_time &&
         other->HasKnownEndTime() &&
         !other->GetEventTimingReportingInfo()->presentation_time.is_null() &&
         other->GetEndTime() ==
             other->GetEventTimingReportingInfo()->presentation_time &&
         arg->GetEndTime() == other->GetEndTime();
}

MATCHER_P(DifferentPaintFrom, other, "") {
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<arg_type>>();
  ::testing::StaticAssertTypeEq<PerformanceEventTiming*,
                                std::decay_t<other_type>>();
  return arg->HasKnownEndTime() &&
         !arg->GetEventTimingReportingInfo()->presentation_time.is_null() &&
         arg->GetEndTime() ==
             arg->GetEventTimingReportingInfo()->presentation_time &&
         other->HasKnownEndTime() &&
         !other->GetEventTimingReportingInfo()->presentation_time.is_null() &&
         other->GetEndTime() ==
             other->GetEventTimingReportingInfo()->presentation_time &&
         arg->GetEndTime() != other->GetEndTime();
}

}  // namespace

class WindowPerformanceTest : public testing::Test,
                              public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> features{
        blink::features::
            kEventTimingIgnorePresentationTimeFromUnexpectedFrameSource,
        blink::kEventTimingReportingInStrictOrderOnly};
    if (GetParam()) {
      features_.InitWithFeatures(features, {});
    } else {
      features_.InitWithFeatures({}, features);
    }

    // We don't want the perf timeline time origin to start at 0.
    FastForwardBy(base::Seconds(500));
    origin_ = base::TimeTicks::Now();
    ResetPerformance();
  }

  base::TimeTicks GetTimeOrigin() { return origin_; }

  void FastForwardBy(base::TimeDelta delta) {
    CHECK_GE(delta, base::TimeDelta());
    task_environment_.FastForwardBy(delta);
  }

  void AddLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ |= PerformanceEntry::kLongTask;
  }

  void RemoveLongTaskObserver() {
    // simulate with filter options.
    performance_->observer_filter_options_ = PerformanceEntry::kInvalid;
  }
  void SimulatePaintAndCommit() {
    performance_->OnPaintFinished();
    performance_->SetCommitFinishTimeStampForPendingEvents(
        base::TimeTicks::Now());
  }
  void SimulateJustPresentationTime(uint64_t frame_index,
                                    uint64_t expected_frame_source_id = 1,
                                    uint64_t actual_frame_source_id = 1) {
    viz::FrameTimingDetails presentation_details;
    presentation_details.frame_id.source_id = actual_frame_source_id;
    presentation_details.presentation_feedback.timestamp =
        base::TimeTicks::Now();
    performance_->OnPresentationPromiseResolved(
        frame_index, expected_frame_source_id, presentation_details);
  }

  void SimulateAllRenderingStages() {
    uint64_t frame_index = GetCurrentFrameIndex();
    SimulatePaintAndCommit();
    SimulateJustPresentationTime(frame_index);
  }

  KeyboardEvent* CreateKeyboardEvent(AtomicString type,
                                     base::TimeTicks start_time,
                                     ui::DomCode code,
                                     EventTarget* target = nullptr) {
    WebInputEvent::Type web_type;
    if (type == event_type_names::kKeyup) {
      web_type = WebInputEvent::Type::kKeyUp;
    } else if (type == event_type_names::kKeypress) {
      web_type = WebInputEvent::Type::kChar;
    } else {
      web_type = WebInputEvent::Type::kRawKeyDown;
    }
    WebKeyboardEvent web_event(web_type, WebInputEvent::kNoModifiers,
                               start_time);
    web_event.dom_code = static_cast<int>(code);
    web_event.windows_key_code =
        static_cast<int>(ui::DomCodeToUsLayoutKeyboardCode(code));
    KeyboardEvent* event = KeyboardEvent::Create(web_event, GetWindow());
    event->SetTrusted(true);
    if (target) {
      event->SetTarget(target);
    }
    return event;
  }

  PointerEvent* CreatePointerEvent(AtomicString type,
                                   base::TimeTicks start_time,
                                   PointerId pointer_id,
                                   EventTarget* target = nullptr) {
    PointerEventInit* init = PointerEventInit::Create();
    init->setPointerId(pointer_id);
    PointerEvent* event = PointerEvent::Create(type, init, start_time);
    event->SetTrusted(true);
    if (target) {
      event->SetTarget(target);
    }
    return event;
  }

  Event* CreateInputEvent(EventTarget* target = nullptr) {
    Event* event = InputEvent::CreateInput(InputEvent::InputType::kNone, "",
                                           InputEvent::kNotComposing, nullptr);
    event->SetTrusted(true);
    if (target) {
      event->SetTarget(target);
    }
    return event;
  }

  CompositionEvent* CreateCompositionEvent(AtomicString type,
                                           EventTarget* target = nullptr) {
    CompositionEvent* event =
        MakeGarbageCollected<CompositionEvent>(type, nullptr, "");
    event->SetTrusted(true);
    if (target) {
      event->SetTarget(target);
    }
    return event;
  }

  PerformanceEventTiming* SimulateEventDispatch(
      const Event& event,
      base::TimeDelta processing_duration) {
    performance_->GetResponsivenessMetrics()
        .SetCurrentInteractionEventQueuedTimestamp(event.PlatformTimeStamp());

    UIEventTiming ui_event_timing(GetFrame(), event);
    PerformanceEventTiming* entry = ui_event_timing.GetEntry();

    FastForwardBy(processing_duration);

    return entry;
  }

  HeapVector<Member<PerformanceEventTiming>>*
  GetEventTimingEntriesStillWaitingForReporting() {
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

  uint64_t GetCurrentFrameIndex() const {
    return performance_->current_frame_index_;
  }

  void ExpectUMACounts(int keyboard_count, int pointer_count) {
    GetHistogramTester().ExpectTotalCount(
        "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes",
        keyboard_count + pointer_count);
    GetHistogramTester().ExpectTotalCount(
        "Blink.Responsiveness.UserInteraction.MaxEventDuration.Keyboard",
        keyboard_count);
    GetHistogramTester().ExpectTotalCount(
        "Blink.Responsiveness.UserInteraction.MaxEventDuration.TapOrClick",
        pointer_count);
  }

  void CheckEntriesAreReportedToRendererUKM(
      const HeapVector<Member<PerformanceEventTiming>>& entries_list) {
    RunPendingTasks();

    auto entries = GetUkmRecorder()->GetEntriesByName(
        ukm::builders::Responsiveness_UserInteraction::kEntryName);

    std::vector<std::pair<int, UserInteractionType>> expected_ukm_pairs;
    for (const auto& entry : entries_list) {
      if (entry->IsKnownToBeAnInteraction()) {
        int duration =
            static_cast<int>(entry->GetExactDuration().InMilliseconds());
        expected_ukm_pairs.emplace_back(duration, entry->InteractionType());
      }
    }

    EXPECT_EQ(expected_ukm_pairs.size(), entries.size());

    std::vector<std::pair<int, UserInteractionType>> actual_ukm_pairs;
    for (const auto& ukm_entry : entries) {
      actual_ukm_pairs.emplace_back(
          static_cast<int>(*GetUkmRecorder()->GetEntryMetric(
              ukm_entry, ukm::builders::Responsiveness_UserInteraction::
                             kMaxEventDurationName)),
          static_cast<UserInteractionType>(*GetUkmRecorder()->GetEntryMetric(
              ukm_entry, ukm::builders::Responsiveness_UserInteraction::
                             kInteractionTypeName)));
    }

    EXPECT_THAT(actual_ukm_pairs,
                testing::UnorderedElementsAreArray(expected_ukm_pairs));
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TracingEnvironment tracing_environment_;
  Persistent<WindowPerformance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  ScopedFakeUkmRecorder scoped_fake_ukm_recorder_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList features_;
  base::TimeTicks origin_;
};

TEST_P(WindowPerformanceTest, SanitizedLongTaskName) {
  // Unable to attribute, when no execution contents are available.
  EXPECT_EQ("unknown", SanitizedAttribution(nullptr, false, GetFrame()));

  // Attribute for same context (and same origin).
  EXPECT_EQ("self", SanitizedAttribution(GetWindow(), false, GetFrame()));

  // Unable to attribute, when multiple script execution contents are involved.
  EXPECT_EQ("multiple-contexts",
            SanitizedAttribution(GetWindow(), true, GetFrame()));
}

TEST_P(WindowPerformanceTest, SanitizedLongTaskName_CrossOrigin) {
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
TEST_P(WindowPerformanceTest, NavigateAway) {
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
  uint64_t navigation_id = perf->NavigationId();

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
  EXPECT_EQ(navigation_id, perf->NavigationId());
  EXPECT_EQ(timing, perf->timing());
  EXPECT_EQ(page_holder->GetFrame().DomWindow(), perf->DomWindow());
  EXPECT_EQ(page_holder->GetFrame().DomWindow(), timing->DomWindow());
  EXPECT_LE(navigation_start, timing->navigationStart());
}

// Make sure the output entries with the same timestamps follow the insertion
// order. (http://crbug.com/767560)
TEST_P(WindowPerformanceTest, EnsureEntryListOrder) {
  // Need to have an active V8 context for ScriptValues to operate.
  v8::HandleScope handle_scope(GetScriptState()->GetIsolate());
  v8::Local<v8::Context> context = GetScriptState()->GetContext();
  v8::Context::Scope context_scope(context);

  DummyExceptionStateForTesting exception_state;
  FastForwardBy(base::Seconds(2));
  for (int i = 0; i < 8; i++) {
    performance_->mark(GetScriptState(), AtomicString::Number(i), nullptr,
                       exception_state);
  }
  FastForwardBy(base::Seconds(2));
  for (int i = 8; i < 17; i++) {
    performance_->mark(GetScriptState(), AtomicString::Number(i), nullptr,
                       exception_state);
  }

  PerformanceEntryVector entries =
      performance_->getEntriesByType(performance_entry_names::kMark);
  EXPECT_EQ(17U, entries.size());
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(AtomicString::Number(i), entries[i]->name());
    EXPECT_EQ(2000, entries[i]->startTime());
  }
  for (int i = 8; i < 17; i++) {
    EXPECT_EQ(AtomicString::Number(i), entries[i]->name());
    EXPECT_EQ(4000, entries[i]->startTime());
  }
}

TEST_P(WindowPerformanceTest, EventTimingEntryBuffering) {
  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  FastForwardBy(base::Seconds(3));

  auto* event1 = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Seconds(2.2), 4);
  SimulateEventDispatch(*event1, base::Seconds(0.5));
  SimulateAllRenderingStages();
  EXPECT_EQ(1u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  page_holder_->GetFrame()
      .Loader()
      .GetDocumentLoader()
      ->GetTiming()
      .MarkLoadEventStart();
  auto* event2 = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Seconds(2.2), 4);
  SimulateEventDispatch(*event2, base::Seconds(0.5));
  SimulateAllRenderingStages();
  EXPECT_EQ(2u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  GetFrame()->DetachDocument();
  EXPECT_FALSE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  auto* event3 = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Seconds(2.2), 4);
  SimulateEventDispatch(*event3, base::Seconds(0.5));
  SimulateAllRenderingStages();
  EXPECT_EQ(3u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());
}

TEST_P(WindowPerformanceTest, Expose100MsEvents) {
  FastForwardBy(base::Seconds(3));

  // Simulate two events, both with 0 processing duration and 99ms
  // presentation delay. The first event has more input delay, such that the
  // first duration is 100.5ms and the second is 99.5ms.
  auto* event1 =
      CreatePointerEvent(event_type_names::kMousedown,
                         base::TimeTicks::Now() - base::Microseconds(1500), 4);
  SimulateEventDispatch(*event1, base::Milliseconds(0));
  auto* event2 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Microseconds(500), 4);
  SimulateEventDispatch(*event2, base::Milliseconds(0));
  FastForwardBy(base::Milliseconds(99));
  SimulateAllRenderingStages();

  // Only the longer event should have been reported, because we round duration
  // to nearest 8ms, and filter out those below 104ms.
  const auto& entries =
      performance_->getBufferedEntriesByType(performance_entry_names::kEvent);
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(event_type_names::kMousedown, entries.at(0)->name());
}

TEST_P(WindowPerformanceTest, EventTimingDuration) {
  FastForwardBy(base::Seconds(3));

  // First event less than 104ms duration. Expect not measured.
  auto* event1 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(0u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  // Second event greater than 104ms duration. Expect measured.
  auto* event2 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event2, base::Milliseconds(1));
  FastForwardBy(base::Seconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(1u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  // Third event less than 104ms duration. Expect not measured.
  auto* event3 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event3, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(1u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());

  // Fourth event greater than 104ms duration. Expect measured.
  auto* event4 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event4, base::Milliseconds(1));
  FastForwardBy(base::Seconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(2u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());
}

// Test the case where multiple events are registered and then their
// presentation promise is resolved.
TEST_P(WindowPerformanceTest, MultipleEventsThenPresent) {
  FastForwardBy(base::Seconds(3));

  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    auto* event =
        CreatePointerEvent(event_type_names::kClick,
                           base::TimeTicks::Now() - base::Milliseconds(100), 4);
    SimulateEventDispatch(*event, base::Milliseconds(100));
  }
  EXPECT_EQ(0u, performance_
                    ->getBufferedEntriesByType(performance_entry_names::kEvent)
                    .size());
  SimulateAllRenderingStages();
  EXPECT_EQ(
      num_events,
      performance_->getBufferedEntriesByType(performance_entry_names::kEvent)
          .size());
}

// Test the case where commit finish timestamps are recorded on all pending
// EventTimings.
TEST_P(WindowPerformanceTest,
       CommitFinishTimeRecordedOnAllPendingEventTimings) {
  FastForwardBy(base::Seconds(3));

  size_t num_events = 3;
  for (size_t i = 0; i < num_events; ++i) {
    auto* event =
        CreatePointerEvent(event_type_names::kClick,
                           base::TimeTicks::Now() - base::Milliseconds(100), 4);
    SimulateEventDispatch(*event, base::Milliseconds(100));
  }

  auto* event_timing_entries = GetEventTimingEntriesStillWaitingForReporting();
  EXPECT_EQ(event_timing_entries->size(), 3u);
  for (const auto event_data : *event_timing_entries) {
    EXPECT_TRUE(event_data->GetEventTimingReportingInfo()
                    ->commit_finish_time.is_null());
    EXPECT_FALSE(event_data->GetEventTimingReportingInfo()
                     ->processing_end_time.is_null());
    EXPECT_TRUE(
        event_data->GetEventTimingReportingInfo()->fallback_time.is_null());
  }
  base::TimeTicks commit_finish_time = base::TimeTicks::Now();
  performance_->SetCommitFinishTimeStampForPendingEvents(commit_finish_time);
  for (const auto event_data : *event_timing_entries) {
    EXPECT_FALSE(event_data->GetEventTimingReportingInfo()
                     ->commit_finish_time.is_null());
    EXPECT_EQ(event_data->GetEventTimingReportingInfo()->commit_finish_time,
              commit_finish_time);
  }
}

// Test the case where a new commit finish timestamps does not affect previous
// EventTiming who has already seen a commit finish.
TEST_P(WindowPerformanceTest, NewCommitNotOverwritePreviousEventTimings) {
  FastForwardBy(base::Seconds(3));

  auto* event =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(100), 4);
  SimulateEventDispatch(*event, base::Milliseconds(100));
  base::TimeTicks commit_finish_time_1 = base::TimeTicks::Now();
  performance_->OnPaintFinished();
  performance_->SetCommitFinishTimeStampForPendingEvents(commit_finish_time_1);
  auto* event_timing_entries = GetEventTimingEntriesStillWaitingForReporting();
  EXPECT_EQ(event_timing_entries->size(), 1u);
  EXPECT_EQ(event_timing_entries->at(0)
                ->GetEventTimingReportingInfo()
                ->commit_finish_time,
            commit_finish_time_1);

  // Set a new commit finish timestamp.
  base::TimeTicks commit_finish_time_2 =
      commit_finish_time_1 + base::Seconds(1);
  performance_->SetCommitFinishTimeStampForPendingEvents(commit_finish_time_2);
  EXPECT_EQ(event_timing_entries->at(0)
                ->GetEventTimingReportingInfo()
                ->commit_finish_time,
            commit_finish_time_1);
  EXPECT_NE(event_timing_entries->at(0)
                ->GetEventTimingReportingInfo()
                ->commit_finish_time,
            commit_finish_time_2);
}

// Test for existence of 'first-input' given different types of first events.
TEST_P(WindowPerformanceTest, FirstInput_Keydown) {
  auto* event = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), ui::DomCode::US_A);
  SimulateEventDispatch(*event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  PerformanceEntryVector firstInputs =
      performance_->getEntriesByType(performance_entry_names::kFirstInput);
  EXPECT_EQ(1u, firstInputs.size());
  EXPECT_EQ(event_type_names::kKeydown, firstInputs[0]->name());
}

TEST_P(WindowPerformanceTest, FirstInput_PointerdownPointerup) {
  auto* event1_pointerdown =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_pointerdown, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());

  auto* event2_pointerup =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(4), 4);
  SimulateEventDispatch(*event2_pointerup, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  PerformanceEntryVector firstInputs =
      performance_->getEntriesByType(performance_entry_names::kFirstInput);
  EXPECT_EQ(1u, firstInputs.size());
  EXPECT_EQ(event_type_names::kPointerdown, firstInputs[0]->name());
}

TEST_P(WindowPerformanceTest, FirstInput_Click) {
  auto* event1_click =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_click, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  PerformanceEntryVector firstInputs =
      performance_->getEntriesByType(performance_entry_names::kFirstInput);
  EXPECT_EQ(1u, firstInputs.size());
  EXPECT_EQ(event_type_names::kClick, firstInputs[0]->name());
}

TEST_P(WindowPerformanceTest,
       FirstInput_PointercancelThenPointerdownPointerup) {
  // 1. Pointerdown
  auto* event1_pointerdown =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_pointerdown, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  // 2. Pointercancel
  auto* event2_pointercancel =
      CreatePointerEvent(event_type_names::kPointercancel,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event2_pointercancel, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());

  // 3. New Pointerdown
  // Save position to assert startTime accurate.
  base::TimeTicks expected_start_time =
      base::TimeTicks::Now() - base::Milliseconds(1);
  auto* event3_pointerdown =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event3_pointerdown, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  // 4. Pointerup
  auto* event4_pointerup =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event4_pointerup, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  PerformanceEntryVector firstInputs =
      performance_->getEntriesByType(performance_entry_names::kFirstInput);
  EXPECT_EQ(1u, firstInputs.size());
  // The first valid interaction is the second pointerdown.
  EXPECT_EQ(event_type_names::kPointerdown, firstInputs[0]->name());
  EXPECT_EQ(
      performance_->MonotonicTimeToDOMHighResTimeStamp(expected_start_time),
      firstInputs[0]->startTime());
}

// Test whether we can detect that the event is fully nested in another event
// during the processing time.
TEST_P(WindowPerformanceTest, NestedEventInProcessingTime) {
  auto* event = CreateKeyboardEvent(event_type_names::kKeydown,
                                    base::TimeTicks::Now(), ui::DomCode::US_A);
  SimulateEventDispatch(*event, base::Milliseconds(120));

  KeyboardEvent* keypress_event = CreateKeyboardEvent(
      event_type_names::kKeypress, base::TimeTicks::Now(), ui::DomCode::US_A);
  Event* input_event = CreateInputEvent();

  PerformanceEventTiming* keypress_entry = nullptr;
  PerformanceEventTiming* input_entry = nullptr;
  {
    UIEventTiming ui_event_timing_keypress(GetFrame(), *keypress_event);
    FastForwardBy(base::Milliseconds(120));
    keypress_entry = ui_event_timing_keypress.GetEntry();

    UIEventTiming ui_event_timing_input(GetFrame(), *input_event);
    FastForwardBy(base::Milliseconds(120));
    input_entry = ui_event_timing_input.GetEntry();
  }
  EXPECT_FALSE(keypress_entry->GetEventTimingReportingInfo()
                   ->is_processing_fully_nested_in_another_event);
  EXPECT_TRUE(input_entry->GetEventTimingReportingInfo()
                  ->is_processing_fully_nested_in_another_event);

  SimulateAllRenderingStages();

  const auto& entries =
      performance_->getBufferedEntriesByType(performance_entry_names::kEvent);
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(event_type_names::kKeydown, entries.at(0)->name());
  EXPECT_EQ(event_type_names::kKeypress, entries.at(1)->name());
  EXPECT_EQ(event_type_names::kInput, entries.at(2)->name());
}

// Test that the 'first-input' is populated after some irrelevant events are
// ignored.
TEST_P(WindowPerformanceTest, FirstInputAfterIgnored) {
  auto* event1_mouseover =
      CreatePointerEvent(event_type_names::kMouseover,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_mouseover, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());

  auto* event2_pointerdown =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event2_pointerdown, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());

  auto* event3_pointerup =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event3_pointerup, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  ASSERT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  EXPECT_EQ(
      event_type_names::kPointerdown,
      performance_->getEntriesByType(performance_entry_names::kFirstInput)[0]
          ->name());
}

// Test that pointerdown followed by pointerup works as a 'firstInput'.
TEST_P(WindowPerformanceTest, FirstPointerUp) {
  auto* event1_pointerdown =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_pointerdown, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());

  auto* event2_pointerup =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event2_pointerup, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  // The name of the entry should be event_type_names::kPointerdown.
  EXPECT_EQ(1u, performance_
                    ->getEntriesByName(event_type_names::kPointerdown,
                                       performance_entry_names::kFirstInput)
                    .size());
}

// When the pointerdown is optimized out, the click works as a
// 'first-input'.
TEST_P(WindowPerformanceTest, PointerdownOptimizedOut) {
  auto* event1_click =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_click, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  // The name of the entry should be event_type_names::kClick.
  EXPECT_EQ(1u, performance_
                    ->getEntriesByName(event_type_names::kClick,
                                       performance_entry_names::kFirstInput)
                    .size());
}

// Test that pointerdown followed by pointerup works as a 'first-input'.
TEST_P(WindowPerformanceTest, PointerdownOnDesktop) {
  auto* event1_pointerdown =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event1_pointerdown, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(0u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());

  auto* event2_pointerup =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*event2_pointerup, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();
  EXPECT_EQ(1u,
            performance_->getEntriesByType(performance_entry_names::kFirstInput)
                .size());
  // The name of the entry should be event_type_names::kPointerdown.
  EXPECT_EQ(1u, performance_
                    ->getEntriesByName(event_type_names::kPointerdown,
                                       performance_entry_names::kFirstInput)
                    .size());
}

TEST_P(WindowPerformanceTest, OneKeyboardInteraction) {
  ui::DomCode key_code = ui::DomCode::US_A;
  // Keydown
  auto* keydown_event = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  PerformanceEventTiming* keydown_entry =
      SimulateEventDispatch(*keydown_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // Keyup
  auto* keyup_event = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(2),
      key_code);
  PerformanceEventTiming* keyup_entry =
      SimulateEventDispatch(*keyup_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();

  CheckEntriesAreReportedToRendererUKM({keydown_entry, keyup_entry});
  ExpectUMACounts(2, 0);
}

TEST_P(WindowPerformanceTest, HoldingDownAKey) {
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  ui::DomCode key_code = ui::DomCode::US_A;

  // First Keydown
  auto* keydown_event1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  PerformanceEventTiming* keydown_entry1 =
      SimulateEventDispatch(*keydown_event1, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();
  EXPECT_THAT(keydown_entry1, IsPainted());
  CheckEntriesAreReportedToRendererUKM({keydown_entry1});

  // Second Keydown
  auto* keydown_event2 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  PerformanceEventTiming* keydown_entry2 =
      SimulateEventDispatch(*keydown_event2, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();
  EXPECT_THAT(keydown_entry2, IsPainted());
  EXPECT_THAT(keydown_entry1, DifferentPaintFrom(keydown_entry2));
  CheckEntriesAreReportedToRendererUKM({keydown_entry1, keydown_entry2});

  // Third Keydown
  auto* keydown_event3 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  PerformanceEventTiming* keydown_entry3 =
      SimulateEventDispatch(*keydown_event3, base::Milliseconds(2));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();
  EXPECT_THAT(keydown_entry3, IsPainted());
  EXPECT_THAT(keydown_entry2, DifferentPaintFrom(keydown_entry3));
  CheckEntriesAreReportedToRendererUKM(
      {keydown_entry1, keydown_entry2, keydown_entry3});

  // Keyup
  auto* keyup_event = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(2),
      key_code);
  PerformanceEventTiming* keyup_entry =
      SimulateEventDispatch(*keyup_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(7));
  SimulateAllRenderingStages();
  EXPECT_THAT(keyup_entry, IsPainted());
  EXPECT_THAT(keydown_entry3, DifferentPaintFrom(keyup_entry));

  CheckEntriesAreReportedToRendererUKM(
      {keydown_entry1, keydown_entry2, keydown_entry3, keyup_entry});
  ExpectUMACounts(4, 0);
}

TEST_P(WindowPerformanceTest, PressMultipleKeys) {
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  ui::DomCode first_key_code = ui::DomCode::US_A;
  ui::DomCode second_key_code = ui::DomCode::US_B;

  // Press the first key.
  auto* keydown_event1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), first_key_code);
  PerformanceEventTiming* keydown_entry1 =
      SimulateEventDispatch(*keydown_event1, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();
  EXPECT_THAT(keydown_entry1, IsPainted());
  CheckEntriesAreReportedToRendererUKM({keydown_entry1});

  // Press the second key.
  auto* keydown_event2 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(2), second_key_code);
  PerformanceEventTiming* keydown_entry2 =
      SimulateEventDispatch(*keydown_event2, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();
  EXPECT_THAT(keydown_entry2, IsPainted());
  EXPECT_THAT(keydown_entry1, DifferentPaintFrom(keydown_entry2));
  CheckEntriesAreReportedToRendererUKM({keydown_entry1, keydown_entry2});

  // Release the first key.
  auto* keyup_event1 = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(2),
      first_key_code);
  PerformanceEventTiming* keyup_entry1 =
      SimulateEventDispatch(*keyup_event1, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(7));
  SimulateAllRenderingStages();
  EXPECT_THAT(keyup_entry1, IsPainted());
  EXPECT_THAT(keydown_entry2, DifferentPaintFrom(keyup_entry1));
  CheckEntriesAreReportedToRendererUKM(
      {keydown_entry1, keydown_entry2, keyup_entry1});

  // Release the second key.
  auto* keyup_event2 = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(0),
      second_key_code);
  PerformanceEventTiming* keyup_entry2 =
      SimulateEventDispatch(*keyup_event2, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(14));
  SimulateAllRenderingStages();
  EXPECT_THAT(keyup_entry2, IsPainted());
  EXPECT_THAT(keyup_entry1, DifferentPaintFrom(keyup_entry2));

  CheckEntriesAreReportedToRendererUKM(
      {keydown_entry1, keydown_entry2, keyup_entry1, keyup_entry2});
  ExpectUMACounts(4, 0);
}

// Test a real world scenario, where keydown got presented first but its
// callback got invoked later than keyup's due to multi processes & threading
// overhead.
TEST_P(WindowPerformanceTest, KeyupFinishLastButCallbackInvokedFirst) {
  // Arbitrary keycode picked for testing from
  // https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/keyCode#value_of_keycode
  ui::DomCode digit_1_key_code = ui::DomCode::DIGIT1;

  // Keydown
  auto* keydown_event = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), digit_1_key_code);
  PerformanceEventTiming* keydown_entry =
      SimulateEventDispatch(*keydown_event, base::Milliseconds(4));
  const uint64_t presentation_index_keydown = GetCurrentFrameIndex();
  SimulatePaintAndCommit();

  // Keyup
  auto* keyup_event = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(3),
      digit_1_key_code);
  PerformanceEventTiming* keyup_entry =
      SimulateEventDispatch(*keyup_event, base::Milliseconds(1));
  const uint64_t presentation_index_keyup = GetCurrentFrameIndex();

  // keyup resolved without a paint, due to no damage.
  FastForwardBy(base::Milliseconds(1));
  SimulateJustPresentationTime(presentation_index_keyup);
  SimulateJustPresentationTime(presentation_index_keydown);

  CheckEntriesAreReportedToRendererUKM({keydown_entry, keyup_entry});
  ExpectUMACounts(2, 0);
}

TEST_P(WindowPerformanceTest, TapOrClick) {
  PointerId pointer_id = 4;

  // Pointerdown
  auto* pointerdown_event = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerdown_entry =
      SimulateEventDispatch(*pointerdown_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // Pointerup
  auto* pointerup_event = CreatePointerEvent(
      event_type_names::kPointerup,
      base::TimeTicks::Now() - base::Milliseconds(2), pointer_id);
  PerformanceEventTiming* pointerup_entry =
      SimulateEventDispatch(*pointerup_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();

  // Click
  auto* click_event = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Milliseconds(2),
      pointer_id);
  PerformanceEventTiming* click_entry =
      SimulateEventDispatch(*click_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();

  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry, pointerup_entry, click_entry});
  ExpectUMACounts(0, 3);
}

TEST_P(WindowPerformanceTest, PageVisibilityChanged) {
  PointerId pointer_id = 4;

  // Pointerdown
  auto* pointerdown_event = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerdown_entry =
      SimulateEventDispatch(*pointerdown_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // Pointerup
  auto* pointerup_event = CreatePointerEvent(
      event_type_names::kPointerup,
      base::TimeTicks::Now() - base::Milliseconds(2), pointer_id);
  PerformanceEventTiming* pointerup_entry =
      SimulateEventDispatch(*pointerup_event, base::Milliseconds(1));

  // Click
  auto* click_event = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Milliseconds(2),
      pointer_id);
  PerformanceEventTiming* click_entry =
      SimulateEventDispatch(*click_event, base::Milliseconds(1));

  // click_timestamp = 13s, proc_start = 15s, proc_end = 16s.
  // PageVisibilityChanged = 18s. So +2ms from current Now() (which is
  // proc_end).
  FastForwardBy(base::Milliseconds(2));
  base::TimeTicks visibility_change_time = base::TimeTicks::Now();
  performance_->GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden, true);
  PageVisibilityChanged(visibility_change_time);

  FastForwardBy(base::Milliseconds(2));
  SimulateAllRenderingStages();

  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry, pointerup_entry, click_entry});

  EXPECT_EQ(1ul, performance_->interactionCount());
}

TEST_P(WindowPerformanceTest, GPUCrashedAndFrameSourceIdChanged) {
  // This test only pass with the experiment feature
  base::test::ScopedFeatureList features_;
  features_.InitAndEnableFeature(
      blink::features::
          kEventTimingIgnorePresentationTimeFromUnexpectedFrameSource);

  PointerId pointer_id = 4;

  // Pointerdown
  auto* pointerdown_event = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(4), pointer_id);
  auto* pointerdown_entry =
      SimulateEventDispatch(*pointerdown_event, base::Milliseconds(1));

  // Simulate the correct frame source presentation time
  // Arbitrary valid id picked for testing
  uint64_t expected_frame_source_id = 4294967300;
  uint64_t actual_frame_source_id = expected_frame_source_id;
  uint64_t frame_index_1 = GetCurrentFrameIndex();
  SimulatePaintAndCommit();
  SimulateJustPresentationTime(frame_index_1, expected_frame_source_id,
                               actual_frame_source_id);

  // Expect that we measure all the way to presentation time, which is Now().
  EXPECT_THAT(pointerdown_entry, IsPainted());
  EXPECT_EQ(pointerdown_entry->GetEventTimingReportingInfo()->presentation_time,
            base::TimeTicks::Now());

  // Pointerup and Click
  auto* pointerup_event = CreatePointerEvent(
      event_type_names::kPointerup,
      base::TimeTicks::Now() - base::Milliseconds(14), pointer_id);
  auto* pointerup_entry =
      SimulateEventDispatch(*pointerup_event, base::Milliseconds(1));
  auto* click_event = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Milliseconds(5),
      pointer_id);
  auto* click_entry =
      SimulateEventDispatch(*click_event, base::Milliseconds(1));

  // Simulate the wrong frame source presentation time
  actual_frame_source_id = expected_frame_source_id + 1;
  uint64_t frame_index_2 = GetCurrentFrameIndex();
  FastForwardBy(base::Milliseconds(1));
  SimulatePaintAndCommit();
  FastForwardBy(base::Seconds(1));
  SimulateJustPresentationTime(frame_index_2, expected_frame_source_id,
                               actual_frame_source_id);

  // Expect that we measure a fallback less than presentation time, which is
  // Now().
  EXPECT_THAT(pointerup_entry, IsFallbackPainted());
  EXPECT_LT(pointerup_entry->GetEndTime(), base::TimeTicks::Now());

  EXPECT_THAT(click_entry, IsFallbackPainted());
  EXPECT_LT(click_entry->GetEndTime(), base::TimeTicks::Now());

  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry, pointerup_entry, click_entry});

  EXPECT_EQ(1ul, performance_->interactionCount());
}

TEST_P(WindowPerformanceTest, Scroll) {
  PointerId pointer_id = 5;

  // Pointerdown
  auto* pointerdown_event = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerdown_entry =
      SimulateEventDispatch(*pointerdown_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // Pointercancel
  auto* pointercancel_event = CreatePointerEvent(
      event_type_names::kPointercancel,
      base::TimeTicks::Now() - base::Milliseconds(2), pointer_id);
  PerformanceEventTiming* pointercancel_entry =
      SimulateEventDispatch(*pointercancel_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();

  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry, pointercancel_entry});
  ExpectUMACounts(0, 0);
}

TEST_P(WindowPerformanceTest, TouchesWithoutClick) {
  PointerId pointer_id = 4;

  // First Pointerdown
  auto* pointerdown_event1 = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerdown_entry1 =
      SimulateEventDispatch(*pointerdown_event1, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // Second Pointerdown
  auto* pointerdown_event2 = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerdown_entry2 =
      SimulateEventDispatch(*pointerdown_event2, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(7));
  SimulateAllRenderingStages();

  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry1, pointerdown_entry2});
}

#if BUILDFLAG(IS_MAC)
//  Test artificial pointerup and click on MacOS fall back to use processingEnd
//  as event duration ending time.
//  See crbug.com/1321819
TEST_P(WindowPerformanceTest, ArtificialPointerupOrClick) {
  // Arbitrary pointerId picked for testing
  PointerId pointer_id = 4;

  base::TimeTicks hardware_time = base::TimeTicks::Now();

  // This test creates three events which all share the same hardware start time
  // but have different processing and presentation times, and expects the
  // duration to only count up to processing_end (for the synthetic events).
  // Pointerdown: start 0, processing [1,2],   presentation 3
  // Pointerup:   start 0, processing [5,6],   presetnation 10
  // Click:       start 0, processing [11,12], presentation 20
  // Expected Durations: 3,6,12

  // 1. Pointerdown
  FastForwardBy(base::Milliseconds(1));
  PerformanceEventTiming* pointerdown_entry =
      SimulateEventDispatch(*CreatePointerEvent(event_type_names::kPointerdown,
                                                hardware_time, pointer_id),
                            base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  // 2. Artificial Pointerup
  FastForwardBy(base::Milliseconds(2));
  PerformanceEventTiming* pointerup_entry =
      SimulateEventDispatch(*CreatePointerEvent(event_type_names::kPointerup,
                                                hardware_time, pointer_id),
                            base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(4));
  SimulateAllRenderingStages();

  // 3. Artificial Click
  FastForwardBy(base::Milliseconds(1));
  PerformanceEventTiming* click_entry = SimulateEventDispatch(
      *CreatePointerEvent(event_type_names::kClick, hardware_time, pointer_id),
      base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(8));
  SimulateAllRenderingStages();

  EXPECT_THAT(pointerdown_entry, IsPainted());
  EXPECT_THAT(pointerup_entry, IsFallbackPainted());
  EXPECT_THAT(click_entry, IsFallbackPainted());

  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry, pointerup_entry, click_entry});
  ExpectUMACounts(0, 3);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_P(WindowPerformanceTest, DeduplicateIdenticalTimings) {
  // Simulate two events for the same interaction with identical timings in the
  // same frame.
  ui::DomCode key_code = ui::DomCode::US_A;

  // 1. Keydown
  auto* keydown_event = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  PerformanceEventTiming* keydown_entry =
      SimulateEventDispatch(*keydown_event, base::Milliseconds(1));

  // 2. Keypress with identical timings.
  auto* keypress_event = CreateKeyboardEvent(
      event_type_names::kKeypress,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  PerformanceEventTiming* keypress_entry =
      SimulateEventDispatch(*keypress_event, base::Milliseconds(1));

  // Both events are in the same frame and will get the same presentation time.
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  EXPECT_THAT(keydown_entry, SamePaintAs(keypress_entry));

  CheckEntriesAreReportedToRendererUKM({keydown_entry, keypress_entry});
  // Note: even though we have 2 events, we expect a single sample, because the
  // durations perfectly overlap for the same interaction.
  ExpectUMACounts(1, 0);
}

TEST_P(WindowPerformanceTest, NoDeduplicateDifferentInteractions) {
  // Simulate two different interactions with identical timings in the same
  // frame.

  // 1. Keydown for key 1
  auto* keydown_event1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), ui::DomCode::US_A);
  PerformanceEventTiming* keydown_entry1 =
      SimulateEventDispatch(*keydown_event1, base::Milliseconds(1));

  // 2. Keydown for key 2
  auto* keydown_event2 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), ui::DomCode::US_B);
  PerformanceEventTiming* keydown_entry2 =
      SimulateEventDispatch(*keydown_event2, base::Milliseconds(1));

  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  EXPECT_THAT(keydown_entry1, SamePaintAs(keydown_entry2));
  ExpectUMACounts(2, 0);
}

TEST_P(WindowPerformanceTest, DeduplicateResetsAcrossFrames) {
  // Simulate two identical interactions in DIFFERENT frames.
  ui::DomCode key_code = ui::DomCode::US_A;

  // Frame 1
  auto* keydown_event = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  SimulateEventDispatch(*keydown_event, base::Milliseconds(1));
  auto* keypress_event = CreateKeyboardEvent(
      event_type_names::kKeypress,
      base::TimeTicks::Now() - base::Milliseconds(1), key_code);
  SimulateEventDispatch(*keypress_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // Frame 2 - Same interaction, same timings.
  auto* keyup_event = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(1),
      key_code);
  SimulateEventDispatch(*keyup_event, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(8));
  SimulateAllRenderingStages();

  // 1 from Frame 1 (deduplicated), 1 from Frame 2. Total 2.
  ExpectUMACounts(2, 0);
}

// The trace_analyzer does not work on platforms on which the migration of
// tracing into Perfetto has not completed.
TEST_P(WindowPerformanceTest, PerformanceMarkTraceEvent) {
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

  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");

  std::optional<double> start_time = arg_dict.FindDouble("startTime");
  ASSERT_TRUE(start_time.has_value());

  // The navigationId should be recorded if performance.mark is executed by a
  // document.
  std::string* navigation_id = arg_dict.FindString("navigationId");
  ASSERT_TRUE(navigation_id);
}

TEST_P(WindowPerformanceTest, ElementTimingTraceEvent) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  // |element| needs to be non-null to prevent a crash.
  performance_->AddElementTiming(
      AtomicString("image-paint"), "url", gfx::RectF(10, 20, 30, 40),
      DOMPaintTimingInfo{2000, 2000},
      GetTimeOrigin() + base::Milliseconds(1000), AtomicString("identifier"),
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
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
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
  ASSERT_TRUE(arg_dict.FindInt("nodeId").has_value());
}

TEST_P(WindowPerformanceTest, EventTimingTraceEvents) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");

  auto* event1 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(5), 4,
                         GetWindow()->document());
  SimulateEventDispatch(*event1, base::Milliseconds(10));

  // presentation_time = processing_end + 10ms. PROC_END was +10ms. ProcStart -
  // 5ms. Helper consumes 10ms. So distance is 15ms. Target duration is 25ms.
  FastForwardBy(base::Milliseconds(10));
  SimulateAllRenderingStages();

  auto* event2 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(5), 4,
                         GetWindow()->document());
  SimulateEventDispatch(*event2, base::Milliseconds(10));

  // Click starts at start_time2 again. ProcStart3 = ProcEnd2.
  // So overlap to start of preceding event setup!
  auto* event3 = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::Milliseconds(15),
      4, GetWindow()->document());
  SimulateEventDispatch(*event3, base::Milliseconds(10));

  // presentation_time2 = processing_end3 + 5ms.
  FastForwardBy(base::Milliseconds(5));
  SimulateAllRenderingStages();

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
  base::DictValue arg_dict = pointerdown_begin->GetKnownArgAsDict("data");
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

TEST_P(WindowPerformanceTest, SlowInteractionToNextPaintTraceEvents) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");

  constexpr ui::DomCode kKeyCode = ui::DomCode::US_A;

  // Short, untraced keyboard event.
  {
    // Keydown.
    auto* event1 = CreateKeyboardEvent(
        event_type_names::kKeydown,
        base::TimeTicks::Now() - base::Milliseconds(1), kKeyCode);
    SimulateEventDispatch(*event1, base::Milliseconds(1));
    FastForwardBy(base::Milliseconds(18));
    SimulateAllRenderingStages();

    // Keyup.
    auto* event2 = CreateKeyboardEvent(
        event_type_names::kKeyup,
        base::TimeTicks::Now() - base::Milliseconds(5), kKeyCode);
    SimulateEventDispatch(*event2, base::Milliseconds(35));
    FastForwardBy(base::Milliseconds(60));
    SimulateAllRenderingStages();
  }

  // Single long event.
  {
    // Keydown (quick).
    auto* event3 = CreateKeyboardEvent(
        event_type_names::kKeydown,
        base::TimeTicks::Now() - base::Milliseconds(1), kKeyCode);
    SimulateEventDispatch(*event3, base::Milliseconds(1));
    FastForwardBy(base::Milliseconds(18));
    SimulateAllRenderingStages();

    // Keyup (start = 210, dur = 101ms).
    auto* event4 = CreateKeyboardEvent(
        event_type_names::kKeyup,
        base::TimeTicks::Now() - base::Milliseconds(5), kKeyCode);
    SimulateEventDispatch(*event4, base::Milliseconds(35));
    FastForwardBy(base::Milliseconds(61));
    SimulateAllRenderingStages();
  }

  // Overlapping events.
  {
    // Keydown (quick).
    auto* event5 = CreateKeyboardEvent(
        event_type_names::kKeydown,
        base::TimeTicks::Now() - base::Milliseconds(1), kKeyCode);
    SimulateEventDispatch(*event5, base::Milliseconds(1));
    FastForwardBy(base::Milliseconds(8));
    SimulateAllRenderingStages();

    // Keyup (start = 1020, dur = 1000ms).
    auto* event6 = CreateKeyboardEvent(
        event_type_names::kKeyup,
        base::TimeTicks::Now() - base::Milliseconds(10), kKeyCode);
    SimulateEventDispatch(*event6, base::Milliseconds(10));
    FastForwardBy(base::Milliseconds(980));
    SimulateAllRenderingStages();

    // Keydown (quick).
    auto* event7 = CreateKeyboardEvent(
        event_type_names::kKeydown,
        base::TimeTicks::Now() - base::Milliseconds(1), kKeyCode);
    SimulateEventDispatch(*event7, base::Milliseconds(1));
    FastForwardBy(base::Milliseconds(8));
    SimulateAllRenderingStages();

    // Keyup (start = 1800, dur = 600ms).
    auto* event8 = CreateKeyboardEvent(
        event_type_names::kKeyup,
        base::TimeTicks::Now() - base::Milliseconds(2), kKeyCode);
    SimulateEventDispatch(*event8, base::Milliseconds(8));
    FastForwardBy(base::Milliseconds(590));
    SimulateAllRenderingStages();
  }

  auto analyzer = trace_analyzer::Stop();
  analyzer->AssociateAsyncBeginEndEvents();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("SlowInteractionToNextPaint") &&
            Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(3u, events.size());

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

TEST_P(WindowPerformanceTest, ContainerTimingTraceEvent) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  performance_->AddContainerTiming(
      DOMPaintTimingInfo{.paint_time = 2000, .presentation_time = 2000},
      gfx::Rect(10, 20, 30, 40), 1200, nullptr, AtomicString("identifier"),
      /*element*/ nullptr,
      DOMPaintTimingInfo{.paint_time = 1000, .presentation_time = 1000});
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("PerformanceContainerTiming");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);
  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
  std::string* element_type = arg_dict.FindString("elementType");
  ASSERT_TRUE(element_type);
  EXPECT_EQ(*element_type, "container-paints");
  EXPECT_EQ(arg_dict.FindInt("startTime").value_or(-1), 2000);
  EXPECT_EQ(arg_dict.FindInt("firstRenderTime").value_or(-1), 1000);
  EXPECT_EQ(arg_dict.FindInt("duration").value_or(-1), 0);
  EXPECT_EQ(arg_dict.FindDouble("rectLeft").value_or(-1), 10);
  EXPECT_EQ(arg_dict.FindDouble("rectTop").value_or(-1), 20);
  EXPECT_EQ(arg_dict.FindDouble("rectWidth").value_or(-1), 30);
  EXPECT_EQ(arg_dict.FindDouble("rectHeight").value_or(-1), 40);
  EXPECT_EQ(arg_dict.FindDouble("size").value_or(-1), 1200);
  std::string* identifier = arg_dict.FindString("identifier");
  ASSERT_TRUE(identifier);
  EXPECT_EQ(*identifier, "identifier");
}

TEST_P(WindowPerformanceTest, InteractionID) {
  ui::DomCode key_code = ui::DomCode::US_A;
  auto* keydown_event =
      CreateKeyboardEvent(event_type_names::kKeydown,
                          base::TimeTicks::Now() - base::TimeDelta(), key_code);
  PerformanceEventTiming* keydown_entry =
      SimulateEventDispatch(*keydown_event, base::Milliseconds(20));
  SimulateAllRenderingStages();

  FastForwardBy(base::Milliseconds(15));
  auto* keyup_event =
      CreateKeyboardEvent(event_type_names::kKeyup,
                          base::TimeTicks::Now() - base::TimeDelta(), key_code);
  PerformanceEventTiming* keyup_entry =
      SimulateEventDispatch(*keyup_event, base::Milliseconds(25));
  SimulateAllRenderingStages();

  EXPECT_THAT(keydown_entry, SameInteractionAs(keyup_entry));

  PointerId pointer_id_1 = 10;
  auto* pointerdown_event = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::TimeDelta(), pointer_id_1);
  PerformanceEventTiming* pointerdown_entry =
      SimulateEventDispatch(*pointerdown_event, base::Milliseconds(20));
  SimulateAllRenderingStages();

  FastForwardBy(base::Milliseconds(10));
  auto* pointerup_event = CreatePointerEvent(
      event_type_names::kPointerup, base::TimeTicks::Now() - base::TimeDelta(),
      pointer_id_1);
  PerformanceEventTiming* pointerup_entry =
      SimulateEventDispatch(*pointerup_event, base::Milliseconds(20));
  SimulateAllRenderingStages();

  auto* click_event = CreatePointerEvent(
      event_type_names::kClick, base::TimeTicks::Now() - base::TimeDelta(),
      pointer_id_1);
  PerformanceEventTiming* click_entry =
      SimulateEventDispatch(*click_event, base::Milliseconds(70));
  SimulateAllRenderingStages();

  EXPECT_THAT(pointerdown_entry, SameInteractionAs(pointerup_entry));
  EXPECT_THAT(pointerup_entry, SameInteractionAs(click_entry));

  // Scroll should not be reported in ukm.
  PointerId pointer_id_2 = 20;
  auto* pointerdown_event_2 = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::TimeDelta(), pointer_id_2);
  PerformanceEventTiming* pointerdown_entry_2 =
      SimulateEventDispatch(*pointerdown_event_2, base::Milliseconds(15));
  SimulateAllRenderingStages();

  auto* pointercancel_event = CreatePointerEvent(
      event_type_names::kPointercancel,
      base::TimeTicks::Now() - base::TimeDelta(), pointer_id_2);
  PerformanceEventTiming* pointercancel_entry =
      SimulateEventDispatch(*pointercancel_event, base::Milliseconds(20));
  SimulateAllRenderingStages();

  EXPECT_THAT(pointerdown_entry_2, testing::Not(IsInteraction()));
  EXPECT_THAT(pointercancel_entry, testing::Not(IsInteraction()));

  CheckEntriesAreReportedToRendererUKM({keydown_entry, keyup_entry,
                                        pointerdown_entry, pointerup_entry,
                                        click_entry});
}

INSTANTIATE_TEST_SUITE_P(All, WindowPerformanceTest, ::testing::Bool());

class InteractionIdTest : public WindowPerformanceTest {};

// Tests English typing.
TEST_P(InteractionIdTest, InputOutsideComposition) {
  auto* event1_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(100), ui::DomCode::US_A);
  auto* entry1_1 = SimulateEventDispatch(*event1_1, base::Milliseconds(50));
  auto* event1_2 = CreateInputEvent();
  auto* entry1_2 = SimulateEventDispatch(*event1_2, base::Milliseconds(100));
  auto* event1_3 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(130), ui::DomCode::US_A);
  auto* entry1_3 = SimulateEventDispatch(*event1_3, base::Milliseconds(20));
  EXPECT_THAT(entry1_2, testing::Not(IsInteraction()));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_3));

  auto* event2_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::DIGIT5);
  auto* entry2_1 = SimulateEventDispatch(*event2_1, base::Milliseconds(20));
  auto* event2_2 = CreateInputEvent();
  auto* entry2_2 = SimulateEventDispatch(*event2_2, base::Milliseconds(100));
  auto* event2_3 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(250), ui::DomCode::DIGIT5);
  auto* entry2_3 = SimulateEventDispatch(*event2_3, base::Milliseconds(40));
  EXPECT_THAT(entry2_2, testing::Not(IsInteraction()));
  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_3));
  EXPECT_THAT(entry1_1, DifferentInteractionFrom(entry2_1));

  auto* event3_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(300), ui::DomCode::BACKSPACE);
  auto* entry3_1 = SimulateEventDispatch(*event3_1, base::Milliseconds(20));
  auto* event3_2 = CreateInputEvent();
  auto* entry3_2 = SimulateEventDispatch(*event3_2, base::Milliseconds(100));
  auto* event3_3 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(300), ui::DomCode::BACKSPACE);
  auto* entry3_3 = SimulateEventDispatch(*event3_3, base::Milliseconds(25));
  EXPECT_THAT(entry3_2, testing::Not(IsInteraction()));
  EXPECT_THAT(entry3_1, SameInteractionAs(entry3_3));
  EXPECT_THAT(entry1_1, DifferentInteractionFrom(entry3_1));
  EXPECT_THAT(entry2_1, DifferentInteractionFrom(entry3_1));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1_1, entry1_2, entry1_3, entry2_1,
                                        entry2_2, entry2_3, entry3_1, entry3_2,
                                        entry3_3});
}

// Tests Japanese on Mac.
TEST_P(InteractionIdTest, CompositionSingleKeydown) {
  auto* event1_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(100), ui::DomCode::US_D);
  auto* entry1_1 = SimulateEventDispatch(*event1_1, base::Milliseconds(100));
  auto* event1_2 = CreateCompositionEvent(event_type_names::kCompositionstart);
  auto* entry1_2 = SimulateEventDispatch(*event1_2, base::Milliseconds(1));
  auto* event1_3 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry1_3 = SimulateEventDispatch(*event1_3, base::Milliseconds(1));
  auto* event1_4 = CreateInputEvent();
  auto* entry1_4 = SimulateEventDispatch(*event1_4, base::Milliseconds(20));
  auto* event1_5 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(120), ui::DomCode::US_A);
  auto* entry1_5 = SimulateEventDispatch(*event1_5, base::Milliseconds(100));
  auto* event2_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::US_D);
  auto* entry2_1 = SimulateEventDispatch(*event2_1, base::Milliseconds(100));
  auto* event2_2 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry2_2 = SimulateEventDispatch(*event2_2, base::Milliseconds(1));
  auto* event2_3 = CreateInputEvent();
  auto* entry2_3 = SimulateEventDispatch(*event2_3, base::Milliseconds(30));
  auto* event2_4 = CreateCompositionEvent(event_type_names::kCompositionend);
  auto* entry2_4 = SimulateEventDispatch(*event2_4, base::Milliseconds(1));
  auto* event2_5 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(270), ui::DomCode::US_B);
  auto* entry2_5 = SimulateEventDispatch(*event2_5, base::Milliseconds(100));
  performance_->GetResponsivenessMetrics().FlushAllEvents();

  EXPECT_THAT(entry1_2, testing::Not(IsInteraction()));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_4));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_5));

  EXPECT_THAT(entry2_4, testing::Not(IsInteraction()));
  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_3));
  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_5));

  EXPECT_THAT(entry1_4, DifferentInteractionFrom(entry2_3));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1_1, entry1_2, entry1_3, entry1_4,
                                        entry1_5, entry2_1, entry2_2, entry2_3,
                                        entry2_4, entry2_5});
}

// Tests Chinese on Mac. Windows is similar, but has more keyups inside the
// composition.
TEST_P(InteractionIdTest, CompositionToFinalInput) {
  // Insert "a" with a duration of 25.
  auto* event1_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(100), ui::DomCode::US_D);
  auto* entry1_1 = SimulateEventDispatch(*event1_1, base::Milliseconds(90));
  auto* event1_2 = CreateCompositionEvent(event_type_names::kCompositionstart);
  auto* entry1_2 = SimulateEventDispatch(*event1_2, base::Milliseconds(1));
  auto* event1_3 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry1_3 = SimulateEventDispatch(*event1_3, base::Milliseconds(1));
  auto* event1_4 = CreateInputEvent();
  auto* entry1_4 = SimulateEventDispatch(*event1_4, base::Milliseconds(25));
  auto* event1_5 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(110), ui::DomCode::US_A);
  auto* entry1_5 = SimulateEventDispatch(*event1_5, base::Milliseconds(80));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_4));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_5));

  auto* event2_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::US_D);
  auto* entry2_1 = SimulateEventDispatch(*event2_1, base::Milliseconds(90));
  auto* event2_2 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry2_2 = SimulateEventDispatch(*event2_2, base::Milliseconds(1));
  auto* event2_3 = CreateInputEvent();
  auto* entry2_3 = SimulateEventDispatch(*event2_3, base::Milliseconds(35));
  auto* event2_4 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(210), ui::DomCode::US_B);
  auto* entry2_4 = SimulateEventDispatch(*event2_4, base::Milliseconds(80));
  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_3));
  EXPECT_THAT(entry1_4, DifferentInteractionFrom(entry2_3));

  auto* event3_1 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry3_1 = SimulateEventDispatch(*event3_1, base::Milliseconds(1));
  auto* event3_2 = CreateInputEvent();
  auto* entry3_2 = SimulateEventDispatch(*event3_2, base::Milliseconds(140));
  auto* event3_3 = CreateCompositionEvent(event_type_names::kCompositionend);
  auto* entry3_3 = SimulateEventDispatch(*event3_3, base::Milliseconds(1));
  EXPECT_THAT(entry3_3, testing::Not(IsInteraction()));
  EXPECT_THAT(entry1_4, DifferentInteractionFrom(entry3_2));
  EXPECT_THAT(entry2_3, DifferentInteractionFrom(entry3_2));

  performance_->GetResponsivenessMetrics().FlushAllEvents();

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM(
      {entry1_1, entry1_2, entry1_3, entry1_4, entry1_5, entry2_1, entry2_2,
       entry2_3, entry2_4, entry3_1, entry3_2, entry3_3});
}

// Tests Chinese on Windows.
TEST_P(InteractionIdTest, CompositionToFinalInputMultipleKeyUps) {
  // Insert "a" with a duration of 66.
  auto* event1_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(0), ui::DomCode::US_D);
  auto* entry1_1 = SimulateEventDispatch(*event1_1, base::Milliseconds(100));
  auto* event1_2 = CreateCompositionEvent(event_type_names::kCompositionstart);
  auto* entry1_2 = SimulateEventDispatch(*event1_2, base::Milliseconds(1));
  auto* event1_3 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry1_3 = SimulateEventDispatch(*event1_3, base::Milliseconds(1));
  auto* event1_4 = CreateInputEvent();
  auto* entry1_4 = SimulateEventDispatch(*event1_4, base::Milliseconds(66));
  auto* event1_5 = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(0),
      ui::DomCode::US_D);
  auto* entry1_5 = SimulateEventDispatch(*event1_5, base::Milliseconds(100));
  auto* event1_6 = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(0),
      ui::DomCode::US_A);
  auto* entry1_6 = SimulateEventDispatch(*event1_6, base::Milliseconds(100));
  auto* event2_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::US_D);
  auto* entry2_1 = SimulateEventDispatch(*event2_1, base::Milliseconds(100));
  auto* event2_2 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry2_2 = SimulateEventDispatch(*event2_2, base::Milliseconds(1));
  auto* event2_3 = CreateInputEvent();
  auto* entry2_3 = SimulateEventDispatch(*event2_3, base::Milliseconds(51));
  auto* event2_4 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::US_D);
  auto* entry2_4 = SimulateEventDispatch(*event2_4, base::Milliseconds(100));
  auto* event2_5 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::US_B);
  auto* entry2_5 = SimulateEventDispatch(*event2_5, base::Milliseconds(100));
  auto* event3_1 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry3_1 = SimulateEventDispatch(*event3_1, base::Milliseconds(1));
  auto* event3_2 = CreateInputEvent();
  auto* entry3_2 = SimulateEventDispatch(*event3_2, base::Milliseconds(85));
  auto* event3_3 = CreateCompositionEvent(event_type_names::kCompositionend);
  auto* entry3_3 = SimulateEventDispatch(*event3_3, base::Milliseconds(1));
  performance_->GetResponsivenessMetrics().FlushAllEvents();
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_4));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_5));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_6));

  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_3));
  EXPECT_THAT(entry1_4, DifferentInteractionFrom(entry2_3));
  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_4));
  EXPECT_THAT(entry2_1, SameInteractionAs(entry2_5));

  EXPECT_THAT(entry1_4, DifferentInteractionFrom(entry3_2));
  EXPECT_THAT(entry2_3, DifferentInteractionFrom(entry3_2));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM(
      {entry1_1, entry1_2, entry1_3, entry1_4, entry1_5, entry1_6, entry2_1,
       entry2_2, entry2_3, entry2_4, entry2_5, entry3_1, entry3_2, entry3_3});
}

// Tests Android smart suggestions (similar to Android Chinese).
TEST_P(InteractionIdTest, SmartSuggestion) {
  auto* event1_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(0), ui::DomCode::US_D);
  auto* entry1_1 = SimulateEventDispatch(*event1_1, base::Milliseconds(16));
  auto* event1_2 = CreateCompositionEvent(event_type_names::kCompositionstart);
  auto* entry1_2 = SimulateEventDispatch(*event1_2, base::Milliseconds(1));
  auto* event1_3 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry1_3 = SimulateEventDispatch(*event1_3, base::Milliseconds(1));
  auto* event1_4 = CreateInputEvent();
  auto* entry1_4 = SimulateEventDispatch(*event1_4, base::Milliseconds(9));
  auto* event1_5 = CreateKeyboardEvent(
      event_type_names::kKeyup, base::TimeTicks::Now() - base::Milliseconds(0),
      ui::DomCode::US_D);
  auto* entry1_5 = SimulateEventDispatch(*event1_5, base::Milliseconds(16));
  auto* event2_1 = CreateCompositionEvent(event_type_names::kCompositionupdate);
  auto* entry2_1 = SimulateEventDispatch(*event2_1, base::Milliseconds(1));
  auto* event2_2 = CreateInputEvent();
  auto* entry2_2 = SimulateEventDispatch(*event2_2, base::Milliseconds(14));
  auto* event2_3 = CreateCompositionEvent(event_type_names::kCompositionend);
  auto* entry2_3 = SimulateEventDispatch(*event2_3, base::Milliseconds(1));
  auto* event3_1 = CreateKeyboardEvent(
      event_type_names::kKeydown,
      base::TimeTicks::Now() - base::Milliseconds(200), ui::DomCode::US_D);
  auto* entry3_1 = SimulateEventDispatch(*event3_1, base::Milliseconds(43));
  auto* event3_2 = CreateInputEvent();
  auto* entry3_2 = SimulateEventDispatch(*event3_2, base::Milliseconds(100));
  auto* event3_3 = CreateKeyboardEvent(
      event_type_names::kKeyup,
      base::TimeTicks::Now() - base::Milliseconds(235), ui::DomCode::US_D);
  auto* entry3_3 = SimulateEventDispatch(*event3_3, base::Milliseconds(35));
  performance_->GetResponsivenessMetrics().FlushAllEvents();
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_4));
  EXPECT_THAT(entry1_1, SameInteractionAs(entry1_5));

  EXPECT_THAT(entry2_2, IsInteraction());
  EXPECT_THAT(entry1_4, DifferentInteractionFrom(entry2_2));
  EXPECT_THAT(entry3_2, testing::Not(IsInteraction()));
  EXPECT_THAT(entry3_1, SameInteractionAs(entry3_3));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1_1, entry1_2, entry1_3, entry1_4,
                                        entry1_5, entry2_1, entry2_2, entry2_3,
                                        entry3_1, entry3_2, entry3_3});
}

TEST_P(InteractionIdTest, TapWithoutClick) {
  auto* event1 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(100), 1);
  auto* entry1 = SimulateEventDispatch(*event1, base::Milliseconds(40));
  auto* event2 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(120), 1);
  auto* entry2 = SimulateEventDispatch(*event2, base::Milliseconds(30));
  EXPECT_THAT(entry1, SameInteractionAs(entry2));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1, entry2});
}

TEST_P(InteractionIdTest, PointerupClick) {
  auto* event1 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(100), 1);
  auto* entry1 = SimulateEventDispatch(*event1, base::Milliseconds(40));
  auto* event2 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(120), 1);
  auto* entry2 = SimulateEventDispatch(*event2, base::Milliseconds(30));
  EXPECT_THAT(entry1, testing::Not(IsInteraction()));
  EXPECT_THAT(entry2, IsInteraction());

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1, entry2});
}

TEST_P(InteractionIdTest, JustClick) {
  auto* event =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(120), 0);
  auto* entry = SimulateEventDispatch(*event, base::Milliseconds(30));
  EXPECT_THAT(entry, IsInteraction());

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry});
}

TEST_P(InteractionIdTest, PointerdownClick) {
  auto* event1 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(100), 1);
  auto* entry1 = SimulateEventDispatch(*event1, base::Milliseconds(40));
  auto* event2 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(120), 1);
  auto* entry2 = SimulateEventDispatch(*event2, base::Milliseconds(30));
  EXPECT_THAT(entry1, SameInteractionAs(entry2));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1, entry2});
}

TEST_P(InteractionIdTest, MultiTouch) {
  auto* event1 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(100), 1);
  auto* entry1 = SimulateEventDispatch(*event1, base::Milliseconds(10));
  auto* event2 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(120), 2);
  auto* entry2 = SimulateEventDispatch(*event2, base::Milliseconds(20));
  auto* event3 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(200), 2);
  auto* entry3 = SimulateEventDispatch(*event3, base::Milliseconds(30));
  auto* event4 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(200), 1);
  auto* entry4 = SimulateEventDispatch(*event4, base::Milliseconds(50));
  EXPECT_THAT(entry1, SameInteractionAs(entry4));
  EXPECT_THAT(entry2, SameInteractionAs(entry3));

  FastForwardBy(base::Seconds(1));

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1, entry2, entry3, entry4});
}

TEST_P(InteractionIdTest, ClickIncorrectPointerId) {
  auto* event1 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(100), 1);
  auto* entry1 = SimulateEventDispatch(*event1, base::Milliseconds(30));
  auto* event2 =
      CreatePointerEvent(event_type_names::kClick,
                         base::TimeTicks::Now() - base::Milliseconds(120), 0);
  auto* entry2 = SimulateEventDispatch(*event2, base::Milliseconds(40));
  EXPECT_THAT(entry1, testing::Not(IsInteraction()));
  EXPECT_THAT(entry2, IsInteraction());

  SimulateAllRenderingStages();
  CheckEntriesAreReportedToRendererUKM({entry1, entry2});
}

TEST_P(InteractionIdTest, ContextMenu) {
  PointerId pointer_id = 4;

  // 1. Pointerdown
  auto* pointerdown_event = CreatePointerEvent(
      event_type_names::kPointerdown,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerdown_entry =
      SimulateEventDispatch(*pointerdown_event, base::Milliseconds(1));

  // pointerdown is pending and should not have an interactionId yet.
  EXPECT_FALSE(pointerdown_entry->HasKnownInteractionID());
  EXPECT_FALSE(pointerdown_entry->HasKnownEndTime());

  // 2. Contextmenu
  auto* contextmenu_event = CreatePointerEvent(
      event_type_names::kContextmenu,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* contextmenu_entry =
      SimulateEventDispatch(*contextmenu_event, base::Milliseconds(1));

  base::TimeTicks processing_end_contextmenu = base::TimeTicks::Now();

  // Now pointerdown should have a fallback time.
  EXPECT_TRUE(pointerdown_entry->HasKnownEndTime());
  EXPECT_EQ(pointerdown_entry->GetEventTimingReportingInfo()->fallback_reason,
            FallbackReason::kInteractionInterruptedByContextMenu);
  EXPECT_EQ(pointerdown_entry->GetEndTime(), processing_end_contextmenu);

  // Contextmenu itself should not have an interactionId, but should have a
  // duration and fallback.
  EXPECT_TRUE(contextmenu_entry->HasKnownInteractionID());
  EXPECT_THAT(contextmenu_entry, testing::Not(IsInteraction()));
  EXPECT_TRUE(contextmenu_entry->HasKnownEndTime());
  EXPECT_EQ(contextmenu_entry->GetEventTimingReportingInfo()->fallback_reason,
            FallbackReason::kInteractionInterruptedByContextMenu);
  EXPECT_EQ(contextmenu_entry->GetEndTime(), processing_end_contextmenu);

  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  // ...And finally, after presentation time arrives, we should have an
  // interactionId.
  EXPECT_TRUE(pointerdown_entry->HasKnownInteractionID());
  EXPECT_THAT(pointerdown_entry, IsInteraction());

  // 3. Pointerup
  auto* pointerup_event = CreatePointerEvent(
      event_type_names::kPointerup,
      base::TimeTicks::Now() - base::Milliseconds(1), pointer_id);
  PerformanceEventTiming* pointerup_entry =
      SimulateEventDispatch(*pointerup_event, base::Milliseconds(1));

  FastForwardBy(base::Milliseconds(3));
  SimulateAllRenderingStages();

  // pointerup should have the same interactionId as pointerdown and a good end
  // time.
  EXPECT_THAT(pointerup_entry, SameInteractionAs(pointerdown_entry));
  EXPECT_TRUE(pointerup_entry->HasKnownEndTime());
  EXPECT_EQ(pointerup_entry->GetEndTime(), base::TimeTicks::Now());

  // After a wait, we should see the UKM.
  FastForwardBy(base::Seconds(1));
  CheckEntriesAreReportedToRendererUKM(
      {pointerdown_entry, contextmenu_entry, pointerup_entry});
}

// Regression test for crbug.com/487091601.
TEST_P(InteractionIdTest, FirstInputInteractionIdCrash) {
  // 1. Pointerdown with pointerId 4
  auto* pointerdown_event_1 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*pointerdown_event_1, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  // 2. Pointerdown with pointerId 5
  auto* pointerdown_event_2 =
      CreatePointerEvent(event_type_names::kPointerdown,
                         base::TimeTicks::Now() - base::Milliseconds(1), 5);
  SimulateEventDispatch(*pointerdown_event_2, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  // 3. Pointerup with pointerId 4
  auto* pointerup_event_2 =
      CreatePointerEvent(event_type_names::kPointerup,
                         base::TimeTicks::Now() - base::Milliseconds(1), 4);
  SimulateEventDispatch(*pointerup_event_2, base::Milliseconds(1));
  FastForwardBy(base::Milliseconds(1));
  SimulateAllRenderingStages();

  PerformanceEntryVector firstInputs =
      performance_->getEntriesByType(performance_entry_names::kFirstInput);
  ASSERT_EQ(1u, firstInputs.size());

  PerformanceEventTiming* firstInput =
      static_cast<PerformanceEventTiming*>(firstInputs[0].Get());
  // This should NOT crash, and should have a valid interactionId.
  EXPECT_THAT(firstInput, IsInteraction());
}

INSTANTIATE_TEST_SUITE_P(All, InteractionIdTest, ::testing::Bool());

class WindowPerformanceNavigationIdTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(WindowPerformanceNavigationIdTest, NavigationIdHardNavigations) {
  // Initial navigation: randomly generated IDs, assumed to be hard nav.
  std::vector<uint32_t> ids;
  for (int i = 0; i < 100; ++i) {
    // Making a new scope is like a hard nav (the ID gets generated via the
    // constructor of LocalDOMWindow).
    V8TestingScope scope;
    const WindowPerformance* performance =
        DOMWindowPerformance::performance(*scope.GetFrame().DomWindow());
    ASSERT_TRUE(performance);
    ids.push_back(performance->NavigationId());
  }
  // We allow 10 collisions, since the IDs are randomly generated between 100
  // and 10000.
  auto last = std::unique(ids.begin(), ids.end());
  auto num_collisions = std::distance(last, ids.end());
  EXPECT_LT(num_collisions, 10u);
  ids.erase(last, ids.end());
  // The IDs are not in sorted order.
  std::vector<uint32_t> sorted_ids(ids.begin(), ids.end());
  std::sort(sorted_ids.begin(), sorted_ids.end());
  EXPECT_NE(sorted_ids, ids);
}

TEST_F(WindowPerformanceNavigationIdTest, NavigationIdSoftNavigations) {
  // Initial navigation: randomly generated ID, assumed to be hard nav.
  V8TestingScope scope;
  WindowPerformance* performance =
      DOMWindowPerformance::performance(*scope.GetFrame().DomWindow());
  uint32_t navigation_id1 = performance->NavigationId();

  // Soft navigation or back-forward cache restoration: incremented ID.
  performance->IncrementNavigationId();
  uint32_t navigation_id3 = performance->NavigationId();
  EXPECT_NE(navigation_id1, navigation_id3);
  EXPECT_LT(navigation_id1, navigation_id3);
}
}  // namespace blink
