// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class HTMLCanvasElementTest : public RenderingTest,
                              public PaintTestConfigurations {
 public:
  HTMLCanvasElementTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void TearDown() override;
};

INSTANTIATE_PAINT_TEST_SUITE_P(HTMLCanvasElementTest);

void HTMLCanvasElementTest::TearDown() {
  RenderingTest::TearDown();
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
}

TEST_P(HTMLCanvasElementTest, CreateLayerUpdatesCompositing) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(GetDocument().getElementById("canvas"));
  EXPECT_FALSE(canvas->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->PaintOffsetTranslation());

  EXPECT_FALSE(canvas->GetLayoutObject()->NeedsPaintPropertyUpdate());
  auto* painting_layer = GetLayoutObjectByElementId("canvas")->PaintingLayer();
  EXPECT_FALSE(painting_layer->SelfNeedsRepaint());
  canvas->CreateLayer();
  EXPECT_FALSE(canvas->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(painting_layer->SelfNeedsRepaint());
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(
      painting_layer,
      To<LayoutBoxModelObject>(canvas->GetLayoutObject())->PaintingLayer());
  EXPECT_FALSE(canvas->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->PaintOffsetTranslation());
}

TEST_P(HTMLCanvasElementTest, CanvasInvalidation) {
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas' width='10px' height='10px'></canvas>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
  RunDocumentLifecycle();
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
}

TEST_P(HTMLCanvasElementTest, CanvasNotInvalidatedOnFirstFrameInDOM) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.createElement('canvas');
    document.body.appendChild(canvas);
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
}

TEST_P(HTMLCanvasElementTest, CanvasNotInvalidatedOnFirstPaint) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<canvas id='canvas' style='display:none'></canvas>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
  RunDocumentLifecycle();
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('canvas');
    canvas.style.display = 'block';
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
}

TEST_P(HTMLCanvasElementTest, CanvasInvalidationInFrame) {
  SetBodyInnerHTML(R"HTML(
    <iframe id='iframe'></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <canvas id='canvas' width='10px' height='10px'></canvas>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  ChildDocument().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(
      ChildDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
  RunDocumentLifecycle();
  auto* script = ChildDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  ChildDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_canvas_invalidation_for_test());
}

TEST_P(HTMLCanvasElementTest, BrokenCanvasHighRes) {
  EXPECT_NE(HTMLCanvasElement::BrokenCanvas(2.0).first,
            HTMLCanvasElement::BrokenCanvas(1.0).first);
  EXPECT_EQ(HTMLCanvasElement::BrokenCanvas(2.0).second, 2.0);
  EXPECT_EQ(HTMLCanvasElement::BrokenCanvas(1.0).second, 1.0);
}

}  // namespace blink
