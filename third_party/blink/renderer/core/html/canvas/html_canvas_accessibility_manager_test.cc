// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_accessibility_manager.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

class HTMLCanvasAccessibilityManagerTest : public PageTestBase {
 public:
  HTMLCanvasAccessibilityManagerTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUpCanvas(const char* html_content) {
    GetDocument().GetSettings()->SetScriptEnabled(true);
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        html_content);
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
    UpdateAllLifecyclePhasesForTest();
  }

  void WaitForAccessibilityManagerUpdate() {
    HTMLCanvasAccessibilityManager* manager =
        canvas_element_->GetAccessibilityManagerForTesting();
    ASSERT_TRUE(manager);
    manager->FlushUmaIfNeeded();
  }

 protected:
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_F(HTMLCanvasAccessibilityManagerTest, NoAccessibilityService) {
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  EXPECT_EQ(manager, nullptr);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, IsIgnored) {
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  EXPECT_EQ(manager, nullptr);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, AriaHiddenIsIgnored) {
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 "
      "aria-hidden='true'></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  EXPECT_FALSE(manager);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, TooSmall) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=5 height=5></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kTooSmall, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasLayoutSubtree) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 layoutsubtree></"
      "canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasLayoutSubtree, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasNonElementFallbackContent) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200>Comment</"
      "canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasFallbackContent) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200><button>Click</button></"
      "canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasFallbackContent, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasAriaRole) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 role='img'></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasAriaAttributes, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasAriaLabel) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 "
      "aria-label='chart'></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasAriaAttributes, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, NeedsA11ySupport) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, DynamicAriaAttributeAdded) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);

  // Dynamically add an aria attribute.
  canvas_element_->setAttribute(html_names::kAriaLabelAttr,
                                AtomicString("chart"));
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(
      canvas_element_->GetAccessibilityManagerForTesting()
          ->GetHeuristicResultForTesting(),
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasAriaAttributes);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, IgnoredStateChanged) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);

  // Simulate AXObject notifying the canvas that its ignored state changed.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kIsIgnored);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, DynamicFallbackContentAdded) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);

  // Dynamically add fallback element content.
  auto* button = GetDocument().CreateRawElement(html_names::kButtonTag);
  canvas_element_->AppendChild(button);
  UpdateAllLifecyclePhasesForTest();
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(
      canvas_element_->GetAccessibilityManagerForTesting()
          ->GetHeuristicResultForTesting(),
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasFallbackContent);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, InitiallyIgnoredBecomesVisible) {
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);
  EXPECT_FALSE(canvas_element_->GetAccessibilityManagerForTesting());

  // Simulate AXObject notifying the canvas that it is no longer ignored.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_TRUE(canvas_element_->GetAccessibilityManagerForTesting());
  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);
}

