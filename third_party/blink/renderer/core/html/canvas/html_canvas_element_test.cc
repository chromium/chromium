// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "build/buildflag.h"
#include "cc/paint/paint_op.h"
#include "cc/test/paint_op_matchers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/dom/focused_element_change_observer.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

using ::blink_testing::ClearRectFlags;
using ::blink_testing::FillFlags;
using ::blink_testing::RecordedOpsAre;
using ::cc::DrawRectOp;
using ::cc::PaintOpEq;

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
  SharedGpuContext::Reset();
}

TEST_P(HTMLCanvasElementTest, CleanCanvasResizeDoesntClearFrameBuffer) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  // Enable printing so that flushes preserve the last recording.
  GetDocument().SetPrinting(Document::kBeforePrinting);
  SetBodyInnerHTML("<canvas id='c' width='10' height='20'></canvas>");

  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('c');
    var ctx = canvas.getContext('2d');
    canvas.width = 10;
    ctx.fillStyle = 'blue';
    ctx.fillRect(0, 0, 5, 5);
  )JS");
  GetDocument().body()->appendChild(script);
  RunDocumentLifecycle();

  auto* canvas = To<HTMLCanvasElement>(GetElementById("c"));

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(canvas->RenderingContext()->GetLastRecording(),
              Optional(RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, 5, 5), fill_flags))));
}

TEST_P(HTMLCanvasElementTest, CanvasResizeClearsFrameBuffer) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  // Enable printing so that flushes preserve the last recording.
  GetDocument().SetPrinting(Document::kBeforePrinting);
  SetBodyInnerHTML("<canvas id='c' width='10' height='20'></canvas>");

  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('c');
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'red';
    ctx.fillRect(0, 0, 10, 10);
    ctx.getImageData(0, 0, 1, 1);  // Force a frame to be rendered.

    canvas.width = 10;

    ctx.fillStyle = 'blue';
    ctx.fillRect(0, 0, 5, 5);
  )JS");
  GetDocument().body()->appendChild(script);
  RunDocumentLifecycle();

  auto* canvas = To<HTMLCanvasElement>(GetElementById("c"));

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(
      canvas->RenderingContext()->GetLastRecording(),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 20),
                                ClearRectFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 5, 5), fill_flags))));
}

TEST_P(HTMLCanvasElementTest, ContextCreationErrorReentrancyPreservesContext) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  // Enable printing so that flushes preserve the last recording.
  GetDocument().SetPrinting(Document::kBeforePrinting);
  SetBodyInnerHTML("<canvas id='c' width='10' height='20'></canvas>");

  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('c');
    var ctx1 = null;
    canvas.addEventListener('webglcontextcreationerror', function() {
      ctx1 = canvas.getContext('2d');
      ctx1.fillStyle = 'red';
      ctx1.fillRect(0, 0, 10, 10);
      ctx1.getImageData(0, 0, 1, 1);  // Force a frame to be rendered.
    });
    var gl = canvas.getContext('webgl');
    var ctx2 = canvas.getContext('2d');
    canvas.width = 10;
    if (gl === null && ctx1 !== null && ctx1 === ctx2) {
      ctx1.fillStyle = 'blue';
      ctx1.fillRect(0, 0, 5, 5);
    }
  )JS");
  GetDocument().body()->appendChild(script);
  RunDocumentLifecycle();

  auto* canvas =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
  ASSERT_NE(canvas->RenderingContext(), nullptr);

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(
      canvas->RenderingContext()->GetLastRecording(),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 20),
                                ClearRectFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 5, 5), fill_flags))));
}

TEST_P(HTMLCanvasElementTest, CreateLayerUpdatesCompositing) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(GetElementById("canvas"));
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

TEST_P(HTMLCanvasElementTest, CanvasMemoryUsage) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas' width='10px' height='10px'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(GetElementById("canvas"));
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());

  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  GetDocument().body()->appendChild(script);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      base::ByteSize(10 * 10 * /* Buffer Count */ 1 * /* Bytes per pixel */ 4),
      canvas->GetMemoryUsage());

  canvas->NotifyGpuContextLost();
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());
}

TEST_P(HTMLCanvasElementTest, CanvasMemoryUsageGpuAccelerated) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  auto raster_context_provider = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContext(raster_context_provider.get());
  ScopedTestingPlatformSupport<GpuCompositingTestPlatform> accelerated_platform;
  GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas' width='10px' height='10px'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(GetElementById("canvas"));
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());

  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  GetDocument().body()->appendChild(script);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      base::ByteSize(10 * 10 * /* Buffer Count */ 1 * /* Bytes per pixel */ 4),
      canvas->GetMemoryUsage());

  canvas->NotifyGpuContextLost();
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());
}

