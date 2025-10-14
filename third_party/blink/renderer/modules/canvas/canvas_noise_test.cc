// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float16array_float32array_uint8clampedarray.h"
#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas_noise_test_util.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/canvas_high_entropy_op_type.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class CanvasNoiseTest : public PageTestBase {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster(
        CreateCanvasNoiseTestRasterInterface());
    InitializeSharedGpuContextRaster(test_context_provider_.get());

    PageTestBase::SetUp();
    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
    NavigateTo(KURL("https://test.example"));
    SetHtmlInnerHTML("<body><canvas id='c' width='300' height='300'></body>");
    UpdateAllLifecyclePhasesForTest();

    canvas_element_ = To<HTMLCanvasElement>(GetElementById("c"));

    CanvasContextCreationAttributesCore attributes;
    attributes.alpha = true;
    attributes.desynchronized = true;
    attributes.premultiplied_alpha = false;
    attributes.will_read_frequently =
        CanvasContextCreationAttributesCore::WillReadFrequently::kFalse;
    canvas_element_->GetCanvasRenderingContext(
        GetDocument().GetExecutionContext(), /*canvas_type=*/"2d", attributes);
    GetDocument().GetExecutionContext()->SetCanvasNoiseToken(
        NoiseToken(0x1234567890123456));
    EnableInterventions();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    SharedGpuContext::Reset();
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

  void DisableInterventions() {
    GetFrame().DomWindow()->GetExecutionContext()->SetCanvasNoiseToken(
        std::nullopt);
  }

  void EnableInterventions() {
    GetFrame().DomWindow()->GetExecutionContext()->SetCanvasNoiseToken(
        NoiseToken(0x1234567890123456));
  }

  base::span<uint8_t> GetNoisedPixels(ExecutionContext* ec) {
    scoped_refptr<StaticBitmapImage> snapshot =
        Context2D()->GetImage(FlushReason::kTesting);
    num_readbacks_++;
    return GetPixels(Context2D(), CanvasElement().width(),
                     CanvasElement().height());
  }

  static base::span<uint8_t> GetPixels(BaseRenderingContext2D* context,
                                       size_t width,
                                       size_t height) {
    NonThrowableExceptionState exception_state;
    return context->getImageData(0, 0, width, height, exception_state)
        ->data()
        ->GetAsUint8ClampedArray()
        ->AsSpan();
  }

  static int GetNumChangedPixels(base::span<uint8_t> pixels_no_interventions,
                                 base::span<uint8_t> pixels_with_interventions,
                                 int max_channel_diff) {
    EXPECT_EQ(pixels_with_interventions.size(), pixels_no_interventions.size());
    int num_changed_pixel_values = 0;
    int too_large_diffs = 0;
    for (size_t i = 0; i < pixels_no_interventions.size(); ++i) {
      int diff =
          std::max(pixels_no_interventions[i], pixels_with_interventions[i]) -
          std::min(pixels_no_interventions[i], pixels_with_interventions[i]);
      if (diff > 0) {
        ++num_changed_pixel_values;
      }
      if (diff > max_channel_diff) {
        ++too_large_diffs;
      }
    }
    EXPECT_EQ(too_large_diffs, 0);
    return num_changed_pixel_values;
  }

  void ExpectInterventionHappened() {
    NonThrowableExceptionState exception_state;
    DisableInterventions();
    String data_url_no_interventions =
        CanvasElement().toDataURL("image/png", exception_state);
    base::span<uint8_t> pixels_no_interventions = GetPixels(
        Context2D(), CanvasElement().width(), CanvasElement().height());
    EnableInterventions();
    EXPECT_NE(Context2D()
                  ->Recorder()
                  ->getRecordingCanvas()
                  .HighEntropyCanvasOpTypes(),
              HighEntropyCanvasOpType::kNone);
    String data_url_with_interventions =
        CanvasElement().toDataURL("image/png", exception_state);
    num_readbacks_++;
    EXPECT_NE(data_url_no_interventions, data_url_with_interventions);
    int num_changed_pixel_values =
        GetNumChangedPixels(pixels_no_interventions,
                            GetPixels(Context2D(), CanvasElement().width(),
                                      CanvasElement().height()),
                            /*max_channel_diff=*/3);
    num_readbacks_++;
    EXPECT_GT(num_changed_pixel_values, 0);
  }

  void ExpectInterventionDidNotHappen() {
    NonThrowableExceptionState exception_state;
    DisableInterventions();
    String data_url_no_interventions =
        CanvasElement().toDataURL("image/png", exception_state);
    base::span<uint8_t> pixels_no_interventions = GetPixels(
        Context2D(), CanvasElement().width(), CanvasElement().height());
    EnableInterventions();
    EXPECT_EQ(Context2D()
                  ->Recorder()
                  ->getRecordingCanvas()
                  .HighEntropyCanvasOpTypes(),
              HighEntropyCanvasOpType::kNone);
    String data_url_with_interventions =
        CanvasElement().toDataURL("image/png", exception_state);
    EXPECT_EQ(data_url_no_interventions, data_url_with_interventions);
    EXPECT_EQ(pixels_no_interventions,
              GetPixels(Context2D(), CanvasElement().width(),
                        CanvasElement().height()));
  }

  void DrawSomethingWithTrigger() {
    Context2D()->setShadowBlur(10);
    Context2D()->setShadowColor("red");
    Context2D()->fillRect(0, 0, 10, 10);
  }

  int GetNumReadbacksHappened() { return num_readbacks_; }

  int num_readbacks_ = 0;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform>
      accelerated_compositing_scope_;
  ScopedAccelerated2dCanvasForTest accelerated_canvas_enabled_scope_ = true;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  Persistent<HTMLCanvasElement> canvas_element_;
};

