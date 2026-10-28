// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "ui/accessibility/accessibility_features.h"

namespace blink {

namespace {
enum class Task { kGetContext, kSetIsDisplayed, kDidProcessTask };

class TestEmptyEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};
}  // namespace

class CanvasAccessibilityUkmMetricsTest
    : public PageTestBase,
      public testing::WithParamInterface<std::tuple<Task, Task, Task>> {
 public:
  CanvasAccessibilityUkmMetricsTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<body><canvas id='c'></canvas></body>");
    canvas_element_ = To<HTMLCanvasElement>(GetElementById("c"));
    UpdateAllLifecyclePhasesForTest();
  }

  void PerformTasksAndVerifyResults() {
    ASSERT_TRUE(canvas_element_);
    CanvasContextCreationAttributesCore attributes;
    base::PendingTask dummy_pending_task1(FROM_HERE, base::OnceClosure());

    std::vector<Task> tasks = {std::get<0>(GetParam()), std::get<1>(GetParam()),
                               std::get<2>(GetParam())};
    for (Task task : tasks) {
      // No UKM should be recorded before all 3 tasks are done at least once.
      EXPECT_EQ(
          recorder_
              .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
              .size(),
          0);

      switch (task) {
        case Task::kGetContext:
          canvas_element_->GetCanvasRenderingContext(
              GetDocument().GetExecutionContext(), "2d", attributes);
          break;

        case Task::kSetIsDisplayed:
          canvas_element_->SetIsDisplayed(true);
          break;

        case Task::kDidProcessTask:
          canvas_element_->RenderingContext()->DidProcessTask(
              dummy_pending_task1);
      }
    }

    // Fast-forward time to let the scheduled UKM recording delay elapse.
    FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

    // Verify that one record exists.
    auto entries = recorder_.GetEntriesByName(
        ukm::builders::Accessibility_Canvas::kEntryName);
    ASSERT_EQ(entries.size(), 1);
    auto* entry = entries[0].get();
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
        static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, ukm::builders::Accessibility_Canvas::kIsOffscreenName, 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, ukm::builders::Accessibility_Canvas::kHasKeyboardListenerName,
        0);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, ukm::builders::Accessibility_Canvas::kHasTextName, 0);

    // Running additional `DidProcessTask` should not result in additional
    // records.
    base::PendingTask dummy_pending_task2(FROM_HERE, base::OnceClosure());
    canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task2);
    FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);
    EXPECT_EQ(
        recorder_
            .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
            .size(),
        1);
  }

 private:
  ukm::TestAutoSetUkmRecorder recorder_;
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_P(CanvasAccessibilityUkmMetricsTest, VerifyMetricRecord) {
  PerformTasksAndVerifyResults();
}

// Running `DidProcessTask` requires a context and hence only permutations
// in which `kDidProcessTask` comes after `kGetContext` are valid.
INSTANTIATE_TEST_SUITE_P(
    All,
    CanvasAccessibilityUkmMetricsTest,
    testing::Values(std::make_tuple(Task::kGetContext,
                                    Task::kSetIsDisplayed,
                                    Task::kDidProcessTask),
                    std::make_tuple(Task::kGetContext,
                                    Task::kDidProcessTask,
                                    Task::kSetIsDisplayed),
                    std::make_tuple(Task::kSetIsDisplayed,
                                    Task::kGetContext,
                                    Task::kDidProcessTask)));