TEST_P(HTMLCanvasElementTest, CanvasMemoryUsageInvalidContext) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas' width='10px' height='10px'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(GetElementById("canvas"));
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());

  // Create a canvas that too big to allocate, causing invalid context.
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var canvas = document.getElementById('canvas');
    canvas.width = 1000000;
    canvas.height = 1000000;
    var ctx = canvas.getContext('%s');
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 10, 10);
  )JS");
  GetDocument().body()->appendChild(script);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(canvas->RenderingContext() == nullptr ||
              canvas->RenderingContext()->isContextLost());
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());
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

TEST_P(HTMLCanvasElementTest, IsCanvasOrInCanvasSubtreeInIframe) {
  SetBodyInnerHTML(R"HTML(
    <div id="div"></div>
    <canvas id="canvas">
      <iframe id="iframe"></iframe>
    </canvas>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="inner_div"></div>
    <canvas id="inner_canvas"></canvas>
  )HTML");
  auto* div = GetElementById("div");
  auto* canvas = GetElementById("canvas");
  auto* iframe = GetElementById("iframe");
  auto* inner_div = ChildDocument().getElementById(AtomicString("inner_div"));
  auto* inner_canvas =
      ChildDocument().getElementById(AtomicString("inner_canvas"));

  EXPECT_TRUE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsInCanvasSubtree());
  EXPECT_TRUE(inner_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_canvas->IsInCanvasSubtree());

  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  div->AppendChild(iframe);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);
  EXPECT_FALSE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(inner_div->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(inner_div->IsInCanvasSubtree());
  EXPECT_TRUE(inner_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(inner_canvas->IsInCanvasSubtree());

  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  canvas->AppendChild(iframe);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);
  EXPECT_TRUE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsInCanvasSubtree());
  EXPECT_TRUE(inner_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_canvas->IsInCanvasSubtree());

  auto* detached_div = ChildDocument().CreateRawElement(html_names::kDivTag);
  EXPECT_FALSE(detached_div->IsInCanvasSubtree());

  auto* fragment = ChildDocument().createDocumentFragment();
  auto* fragment_div = ChildDocument().CreateRawElement(html_names::kDivTag);
  fragment->AppendChild(fragment_div);
  EXPECT_FALSE(fragment_div->IsCanvasOrInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, IsCanvasOrInCanvasSubtreeInShadowInIframe) {
  SetBodyInnerHTML(R"HTML(
    <div id="div"></div>
    <canvas id="canvas">
      <iframe id="iframe"></iframe>
    </canvas>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="host"></div>
  )HTML");
  auto* div = GetElementById("div");
  auto* canvas = GetElementById("canvas");
  auto* iframe = GetElementById("iframe");
  auto* host = ChildDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="inner_div"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* inner_div = shadow_root.getElementById(AtomicString("inner_div"));

  EXPECT_TRUE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host->IsInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsInCanvasSubtree());

  // Move iframe out of canvas.
  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  div->AppendChild(iframe);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(host->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(host->IsInCanvasSubtree());
  EXPECT_FALSE(inner_div->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(inner_div->IsInCanvasSubtree());

  // Move iframe back into canvas.
  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  canvas->AppendChild(iframe);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host->IsInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, IsCanvasOrInCanvasSubtreeInNestedShadowInIframe) {
  SetBodyInnerHTML(R"HTML(
    <div id="div"></div>
    <canvas id="canvas">
      <iframe id="iframe"></iframe>
    </canvas>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="host1"></div>
  )HTML");
  auto* div = GetElementById("div");
  auto* canvas = GetElementById("canvas");
  auto* iframe = GetElementById("iframe");
  auto* host1 = ChildDocument().getElementById(AtomicString("host1"));

  ShadowRoot& shadow_root1 =
      host1->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto* host2 = ChildDocument().CreateRawElement(html_names::kDivTag);
  shadow_root1.AppendChild(host2);

  ShadowRoot& shadow_root2 =
      host2->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto* inner_div = ChildDocument().CreateRawElement(html_names::kDivTag);
  shadow_root2.AppendChild(inner_div);

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host1->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host2->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsCanvasOrInCanvasSubtree());

  // Move iframe out of canvas.
  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  div->AppendChild(iframe);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(host1->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(host2->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(inner_div->IsCanvasOrInCanvasSubtree());

  // Move iframe back into canvas.
  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  canvas->AppendChild(iframe);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(iframe->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host1->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(host2->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(inner_div->IsCanvasOrInCanvasSubtree());
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

TEST_P(HTMLCanvasElementTest, FallbackContentUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <canvas></canvas>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCanvasFallbackContent));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCanvasFallbackElementContent));

  SetBodyInnerHTML(R"HTML(
    <canvas>fallback</canvas>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCanvasFallbackContent));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCanvasFallbackElementContent));

  GetDocument().ClearUseCounterForTesting(WebFeature::kCanvasFallbackContent);

  SetBodyInnerHTML(R"HTML(
    <canvas><div>hello</div></canvas>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCanvasFallbackContent));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kCanvasFallbackElementContent));
}

