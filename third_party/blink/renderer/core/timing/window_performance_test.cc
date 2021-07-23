// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include "base/test/test_mock_time_task_runner.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using test::RunPendingTasks;

namespace {

base::TimeTicks GetTimeOrigin() {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(500);
}

base::TimeTicks GetTimeStamp(int64_t time) {
  return GetTimeOrigin() + base::TimeDelta::FromMilliseconds(time);
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
    // Stop UKM sampling for testing.
    performance_->GetResponsivenessMetrics().StopUkmSamplingForTesting();
  }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(page_holder_->GetDocument().GetFrame());
  }

  ukm::TestUkmRecorder* GetUkmRecorder() {
    return scoped_fake_ukm_recorder_.recorder();
  }

  uint64_t frame_counter = 1;
  Persistent<WindowPerformance> performance_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  ScopedFakeUkmRecorder scoped_fake_ukm_recorder_;
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
  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());

  base::TimeTicks start_time =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(1.1);
  base::TimeTicks processing_start =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(3.3);
  base::TimeTicks processing_end =
      GetTimeOrigin() + base::TimeDelta::FromSecondsD(3.8);
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);
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
                                    processing_end, true, nullptr, 4, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(2u, performance_->getBufferedEntriesByType("event").size());

  EXPECT_TRUE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  GetFrame()->DetachDocument();
  EXPECT_FALSE(page_holder_->GetFrame().Loader().GetDocumentLoader());
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(3u, performance_->getBufferedEntriesByType("event").size());
}

TEST_F(WindowPerformanceTest, Expose100MsEvents) {
  base::TimeTicks start_time =
      GetTimeOrigin() + base::TimeDelta::FromSeconds(1);
  base::TimeTicks processing_start =
      start_time + base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks processing_end =
      processing_start + base::TimeDelta::FromMilliseconds(10);
  performance_->RegisterEventTiming("mousedown", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);

  base::TimeTicks start_time2 =
      start_time + base::TimeDelta::FromMicroseconds(200);
  performance_->RegisterEventTiming("click", start_time2, processing_start,
                                    processing_end, false, nullptr, 4, 4);

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
  base::TimeTicks start_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1000);
  base::TimeTicks processing_start =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1001);
  base::TimeTicks processing_end =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1002);
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);
  base::TimeTicks short_swap_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(1003);
  SimulateSwapPromise(short_swap_time);
  EXPECT_EQ(0u, performance_->getBufferedEntriesByType("event").size());

  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, true, nullptr, 4, 4);
  base::TimeTicks long_swap_time =
      GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2000);
  SimulateSwapPromise(long_swap_time);
  EXPECT_EQ(1u, performance_->getBufferedEntriesByType("event").size());

  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, true, nullptr, 4, 4);
  SimulateSwapPromise(short_swap_time);
  performance_->RegisterEventTiming("click", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);
  SimulateSwapPromise(long_swap_time);
  EXPECT_EQ(2u, performance_->getBufferedEntriesByType("event").size());
}

// Test the case where multiple events are registered and then their swap
// promise is resolved.
TEST_F(WindowPerformanceTest, MultipleEventsThenSwap) {
  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    base::TimeTicks start_time =
        GetTimeOrigin() + base::TimeDelta::FromSeconds(i);
    base::TimeTicks processing_start =
        start_time + base::TimeDelta::FromMilliseconds(100);
    base::TimeTicks processing_end =
        start_time + base::TimeDelta::FromMilliseconds(200);
    performance_->RegisterEventTiming("click", start_time, processing_start,
                                      processing_end, false, nullptr, 4, 4);
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
        GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2), false, nullptr,
        4, 4);
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
        GetTimeOrigin() + base::TimeDelta::FromMilliseconds(2), false, nullptr,
        4, 4);
    SimulateSwapPromise(GetTimeOrigin() + base::TimeDelta::FromMilliseconds(3));
  }
  ASSERT_EQ(1u, performance_->getEntriesByType("first-input").size());
  EXPECT_EQ("mousedown",
            performance_->getEntriesByType("first-input")[0]->name());
}