class CanvasAccessibilityUkmMetricsValueTest : public PageTestBase {
 public:
  CanvasAccessibilityUkmMetricsValueTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<body><canvas id='c' width=300 height=300></canvas></body>");
    canvas_element_ = To<HTMLCanvasElement>(GetElementById("c"));
    UpdateAllLifecyclePhasesForTest();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
  }

  // Runs a callback in a posted task and runs pending tasks. Drawing inside a
  // task observer lifecycle ensures DidDraw registers the context observer
  // and DidProcessTask is invoked upon task completion, mirroring production.
  void RunInTask(base::OnceClosure callback) {
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, std::move(callback));
    test::RunPendingTasks();
  }

 protected:
  ukm::TestAutoSetUkmRecorder recorder_;
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_F(CanvasAccessibilityUkmMetricsValueTest, VerifyHasText) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_
      ->GetCanvasRenderingContext(GetDocument().GetExecutionContext(), "2d",
                                  attributes)
      ->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

  canvas_element_->SetIsDisplayed(true);
  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);
  // Drawing to canvas directly in a test body registers
  // CanvasPerformanceMonitor as a TaskTimeObserver outside an active message
  // loop task. Reset it before FastForwardBy() runs new tasks, otherwise the
  // task runner invokes CanvasPerformanceMonitor::WillProcessTask() and trips
  // NOTREACHED().
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  // No UKM should be recorded before the delay has elapsed.
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0);

  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  // Verify that one record exists.
  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasTextName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest, VerifyRetryWhenDisplayedLater) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);

  canvas_element_->SetIsDisplayed(false);
  EXPECT_FALSE(canvas_element_->IsDisplayed());

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);

  // Fast forward delay while canvas is not displayed.
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  // UKM should not be recorded since the canvas is not displayed, and the
  // recording schedule flag should be reset.
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0);

  // Later, canvas becomes displayed. This should reschedule the recording.
  canvas_element_->SetIsDisplayed(true);

  // Before delay elapses, still no record.
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0);

  // Fast forward delay again.
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  // UKM record should now exist.
  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyPointerListenerRecordedInUkm) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  canvas_element_->addEventListener(
      event_type_names::kPointerdown,
      MakeGarbageCollected<TestEmptyEventListener>());

  RunInTask(base::BindLambdaForTesting([&]() {
    canvas_element_->RenderingContext()->DidDraw(
        CanvasPerformanceMonitor::DrawType::kOther);
  }));
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyTouchListenerRecordedInUkm) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  canvas_element_->addEventListener(
      event_type_names::kTouchstart,
      MakeGarbageCollected<TestEmptyEventListener>());

  RunInTask(base::BindLambdaForTesting([&]() {
    canvas_element_->RenderingContext()->DidDraw(
        CanvasPerformanceMonitor::DrawType::kOther);
  }));
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyTouchMoveListenerRecordedInUkm) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  canvas_element_->addEventListener(
      event_type_names::kTouchmove,
      MakeGarbageCollected<TestEmptyEventListener>());

  RunInTask(base::BindLambdaForTesting([&]() {
    canvas_element_->RenderingContext()->DidDraw(
        CanvasPerformanceMonitor::DrawType::kOther);
  }));
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyPointerCancelListenerRecordedInUkm) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  canvas_element_->addEventListener(
      event_type_names::kPointercancel,
      MakeGarbageCollected<TestEmptyEventListener>());

  RunInTask(base::BindLambdaForTesting([&]() {
    canvas_element_->RenderingContext()->DidDraw(
        CanvasPerformanceMonitor::DrawType::kOther);
  }));
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyNeedsAccessibilitySupportDecoupledWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(::features::kAccessibilityCanvas);

  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  // Simulate an assistive technology session where the AX tree marked the
  // canvas as ignored.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  // With kAccessibilityCanvas disabled, GetNeedsAccessibilitySupportHeuristic()
  // should ignore last_ax_ignored_state_ and evaluate to true for displayed,
  // unlabeled canvases.
  EXPECT_TRUE(canvas_element_->GetNeedsAccessibilitySupportHeuristic());
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyNeedsAccessibilitySupportRespectedWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kAccessibilityCanvas);

  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  // Simulate an assistive technology session where the AX tree marked the
  // canvas as ignored.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  // With kAccessibilityCanvas enabled, true accessibility ignored state should
  // be respected.
  EXPECT_FALSE(canvas_element_->GetNeedsAccessibilitySupportHeuristic());
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyNeedsAccessibilitySupportRecordedInUkm) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {::features::kEnableCollectAccessibilityHeuristicInCanvasUkm},
      /*disabled_features=*/{::features::kAccessibilityCanvas});

  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  // Even if last_ax_ignored_state_ is true (simulating an AT session without
  // kAccessibilityCanvas), the heuristic correctly reports 1 for a displayed
  // canvas.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry,
      ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyOffscreenCanvasWithoutPlaceholderDoesNotRecord) {
  auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
      GetDocument().GetExecutionContext(), gfx::Size(300, 300), 0, 0,
      kInvalidDOMNodeId);
  ASSERT_TRUE(offscreen_canvas);
  EXPECT_FALSE(offscreen_canvas->HasPlaceholderCanvas());

  CanvasContextCreationAttributesCore attributes;
  auto* context = offscreen_canvas->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D, attributes);
  ASSERT_TRUE(context);
  context->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  context->DidProcessTask(dummy_pending_task);
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyOffscreenCanvasWithDestroyedPlaceholderFailsClosed) {
  DOMNodeId non_existent_node_id = 999999;
  auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
      GetDocument().GetExecutionContext(), gfx::Size(300, 300), 0, 0,
      non_existent_node_id);
  ASSERT_TRUE(offscreen_canvas);
  EXPECT_TRUE(offscreen_canvas->HasPlaceholderCanvas());

  CanvasContextCreationAttributesCore attributes;
  auto* context = offscreen_canvas->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D, attributes);
  ASSERT_TRUE(context);
  context->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  context->DidProcessTask(dummy_pending_task);
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyOffscreenCanvasWithPlaceholderRecordsMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kEnableCollectAccessibilityHeuristicInCanvasUkm);

  ASSERT_TRUE(canvas_element_);
  canvas_element_->addEventListener(
      event_type_names::kKeydown,
      MakeGarbageCollected<TestEmptyEventListener>());
  canvas_element_->addEventListener(
      event_type_names::kMousedown,
      MakeGarbageCollected<TestEmptyEventListener>());
  canvas_element_->SetIsDisplayed(true);

  DOMNodeId canvas_id = canvas_element_->GetDomNodeId();
  canvas_element_->RegisterPlaceholderCanvas(canvas_id);

  auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
      GetDocument().GetExecutionContext(), gfx::Size(300, 300), 0, 0,
      canvas_id);
  ASSERT_TRUE(offscreen_canvas);
  EXPECT_TRUE(offscreen_canvas->HasPlaceholderCanvas());

  CanvasContextCreationAttributesCore attributes;
  auto* context = offscreen_canvas->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D, attributes);
  ASSERT_TRUE(context);
  context->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  context->DidProcessTask(dummy_pending_task);
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kIsOffscreenName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasTextName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasKeyboardListenerName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry,
      ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyOffscreenCanvasWithPlaceholderRetryWhenDisplayedLater) {
  ASSERT_TRUE(canvas_element_);
  canvas_element_->SetIsDisplayed(false);
  EXPECT_FALSE(canvas_element_->IsDisplayed());

  DOMNodeId canvas_id = canvas_element_->GetDomNodeId();
  canvas_element_->RegisterPlaceholderCanvas(canvas_id);

  auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
      GetDocument().GetExecutionContext(), gfx::Size(300, 300), 0, 0,
      canvas_id);
  ASSERT_TRUE(offscreen_canvas);

  CanvasContextCreationAttributesCore attributes;
  auto* context = offscreen_canvas->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D, attributes);
  ASSERT_TRUE(context);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  context->DidProcessTask(dummy_pending_task);
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  // Fast forward delay while placeholder is not displayed.
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  // UKM should not be recorded since the placeholder is not displayed.
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);

  // Later, placeholder canvas becomes displayed.
  canvas_element_->SetIsDisplayed(true);

  // Render task ended / new frame drawn reschedules UKM recording.
  context->DidProcessTask(dummy_pending_task);
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  // UKM record should now exist.
  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kIsOffscreenName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyNon2DContextBitmapRendererRecordedInUkm) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "bitmaprenderer", attributes);
  ASSERT_TRUE(canvas_element_->RenderingContext());
  EXPECT_EQ(canvas_element_->RenderingContext()->GetRenderingAPI(),
            CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer);

  canvas_element_->SetIsDisplayed(true);
  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);

  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(
          CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasTextName, 0);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyDisposalFlushesPendingUkmBeforeDelayElapses) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);

  // Before delay elapses, no UKM should be recorded yet.
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);

  // Disposing the context flushes the pending UKM recording immediately
  // without waiting for the 5s timer.
  canvas_element_->RenderingContext()->DisposeForTesting();

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kIsOffscreenName, 0);

  // Fast forward past the 5-second delay to ensure no duplicate entry is emitted.
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      1u);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyDelayExactBoundary) {
  ASSERT_TRUE(canvas_element_);
  CanvasContextCreationAttributesCore attributes;
  canvas_element_->GetCanvasRenderingContext(
      GetDocument().GetExecutionContext(), "2d", attributes);
  canvas_element_->SetIsDisplayed(true);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);

  // At t < 5s (5s - 1ms = 4999ms), delay has not elapsed.
  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay -
                base::Milliseconds(1));
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);

  // Advancing by the final 1ms reaches exactly t = 5s and triggers recording.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      1u);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyZeroSizedCanvasRecordedInUkm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kEnableCollectAccessibilityHeuristicInCanvasUkm);

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><canvas id='c_zero' width=0 height=0></canvas></body>");
  auto* zero_canvas = To<HTMLCanvasElement>(
      GetDocument().getElementById(AtomicString("c_zero")));
  ASSERT_TRUE(zero_canvas);
  UpdateAllLifecyclePhasesForTest();

  CanvasContextCreationAttributesCore attributes;
  zero_canvas->GetCanvasRenderingContext(GetDocument().GetExecutionContext(),
                                         "2d", attributes);
  zero_canvas->SetIsDisplayed(true);

  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  zero_canvas->RenderingContext()->DidProcessTask(dummy_pending_task);

  FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasTextName, 0);
  // Zero-sized canvas (0x0 < 10x10) triggers IsTooSmall() == true, yielding
  // HeuristicResult::kTooSmall, which evaluates to NeedsAccessibilitySupport = 0.
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry,
      ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName, 0);
}

