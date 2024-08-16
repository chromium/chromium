// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "build/buildflag.h"
#include "cc/paint/paint_op.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

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
  CanvasResourceProvider* provider =
      canvas->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(provider->LastRecording(),
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
  CanvasResourceProvider* provider =
      canvas->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);

  cc::PaintFlags fill_flags = FillFlags();
  fill_flags.setColor(SkColors::kBlue);
  EXPECT_THAT(
      provider->LastRecording(),
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

class Resolve final : public ScriptFunction::Callable {
 public:
  explicit Resolve(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}

  ScriptValue Call(ScriptState*, ScriptValue) override {
    callback_.Run();
    return ScriptValue();
  }
  int Length() const override { return 1; }

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
  ScriptFunction* fn = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<Resolve>(run_loop.QuitClosure()));

  ClassicScript* script = ClassicScript::CreateUnspecifiedScript(
      GetParam().first, ScriptSourceLocationType::kUnknown,
      SanitizeScriptErrors::kSanitize);

  ScriptEvaluationResult script_result =
      script->RunScriptOnScriptStateAndReturnValue(script_state);

  auto promise =
      ToResolvedPromise<IDLAny>(script_state, script_result.GetSuccessValue());
  promise.Then(fn, fn);

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