// Test that pointerdown followed by pointerup works as a 'firstInput'.
TEST_F(WindowPerformanceTest, FirstPointerUp) {
  base::TimeTicks start_time = GetTimeStamp(0);
  base::TimeTicks processing_start = GetTimeStamp(1);
  base::TimeTicks processing_end = GetTimeStamp(2);
  base::TimeTicks swap_time = GetTimeStamp(3);
  performance_->RegisterEventTiming("pointerdown", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(0u, performance_->getEntriesByType("first-input").size());
  performance_->RegisterEventTiming("pointerup", start_time, processing_start,
                                    processing_end, false, nullptr, 4, 4);
  SimulateSwapPromise(swap_time);
  EXPECT_EQ(1u, performance_->getEntriesByType("first-input").size());
  // The name of the entry should be "pointerdown".
  EXPECT_EQ(
      1u, performance_->getEntriesByName("pointerdown", "first-input").size());
}

TEST_F(WindowPerformanceTest, OneKeyboardInteraction) {
  base::TimeTicks keydown_timestamp = GetTimeStamp(0);
  // Keydown
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id = absl::nullopt;
  absl::optional<int> key_code = 2;
  performance_->RegisterEventTiming(
      "keydown", keydown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);
  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(10);
  performance_->RegisterEventTiming(
      "keyup", keyup_timestamp, processing_start_keyup, processing_end_keyup,
      false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keyup);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto merged_entries = GetUkmRecorder()->GetMergedEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, merged_entries.size());
  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
        7);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
        10);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 0);
  }
}

TEST_F(WindowPerformanceTest, HoldingDownAKey) {
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  base::TimeTicks keydown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id = absl::nullopt;
  absl::optional<int> key_code = 2;
  performance_->RegisterEventTiming(
      "keydown", keydown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);

  // Second Keydown
  keydown_timestamp = GetTimeStamp(1);
  processing_start_keydown = GetTimeStamp(2);
  processing_end_keydown = GetTimeStamp(3);
  swap_time_keydown = GetTimeStamp(7);
  performance_->RegisterEventTiming(
      "keydown", keydown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);

  // Third Keydown
  keydown_timestamp = GetTimeStamp(2);
  processing_start_keydown = GetTimeStamp(3);
  processing_end_keydown = GetTimeStamp(5);
  swap_time_keydown = GetTimeStamp(9);
  performance_->RegisterEventTiming(
      "keydown", keydown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);

  // Keyup
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(13);
  performance_->RegisterEventTiming(
      "keyup", keyup_timestamp, processing_start_keyup, processing_end_keyup,
      false, nullptr, key_code, pointer_id);
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
  absl::optional<PointerId> pointer_id = absl::nullopt;
  absl::optional<int> first_key_code = 2;
  performance_->RegisterEventTiming(
      "keydown", keydown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, first_key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);

  // Press the second key.
  processing_start_keydown = GetTimeStamp(2);
  processing_end_keydown = GetTimeStamp(3);
  swap_time_keydown = GetTimeStamp(7);
  absl::optional<int> second_key_code = 4;
  performance_->RegisterEventTiming(
      "keydown", keydown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, second_key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);

  // Release the first key.
  base::TimeTicks keyup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(13);
  performance_->RegisterEventTiming(
      "keyup", keyup_timestamp, processing_start_keyup, processing_end_keyup,
      false, nullptr, first_key_code, pointer_id);
  SimulateSwapPromise(swap_time_keyup);

  // Release the second key.
  keyup_timestamp = GetTimeStamp(5);
  processing_start_keyup = GetTimeStamp(5);
  processing_end_keyup = GetTimeStamp(6);
  swap_time_keyup = GetTimeStamp(20);
  performance_->RegisterEventTiming(
      "keyup", keyup_timestamp, processing_start_keyup, processing_end_keyup,
      false, nullptr, second_key_code, pointer_id);
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
  base::TimeTicks pointerdwon_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id = 4;
  absl::optional<int> key_code = absl::nullopt;
  performance_->RegisterEventTiming(
      "pointerdown", pointerdwon_timestamp, processing_start_pointerdown,
      processing_end_pointerdown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);
  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks swap_time_pointerup = GetTimeStamp(10);
  performance_->RegisterEventTiming(
      "pointerup", pointerup_timestamp, processing_start_pointerup,
      processing_end_pointerup, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks swap_time_click = GetTimeStamp(20);
  performance_->RegisterEventTiming(
      "click", click_timestamp, processing_start_click, processing_end_click,
      false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      GetUkmRecorder()->GetMergedEntriesByName(
          ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, merged_entries.size());
  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
        7);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
        17);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 1);
  }
}