TEST_F(CanvasAccessibilityUkmMetricsValueTest,
       VerifyFeatureFlagControlsHeuristicMetricEmission) {
  // 1. When feature flag is disabled:
  {
    ASSERT_TRUE(canvas_element_);
    CanvasContextCreationAttributesCore attributes;
    canvas_element_->GetCanvasRenderingContext(
        GetDocument().GetExecutionContext(), "2d", attributes);
    canvas_element_->SetIsDisplayed(true);

    base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
    canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);

    FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

    auto entries = recorder_.GetEntriesByName(
        ukm::builders::Accessibility_Canvas::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    auto* entry = entries[0].get();
    // With feature flag disabled, kNeedsAccessibilitySupportName is NOT emitted.
    EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
        entry,
        ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName));
  }

  // 2. When feature flag is enabled:
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kEnableCollectAccessibilityHeuristicInCanvasUkm);

    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<body><canvas id='c_flag' width=300 height=300></canvas></body>");
    auto* flag_canvas = To<HTMLCanvasElement>(
        GetDocument().getElementById(AtomicString("c_flag")));
    ASSERT_TRUE(flag_canvas);
    UpdateAllLifecyclePhasesForTest();

    CanvasContextCreationAttributesCore attributes;
    flag_canvas->GetCanvasRenderingContext(
        GetDocument().GetExecutionContext(), "2d", attributes);
    flag_canvas->SetIsDisplayed(true);

    base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
    flag_canvas->RenderingContext()->DidProcessTask(dummy_pending_task);

    FastForwardBy(CanvasRenderingContext::kAccessibilityUkmRecordingDelay);

    auto entries = recorder_.GetEntriesByName(
        ukm::builders::Accessibility_Canvas::kEntryName);
    ASSERT_EQ(entries.size(), 2u);
    auto* entry2 = entries[1].get();
    // With feature flag enabled, kNeedsAccessibilitySupportName IS emitted.
    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
        entry2,
        ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName));
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry2,
        ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName, 1);
  }
}

