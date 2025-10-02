// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/trace_event.h"
#include "build/buildflag.h"
#include "cc/paint/paint_op.h"
#include "cc/test/paint_op_matchers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"

using ::blink_testing::ClearRectFlags;
using ::blink_testing::FillFlags;
using ::blink_testing::RecordedOpsAre;
using ::cc::DrawRectOp;
using ::cc::PaintOpEq;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::StartsWith;

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

  auto* canvas =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(canvas->RenderingContext()->GetLastRecordingForCanvas2D(),
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

  auto* canvas =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(
      canvas->RenderingContext()->GetLastRecordingForCanvas2D(),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 20),
                                ClearRectFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 5, 5), fill_flags))));
}

TEST_P(HTMLCanvasElementTest, CreateLayerUpdatesCompositing) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().getElementById(AtomicString("canvas")));
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
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().getElementById(AtomicString("canvas")));
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
      base::ByteCount(10 * 10 * /* Buffer Count */ 1 * /* Bytes per pixel */ 4),
      canvas->GetMemoryUsage());

  canvas->NotifyGpuContextLost();
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());
}

TEST_P(HTMLCanvasElementTest, CanvasMemoryUsageGpuAccelerated) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  auto raster_context_provider = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContextRaster(raster_context_provider.get());
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform>
      accelerated_platform;
  GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas' width='10px' height='10px'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().getElementById(AtomicString("canvas")));
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
      base::ByteCount(10 * 10 * /* Buffer Count */ 3 * /* Bytes per pixel */ 4),
      canvas->GetMemoryUsage());

  canvas->NotifyGpuContextLost();
  EXPECT_TRUE(canvas->GetMemoryUsage().is_zero());
}

TEST_P(HTMLCanvasElementTest, CanvasMemoryUsageInvalidContext) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas' width='10px' height='10px'></canvas>");
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().getElementById(AtomicString("canvas")));
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
  auto* div = GetDocument().getElementById(AtomicString("div"));
  EXPECT_FALSE(div->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(div->IsInCanvasSubtree());
  auto* canvas = GetDocument().getElementById(AtomicString("canvas"));
  EXPECT_TRUE(canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_FALSE(canvas->IsInCanvasSubtree());
  auto* nested_div = GetDocument().getElementById(AtomicString("nested_div"));
  EXPECT_TRUE(nested_div->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_div->IsInCanvasSubtree());
  auto* nested_canvas =
      GetDocument().getElementById(AtomicString("nested_canvas"));
  EXPECT_TRUE(nested_canvas->IsCanvasOrInCanvasSubtree());
  EXPECT_TRUE(nested_canvas->IsInCanvasSubtree());
  auto* nested_input =
      GetDocument().getElementById(AtomicString("nested_input"));
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

TEST_P(HTMLCanvasElementTest, CanvasReadbackBlocked) {
  V8TestingScope scope;
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<canvas id='c' width='10' height='20'></canvas>");

  auto* canvas =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
  canvas->GetCanvasRenderingContext(GetDocument().GetExecutionContext(), "2d",
                                    CanvasContextCreationAttributesCore());
  auto* callback = V8BlobCallback::Create(scope.GetContext()->Global());

  {
    // When the BlockCanvasReadback feature is enabled, reading back should
    // throw a DOM exception.
    ScopedBlockCanvasReadbackForTest scoped_feature(true);
    DummyExceptionStateForTesting exception_state;
    canvas->toDataURL("image/png", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNotAllowedError);
  }

  {
    // When the feature is disabled it should not throw.
    ScopedBlockCanvasReadbackForTest scoped_feature(false);
    DummyExceptionStateForTesting exception_state;
    canvas->toDataURL("image/png", exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }

  {
    // When the BlockCanvasReadback feature is enabled, reading back should
    // throw a DOM exception.
    ScopedBlockCanvasReadbackForTest scoped_feature(true);
    DummyExceptionStateForTesting exception_state;
    canvas->toBlob(callback, "image/png", exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNotAllowedError);
  }

  {
    // When the feature is disabled it should not throw.
    ScopedBlockCanvasReadbackForTest scoped_feature(false);
    DummyExceptionStateForTesting exception_state;
    canvas->toBlob(callback, "image/png", exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }
}

class HTMLCanvasElementWithTracingTest : public RenderingTest {
 public:
  HTMLCanvasElementWithTracingTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void TearDown() override {
    RenderingTest::TearDown();
    CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
  }

  base::test::TracingEnvironment tracing_environment_;
};

class HTMLCanvasElementWithTracingSyncTest
    : public HTMLCanvasElementWithTracingTest,
      public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         HTMLCanvasElementWithTracingSyncTest,
                         testing::ValuesIn({R"JS(
          let canvas = document.getElementById('canvas');
          let ctx = canvas.getContext('2d');
          ctx.fillText("abc", 0, 10);
          canvas.toDataURL();)JS",
                                            R"JS(
          let canvas = document.getElementById('canvas');
          let ctx = canvas.getContext('2d');
          ctx.fillText("abc", 0, 10);
          ctx.getImageData(0, 0, 10, 10);)JS"}));

TEST_P(HTMLCanvasElementWithTracingSyncTest,
       CanvasReadbackEmitsIdentifiabilityTraces) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas'></canvas>");

  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace(
      base::test::DefaultTraceConfig(
          "disabled-by-default-identifiability.high_entropy_api", false),
      perfetto::kInProcessBackend);
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(String(GetParam()));
  GetDocument().body()->appendChild(script);

  absl::Status status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query = R"sql(
    SELECT slice.name, args.display_value FROM slice
      LEFT JOIN args USING (arg_set_id)
      WHERE slice.category =
        'disabled-by-default-identifiability.high_entropy_api'
      AND args.key = 'debug.data_url'
  )sql";
  auto result = test_trace_processor.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              Contains(ElementsAre(Eq("CanvasReadback"), StartsWith("data:"))));
}

class HTMLCanvasElementWithTracingAsyncTest
    : public HTMLCanvasElementWithTracingTest,
      public testing::WithParamInterface<std::pair<const char*, const char*>> {
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HTMLCanvasElementWithTracingAsyncTest,
    testing::ValuesIn({std::make_pair(
                           R"JS(
          (async () => {
            let canvas = document.getElementById('canvas');
            let ctx = canvas.getContext('2d');
            ctx.fillText("abc", 0, 10);
            await new Promise(resolve => {canvas.toBlob(resolve)});
          })()
         )JS",
                           "HTMLCanvasElement.toBlob"),
                       std::make_pair(
                           R"JS(
          (async () => {
            let offscreen = new OffscreenCanvas(10, 10);
            let ctx = offscreen.getContext('2d');
            ctx.fillText("abc", 0, 10);
            await new Promise(resolve => {
              offscreen.convertToBlob().then(resolve);
            });
          })()
         )JS",
                           "OffscreenCanvas.convertToBlob")}));