TEST_F(WindowPerformanceTest, Drag) {
  // Pointerdown
  base::TimeTicks pointerdwon_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id = 4;
  absl::optional<int> key_code = absl::nullopt;
  performance_->RegisterEventTiming(
      "pointerdown", pointerdwon_timestamp, processing_start_pointerdown,
      processing_end_pointerdown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);
  // Notify drag.
  performance_->NotifyPotentialDrag();
  // Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks swap_time_pointerup = GetTimeStamp(10);
  performance_->RegisterEventTiming(
      "pointerup", pointerup_timestamp, processing_start_pointerup,
      processing_end_pointerup, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_pointerup);
  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks swap_time_click = GetTimeStamp(20);
  performance_->RegisterEventTiming(
      "click", click_timestamp, processing_start_click, processing_end_click,
      false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      GetUkmRecorder()->GetMergedEntriesByName(
          ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, merged_entries.size());
  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName,
        7);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
        17);
    GetUkmRecorder()->ExpectEntryMetric(
        ukm_entry,
        ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 2);
  }
}

TEST_F(WindowPerformanceTest, Scroll) {
  // Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_keydown = GetTimeStamp(1);
  base::TimeTicks processing_end_keydown = GetTimeStamp(2);
  base::TimeTicks swap_time_keydown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id = 5;
  absl::optional<int> key_code = absl::nullopt;
  performance_->RegisterEventTiming(
      "pointerdown", pointerdown_timestamp, processing_start_keydown,
      processing_end_keydown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keydown);
  // Pointercancel
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_keyup = GetTimeStamp(5);
  base::TimeTicks processing_end_keyup = GetTimeStamp(6);
  base::TimeTicks swap_time_keyup = GetTimeStamp(10);
  performance_->RegisterEventTiming(
      "pointercancel", pointerup_timestamp, processing_start_keyup,
      processing_end_keyup, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_keyup);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      GetUkmRecorder()->GetMergedEntriesByName(
          ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, merged_entries.size());
}