class CanvasAccessibilityUkmMetricsWorkerTest : public DedicatedWorkerTest {
 public:
  CanvasAccessibilityUkmMetricsWorkerTest()
      : DedicatedWorkerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    DedicatedWorkerTest::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<body><canvas id='c' width=300 height=300></canvas></body>");
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
    UpdateAllLifecyclePhasesForTest();
  }

  void TearDown() override {
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
    DedicatedWorkerTest::TearDown();
  }

 protected:
  ukm::TestAutoSetUkmRecorder recorder_;
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_F(CanvasAccessibilityUkmMetricsWorkerTest,
       VerifyOffscreenCanvasOnWorkerWithPlaceholderRecordsMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kEnableCollectAccessibilityHeuristicInCanvasUkm);

  ASSERT_TRUE(canvas_element_);
  canvas_element_->addEventListener(
      event_type_names::kKeydown,
      MakeGarbageCollected<TestEmptyEventListener>());
  canvas_element_->addEventListener(
      event_type_names::kMousedown,
      MakeGarbageCollected<TestEmptyEventListener>());
  canvas_element_->SetIsDisplayed(true);

  DOMNodeId canvas_id = canvas_element_->GetDomNodeId();
  canvas_element_->RegisterPlaceholderCanvas(canvas_id);

  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(KURL("http://fake.url/"));
  auto params = GlobalScopeCreationParams::CreateForWorkerForTesting(
      security_origin.get(), KURL("http://fake.url/"));
  params->agent_group_scheduler_compositor_task_runner =
      GetDocument()
          .GetFrame()
          ->GetFrameScheduler()
          ->GetAgentGroupScheduler()
          ->CompositorTaskRunner();

  StartWorker(/*source_code=*/"", std::move(params));
  WaitUntilWorkerIsRunning();

  RunOnWorkerThread(CrossThreadBindOnce(
      [](DOMNodeId canvas_id, ExecutionContext* execution_context) {
        auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
            execution_context, gfx::Size(300, 300), 0, 0, canvas_id);
        EXPECT_TRUE(offscreen_canvas);
        if (!offscreen_canvas) {
          return;
        }
        EXPECT_TRUE(offscreen_canvas->HasPlaceholderCanvas());

        CanvasContextCreationAttributesCore attributes;
        auto* context = offscreen_canvas->GetCanvasRenderingContext(
            execution_context, CanvasRenderingContext::CanvasRenderingAPI::k2D,
            attributes);
        EXPECT_TRUE(context);
        if (!context) {
          return;
        }
        context->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

        base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
        context->DidProcessTask(dummy_pending_task);
        // Note: Invoke RecordUKMCanvasAccessibilityForTesting() directly
        // because DedicatedWorkerTest's main-thread mock time domain does not
        // advance the dedicated worker thread's internal message loop timer.
        context->RecordUKMCanvasAccessibilityForTesting();
      },
      canvas_id));

  // The worker thread posted RecordPlaceholderCanvasUKMOnMainThread to the
  // main thread compositor task runner. Running pending tasks on the main
  // thread processes the posted callback.
  test::RunPendingTasks();

  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kRenderingContextName,
      static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::k2D));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kIsOffscreenName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasTextName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasKeyboardListenerName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasMouseListenerName, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry,
      ukm::builders::Accessibility_Canvas::kNeedsAccessibilitySupportName, 1);
}

