// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {
enum class Task { kGetContext, kSetIsDisplayed, kDidProcessTask };
}

class CanvasAccessibilityUkmMetricsTest
    : public PageTestBase,
      public testing::WithParamInterface<std::tuple<Task, Task, Task>> {
 public:
  CanvasAccessibilityUkmMetricsTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<body><canvas id='c'></canvas></body>");
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
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
  CanvasAccessibilityUkmMetricsValueTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        "<body><canvas id='c' width=300 height=300></canvas></body>");
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
    UpdateAllLifecyclePhasesForTest();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
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
      ->fillTextForTesting("Hello World", 10, 10);

  canvas_element_->SetIsDisplayed(true);
  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  canvas_element_->RenderingContext()->DidProcessTask(dummy_pending_task);

  // Verify that one record exists.
  auto entries = recorder_.GetEntriesByName(
      ukm::builders::Accessibility_Canvas::kEntryName);
  ASSERT_EQ(entries.size(), 1);
  auto* entry = entries[0].get();
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entry, ukm::builders::Accessibility_Canvas::kHasTextName, 1);
}

}  // namespace blink