class Resolve final : public ThenCallable<IDLAny, Resolve> {
 public:
  explicit Resolve(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}

  void React(ScriptState*, ScriptValue) { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

TEST_P(HTMLCanvasElementWithTracingAsyncTest,
       CanvasReadbackEmitsIdentifiabilityTraces) {
  // Enable script so that the canvas will create a LayoutHTMLCanvas.
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<canvas id='canvas'></canvas>");

  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace(
      base::test::DefaultTraceConfig(
          "disabled-by-default-identifiability.high_entropy_api", false),
      perfetto::kInProcessBackend);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_state_scope(script_state);

  base::RunLoop run_loop;
  auto* resolve = MakeGarbageCollected<Resolve>(run_loop.QuitClosure());

  ClassicScript* script = ClassicScript::CreateUnspecifiedScript(
      GetParam().first, ScriptSourceLocationType::kUnknown,
      SanitizeScriptErrors::kSanitize);

  ScriptEvaluationResult script_result =
      script->RunScriptOnScriptStateAndReturnValue(script_state);

  auto promise =
      ToResolvedPromise<IDLAny>(script_state, script_result.GetSuccessValue());
  promise.Then(script_state, resolve, resolve);

  // Avoid the NOTREACHED in CanvasPerformanceMonitor::WillProcessTask().
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  run_loop.Run();

  absl::Status status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  {
    // Check that there is a flow connecting the CanvasReadback traces emitted
    // by CanvasAsyncBlobCreator.
    std::string query = R"sql(
      SELECT s_in.name, s_out.name
        FROM flow
        LEFT JOIN slice AS s_in ON slice_in = s_in.id
        LEFT JOIN slice AS s_out ON slice_out = s_out.id
        WHERE s_in.category =
            'disabled-by-default-identifiability.high_entropy_api'
          AND s_out.category =
            'disabled-by-default-identifiability.high_entropy_api'
    )sql";
    auto result = test_trace_processor.RunQuery(query);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_THAT(result.value(), Contains(ElementsAre(Eq("CanvasReadback"),
                                                     Eq("CanvasReadback")))
                                    .Times(2));
  }

  {
    // Check that it is possible to join the canvas readback data emitted by
    // CanvasAsyncBlobCreator with the HighEntropyJavaScriptAPICall that was
    // responsible for it.
    std::string query = R"sql(
      WITH RECURSIVE canvas_readback AS (
        SELECT slice_out AS flow_start_id,
          args.display_value AS canvas_data
        FROM flow
        INNER JOIN slice ON slice.id = flow.slice_in
        LEFT JOIN args ON slice.arg_set_id = args.arg_set_id
        WHERE
          slice.category =
            'disabled-by-default-identifiability.high_entropy_api'
          AND slice.name = 'CanvasReadback'
          AND args.key = 'debug.data_url'
      ), ancestors AS (
          SELECT slice.id, slice.parent_id
          FROM slice
          INNER JOIN canvas_readback ON slice.id = canvas_readback.flow_start_id
          UNION ALL
          SELECT ancestors.id, slice.parent_id
          FROM slice
          JOIN ancestors ON slice.id = ancestors.parent_id
          WHERE slice.parent_id IS NOT NULL
      ), data_with_ancestors AS (
        SELECT args.display_value, canvas_data FROM canvas_readback
        LEFT JOIN ancestors ON (canvas_readback.flow_start_id = ancestors.id)
        LEFT JOIN slice on (ancestors.parent_id = slice.id)
        LEFT JOIN args ON args.arg_set_id = slice.arg_set_id
        WHERE
          slice.category =
            'disabled-by-default-identifiability.high_entropy_api'
          AND slice.name =  'HighEntropyJavaScriptAPICall'
          AND args.key = 'high_entropy_api.called_api.identifier'
      ) SELECT * FROM data_with_ancestors
    )sql";
    auto result = test_trace_processor.RunQuery(query);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_THAT(result.value(), Contains(ElementsAre(Eq(GetParam().second),
                                                     StartsWith("data:"))));
  }
}

}  // namespace blink