TEST_F(CanvasAccessibilityUkmMetricsWorkerTest,
       VerifyOffscreenCanvasOnWorkerWithoutPlaceholderDoesNotRecord) {
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(KURL("http://fake.url/"));
  auto params = GlobalScopeCreationParams::CreateForWorkerForTesting(
      security_origin.get(), KURL("http://fake.url/"));
  params->agent_group_scheduler_compositor_task_runner =
      GetDocument()
          .GetFrame()
          ->GetFrameScheduler()
          ->GetAgentGroupScheduler()
          ->CompositorTaskRunner();

  StartWorker(/*source_code=*/"", std::move(params));
  WaitUntilWorkerIsRunning();

  RunOnWorkerThread(CrossThreadBindOnce(
      [](ExecutionContext* execution_context) {
        auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
            execution_context, gfx::Size(300, 300), 0, 0,
            kInvalidDOMNodeId);
        EXPECT_TRUE(offscreen_canvas);
        if (!offscreen_canvas) {
          return;
        }
        EXPECT_FALSE(offscreen_canvas->HasPlaceholderCanvas());

        CanvasContextCreationAttributesCore attributes;
        auto* context = offscreen_canvas->GetCanvasRenderingContext(
            execution_context, CanvasRenderingContext::CanvasRenderingAPI::k2D,
            attributes);
        EXPECT_TRUE(context);
        if (!context) {
          return;
        }
        context->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

        base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
        context->DidProcessTask(dummy_pending_task);
        context->RecordUKMCanvasAccessibilityForTesting();
      }));

  test::RunPendingTasks();

  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);
}

