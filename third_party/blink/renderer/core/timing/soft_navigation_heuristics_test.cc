// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <memory>

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics_test_util.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using TaskScope = scheduler::TaskAttributionTracker::TaskScope;

class RecordingLocalFrameClient : public EmptyLocalFrameClient {
 public:
  void DidObserveSoftNavigation(
      SoftNavigationMetricsForReporting metrics) override {
    ++soft_navigation_count_;
  }

  int SoftNavigationCount() const { return soft_navigation_count_; }

 private:
  int soft_navigation_count_ = 0;
};

class SoftNavigationHeuristicsTest : public testing::Test {
 protected:
  void SetUp() override {
    local_frame_client_ = MakeGarbageCollected<RecordingLocalFrameClient>();
    page_holder_ = std::make_unique<DummyPageHolder>(
        gfx::Size(800, 600), /*chrome_client=*/nullptr, local_frame_client_);
    task_environment_.AdvanceClock(base::Seconds(1));
  }

  SoftNavigationHeuristics* CreateSoftNavigationHeuristicsForTest() {
    return page_holder_->GetDocument()
        .domWindow()
        ->GetSoftNavigationHeuristics();
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  int ObservedSoftNavigationCount() const {
    return local_frame_client_->SoftNavigationCount();
  }

  Node* CreateNodeForTest() {
    return GetDocument().CreateRawElement(html_names::kDivTag);
  }

  v8::Isolate* GetIsolate() {
    return GetDocument().GetExecutionContext()->GetIsolate();
  }

  ScriptState* GetScriptStateForTest() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  std::optional<scheduler::TaskAttributionTracker::TaskScope>
  CreateTaskScopeForEvent(SoftNavigationHeuristics* heuristics,
                          const AtomicString& event_type) {
    auto* entry = blink::CreatePerformanceEventTimingForTest(
        event_type, base::TimeTicks::Now(), GetDocument().body(),
        GetDocument().domWindow());
    return heuristics->MaybeCreateTaskScopeForEvent(entry);
  }

  void TriggerSoftNavigationWithStartTime(SoftNavigationHeuristics* heuristics,
                                          base::TimeTicks start_time,
                                          uint64_t interaction_id) {
    auto* tracker = scheduler::TaskAttributionTracker::From(
        GetScriptStateForTest()->GetIsolate());
    ASSERT_TRUE(tracker);

    auto* entry = blink::CreatePerformanceEventTimingForTest(
        event_type_names::kClick, start_time, GetDocument().body(),
        GetDocument().domWindow());
    entry->SetInteractionIdInfo(
        PerformanceTimelineEntryIdInfo(interaction_id, interaction_id));

    SoftNavigationContext* context = nullptr;
    Node* node = CreateNodeForTest();
    {
      auto event_scope = heuristics->MaybeCreateTaskScopeForEvent(entry);
      context = tracker->CurrentTaskState()->GetSoftNavigationContext();
      ASSERT_TRUE(context);

      String new_url =
          String("http://foo.com/#nav") + String::Number(interaction_id);
      heuristics->SameDocumentNavigationCommitted(
          KURL("http://foo.com/"), KURL(new_url), WebFrameLoadType::kStandard,
          base::UnguessableToken::Create(), context->GetInteractionIdInfo());
      heuristics->ModifiedDOM(node);
    }

    heuristics->SetContextForTest(context, node);
    TextRecord* record = CreateTextRecordForTest(node, 1000, 1000);
    heuristics->OnPaintFinished({}, {record});
    heuristics->OnFramePresented({}, {record}, base::TimeTicks::Now(),
                                 DOMPaintTimingInfo(), /*navigation_id=*/1);
  }

 private:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  Persistent<RecordingLocalFrameClient> local_frame_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

// TODO(crbug.com/1503284): This test validates that the renderer does not crash
// when presented with an unset timestamp. Figure out whether it is possible to
// void ever calling InteractionCallbackCalled in that situation instead.
TEST_F(SoftNavigationHeuristicsTest,
       EarlyReturnOnInvalidPendingInteractionTimestamp) {
  auto* test_heuristics = CreateSoftNavigationHeuristicsForTest();
  // A non-new interaction will try to use the pending timestamp, which will
  // never have been set in this case.
  auto event_scope =
      CreateTaskScopeForEvent(test_heuristics, event_type_names::kKeypress);
}

TEST_F(SoftNavigationHeuristicsTest, ResetHeuristicOnSetBecameEmpty) {
  base::HistogramTester histogram_tester;

  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  ASSERT_TRUE(heuristics);

  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  Persistent<scheduler::TaskAttributionInfo> root_task_state = nullptr;
  // Simulate a click.
  {
    EXPECT_FALSE(heuristics->IsTrackingSoftNavigationsForTest());
    auto task_scope =
        CreateTaskScopeForEvent(heuristics, event_type_names::kClick);
    root_task_state = tracker->CurrentTaskState();

    // Set the URL so a histogram entry gets logged. Do this directly through
    // the context to avoid other objects holding a reference to it, which would
    // prevent garbage collection.
    root_task_state->GetSoftNavigationContext()->AddUrl(
        "foo", V8NavigationType::Enum::kPush, base::UnguessableToken::Create());
  }
  EXPECT_TRUE(root_task_state);
  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());

  // Simulate a descendant task.
  Persistent<scheduler::TaskAttributionInfo> descendant_task_state = nullptr;
  {
    std::optional<TaskScope> task_scope =
        tracker->SetCurrentTaskStateIfTopLevel(root_task_state,
                                               TaskScopeType::kCallback);
    descendant_task_state = tracker->CurrentTaskState();
  }
  EXPECT_TRUE(descendant_task_state);

  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());
  EXPECT_EQ(root_task_state.Get(), descendant_task_state.Get());

