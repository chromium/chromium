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
#include "third_party/blink/public/common/fingerprinting_protection/canvas_noise_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float16array_float32array_uint8clampedarray.h"
#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_enums.h"
#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

// Raster interface that always returns the same randomized image when read
// back.
class CanvasNoiseTestRasterInterface : public viz::TestRasterInterface {
 public:
  CanvasNoiseTestRasterInterface() { set_gpu_rasterization(true); }

 private:
  UNSAFE_BUFFER_USAGE bool ReadbackImagePixels(
      const gpu::Mailbox& source_mailbox,
      const SkImageInfo& dst_info,
      GLuint dst_row_bytes,
      int src_x,
      int src_y,
      int plane_index,
      void* dst_pixels) override {
    size_t size = dst_info.computeByteSize(dst_row_bytes);
    uint8_t* data = static_cast<uint8_t*>(dst_pixels);
    for (size_t i = 0; i < size; ++i) {
      data[i] = (i % 4 == 3) ? 255 : i % 256;
    }
    return true;
  }
};

class CanvasNoiseTest : public PageTestBase {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster(
        std::make_unique<CanvasNoiseTestRasterInterface>());
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
    canvas_element_->GetCanvasRenderingContext(/*canvas_type=*/"2d",
                                               attributes);
    canvas_element_->GetOrCreateCanvasResourceProvider();
    CanvasNoiseToken::Set(0x1234567890123456);
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
    GetFrame()
        .DomWindow()
        ->GetRuntimeFeatureStateOverrideContext()
        ->SetCanvasInterventionsForceDisabled();
  }

  void EnableInterventions() {
    GetFrame()
        .DomWindow()
        ->GetRuntimeFeatureStateOverrideContext()
        ->SetCanvasInterventionsForceEnabled();
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
    EXPECT_TRUE(Context2D()->HasTriggerForIntervention());
    EXPECT_TRUE(Context2D()->ShouldTriggerIntervention());
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
    EXPECT_FALSE(Context2D()->HasTriggerForIntervention());
    EXPECT_FALSE(Context2D()->ShouldTriggerIntervention());
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

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotNoiseWhenCanvasInterventionsEnabled) {
  base::HistogramTester histogram_tester;

  auto* window = GetFrame().DomWindow();
  // Enable CanvasInterventions.
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();

  DrawSomethingWithTrigger();
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_TRUE(CanvasInterventionsHelper::MaybeNoiseSnapshot(Context2D(), window,
                                                            snapshot));
  num_readbacks_++;
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName,
      static_cast<int>(CanvasNoiseReason::kAllConditionsMet), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kCanvasSizeMetricName, CanvasElement().width() * CanvasElement().height(),
      1);
  EXPECT_NE(snapshot_copy, snapshot);

  histogram_tester.ExpectUniqueSample(kCanvasOperationMetricName,
                                      CanvasOperationType::kSetShadowBlur |
                                          CanvasOperationType::kSetShadowColor,
                                      GetNumReadbacksHappened());
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName,
                                    GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest,
       MaybeNoiseSnapshotDoesNotNoiseWhenCanvasInterventionsDisabled) {
  base::HistogramTester histogram_tester;

  auto* window = GetFrame().DomWindow();
  // Disable CanvasInterventions.
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceDisabled();

  DrawSomethingWithTrigger();
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(Context2D(),
                                                             window, snapshot));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName,
      static_cast<int>(CanvasNoiseReason::kNotEnabledInMode), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  EXPECT_EQ(snapshot_copy, snapshot);

  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
}

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotDoesNotNoiseForCpuCanvas) {
  CanvasElement().DisableAcceleration();
  base::HistogramTester histogram_tester;

  auto* window = GetFrame().DomWindow();
  // Enable CanvasInterventions.
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();

  DrawSomethingWithTrigger();
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(Context2D(),
                                                             window, snapshot));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName, static_cast<int>(CanvasNoiseReason::kNoGpu), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  EXPECT_EQ(snapshot_copy, snapshot);

  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
}

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotDifferentNoiseTokenNoiseDiffers) {
  base::HistogramTester histogram_tester;
  NonThrowableExceptionState exception_state;

  auto* window = GetFrame().DomWindow();
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();
  DrawSomethingWithTrigger();

  // Save a copy of the image data to reset.
  base::span<uint8_t> original_noised_pixels = GetNoisedPixels(window);

  // Sanity check to ensure GetNoisedPixels performs the same noising
  // pattern without changing the noise token.
  // This will still update the histogram.
  EXPECT_EQ(original_noised_pixels, GetNoisedPixels(window));

  // Now change the noise token.
  CanvasNoiseToken::Set(0xdeadbeef);
  base::span<uint8_t> updated_noised_pixels = GetNoisedPixels(window);

  EXPECT_NE(original_noised_pixels, updated_noised_pixels);

  histogram_tester.ExpectUniqueSample(kCanvasOperationMetricName,
                                      CanvasOperationType::kSetShadowBlur |
                                          CanvasOperationType::kSetShadowColor,
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
                           CanvasOperationType::kSetShadowBlur |
                               CanvasOperationType::kSetShadowColor,
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
                           CanvasOperationType::kArc,
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
                           CanvasOperationType::kEllipse,
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
                           CanvasOperationType::kGlobalCompositionOperation,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnFillText) {
  base::HistogramTester tester;
  Context2D()->fillText("CanvasNoiseTest", 0, 0);
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           CanvasOperationType::kFillText,
                           GetNumReadbacksHappened());
  tester.ExpectTotalCount(kCanvasOperationMetricName,
                          GetNumReadbacksHappened());
}