TEST_F(WindowPerformanceTest, TouchesWithoutClick) {
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  // First Pointerdown
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id = 4;
  absl::optional<int> key_code = absl::nullopt;
  performance_->RegisterEventTiming(
      "pointerdown", pointerdown_timestamp, processing_start_pointerdown,
      processing_end_pointerdown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);

  // Second Pointerdown
  pointerdown_timestamp = GetTimeStamp(6);
  processing_start_pointerdown = GetTimeStamp(7);
  processing_end_pointerdown = GetTimeStamp(8);
  swap_time_pointerdown = GetTimeStamp(15);
  performance_->RegisterEventTiming(
      "pointerdown", pointerdown_timestamp, processing_start_pointerdown,
      processing_end_pointerdown, false, nullptr, key_code, pointer_id);
  SimulateSwapPromise(swap_time_pointerdown);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      GetUkmRecorder()->GetMergedEntriesByName(
          ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(0u, merged_entries.size());
}

// For multi-touch, we only record the innermost pair of pointerdown and
// pointerup.
// TODO(hbsong): Record each touch by pointer_id separately.
TEST_F(WindowPerformanceTest, MultiTouch) {
  // First Pointerdown
  base::TimeTicks pointerdown_timestamp = GetTimeOrigin();
  base::TimeTicks processing_start_pointerdown = GetTimeStamp(1);
  base::TimeTicks processing_end_pointerdown = GetTimeStamp(2);
  base::TimeTicks swap_time_pointerdown = GetTimeStamp(5);
  absl::optional<PointerId> pointer_id_1 = 4;
  absl::optional<int> key_code = absl::nullopt;
  performance_->RegisterEventTiming(
      "pointerdown", pointerdown_timestamp, processing_start_pointerdown,
      processing_end_pointerdown, false, nullptr, key_code, pointer_id_1);
  SimulateSwapPromise(swap_time_pointerdown);
  // Second Pointerdown
  pointerdown_timestamp = GetTimeOrigin();
  processing_start_pointerdown = GetTimeStamp(1);
  processing_end_pointerdown = GetTimeStamp(2);
  swap_time_pointerdown = GetTimeStamp(6);
  absl::optional<PointerId> pointer_id_2 = 6;
  performance_->RegisterEventTiming(
      "pointerdown", pointerdown_timestamp, processing_start_pointerdown,
      processing_end_pointerdown, false, nullptr, key_code, pointer_id_2);
  SimulateSwapPromise(swap_time_pointerdown);

  // First Pointerup
  base::TimeTicks pointerup_timestamp = GetTimeStamp(3);
  base::TimeTicks processing_start_pointerup = GetTimeStamp(5);
  base::TimeTicks processing_end_pointerup = GetTimeStamp(6);
  base::TimeTicks swap_time_pointerup = GetTimeStamp(9);
  performance_->RegisterEventTiming(
      "pointerup", pointerup_timestamp, processing_start_pointerup,
      processing_end_pointerup, false, nullptr, key_code, pointer_id_2);
  SimulateSwapPromise(swap_time_pointerup);

  // Second Pointerup
  pointerup_timestamp = GetTimeStamp(5);
  processing_start_pointerup = GetTimeStamp(6);
  processing_end_pointerup = GetTimeStamp(7);
  swap_time_pointerup = GetTimeStamp(13);
  performance_->RegisterEventTiming(
      "pointerup", pointerup_timestamp, processing_start_pointerup,
      processing_end_pointerup, false, nullptr, key_code, pointer_id_1);
  SimulateSwapPromise(swap_time_pointerup);

  // Click
  base::TimeTicks click_timestamp = GetTimeStamp(13);
  base::TimeTicks processing_start_click = GetTimeStamp(15);
  base::TimeTicks processing_end_click = GetTimeStamp(16);
  base::TimeTicks swap_time_click = GetTimeStamp(20);
  performance_->RegisterEventTiming(
      "click", click_timestamp, processing_start_click, processing_end_click,
      false, nullptr, key_code, pointer_id_2);
  SimulateSwapPromise(swap_time_click);

  // Flush UKM logging mojo request.
  RunPendingTasks();

  // Check UKM recording.
  auto entries = GetUkmRecorder()->GetEntriesByName(
      ukm::builders::Responsiveness_UserInteraction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  GetUkmRecorder()->ExpectEntryMetric(
      entry,
      ukm::builders::Responsiveness_UserInteraction::kMaxEventDurationName, 7);
  GetUkmRecorder()->ExpectEntryMetric(
      entry,
      ukm::builders::Responsiveness_UserInteraction::kTotalEventDurationName,
      16);
  GetUkmRecorder()->ExpectEntryMetric(
      entry,
      ukm::builders::Responsiveness_UserInteraction::kInteractionTypeName, 1);
}

}  // namespace blink