  root_task_state = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  // The heuristics still should not have been reset since there is a live
  // root task, which is being held onto by its descendant task.
  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());
  histogram_tester.ExpectTotalCount("PageLoad.Internal.SoftNavigationOutcome",
                                    0);

  // Finally, this should allow the click task to be GCed, which should cause
  // the heuristics to be reset.
  descendant_task_state = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(heuristics->IsTrackingSoftNavigationsForTest());
  histogram_tester.ExpectTotalCount("PageLoad.Internal.SoftNavigationOutcome",
                                    1);
}

TEST_F(SoftNavigationHeuristicsTest, NestedEventScopesAreMerged) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto outer_event_scope =
      CreateTaskScopeForEvent(heuristics, event_type_names::kClick);
  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  SoftNavigationContext* context1 =
      tracker->CurrentTaskState()->GetSoftNavigationContext();
  EXPECT_TRUE(context1);

  auto inner_event_scope =
      CreateTaskScopeForEvent(heuristics, event_type_names::kNavigate);

  SoftNavigationContext* context2 =
      tracker->CurrentTaskState()->GetSoftNavigationContext();
  EXPECT_TRUE(context2);

  EXPECT_EQ(context1, context2);
}

TEST_F(SoftNavigationHeuristicsTest, EventAfterSoftNavDetection) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto outer_event_scope =
      CreateTaskScopeForEvent(heuristics, event_type_names::kClick);
  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  heuristics->ModifiedDOM(CreateNodeForTest());

  heuristics->SameDocumentNavigationCommitted(
      KURL("http://foo.com/"), KURL("http://foo.com/#foo"),
      WebFrameLoadType::kStandard,
      /*same_document_metrics_token=*/base::UnguessableToken::Create(),
      PerformanceTimelineEntryIdInfo::kNone);
  {
    auto inner_event_scope =
        CreateTaskScopeForEvent(heuristics, event_type_names::kNavigate);
  }
}

TEST_F(SoftNavigationHeuristicsTest,
       HeuristicNotResetDuringGCWithActiveContext) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  {
    auto event_scope =
        CreateTaskScopeForEvent(heuristics, event_type_names::kClick);
  }
  // At this point there is a single `SoftNavigationContext` being tracked, but
  // it wasn't propagated anywhere, so it is eligible for GC.
  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());

  auto event_scope =
      CreateTaskScopeForEvent(heuristics, event_type_names::kClick);

  // If GC occurs here, e.g. during a blink allocation, the heuristic should not
  // be reset, otherwise the `SoftNavigationContext` created above will be
  // cleared.
  ThreadState::Current()->CollectAllGarbageForTesting(
      cppgc::EmbedderStackState::kMayContainHeapPointers);

  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());
}

