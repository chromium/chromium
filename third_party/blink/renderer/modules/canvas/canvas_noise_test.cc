// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"

namespace blink {

class CanvasNoiseTest : public PageTestBase {
 public:
  CanvasNoiseTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML("<body><canvas id='c' width='300' height='300'></body>");
    UpdateAllLifecyclePhasesForTest();

    canvas_element_ = To<HTMLCanvasElement>(GetElementById("c"));

    CanvasContextCreationAttributesCore attributes;
    attributes.alpha = true;
    attributes.desynchronized = true;
    attributes.will_read_frequently =
        CanvasContextCreationAttributesCore::WillReadFrequently::kFalse;
    canvas_element_->GetCanvasRenderingContext(/*canvas_type=*/"2d",
                                               attributes);
    GetFrame()
        .DomWindow()
        ->GetRuntimeFeatureStateOverrideContext()
        ->SetCanvasInterventionsForceEnabled();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
  }

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }

  CanvasRenderingContext2D* Context2D() const {
    return static_cast<CanvasRenderingContext2D*>(
        CanvasElement().RenderingContext());
  }

  Document& GetDocument() const { return *GetFrame().DomWindow()->document(); }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_F(CanvasNoiseTest, NoTriggerOnFillRect) {
  V8TestingScope scope;
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillRect(0, 0, 10, 10);
  EXPECT_FALSE(Context2D()->HasTriggerForIntervention());
  EXPECT_FALSE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnShadowBlur) {
  Context2D()->setShadowBlur(10);
  Context2D()->setShadowColor("red");
  Context2D()->fillRect(0, 0, 10, 10);
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnArc) {
  Context2D()->beginPath();
  NonThrowableExceptionState exception_state;
  Context2D()->arc(10, 10, 10, 0, 6, false, exception_state);
  Context2D()->stroke();
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnEllipse) {
  Context2D()->beginPath();
  NonThrowableExceptionState exception_state;
  Context2D()->ellipse(10, 10, 5, 7, 3, 0, 3, false, exception_state);
  Context2D()->fill();
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnSetGlobalCompositeOperation) {
  Context2D()->setGlobalCompositeOperation("multiply");
  V8TestingScope scope;
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillRect(0, 0, 10, 10);
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnFillText) {
  Context2D()->fillText("CanvasNoiseTest", 0, 0);
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnStrokeText) {
  Context2D()->strokeText("CanvasNoiseTest", 0, 0);
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnFillWithPath2DNoNoise) {
  Path2D* canvas_path = Path2D::Create(GetScriptState());
  canvas_path->lineTo(10, 10);
  canvas_path->lineTo(15, 15);
  canvas_path->closePath();
  Context2D()->fill(canvas_path);
  EXPECT_FALSE(canvas_path->HasTriggerForIntervention());
  EXPECT_FALSE(Context2D()->HasTriggerForIntervention());
  EXPECT_FALSE(Context2D()->ShouldTriggerIntervention());
}

TEST_F(CanvasNoiseTest, TriggerOnFillWithPath2DWithNoise) {
  Path2D* canvas_path = Path2D::Create(GetScriptState());
  canvas_path->lineTo(10, 10);
  canvas_path->lineTo(15, 15);
  canvas_path->closePath();
  NonThrowableExceptionState exception_state;
  EXPECT_FALSE(canvas_path->HasTriggerForIntervention());
  canvas_path->arc(10, 10, 10, 0, 6, false, exception_state);
  EXPECT_TRUE(canvas_path->HasTriggerForIntervention());
  EXPECT_FALSE(Context2D()->HasTriggerForIntervention());
  EXPECT_FALSE(Context2D()->ShouldTriggerIntervention());
  Context2D()->fill(canvas_path);
  EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
  EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
}

}  // namespace blink