class HTMLCanvasAccessibilityManagerCaptureTest
    : public HTMLCanvasAccessibilityManagerTest {
 public:
  void SetUp() override {
    HTMLCanvasAccessibilityManagerTest::SetUp();
    feature_list_.InitAndEnableFeature(::features::kAccessibilityCanvas);
    SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
    canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
    WaitForAccessibilityManagerUpdate();

    manager_ = canvas_element_->GetAccessibilityManagerForTesting();
    ASSERT_TRUE(manager_);
    ASSERT_TRUE(manager_->ShouldCaptureRenderedTextForTesting());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  Persistent<HTMLCanvasAccessibilityManager> manager_;
};

TEST_F(HTMLCanvasAccessibilityManagerCaptureTest,
       RecordTextRunsVerticalSorting) {
  // "World" is lower (larger Y) so it should come second.
  manager_->RecordRenderedText("World", gfx::RectF(0, 50, 100, 20), 12.0f);
  // "Hello" is higher (smaller Y) so it should come first.
  manager_->RecordRenderedText("Hello", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  EXPECT_EQ(manager_->CanvasAnnotation(), "Hello World");
}

TEST_F(HTMLCanvasAccessibilityManagerCaptureTest,
       RecordTextRunsHorizontalSorting) {
  manager_->RecordRenderedText("World", gfx::RectF(0, 50, 100, 20), 12.0f);
  // "Again" is to the right of "World" (bounds.x() = 110) on the same line.
  manager_->RecordRenderedText("Again", gfx::RectF(110, 50, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  EXPECT_EQ(manager_->CanvasAnnotation(), "World Again");
}

TEST_F(HTMLCanvasAccessibilityManagerCaptureTest, RecordTextRunsOverwrite) {
  manager_->RecordRenderedText("Hello", gfx::RectF(0, 0, 100, 20), 12.0f);
  // Recording text in a sufficiently overlapping area should overwrite the
  // existing run.
  manager_->RecordRenderedText("Hi", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  EXPECT_EQ(manager_->CanvasAnnotation(), "Hi");
}

TEST_F(HTMLCanvasAccessibilityManagerCaptureTest, RecordTextRunsClearRegion) {
  manager_->RecordRenderedText("Hi", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager_->RecordRenderedText("World", gfx::RectF(0, 50, 100, 20), 12.0f);
  manager_->RecordRenderedText("Again", gfx::RectF(110, 50, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  ASSERT_EQ(manager_->CanvasAnnotation(), "Hi World Again");

  // This rect intersects mainly with the "World" run (at Y=50).
  manager_->ClearRenderedText(gfx::RectF(-10, 40, 120, 30));
  manager_->UpdateAnnotation();
  EXPECT_EQ(manager_->CanvasAnnotation(), "Hi Again");
}

TEST_F(HTMLCanvasAccessibilityManagerCaptureTest, RecordTextRunsFullClear) {
  manager_->RecordRenderedText("Hi", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager_->RecordRenderedText("Again", gfx::RectF(110, 50, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  ASSERT_EQ(manager_->CanvasAnnotation(), "Hi Again");

  manager_->ClearRenderedText();
  manager_->UpdateAnnotation();
  EXPECT_TRUE(manager_->CanvasAnnotation().empty());
}

TEST_F(HTMLCanvasAccessibilityManagerTest,
       EnsureAccessibilityManagerEarlyDrawing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(::features::kAccessibilityCanvas);

  // Enable accessibility complete mode.
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);
  ASSERT_TRUE(GetDocument().ExistingAXObjectCache());

  // Set inner HTML, but do not force layout setup.
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_ =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));

  // Verify that the layout object is indeed null.
  ASSERT_FALSE(canvas_element_->GetLayoutObject());

  // Initially accessibility_manager_ should not be initialized because it is
  // created lazily.
  EXPECT_FALSE(canvas_element_->GetAccessibilityManagerForTesting());

  // Triggering text recording should internally call
  // EnsureAccessibilityManager.
  canvas_element_->RecordRenderedText("Hello", gfx::RectF(0, 0, 100, 20),
                                      12.0f);

  // accessibility_manager_ should now be lazily initialized.
  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  ASSERT_TRUE(manager);

  // Because layout size is 0 (as layout object is null but physical canvas size
  // is 300x200), IsTooSmall() should result in heuristic result of kTooSmall.
  // But since the manager is initialized in text collection mode, text capture
  // should be enabled.
  EXPECT_TRUE(manager->ShouldCaptureRenderedTextForTesting());

  // Verify that the annotation was stored in the manager.
  manager->UpdateAnnotation();
  EXPECT_EQ(manager->CanvasAnnotation(), "Hello");

  // Since we haven't received AXObject ignored state yet, it shouldn't be sent
  // downstream.
  EXPECT_FALSE(manager->NeedsA11ySupport());
  EXPECT_TRUE(canvas_element_->CanvasAnnotation().empty());

  // Force layout update to assign a layout object and size before the AXObject
  // state known transition.
  UpdateAllLifecyclePhasesForTest();

  // Simulating AXObject notifying whether the canvas is ignored. Now the
  // judgment is trusted.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);

  // The manager object should be the same, and now it should be initialized.
  CHECK_EQ(manager, canvas_element_->GetAccessibilityManagerForTesting());
  EXPECT_TRUE(manager->NeedsA11ySupport());

  // Now, the annotation is sent downstream!
  EXPECT_EQ(canvas_element_->CanvasAnnotation(), "Hello");

  // If the canvas becomes ignored, the manager updates its heuristic to
  // kIsIgnored and clears capture.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);
  EXPECT_EQ(manager->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kIsIgnored);
  EXPECT_TRUE(canvas_element_->CanvasAnnotation().empty());
}

class HTMLCanvasAccessibilityManagerOCRTest
    : public HTMLCanvasAccessibilityManagerTest {
 public:
  void SetUp() override {
    HTMLCanvasAccessibilityManagerTest::SetUp();
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kAccessibilityCanvas,
        {{"CanvasAccessibilityMode", "Advanced"}});
    ax_context_ =
        std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);
    SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
    canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
    WaitForAccessibilityManagerUpdate();

    manager_ = canvas_element_->GetAccessibilityManagerForTesting();
    ASSERT_TRUE(manager_);
    ASSERT_TRUE(manager_->NeedsA11ySupport());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AXContext> ax_context_;
  Persistent<HTMLCanvasAccessibilityManager> manager_;
};

TEST_F(HTMLCanvasAccessibilityManagerOCRTest, OCRTriggerFromEmptyTextRuns) {
  // Initially, canvas_annotation_ is empty, which meets the "no text extracted"
  // criteria for ShouldRunOCR(). So UpdateAnnotation() should start the OCR
  // timer.
  manager_->UpdateAnnotation();
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest, OCRNoTriggerWithValidTextRuns) {
  // If we record some text runs that are valid and non-empty (so
  // canvas_annotation_ is neither empty nor too long), ShouldRunOCR() should be
  // false, and the timer should stop.
  manager_->RecordRenderedText("Hello", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  EXPECT_FALSE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest, OCRTriggerWithTooManyTextRuns) {
  // If too much text is extracted (exceeds kMaxTextRuns), the OCR timer
  // should be active again.
  for (size_t i = 0; i < HTMLCanvasAccessibilityManager::kMaxTextRuns + 1;
       ++i) {
    manager_->RecordRenderedText(
        "A", gfx::RectF(0, static_cast<float>(i * 50), 100, 20), 12.0f);
  }
  manager_->UpdateAnnotation();
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest, OCRTriggerOnFirstCanvasDrawing) {
  // OCR timer is active from initialization. Stop it by recording a valid text
  // run.
  manager_->RecordRenderedText("Hello", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager_->UpdateAnnotation();
  ASSERT_FALSE(manager_->IsOCRTimerActiveForTesting());

  // Clear text runs (simulating canvas reset or redrawing).
  manager_->ClearRenderedText();

  // First draw / frame finalization: triggers the OCR timer.
  canvas_element_->PostFinalizeFrame(FlushReason::kOther);
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest,
       OCRRescheduleOnSubsequentCanvasDrawing) {
  ASSERT_LT(HTMLCanvasAccessibilityManager::kOCRDelay,
            HTMLCanvasAccessibilityManager::kMaxOCRDelay);

  // Trigger initial OCR timer scheduling.
  canvas_element_->PostFinalizeFrame(FlushReason::kOther);
  ASSERT_TRUE(manager_->IsOCRTimerActiveForTesting());

  const base::TimeDelta kRescheduleDelay =
      HTMLCanvasAccessibilityManager::kOCRDelay / 2;

  // Fast forward by reschedule interval.
  FastForwardBy(kRescheduleDelay);
  ASSERT_TRUE(manager_->IsOCRTimerActiveForTesting());

  // Second draw reschedules the timer.
  canvas_element_->PostFinalizeFrame(FlushReason::kOther);
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());

  // Fast forward a little before another complete OCR delay.
  FastForwardBy(HTMLCanvasAccessibilityManager::kOCRDelay -
                HTMLCanvasAccessibilityManager::kOCRDelay / 10);
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest, OCRFiresAfterCanvasDrawingDelay) {
  // Trigger initial OCR timer scheduling.
  canvas_element_->PostFinalizeFrame(FlushReason::kOther);
  ASSERT_TRUE(manager_->IsOCRTimerActiveForTesting());

  // Fast forward past the OCR delay.
  FastForwardBy(HTMLCanvasAccessibilityManager::kOCRDelay +
                HTMLCanvasAccessibilityManager::kOCRDelay / 10);

  // The timer must have fired and become inactive.
  EXPECT_FALSE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest,
       OCRTriggerFromContinuousCanvasDrawing) {
  // First draw / frame finalization.
  canvas_element_->PostFinalizeFrame(FlushReason::kOther);
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());

  const base::TimeDelta kDrawInterval =
      HTMLCanvasAccessibilityManager::kOCRDelay / 10;
  base::TimeDelta total_draw_time = base::TimeDelta();

  // Continuously draw at small intervals and check that OCR is still scheduled
  // until the total drawn time is above the threshold.
  while (total_draw_time < HTMLCanvasAccessibilityManager::kMaxOCRDelay) {
    FastForwardBy(kDrawInterval);
    canvas_element_->PostFinalizeFrame(FlushReason::kOther);
    EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());
    total_draw_time += kDrawInterval;
  }

  // At this point, total_draw_time is >= kMaxOCRDelay.
  // The last slot where we rescheduled was at (total_draw_time -
  // kDrawInterval). So the timer is scheduled to run at: (total_draw_time -
  // kDrawInterval) + HTMLCanvasAccessibilityManager::kOCRDelay
  //
  // Since we are currently at total_draw_time, the remaining time until the
  // timer fires is exactly: HTMLCanvasAccessibilityManager::kOCRDelay -
  // kDrawInterval
  //
  // For safety, fast forward by a duration slightly less than this remaining
  // time and check that the timer is still active.
  const base::TimeDelta time_before_fire =
      HTMLCanvasAccessibilityManager::kOCRDelay - kDrawInterval - kDrawInterval;
  FastForwardBy(time_before_fire);
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());

  // Fast forward past the fire time and check that it has fired (inactive).
  FastForwardBy(kDrawInterval * 2);
  EXPECT_FALSE(manager_->IsOCRTimerActiveForTesting());
}

TEST_F(HTMLCanvasAccessibilityManagerOCRTest, OCRNeededSignalPropagation) {
  // Verify that canvas_element_->HasRequestedOCR() is initially false.
  EXPECT_FALSE(canvas_element_->HasRequestedOCR());

  // Call PostFinalizeFrame to trigger ocr_timer_ scheduling (since text_runs_
  // is empty).
  canvas_element_->PostFinalizeFrame(FlushReason::kOther);
  EXPECT_TRUE(manager_->IsOCRTimerActiveForTesting());
  EXPECT_FALSE(canvas_element_->HasRequestedOCR());

  // Fast forward by the OCR delay to let the timer fire.
  FastForwardBy(HTMLCanvasAccessibilityManager::kOCRDelay);
  EXPECT_FALSE(manager_->IsOCRTimerActiveForTesting());

  // Verify that canvas_element_->HasRequestedOCR() becomes true.
  EXPECT_TRUE(canvas_element_->HasRequestedOCR());

  // Access the WebAXObject for the canvas element.
  WebAXObject web_ax_object = WebAXObject::FromWebNode(canvas_element_.Get());
  ASSERT_FALSE(web_ax_object.IsNull());

  // Verify web_ax_object.HasRequestedOCR() is true.
  EXPECT_TRUE(web_ax_object.HasRequestedOCR());

  // Reset it back to false by calling web_ax_object.ClearHasRequestedOCR().
  web_ax_object.ClearHasRequestedOCR();
  EXPECT_FALSE(web_ax_object.HasRequestedOCR());
  EXPECT_FALSE(canvas_element_->HasRequestedOCR());
}

TEST_F(HTMLCanvasAccessibilityManagerTest,
       OCRTriggeredOnAccessibilityEnabledLater) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ::features::kAccessibilityCanvas,
      {{"CanvasAccessibilityMode", "Advanced"}});

  // Set up canvas without accessibility active initially.
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");

  // No active AXContext, no manager yet.
  EXPECT_FALSE(canvas_element_->GetAccessibilityManagerForTesting());

  // Now, enable accessibility.
  AXContext ax_context(GetDocument(), ui::kAXModeComplete);
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  ASSERT_TRUE(manager);
  EXPECT_TRUE(manager->NeedsA11ySupport());

  // Since accessibility was enabled later, the manager should schedule OCR
  // immediately.
  EXPECT_TRUE(manager->IsOCRTimerActiveForTesting());
}

// TODO(crbug.com/498093320): Add a browser test that uses OCR service.

}  // namespace blink