TEST_F(SoftNavigationHeuristicsTest, SoftNavigationEmittedOnlyOnce) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  scheduler::TaskAttributionInfo* task_state = nullptr;
  SoftNavigationContext* context = nullptr;

  Node* node1 = CreateNodeForTest();
  Node* node2 = CreateNodeForTest();

  // Simulate an event listener that starts a soft-nav
  {
    auto event_scope =
        CreateTaskScopeForEvent(heuristics, event_type_names::kClick);
    task_state = tracker->CurrentTaskState();
    ASSERT_TRUE(task_state);
    context = task_state->GetSoftNavigationContext();
    ASSERT_TRUE(context);

    EXPECT_FALSE(context->TimeOrigin().is_null());
    EXPECT_TRUE(context->UrlChangeTime().is_null());

    EXPECT_FALSE(context->SatisfiesSoftNavNonPaintCriteria());
    heuristics->SameDocumentNavigationCommitted(
        KURL("http://foo.com/"), KURL("http://foo.com/#foo"),
        WebFrameLoadType::kStandard,
        /*same_document_metrics_token=*/base::UnguessableToken::Create(),
        context->GetInteractionIdInfo());
    heuristics->ModifiedDOM(node1);
    EXPECT_TRUE(context->SatisfiesSoftNavNonPaintCriteria());

    EXPECT_FALSE(context->UrlChangeTime().is_null());
  }
  // The interaction processing has now ended.
  EXPECT_TRUE(context->SatisfiesSoftNavNonPaintCriteria());
  EXPECT_FALSE(context->SatisfiesSoftNavPaintCriteria(1));

  // Simulate a paint in a separate task.
  {
    heuristics->SetContextForTest(context, node1);
    TextRecord* record = CreateTextRecordForTest(node1, 100, 100);
    heuristics->OnPaintFinished({}, {record});
    heuristics->OnFramePresented({}, {record}, base::TimeTicks::Now(),
                                 DOMPaintTimingInfo(), /*navigation_id=*/1);
    EXPECT_TRUE(context->SatisfiesSoftNavPaintCriteria(1));
    EXPECT_TRUE(context->HasFirstContentfulPaint());
    EXPECT_EQ(heuristics->SoftNavigationCount(), 1u);
  }

  // Simulate another task for the same context, which does a second soft-nav
  {
    std::optional<TaskScope> task_scope =
        tracker->SetCurrentTaskStateIfTopLevel(task_state,
                                               TaskScopeType::kCallback);
    EXPECT_EQ(tracker->CurrentTaskState()->GetSoftNavigationContext(), context);
    heuristics->SameDocumentNavigationCommitted(
        KURL("http://foo.com/"), KURL("http://foo.com/#bar"),
        WebFrameLoadType::kStandard,
        /*same_document_metrics_token=*/base::UnguessableToken::Create(),
        context->GetInteractionIdInfo());
    heuristics->ModifiedDOM(node2);
  }

  // And another paint
  {
    heuristics->SetContextForTest(context, node2);
    TextRecord* record = CreateTextRecordForTest(node2, 100, 200);
    heuristics->OnPaintFinished({}, {record});
    heuristics->OnFramePresented({}, {record}, base::TimeTicks::Now(),
                                 DOMPaintTimingInfo(), /*navigation_id=*/1);
    EXPECT_TRUE(context->SatisfiesSoftNavPaintCriteria(1));
    // Should still just have one single soft-nav because a single context
    // with a single Interaction should only emit once, even if it e.g.
    // navigates twice (i.e. client-side redirects).
    EXPECT_EQ(heuristics->SoftNavigationCount(), 1u);
  }
}