scoped_refptr<StaticBitmapImage> CreateTriggeringSnapshot() {
  SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  auto image = StaticBitmapImage::Create(
      cc::PaintImageBuilder::WithDefault()
          .set_id(cc::PaintImage::GetNextId())
          .set_image(bitmap.asImage(), cc::PaintImage::GetNextContentId())
          .TakePaintImage());

  // Set the high entropy op types and noise token so that noise is
  // applied.
  image->SetHighEntropyCanvasOpTypes(HighEntropyCanvasOpType::kSetShadowBlur |
                                     HighEntropyCanvasOpType::kSetShadowColor);
  return image;
}

enum class ContextType {
  kWindow,
  kDedicatedWorker,
};

class MaybeNoiseSnapshotTest : public DedicatedWorkerTest,
                               public testing::WithParamInterface<ContextType> {
 public:
  MaybeNoiseSnapshotTest() = default;
  ~MaybeNoiseSnapshotTest() override = default;

  void SetUp() override {
    DedicatedWorkerTest::SetUp();
    StartWorker();
    WaitUntilWorkerIsRunning();
  }

  std::string GetOperationTriggeredMetricName() {
    switch (GetParam()) {
      case ContextType::kWindow:
        return base::StrCat({kCanvasOperationMetricName, ".Window"});
      case ContextType::kDedicatedWorker:
        return base::StrCat({kCanvasOperationMetricName, ".DedicatedWorker"});
    }
    NOTREACHED();
  }

  std::string GetReadbacksPerContextMetricName() {
    switch (GetParam()) {
      case ContextType::kWindow:
        return base::StrCat(
            {kCanvasNoiseReadbacksPerContextMetricName, ".Window"});
      case ContextType::kDedicatedWorker:
        return base::StrCat(
            {kCanvasNoiseReadbacksPerContextMetricName, ".DedicatedWorker"});
    }
    NOTREACHED();
  }

  void FakeDestroyHelperContext() {
    auto destroy_context =
        base::BindOnce([](ExecutionContext* execution_context) {
          auto* helper = CanvasInterventionsHelper::From(execution_context);
          helper->ContextDestroyed();
        });

    switch (GetParam()) {
      case ContextType::kWindow:
        std::move(destroy_context).Run(GetFrame().DomWindow());
        break;
      case ContextType::kDedicatedWorker:
        RunOnWorkerThread(CrossThreadBindOnce(std::move(destroy_context)));
        break;
    }
  }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    ,
    MaybeNoiseSnapshotTest,
    testing::Values(ContextType::kWindow, ContextType::kDedicatedWorker),
    [](testing::TestParamInfo<MaybeNoiseSnapshotTest::ParamType> param_info) {
      switch (param_info.param) {
        case ContextType::kWindow:
          return "Window";
        case ContextType::kDedicatedWorker:
          return "DedicatedWorker";
      }
      NOTREACHED();
    });