TEST_F(CanvasAccessibilityUkmMetricsWorkerTest,
       VerifyOffscreenCanvasOnWorkerWithUndisplayedPlaceholderDoesNotRecord) {
  ASSERT_TRUE(canvas_element_);
  canvas_element_->SetIsDisplayed(false);

  DOMNodeId canvas_id = canvas_element_->GetDomNodeId();
  canvas_element_->RegisterPlaceholderCanvas(canvas_id);

  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(KURL("http://fake.url/"));
  auto params = GlobalScopeCreationParams::CreateForWorkerForTesting(
      security_origin.get(), KURL("http://fake.url/"));
  params->agent_group_scheduler_compositor_task_runner =
      GetDocument()
          .GetFrame()
          ->GetFrameScheduler()
          ->GetAgentGroupScheduler()
          ->CompositorTaskRunner();

  StartWorker(/*source_code=*/"", std::move(params));
  WaitUntilWorkerIsRunning();

  RunOnWorkerThread(CrossThreadBindOnce(
      [](DOMNodeId canvas_id, ExecutionContext* execution_context) {
        auto* offscreen_canvas = MakeGarbageCollected<OffscreenCanvas>(
            execution_context, gfx::Size(300, 300), 0, 0, canvas_id);
        EXPECT_TRUE(offscreen_canvas);
        if (!offscreen_canvas) {
          return;
        }
        EXPECT_TRUE(offscreen_canvas->HasPlaceholderCanvas());

        CanvasContextCreationAttributesCore attributes;
        auto* context = offscreen_canvas->GetCanvasRenderingContext(
            execution_context, CanvasRenderingContext::CanvasRenderingAPI::k2D,
            attributes);
        EXPECT_TRUE(context);
        if (!context) {
          return;
        }
        context->DidDraw(CanvasPerformanceMonitor::DrawType::kText);

        base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
        context->DidProcessTask(dummy_pending_task);
        context->RecordUKMCanvasAccessibilityForTesting();
      },
      canvas_id));

  test::RunPendingTasks();

  EXPECT_EQ(
      recorder_
          .GetEntriesByName(ukm::builders::Accessibility_Canvas::kEntryName)
          .size(),
      0u);
}

}  // namespace blink