TEST_F(CanvasNoiseTest, TriggerOnStrokeText) {
  base::HistogramTester tester;
  Context2D()->strokeText("CanvasNoiseTest", 0, 0);
  ExpectInterventionHappened();
  tester.ExpectBucketCount(kCanvasOperationMetricName,
                           CanvasOperationType::kStrokeText,
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
  EXPECT_FALSE(canvas_path->HasTriggerForIntervention());
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
      Context2D(), GetFrame().DomWindow(), snapshot));
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
  Path2D* canvas_path = Path2D::Create(GetScriptState());
  canvas_path->lineTo(10, 10);
  canvas_path->lineTo(15, 15);
  canvas_path->closePath();
  EXPECT_FALSE(canvas_path->HasTriggerForIntervention());
  canvas_path->arc(10, 10, 10, 0, 6, false, exception_state);
  EXPECT_TRUE(canvas_path->HasTriggerForIntervention());
  ExpectInterventionDidNotHappen();
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 0);
  Context2D()->fill(canvas_path);
  ExpectInterventionHappened();
  histogram_tester.ExpectBucketCount(kCanvasOperationMetricName,
                                     CanvasOperationType::kArc,
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
  context->fillText("CanvasNoiseTest", 0, 0);
  EXPECT_TRUE(context->HasTriggerForIntervention());
  EXPECT_TRUE(context->ShouldTriggerIntervention());
  host->GetExecutionContext()
      ->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceDisabled();
  base::span<uint8_t> pixels_no_interventions =
      GetPixels(context, host->width(), host->height());
  host->GetExecutionContext()
      ->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();
  int num_changed_pixel_values =
      GetNumChangedPixels(pixels_no_interventions,
                          GetPixels(context, host->width(), host->height()),
                          /*max_channel_diff=*/3);
  EXPECT_GT(num_changed_pixel_values, 0);
  histogram_tester.ExpectUniqueSample(
      kCanvasOperationMetricName,
      static_cast<int>(CanvasOperationType::kFillText), 1);
  histogram_tester.ExpectTotalCount(kCanvasOperationMetricName, 1);
}

TEST_F(CanvasNoiseTest, NoiseDiffersPerSite) {
  base::HistogramTester histogram_tester;

  Context2D()->fillText("CanvasNoiseTest", 0, 0);
  base::span<uint8_t> pixels_test_site =
      GetPixels(Context2D(), CanvasElement().width(), CanvasElement().height());

  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  // Navigate to a different origin.
  NavigateTo(KURL("https://different.example"));
  // Need to re-enable after navigating.
  EnableInterventions();

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
      diff_canvas_element->GetCanvasRenderingContext(/*canvas_type=*/"2d",
                                                     attributes));

  diff_context->fillText("CanvasNoiseTest", 0, 0);
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
      static_cast<int>(CanvasOperationType::kFillText), 2);
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
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.CanvasNoise.NoisedReadbacksPerContext", 3, 1);
}

}  // namespace blink