TEST_P(MaybeNoiseSnapshotTest, NoiseWhenCanvasInterventionsEnabled) {
  base::HistogramTester histogram_tester;

  auto test = base::BindOnce([](ExecutionContext* execution_context) {
    // Enable CanvasInterventions.
    execution_context->SetCanvasNoiseToken(NoiseToken(0x1234567890123456));

    auto snapshot = CreateTriggeringSnapshot();
    auto snapshot_copy = snapshot;
    ASSERT_TRUE(CanvasInterventionsHelper::MaybeNoiseSnapshot(execution_context,
                                                              snapshot));
    EXPECT_NE(snapshot, snapshot_copy);
  });

  switch (GetParam()) {
    case ContextType::kWindow:
      std::move(test).Run(GetFrame().DomWindow());
      break;
    case ContextType::kDedicatedWorker:
      RunOnWorkerThread(CrossThreadBindOnce(std::move(test)));
      break;
  }

  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName,
      static_cast<int>(CanvasNoiseReason::kAllConditionsMet), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 1);
  histogram_tester.ExpectUniqueSample(kCanvasSizeMetricName, 10 * 10, 1);

  histogram_tester.ExpectUniqueSample(
      GetOperationTriggeredMetricName(),
      HighEntropyCanvasOpType::kSetShadowBlur |
          HighEntropyCanvasOpType::kSetShadowColor,
      1);
  histogram_tester.ExpectUniqueSample(
      kCanvasOperationMetricName,
      HighEntropyCanvasOpType::kSetShadowBlur |
          HighEntropyCanvasOpType::kSetShadowColor,
      1);

  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 1);
  histogram_tester.ExpectTotalCount(GetOperationTriggeredMetricName(), 1);

  histogram_tester.ExpectTotalCount(kCanvasNoiseReadbacksPerContextMetricName,
                                    0);
  histogram_tester.ExpectTotalCount(GetReadbacksPerContextMetricName(), 0);

  FakeDestroyHelperContext();
  histogram_tester.ExpectTotalCount(kCanvasNoiseReadbacksPerContextMetricName,
                                    1);
  histogram_tester.ExpectTotalCount(GetReadbacksPerContextMetricName(), 1);
}

TEST_P(MaybeNoiseSnapshotTest, NoNoiseWhenCanvasInterventionsDisabled) {
  base::HistogramTester histogram_tester;

  auto test = base::BindOnce([](ExecutionContext* execution_context) {
    // Disable CanvasInterventions.
    execution_context->SetCanvasNoiseToken(std::nullopt);

    auto snapshot = CreateTriggeringSnapshot();
    auto snapshot_copy = snapshot;
    ASSERT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
        execution_context, snapshot));
    EXPECT_EQ(snapshot, snapshot_copy);
  });

  switch (GetParam()) {
    case ContextType::kWindow:
      std::move(test).Run(GetFrame().DomWindow());
      break;
    case ContextType::kDedicatedWorker:
      RunOnWorkerThread(CrossThreadBindOnce(std::move(test)));
      break;
  }

  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName,
      static_cast<int>(CanvasNoiseReason::kNotEnabledInMode), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  histogram_tester.ExpectTotalCount(GetOperationTriggeredMetricName(), 0);
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
  histogram_tester.ExpectTotalCount(GetReadbacksPerContextMetricName(), 0);
  histogram_tester.ExpectTotalCount(kCanvasNoiseReadbacksPerContextMetricName,
                                    0);
}

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotDoesNotNoiseForCpuCanvas) {
  // Note: This test requires the context's paint canvas to be present as a
  // precondition.
  Context2D()->GetOrCreatePaintCanvas();

  Context2D()->DisableAcceleration();
  base::HistogramTester histogram_tester;

  auto* window = GetFrame().DomWindow();
  EnableInterventions();

  DrawSomethingWithTrigger();
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(window, snapshot));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName, static_cast<int>(CanvasNoiseReason::kNoTrigger),
      1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  EXPECT_EQ(snapshot_copy, snapshot);

  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
}

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotDifferentNoiseTokenNoiseDiffers) {
  base::HistogramTester histogram_tester;
  NonThrowableExceptionState exception_state;

  EnableInterventions();
  DrawSomethingWithTrigger();

  auto* window = GetFrame().DomWindow();
  // Save a copy of the image data to reset.
  base::span<uint8_t> original_noised_pixels = GetNoisedPixels(window);

  // Sanity check to ensure GetNoisedPixels performs the same noising
  // pattern without changing the noise token.
  // This will still update the histogram.
  EXPECT_EQ(original_noised_pixels, GetNoisedPixels(window));

  // Now change the noise token.
  window->SetCanvasNoiseToken(NoiseToken(0xdeadbeef));
  base::span<uint8_t> updated_noised_pixels = GetNoisedPixels(window);

  EXPECT_NE(original_noised_pixels, updated_noised_pixels);

  histogram_tester.ExpectUniqueSample(
      kCanvasOperationMetricName,
      HighEntropyCanvasOpType::kSetShadowBlur |
          HighEntropyCanvasOpType::kSetShadowColor,
      GetNumReadbacksHappened());
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName,
                                    GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, NoTriggerOnFillRect) {
  base::HistogramTester tester;
  V8TestingScope scope;
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillRect(0, 0, 10, 10);
  ExpectInterventionDidNotHappen();
  tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
}