TEST_F(SoftNavigationHeuristicsTest, AsyncSameDocumentNavigation) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  scheduler::TaskAttributionInfo* task_state = nullptr;
  SoftNavigationContext* context = nullptr;

  {
    auto event_scope =
        CreateTaskScopeForEvent(heuristics, event_type_names::kClick);
    task_state = tracker->CurrentTaskState();
    ASSERT_TRUE(task_state);
    context = task_state->GetSoftNavigationContext();
    ASSERT_TRUE(context);
  }
  EXPECT_FALSE(context->TimeOrigin().is_null());
  EXPECT_TRUE(context->UrlChangeTime().is_null());

  // Simulate starting a same-document navigation in a JavaScript task
  // associated with `context`.
  std::optional<scheduler::TaskAttributionId> navigation_task_id;
  {
    std::optional<TaskScope> task_scope =
        tracker->SetCurrentTaskStateIfTopLevel(task_state,
                                               TaskScopeType::kCallback);
    navigation_task_id = tracker->AsyncSameDocumentNavigationStarted();
  }
  ASSERT_TRUE(navigation_task_id);

  EXPECT_FALSE(context->HasUrl());

  // Simulate committing the same-document navigation asynchronously.
  {
    task_state = tracker->CommitSameDocumentNavigation(*navigation_task_id);
    ASSERT_TRUE(task_state);
    EXPECT_EQ(task_state->GetSoftNavigationContext(), context);
    std::optional<scheduler::TaskAttributionTracker::TaskScope> task_scope(
        tracker->SetCurrentTaskStateIfTopLevel(task_state,
                                               TaskScopeType::kPopState));
    heuristics->SameDocumentNavigationCommitted(
        KURL("http://foo.com/"), KURL("http://foo.com/#foo"),
        WebFrameLoadType::kStandard,
        /*same_document_metrics_token=*/base::UnguessableToken::Create(),
        context->GetInteractionIdInfo());
    EXPECT_TRUE(context->HasUrl());
    EXPECT_FALSE(context->UrlChangeTime().is_null());
  }
}

TEST_F(SoftNavigationHeuristicsTest, AsyncSameDocumentNavigationNoContext) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  // Simulate starting a same-document navigation in a JavaScript task that
  // isn't associated with a `SoftNavigationContext`
  std::optional<scheduler::TaskAttributionId> navigation_task_id =
      tracker->AsyncSameDocumentNavigationStarted();
  EXPECT_FALSE(navigation_task_id);

  // Simulate committing the same-document navigation asynchronously without a
  // `SoftNavigationContext`. This shouldn't crash.
  heuristics->SameDocumentNavigationCommitted(
      KURL("http://foo.com/"), KURL("http://foo.com/#foo"),
      WebFrameLoadType::kStandard,
      /*same_document_metrics_token=*/base::UnguessableToken::Create(),
      PerformanceTimelineEntryIdInfo::kNone);
}

// Regression test for crbug.com/585428860.
TEST_F(SoftNavigationHeuristicsTest, EarlyReturnOnInvalidTimeOrigin) {
  base::HistogramTester histogram_tester;
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  const base::TimeTicks ref_time =
      GetDocument().Loader()->GetTiming().ReferenceMonotonicTime();

  // TimeOrigin < ReferenceMonotonicTime should early-return without crashing.
  TriggerSoftNavigationWithStartTime(heuristics,
                                     ref_time - base::Milliseconds(10), 2);
  // TimeOrigin == ReferenceMonotonicTime should also early-return.
  TriggerSoftNavigationWithStartTime(heuristics, ref_time, 3);
  EXPECT_EQ(ObservedSoftNavigationCount(), 0);
  constexpr int kInvalidTimeOrigin = 16;
  histogram_tester.ExpectUniqueSample("PageLoad.Internal.SoftNavigationOutcome",
                                      kInvalidTimeOrigin, 2);

  // Subsequent normal TimeOrigin > ReferenceMonotonicTime should report.
  TriggerSoftNavigationWithStartTime(heuristics,
                                     ref_time + base::Milliseconds(10), 4);
  EXPECT_EQ(ObservedSoftNavigationCount(), 1);
  constexpr int kSoftNavigationDetected = 0;
  histogram_tester.ExpectBucketCount("PageLoad.Internal.SoftNavigationOutcome",
                                     kSoftNavigationDetected, 1);
}

}  // namespace blink
