// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"

namespace blink {
namespace {

class CanvasInterventionsHelperTest : public PageTestBase {
 public:
  CanvasInterventionsHelperTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML(
        "<body><canvas id='c'></canvas><canvas id='d'></canvas></body>");
    UpdateAllLifecyclePhasesForTest();
    canvas_element_ = To<HTMLCanvasElement>(GetElementById("c"));
    CreateContext();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
  }

  Document* GetDocument() const { return GetFrame().DomWindow()->document(); }

  void CreateContext() {
    CanvasContextCreationAttributesCore attributes;
    attributes.alpha = true;
    attributes.desynchronized = true;
    attributes.will_read_frequently =
        CanvasContextCreationAttributesCore::WillReadFrequently::kFalse;
    canvas_element_->GetCanvasRenderingContext(/*canvas_type=*/"2d",
                                               attributes);
  }

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }

  CanvasRenderingContext* RenderingContext() const {
    return CanvasElement().RenderingContext();
  }

  void DrawSomething() {
    CanvasElement().DidDraw();
    CanvasElement().PreFinalizeFrame();
    RenderingContext()->FinalizeFrame(FlushReason::kTesting);
    CanvasElement().PostFinalizeFrame(FlushReason::kTesting);
  }

 private:
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_F(CanvasInterventionsHelperTest,
       MaybeGetNoisedPixelsNoiseWhenCanvasInterventionsEnabled) {
  auto* window = GetFrame().DomWindow();
  // Enable CanvasInterventions.
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();

  CanvasInterventionsHelper* interventions_helper_handle =
      CanvasInterventionsHelper::Create(window);

  DrawSomething();
  scoped_refptr<StaticBitmapImage> snapshot =
      RenderingContext()->GetImage(FlushReason::kTesting);

  // TODO(https://crbug.com/385739564): Add test logic for noised snapshots.
  EXPECT_NE(nullptr,
            interventions_helper_handle->MaybeGetNoisedSnapshot(snapshot));
  EXPECT_NE(snapshot,
            interventions_helper_handle->MaybeGetNoisedSnapshot(snapshot));
}

TEST_F(CanvasInterventionsHelperTest,
       MaybeGetNoisedPixelsDoesNotNoiseWhenCanvasInterventionsDisabled) {
  auto* window = GetFrame().DomWindow();
  // Disable CanvasInterventions.
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceDisabled();

  CanvasInterventionsHelper* interventions_helper_handle =
      CanvasInterventionsHelper::Create(window);

  DrawSomething();
  scoped_refptr<StaticBitmapImage> snapshot =
      RenderingContext()->GetImage(FlushReason::kTesting);

  EXPECT_EQ(snapshot,
            interventions_helper_handle->MaybeGetNoisedSnapshot(snapshot));
}

}  // namespace
}  // namespace blink