TEST_F(CanvasNoiseTest, TriggerOnShadowBlur) {
  base::HistogramTester tester;
  Context2D()->setShadowBlur(10);
  Context2D()->setShadowColor("red");
  Context2D()->fillRect(0, 0, 10, 10);
  // Two successful readbacks occur in this function, histograms should reflect
  // this.
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           HighEntropyCanvasOpType::kSetShadowBlur |
                               HighEntropyCanvasOpType::kSetShadowColor,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnArc) {
  base::HistogramTester tester;
  NonThrowableExceptionState exception_state;
  Context2D()->beginPath();
  Context2D()->arc(10, 10, 10, 0, 6, false, exception_state);
  Context2D()->stroke();
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           HighEntropyCanvasOpType::kArc,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnEllipse) {
  base::HistogramTester tester;
  NonThrowableExceptionState exception_state;
  Context2D()->beginPath();
  Context2D()->ellipse(10, 10, 5, 7, 3, 0, 3, false, exception_state);
  Context2D()->fill();
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           HighEntropyCanvasOpType::kEllipse,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnSetGlobalCompositeOperation) {
  base::HistogramTester tester;
  V8TestingScope scope;
  Context2D()->setGlobalCompositeOperation("multiply");
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillRect(0, 0, 10, 10);
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           HighEntropyCanvasOpType::kGlobalCompositionOperation,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnFillText) {
  base::HistogramTester tester;
  Context2D()->fillText("CanvasNoiseTest", 20, 20);
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           HighEntropyCanvasOpType::kFillText,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnStrokeText) {
  base::HistogramTester tester;
  Context2D()->strokeText("CanvasNoiseTest", 0, 0);
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           HighEntropyCanvasOpType::kStrokeText,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnFillWithPath2DNoNoise) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  Path2D* canvas_path = Path2D::Create(GetScriptState());
  canvas_path->lineTo(10, 10);
  canvas_path->lineTo(15, 15);
  canvas_path->closePath();
  Context2D()->fill(canvas_path);
  EXPECT_EQ(canvas_path->HighEntropyPathOpTypes(),
            HighEntropyCanvasOpType::kNone);
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
      GetFrame().DomWindow(), snapshot));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName, static_cast<int>(CanvasNoiseReason::kNoTrigger),
      1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  ExpectInterventionDidNotHappen();
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
}

TEST_F(CanvasNoiseTest, TriggerOnFillWithPath2DWithNoise) {
  base::HistogramTester histogram_tester;
  NonThrowableExceptionState exception_state;
  V8TestingScope scope;

  // Note: This test requires the context's paint canvas to be present as a
  // precondition.
  Context2D()->GetOrCreatePaintCanvas();

  Path2D* canvas_path = Path2D::Create(GetScriptState());
  canvas_path->lineTo(10, 10);
  canvas_path->lineTo(15, 15);
  canvas_path->closePath();
  EXPECT_EQ(canvas_path->HighEntropyPathOpTypes(),
            HighEntropyCanvasOpType::kNone);
  canvas_path->arc(10, 10, 10, 0, 6, false, exception_state);
  EXPECT_EQ(canvas_path->HighEntropyPathOpTypes(),
            HighEntropyCanvasOpType::kArc);
  ExpectInterventionDidNotHappen();
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
  Context2D()->fill(canvas_path);
  ExpectInterventionHappened();
  histogram_tester.ExpectBucketCount(kCanvasOperationMetricName,
                                     HighEntropyCanvasOpType::kArc,
                                     GetNumReadbacksHappened());
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName,
                                    GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, OffscreenCanvasNoise) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), 300, 300);
  OffscreenCanvasRenderingContext2D* context =
      static_cast<OffscreenCanvasRenderingContext2D*>(
          host->GetCanvasRenderingContext(
              scope.GetExecutionContext(),
              CanvasRenderingContext::CanvasRenderingAPI::k2D,
              CanvasContextCreationAttributesCore()));
  context->fillText("CanvasNoiseTest", 20, 20);
  EXPECT_EQ(
      context->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kFillText);
  host->GetExecutionContext()->SetCanvasNoiseToken(std::nullopt);
  base::span<uint8_t> pixels_no_interventions =
      GetPixels(context, host->width(), host->height());
  host->GetExecutionContext()->SetCanvasNoiseToken(
      NoiseToken(0x1234567890123456));
  int num_changed_pixel_values =
      GetNumChangedPixels(pixels_no_interventions,
                          GetPixels(context, host->width(), host->height()),
                          /*max_channel_diff=*/3);
  EXPECT_GT(num_changed_pixel_values, 0);
  histogram_tester.ExpectUniqueSample(
      kCanvasOperationMetricName,
      static_cast<int>(HighEntropyCanvasOpType::kFillText), 1);
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 1);
}