TEST_P(HTMLCanvasElementTest, IsCanvasOrInCanvasSubtree) {
  SetBodyInnerHTML(R"HTML(
    <div id=div></div>
    <canvas id=canvas>
      <div id=nested_div></div>
      <canvas id=nested_canvas></canvas>
      <input id=nested_input>
    </canvas>
  )HTML");
  auto* div = GetElementById("div");
  EXPECT_FALSE(div->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(div->IsInCanvasSubtree());
  auto* canvas = GetElementById("canvas");
  EXPECT_TRUE(canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(canvas->IsInCanvasSubtree());
  auto* nested_div = GetElementById("nested_div");
  EXPECT_TRUE(nested_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_div->IsInCanvasSubtree());
  auto* nested_canvas = GetElementById("nested_canvas");
  EXPECT_TRUE(nested_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_canvas->IsInCanvasSubtree());
  auto* nested_input = GetElementById("nested_input");
  EXPECT_TRUE(nested_input->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_input->IsInCanvasSubtree());
  auto* nested_input_shadow =
      To<Element>(nested_input->UserAgentShadowRoot()->firstChild());
  EXPECT_TRUE(nested_input_shadow->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_input_shadow->IsInCanvasSubtree());

  // Check `IsCanvasOrInCanvasSubtree` after a dynamic change where the nested
  // elements are individually moved out of the canvas subtree.
  div->appendChild(nested_div);
  EXPECT_FALSE(nested_div->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(nested_div->IsInCanvasSubtree());
  div->appendChild(nested_canvas);
  EXPECT_TRUE(nested_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(nested_canvas->IsInCanvasSubtree());
  EXPECT_TRUE(nested_input->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_input->IsInCanvasSubtree());
  EXPECT_TRUE(nested_input_shadow->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_input_shadow->IsInCanvasSubtree());
  div->appendChild(nested_input);
  EXPECT_FALSE(nested_input->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(nested_input->IsInCanvasSubtree());
  EXPECT_FALSE(nested_input_shadow->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(nested_input_shadow->IsInCanvasSubtree());

  // Check `IsCanvasOrInCanvasSubtree` after a dynamic change where an
  // entire subtree is moved under canvas.
  canvas->appendChild(div);
  EXPECT_TRUE(nested_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_div->IsInCanvasSubtree());
  EXPECT_TRUE(nested_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_canvas->IsInCanvasSubtree());
  EXPECT_TRUE(nested_input->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_input->IsInCanvasSubtree());
  EXPECT_TRUE(nested_input_shadow->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_input_shadow->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, IsCanvasOrInCanvasSubtreeSlotted) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div id=slotHost>
      <template shadowrootmode=open>
        <canvas content=drawable>
          <slot name="slot1"></slot>
        </canvas>
      </template>
      <div id=slotted slot="slot1">
        <p id=slotchild>Hello</p>
      </div>
    </div>
  )");
  UpdateAllLifecyclePhasesForTest();

  auto* slotted = GetElementById("slotted");
  EXPECT_TRUE(slotted->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(slotted->IsInCanvasSubtree());

  auto* slotted_child = GetElementById("slotchild");
  EXPECT_TRUE(slotted_child->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(slotted_child->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, InCanvasSubtreeUnslotted) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="slotHost">
      <template shadowrootmode="open">
        <canvas content=drawable>
          <slot name="slot1">
            <button id="target">fallback</button>
          </slot>
        </canvas>
      </template>
      <div id="slotted" slot="slot1">
        <p id="slotchild">Hello</p>
      </div>
      <div id="unassigned">Unassigned</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetElementById("slotHost");
  auto* target = host->GetShadowRoot()->getElementById(AtomicString("target"));
  auto* slotted = GetElementById("slotted");
  auto* slotchild = GetElementById("slotchild");
  auto* unassigned = GetElementById("unassigned");

  EXPECT_FALSE(target->IsInCanvasSubtree());
  EXPECT_TRUE(slotted->IsInCanvasSubtree());
  EXPECT_TRUE(slotchild->IsInCanvasSubtree());
  EXPECT_FALSE(unassigned->IsInCanvasSubtree());

  // Removing the slotted child activates fallback content in the flat tree.
  slotted->remove();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(target->IsInCanvasSubtree());
  EXPECT_FALSE(slotted->IsInCanvasSubtree());
  EXPECT_FALSE(slotchild->IsInCanvasSubtree());

  // Adding the slotted child back deactivates fallback content.
  host->appendChild(slotted);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->IsInCanvasSubtree());
  EXPECT_TRUE(slotted->IsInCanvasSubtree());
  EXPECT_TRUE(slotchild->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, InCanvasSubtreeUnslottedInIframe) {
  SetBodyInnerHTML(R"HTML(
    <canvas id="canvas">
      <iframe id="iframe"></iframe>
    </canvas>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="slotHost">
      <div id="unassigned">Unassigned</div>
    </div>
  )HTML");
  auto* host = ChildDocument().getElementById(AtomicString("slotHost"));
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhasesForTest();

  auto* unassigned = ChildDocument().getElementById(AtomicString("unassigned"));
  EXPECT_FALSE(unassigned->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, ContentDrawableInvalidation) {
  SetBodyInnerHTML(R"HTML(<canvas id=canvas></canvas>)HTML");
  auto* canvas = To<HTMLCanvasElement>(GetElementById("canvas"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(canvas->IsContentDrawable());
  EXPECT_FALSE(canvas->NeedsStyleRecalc());

  // Adding content=drawable should cause a style recalc.
  canvas->setAttribute(html_names::kContentAttr, AtomicString("drawable"));
  EXPECT_TRUE(canvas->IsContentDrawable());
  EXPECT_TRUE(canvas->NeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(canvas->NeedsStyleRecalc());

  // Setting content to the same value should not cause a style recalc.
  canvas->setAttribute(html_names::kContentAttr, AtomicString("drawable"));
  EXPECT_TRUE(canvas->IsContentDrawable());
  EXPECT_FALSE(canvas->NeedsStyleRecalc());

  // Setting content to fallback should cause a style recalc.
  canvas->setAttribute(html_names::kContentAttr, AtomicString("fallback"));
  EXPECT_FALSE(canvas->IsContentDrawable());
  EXPECT_TRUE(canvas->NeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(canvas->NeedsStyleRecalc());

  // Setting content to an unknown value (which is also not drawable) should not
  // cause a style recalc.
  canvas->setAttribute(html_names::kContentAttr, AtomicString("unknown"));
  EXPECT_FALSE(canvas->IsContentDrawable());
  EXPECT_FALSE(canvas->NeedsStyleRecalc());

  // Setting back to drawable should cause a style recalc.
  canvas->setAttribute(html_names::kContentAttr, AtomicString("drawable"));
  EXPECT_TRUE(canvas->IsContentDrawable());
  EXPECT_TRUE(canvas->NeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(canvas->NeedsStyleRecalc());

  // Removing content attribute should cause a style recalc.
  canvas->removeAttribute(html_names::kContentAttr);
  EXPECT_FALSE(canvas->IsContentDrawable());
  EXPECT_TRUE(canvas->NeedsStyleRecalc());
}

TEST_P(HTMLCanvasElementTest, HTMLInCanvasUseCounter) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML(R"HTML(
    <canvas id=cvs content=drawable>
      <div id=target>hello world</div>
    </canvas>
  )HTML");
  RunDocumentLifecycle();

  // Metrics should not be recorded until the feature is actually used.
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kHTMLInCanvas));

  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    cvs.getContext('2d').drawElementImage(target, 0, 0);
  )JS");
  GetDocument().body()->appendChild(script);
  RunDocumentLifecycle();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kHTMLInCanvas));
}

TEST_P(HTMLCanvasElementTest, StaleSlotAssignmentDoesNotCorruptMovedChild) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <canvas id="c1">
      <div id="host">
        <template shadowrootmode="open">
          <slot id="slot"></slot>
        </template>
        <div id="child">Child</div>
        <div id="child2">Child 2</div>
      </div>
    </canvas>
    <canvas id="c2"></canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetElementById("host");
  auto* child = GetElementById("child");
  auto* c2 = To<HTMLCanvasElement>(GetElementById("c2"));

  EXPECT_TRUE(child->IsInCanvasSubtree());
  child->remove();
  c2->appendChild(child);
  EXPECT_TRUE(child->IsInCanvasSubtree());
  host->remove();
  EXPECT_TRUE(child->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, AppendHostWithUnslottedChildToCanvas) {
  SetBodyInnerHTML(R"HTML(
    <canvas id="c"></canvas>
  )HTML");
  auto* c = To<HTMLCanvasElement>(GetElementById("c"));
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto* child = GetDocument().CreateRawElement(html_names::kDivTag);
  host->appendChild(child);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(child->IsInCanvasSubtree());

  c->appendChild(host);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(host->IsInCanvasSubtree());
  EXPECT_FALSE(child->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, MoveConnectedHostWithSlottedChildIntoCanvas) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="container">
      <div id="host">
        <template shadowrootmode="open">
          <slot id="slot"></slot>
        </template>
        <div id="child">
          <div id="grandchild"></div>
        </div>
      </div>
    </div>
    <canvas id="c"></canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetElementById("host");
  auto* child = GetElementById("child");
  auto* grandchild = GetElementById("grandchild");
  auto* c = To<HTMLCanvasElement>(GetElementById("c"));

  EXPECT_FALSE(host->IsInCanvasSubtree());
  EXPECT_FALSE(child->IsInCanvasSubtree());
  EXPECT_FALSE(grandchild->IsInCanvasSubtree());

  c->appendChild(host);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(host->IsInCanvasSubtree());
  EXPECT_TRUE(child->IsInCanvasSubtree());
  EXPECT_TRUE(grandchild->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, PseudoElementChildOfShadowHost) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      #host::after {
        content: "Hello, world!";
      }
    </style>
    <div id="host">
      <template shadowrootmode="open"></template>
    </div>
    <canvas id="c"></canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetElementById("host");
  EXPECT_FALSE(host->IsInCanvasSubtree());
  EXPECT_FALSE(host->GetPseudoElement(kPseudoIdAfter)->IsInCanvasSubtree());
  GetElementById("c")->appendChild(host);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(host->IsInCanvasSubtree());
  EXPECT_TRUE(host->GetPseudoElement(kPseudoIdAfter)->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, TopLayerRemovalInvalidatesSubtreeAcrossFrames) {
  SetBodyInnerHTML(R"HTML(
    <style>
      [popover] {
        display: block !important;
      }
    </style>
    <canvas id="canvas" content="drawable">
      <div id="drawable" drawable>
        <div id="popover" popover="manual">
          <div id="child"></div>
          <iframe id="iframe"></iframe>
        </div>
      </div>
    </canvas>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="inner_target">Hello</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* popover = To<HTMLElement>(GetElementById("popover"));
  auto* child = GetElementById("child");
  auto* iframe = GetElementById("iframe");
  auto* inner_target =
      ChildDocument().getElementById(AtomicString("inner_target"));

  popover->ShowPopoverInternal(nullptr, nullptr);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(popover->IsInTopLayer());

  EXPECT_FALSE(
      popover->GetLayoutObject()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_FALSE(
      child->GetLayoutObject()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_FALSE(child->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      iframe->GetLayoutObject()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_FALSE(iframe->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      ChildDocument().GetLayoutView()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_FALSE(ChildDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      inner_target->GetLayoutObject()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_FALSE(inner_target->GetLayoutObject()->NeedsPaintPropertyUpdate());

  popover->HidePopoverInternal(
      /*invoker=*/nullptr, HidePopoverFocusBehavior::kNone,
      HidePopoverTransitionBehavior::kNoEventsNoWaiting,
      /*exception_state=*/nullptr);
  GetDocument().UpdateStyleAndLayoutTree();
  ASSERT_FALSE(popover->IsInTopLayer());

  EXPECT_TRUE(popover->GetLayoutObject()->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(popover->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(child->GetLayoutObject()->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(child->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(iframe->GetLayoutObject()->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(iframe->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(
      ChildDocument().GetLayoutView()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_TRUE(ChildDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(
      inner_target->GetLayoutObject()->SubtreeShouldDoFullPaintInvalidation());
  EXPECT_TRUE(inner_target->GetLayoutObject()->NeedsPaintPropertyUpdate());
}

TEST_P(HTMLCanvasElementTest, SlotAssignmentDuringSetIsInCanvasSubtree) {
  class GCOnFocusChangeObserver final
      : public GarbageCollected<GCOnFocusChangeObserver>,
        public FocusedElementChangeObserver {
   public:
    void DidChangeFocus() override {
      invoked_ = true;
      ThreadState::Current()->CollectAllGarbageForTesting(
          ThreadState::StackState::kMayContainHeapPointers);
    }
    bool Invoked() const { return invoked_; }
    void Trace(Visitor* visitor) const override {
      FocusedElementChangeObserver::Trace(visitor);
    }

   private:
    bool invoked_ = false;
  };

  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetPage().GetFocusController().SetActive(true);
  GetPage().GetFocusController().SetFocused(true);
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <canvas id="cv" content="drawable">
      <div id="h2">
        <template shadowrootmode="open">
          <slot id="s2"></slot>
        </template>
        <span></span>
        <span></span>
        <span></span>
        <input id="d" type="date" value="2001-02-03">
        <span id="y"></span>
        <span></span>
        <span></span>
        <span></span>
        <span></span>
      </div>
    </canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* h2 = GetElementById("h2");
  auto* d = To<HTMLInputElement>(GetElementById("d"));
  auto* y = GetElementById("y");
  auto* s2 = To<HTMLSlotElement>(
      h2->GetShadowRoot()->getElementById(AtomicString("s2")));

  s2->AssignedNodes();
  d->SetValue("2001-02-04");
  h2->SetIsInCanvasSubtree(false);
  // A suggested value makes SetIsInCanvasSubtree(true) clear it, which rebuilds
  // the date fields and changes focus.
  d->SetSuggestedValue("2005-06-07");
  d->Focus();
  d->setAttribute(html_names::kValueAttr, AtomicString("2003-04-05"));
  y->setAttribute(html_names::kSlotAttr, AtomicString("away"));

  auto* observer = MakeGarbageCollected<GCOnFocusChangeObserver>();
  GetDocument().AddFocusedElementChangeObserver(observer);

  h2->SetIsInCanvasSubtree(true);
  EXPECT_TRUE(observer->Invoked());
  EXPECT_FALSE(y->IsInCanvasSubtree());
}

TEST_P(HTMLCanvasElementTest, SlotReassignmentInvalidatesVisitedLinkStyle) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host">
      <template shadowrootmode="open">
        <div>
          <slot name="outside"></slot>
        </div>
        <canvas content="drawable">
          <div drawable>
            <slot name="inside"></slot>
          </div>
        </canvas>
      </template>
      <div id="payload" slot="outside">
        <a id="link" href=""><span id="child">Link</span></a>
        <svg><a id="svg_link" href=""><rect width="10" height="10"/></a></svg>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* payload = GetElementById("payload");
  auto* link = GetElementById("link");
  auto* child = GetElementById("child");
  auto* svg_link = GetElementById("svg_link");

  EXPECT_FALSE(link->IsInCanvasSubtree());
  EXPECT_EQ(link->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideVisitedLink);
  EXPECT_EQ(child->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideVisitedLink);
  EXPECT_EQ(svg_link->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideVisitedLink);

  payload->setAttribute(html_names::kSlotAttr, AtomicString("inside"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(link->IsInCanvasSubtree());
  EXPECT_EQ(link->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideUnvisitedLink);
  EXPECT_EQ(child->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideUnvisitedLink);
  EXPECT_EQ(svg_link->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideUnvisitedLink);

  payload->setAttribute(html_names::kSlotAttr, AtomicString("outside"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(link->IsInCanvasSubtree());
  EXPECT_EQ(link->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideVisitedLink);
  EXPECT_EQ(child->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideVisitedLink);
  EXPECT_EQ(svg_link->GetComputedStyle()->InsideLink(),
            EInsideLink::kInsideVisitedLink);
}

TEST_P(HTMLCanvasElementTest, RemoveAndReinsertSlottedElementToSameHost) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host1">
      <template shadowrootmode="open">
        <canvas content="drawable">
          <slot></slot>
        </canvas>
      </template>
      <div id="slotted1">
        <div id="child1"></div>
      </div>
    </div>
    <canvas content="drawable">
      <div id="host2">
        <template shadowrootmode="open">
          <slot></slot>
        </template>
        <div id="slotted2">
          <div id="child2"></div>
        </div>
      </div>
    </canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host1 = GetElementById("host1");
  auto* slotted1 = GetElementById("slotted1");
  auto* child1 = GetElementById("child1");
  auto* host2 = GetElementById("host2");
  auto* slotted2 = GetElementById("slotted2");
  auto* child2 = GetElementById("child2");

  EXPECT_TRUE(slotted1->IsInCanvasSubtree());
  EXPECT_TRUE(child1->IsInCanvasSubtree());
  EXPECT_TRUE(slotted2->IsInCanvasSubtree());
  EXPECT_TRUE(child2->IsInCanvasSubtree());

  // Remove and reinsert into the same shadow host without an intermediate
  // slot assignment recalc, so both elements retain their previous slotting.
  slotted1->remove();
  EXPECT_FALSE(slotted1->IsInCanvasSubtree());
  EXPECT_FALSE(child1->IsInCanvasSubtree());
  host1->appendChild(slotted1);
  EXPECT_TRUE(slotted1->IsInCanvasSubtree());
  EXPECT_TRUE(child1->IsInCanvasSubtree());

  slotted2->remove();
  EXPECT_FALSE(slotted2->IsInCanvasSubtree());
  EXPECT_FALSE(child2->IsInCanvasSubtree());
  host2->appendChild(slotted2);
  EXPECT_TRUE(slotted2->IsInCanvasSubtree());
  EXPECT_TRUE(child2->IsInCanvasSubtree());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(slotted1->IsInCanvasSubtree());
  EXPECT_TRUE(child1->IsInCanvasSubtree());
  EXPECT_TRUE(slotted2->IsInCanvasSubtree());
  EXPECT_TRUE(child2->IsInCanvasSubtree());

  // Also test single-call re-insertion of an already-attached slotted child
  // (`host->appendChild(slotted)`), which internally removes and re-inserts
  // in the same task.
  host1->appendChild(slotted1);
  EXPECT_TRUE(slotted1->IsInCanvasSubtree());
  EXPECT_TRUE(child1->IsInCanvasSubtree());
  host2->appendChild(slotted2);
  EXPECT_TRUE(slotted2->IsInCanvasSubtree());
  EXPECT_TRUE(child2->IsInCanvasSubtree());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(slotted1->IsInCanvasSubtree());
  EXPECT_TRUE(child1->IsInCanvasSubtree());
  EXPECT_TRUE(slotted2->IsInCanvasSubtree());
  EXPECT_TRUE(child2->IsInCanvasSubtree());
}

// Moving a focused <input type=date> with no suggested value into a canvas
// subtree must not rebuild its fields, so slot assignment recalc does not move
// focus or run script.
TEST_P(HTMLCanvasElementTest, SlotIntoCanvasWithoutSuggestedValueKeepsFocus) {
  class FocusChangeCounter final : public GarbageCollected<FocusChangeCounter>,
                                   public FocusedElementChangeObserver {
   public:
    void DidChangeFocus() override { ++count_; }
    int Count() const { return count_; }

   private:
    int count_ = 0;
  };

  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetPage().GetFocusController().SetActive(true);
  GetPage().GetFocusController().SetFocused(true);

  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(
      "<div id=\"host\">"
      "<template shadowrootmode=\"open\">"
      "<slot name=\"outside\"></slot>"
      "<canvas content=\"drawable\"><slot id=\"inside\" name=\"inside\">"
      "</slot></canvas>"
      "</template>"
      "<input id=\"d\" type=\"date\" value=\"2001-02-03\" slot=\"outside\">"
      "</div>");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* date =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("d")));
  auto* inside = To<HTMLSlotElement>(
      host->GetShadowRoot()->getElementById(AtomicString("inside")));
  date->Focus();
  Element* focused = GetDocument().FocusedElement();
  ASSERT_TRUE(focused);
  EXPECT_FALSE(date->IsInCanvasSubtree());
  // Make the date field layout stale, as in date.value = date.value;
  // date.defaultValue = '2003-04-05'. Without this, DateTimeEditElement::
  // GetLayout() returns early and would not move focus even without the fix.
  date->SetValue(date->Value());
  date->setAttribute(html_names::kValueAttr, AtomicString("2003-04-05"));

  auto* counter = MakeGarbageCollected<FocusChangeCounter>();
  GetDocument().AddFocusedElementChangeObserver(counter);

  date->setAttribute(html_names::kSlotAttr, AtomicString("inside"));
  inside->AssignedNodes();

  EXPECT_TRUE(date->IsInCanvasSubtree());
  EXPECT_EQ(0, counter->Count());
  EXPECT_EQ(focused, GetDocument().FocusedElement());
}

// Regression test for crbug.com/573335176. Moving a focused <input type=date>
// that shows an autofill preview into a canvas subtree clears the preview,
// which rebuilds the fields and makes slot assignment recalc dispatch a blur.
// A handler
// that re-enters recalc through assignedNodes() must bail out, so the slot it
// adds has no assigned nodes until the next recalc. This checks the re-entrancy
// guard directly instead of relying on the freed memory read crashing.
TEST_P(HTMLCanvasElementTest, ReentrantSlotRecalcDuringDidRecalcAssignedNodes) {
#if DCHECK_IS_ON()
  // Reaching this path trips a separate, pre-existing DCHECK:
  // RecalcAssignment() holds a FlatTreeTraversalForbiddenScope while the date
  // field dispatches focus and blur. See crbug.com/574469802.
  GTEST_SKIP() << "Blocked by a pre-existing DCHECK in Element::Focus()";
#else
  class ReentrantRecalcObserver final
      : public GarbageCollected<ReentrantRecalcObserver>,
        public FocusedElementChangeObserver {
   public:
    explicit ReentrantRecalcObserver(ShadowRoot* shadow_root, Element* spare)
        : shadow_root_(shadow_root), spare_(spare) {}

    void DidChangeFocus() override {
      if (invoked_) {
        return;
      }
      invoked_ = true;
      // Records that the blur really was dispatched from inside
      // SlotAssignment::RecalcAssignment(), so this test cannot silently stop
      // covering the re-entrant path.
      reentrant_ = shadow_root_->GetDocument().IsInSlotAssignmentRecalc();

      // Add a slot and move a light child into it so the assignment is dirty,
      // then re-enter recalc through assignedNodes(). Without the fix this
      // nested recalc rebuilds slots_ (freeing the buffer the outer loop is
      // iterating) and assigns |spare_| to the new slot. With the fix the
      // re-entrant recalc bails, so the new slot has no assigned nodes yet.
      auto* extra =
          MakeGarbageCollected<HTMLSlotElement>(shadow_root_->GetDocument());
      extra->setAttribute(html_names::kNameAttr, AtomicString("extra"));
      shadow_root_->AppendChild(extra);
      spare_->setAttribute(html_names::kSlotAttr, AtomicString("extra"));
      assigned_in_handler_ = extra->AssignedNodes().size();
    }

    bool Invoked() const { return invoked_; }
    bool Reentrant() const { return reentrant_; }
    wtf_size_t AssignedInHandler() const { return assigned_in_handler_; }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(shadow_root_);
      visitor->Trace(spare_);
      FocusedElementChangeObserver::Trace(visitor);
    }

   private:
    Member<ShadowRoot> shadow_root_;
    Member<Element> spare_;
    bool invoked_ = false;
    bool reentrant_ = false;
    wtf_size_t assigned_in_handler_ = 0;
  };

  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetPage().GetFocusController().SetActive(true);
  GetPage().GetFocusController().SetFocused(true);

  // The <input> starts in the second slot and is moved to the first, so the
  // re-entrant recalc happens while the outer DidRecalcAssignedNodes() loop is
  // still running. No filler slots are needed because the test asserts the
  // re-entrancy behavior rather than relying on a freed-buffer read crashing.
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(
      "<canvas id=\"cv\" content=\"drawable\"><div id=\"host\">"
      "<template shadowrootmode=\"open\">"
      "<slot id=\"s1\" name=\"first\"></slot>"
      "<slot id=\"s2\" name=\"second\"></slot>"
      "</template>"
      "<input id=\"d\" type=\"date\" value=\"2001-02-03\" slot=\"second\">"
      "<span id=\"spare\" slot=\"unslotted\"></span>"
      "</div></canvas>");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* date =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("d")));
  auto* shadow_root = host->GetShadowRoot();
  auto* s1 =
      To<HTMLSlotElement>(shadow_root->getElementById(AtomicString("s1")));

  s1->AssignedNodes();
  date->SetValue("2001-02-04");
  // Clear the bit on the <input> only, so that re-slotting it flips the bit
  // back on and clears the suggested value, which rebuilds the date fields.
  // The slots themselves stay in the canvas subtree.
  date->SetIsInCanvasSubtree(false);
  date->SetSuggestedValue("2005-06-07");
  ASSERT_FALSE(date->SuggestedValue().IsNull());
  date->Focus();
  date->setAttribute(html_names::kValueAttr, AtomicString("2003-04-05"));

  auto* observer = MakeGarbageCollected<ReentrantRecalcObserver>(
      shadow_root, GetDocument().getElementById(AtomicString("spare")));
  GetDocument().AddFocusedElementChangeObserver(observer);

  // Moving the <input> to the first slot re-assigns slots and dispatches the
  // blur from inside the DidRecalcAssignedNodes() loop.
  date->setAttribute(html_names::kSlotAttr, AtomicString("first"));
  s1->AssignedNodes();

  ASSERT_TRUE(observer->Invoked());
  EXPECT_TRUE(observer->Reentrant());
  EXPECT_TRUE(date->SuggestedValue().IsNull());
  // The re-entrant recalc must have bailed out instead of rebuilding slots_,
  // so the slot added during the handler has no assigned nodes yet.
  EXPECT_EQ(0u, observer->AssignedInHandler());

  // The deferred recalc is picked up here and produces the correct assignment.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(date->AssignedSlot(), s1);
#endif  // DCHECK_IS_ON()
}

}  // namespace blink