TEST_F(CanvasNoiseTest, NoiseDiffersPerSite) {
  base::HistogramTester histogram_tester;

  Context2D()->fillText("CanvasNoiseTest", 20, 20);
  base::span<uint8_t> pixels_test_site =
      GetPixels(Context2D(), CanvasElement().width(), CanvasElement().height());

  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  // Navigate to a different origin.
  NavigateTo(KURL("https://different.example"));
  // Need to re-enable with a different noise token after navigating.
  GetDocument().GetExecutionContext()->SetCanvasNoiseToken(
      NoiseToken(0x43251612612781));

  SetHtmlInnerHTML("<body><canvas id='c' width='300' height='300'></body>");
  UpdateAllLifecyclePhasesForTest();
  auto* diff_canvas_element = To<HTMLCanvasElement>(GetElementById("c"));

  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  attributes.desynchronized = true;
  attributes.premultiplied_alpha = false;
  attributes.will_read_frequently =
      CanvasContextCreationAttributesCore::WillReadFrequently::kFalse;
  auto* diff_context = static_cast<CanvasRenderingContext2D*>(
      diff_canvas_element->GetCanvasRenderingContext(
          GetDocument().GetExecutionContext(),
          /*canvas_type=*/"2d", attributes));

  diff_context->fillText("CanvasNoiseTest", 20, 20);
  // We're taking 2 canvases with different noise applied to them, so the max
  // difference for per pixel value is 6 (= 2 * max noise per channel).
  // Still need to figure out why the noise is higher than expected.
  int num_changed_pixel_values =
      GetNumChangedPixels(pixels_test_site,
                          GetPixels(diff_context, diff_canvas_element->width(),
                                    diff_canvas_element->height()),
                          /*max_channel_diff=*/6);
  EXPECT_GT(num_changed_pixel_values, 0);

  histogram_tester.ExpectUniqueSample(
      kCanvasOperationMetricName,
      static_cast<int>(HighEntropyCanvasOpType::kFillText), 2);
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 2);
}

TEST_F(CanvasNoiseTest, NumberOfNoisedReadbackPerPage) {
  base::HistogramTester histogram_tester;
  NonThrowableExceptionState exception_state;
  DrawSomethingWithTrigger();
  CanvasElement().toDataURL("image/png", exception_state);
  CanvasElement().toDataURL("image/jpeg", exception_state);
  Context2D()->getImageData(0, 0, 10, 10, exception_state);
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
  // Navigate away from page to destroy the execution context.
  NavigateTo(KURL("https://different.example"));
  histogram_tester.ExpectUniqueSample(kCanvasNoiseReadbacksPerContextMetricName,
                                      3, 1);
}

TEST_F(CanvasNoiseTest, NoisedAfterPattern) {
  NonThrowableExceptionState exception_state;
  V8TestingScope scope;
  SetHtmlInnerHTML(
      "<body><canvas id='c1' width='300' height='300'></canvas><canvas id='c2' "
      "width='300' height='300'></canvas</body>");
  UpdateAllLifecyclePhasesForTest();
  auto* canvas_1 = To<HTMLCanvasElement>(GetElementById("c1"));
  auto* canvas_2 = To<HTMLCanvasElement>(GetElementById("c2"));

  CanvasContextCreationAttributesCore attributes;
  auto* context_1 = static_cast<CanvasRenderingContext2D*>(
      canvas_1->GetCanvasRenderingContext(GetDocument().GetExecutionContext(),
                                          "2d", attributes));
  ASSERT_NE(context_1, nullptr);
  auto* context_2 = static_cast<CanvasRenderingContext2D*>(
      canvas_2->GetCanvasRenderingContext(GetDocument().GetExecutionContext(),
                                          "2d", attributes));
  ASSERT_NE(context_2, nullptr);

  CanvasPattern* empty_pattern =
      context_2->createPattern(canvas_1, "repeat", exception_state);
  context_2->setFillStyle(
      GetScriptState()->GetIsolate(),
      ToV8Traits<CanvasPattern>::ToV8(GetScriptState(), empty_pattern),
      exception_state);
  context_2->fillRect(0, 0, 300, 300);
  EXPECT_EQ(
      context_2->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kNone);

  context_1->setShadowBlur(10);
  context_1->setShadowColor("red");
  context_1->fillRect(0, 0, 10, 10);

  EXPECT_EQ(
      context_1->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kSetShadowBlur |
          HighEntropyCanvasOpType::kSetShadowColor);
  EXPECT_EQ(
      context_2->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kNone);

  CanvasPattern* to_be_noised_pattern =
      context_2->createPattern(canvas_1, "repeat", exception_state);
  context_2->setFillStyle(
      GetScriptState()->GetIsolate(),
      ToV8Traits<CanvasPattern>::ToV8(GetScriptState(), to_be_noised_pattern),
      exception_state);
  context_2->fillRect(0, 0, 300, 300);
  EXPECT_EQ(
      context_2->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kSetShadowBlur |
          HighEntropyCanvasOpType::kSetShadowColor |
          HighEntropyCanvasOpType::kCopyFromCanvas);
}

TEST_F(CanvasNoiseTest, NoisedAfterPatternFromOffscreenCanvas) {
  V8TestingScope scope;
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  NonThrowableExceptionState exception_state;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), 300, 300);

  // Note: This test requires the context's paint canvas to be present as a
  // precondition.
  Context2D()->GetOrCreatePaintCanvas();

  OffscreenCanvasRenderingContext2D* context =
      static_cast<OffscreenCanvasRenderingContext2D*>(
          host->GetCanvasRenderingContext(
              scope.GetExecutionContext(),
              CanvasRenderingContext::CanvasRenderingAPI::k2D,
              CanvasContextCreationAttributesCore()));
  context->fillText("CanvasNoiseTest", 20, 20);
  EXPECT_EQ(
      context->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kFillText);
  EXPECT_EQ(
      Context2D()->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kNone);

  CanvasPattern* pattern =
      Context2D()->createPattern(host, "repeat", exception_state);
  EXPECT_EQ(pattern->HighEntropyCanvasOpTypes(),
            HighEntropyCanvasOpType::kFillText);
  Context2D()->setFillStyle(
      script_state->GetIsolate(),
      ToV8Traits<CanvasPattern>::ToV8(script_state, pattern), exception_state);
  Context2D()->fillRect(0, 0, 10, 10);
  EXPECT_EQ(
      Context2D()->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kFillText |
          HighEntropyCanvasOpType::kCopyFromCanvas);
}

TEST_F(CanvasNoiseTest, NoisedAfterPatternOnOffscreenCanvas) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  Context2D()->fillText("CanvasNoiseTest", 20, 20);

  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), 300, 300);
  OffscreenCanvasRenderingContext2D* context =
      static_cast<OffscreenCanvasRenderingContext2D*>(
          host->GetCanvasRenderingContext(
              scope.GetExecutionContext(),
              CanvasRenderingContext::CanvasRenderingAPI::k2D,
              CanvasContextCreationAttributesCore()));
  CanvasPattern* pattern =
      context->createPattern(&CanvasElement(), "repeat", exception_state);
  context->setFillStyle(
      GetScriptState()->GetIsolate(),
      ToV8Traits<CanvasPattern>::ToV8(GetScriptState(), pattern),
      exception_state);
  context->fillRect(0, 0, 10, 10);
  EXPECT_EQ(
      context->Recorder()->getRecordingCanvas().HighEntropyCanvasOpTypes(),
      HighEntropyCanvasOpType::kFillText |
          HighEntropyCanvasOpType::kCopyFromCanvas);
}
}  // namespace blink
