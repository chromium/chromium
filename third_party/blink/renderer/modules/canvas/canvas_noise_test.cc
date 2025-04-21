// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fingerprinting_protection/canvas_noise_token.h"
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
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/path_2d.h"
#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace {

constexpr char kNoiseReasonMetricName[] =
    "FingerprintingProtection.CanvasNoise.InterventionReason";
constexpr char kNoiseDurationMetricName[] =
    "FingerprintingProtection.CanvasNoise.NoiseDuration";
constexpr char kCanvasSizeMetricName[] =
    "FingerprintingProtection.CanvasNoise.NoisedCanvasSize";

}  // namespace

namespace blink {

class CanvasNoiseTest : public PageTestBase {
 public:
  CanvasNoiseTest() : scoped_cpu_test_(true) {}

  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML("<body><canvas id='c' width='300' height='300'></body>");
    UpdateAllLifecyclePhasesForTest();

    canvas_element_ = To<HTMLCanvasElement>(GetElementById("c"));

    CanvasContextCreationAttributesCore attributes;
    attributes.alpha = true;
    attributes.desynchronized = true;
    attributes.premultiplied_alpha = false;
    attributes.will_read_frequently =
        CanvasContextCreationAttributesCore::WillReadFrequently::kFalse;
    auto* context = static_cast<CanvasRenderingContext2D*>(
        canvas_element_->GetCanvasRenderingContext(/*canvas_type=*/"2d",
                                                   attributes));
    PutRandomPixels(context, canvas_element_->width(),
                    canvas_element_->height());
    CanvasNoiseToken::Set(0x1234567890123456);
    EnableInterventions();
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

  base::span<uint8_t> GetNoisedPixelsWithData(ImageData* original_data,
                                              ExecutionContext* ec) {
    NonThrowableExceptionState exception_state;
    Context2D()->putImageData(original_data, 0, 0, exception_state);
    scoped_refptr<StaticBitmapImage> snapshot =
        Context2D()->GetImage(FlushReason::kTesting);
    EXPECT_TRUE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
        Context2D(), ec, snapshot, RasterMode::kGPU));
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

  static int GetNumChangedPixels(
      base::span<uint8_t> pixels_no_interventions,
      base::span<uint8_t> pixels_with_interventions) {
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
      if (diff > 6) {
        ++too_large_diffs;
      }
    }
    EXPECT_EQ(too_large_diffs, 0);
    return num_changed_pixel_values;
  }

  static void PutRandomPixels(BaseRenderingContext2D* context,
                              size_t width,
                              size_t height) {
    size_t canvas_size = width * height * 4;
    std::vector<uint8_t> data(canvas_size);
    for (size_t i = 0; i < canvas_size; ++i) {
      if (i % 4 == 3) {
        data[i] = 255;
        continue;
      }
      data[i] = i % 256;
    }
    NotShared<DOMUint8ClampedArray> data_u8(DOMUint8ClampedArray::Create(data));
    NonThrowableExceptionState exception_state;
    context->putImageData(ImageData::Create(data_u8, width, exception_state), 0,
                          0, exception_state);
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
    EXPECT_NE(data_url_no_interventions, data_url_with_interventions);
    int num_changed_pixel_values = GetNumChangedPixels(
        pixels_no_interventions, GetPixels(Context2D(), CanvasElement().width(),
                                           CanvasElement().height()));
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

  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  Persistent<HTMLCanvasElement> canvas_element_;
  ScopedCanvasInterventionsOnCpuForTestingForTest scoped_cpu_test_;
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

  EXPECT_TRUE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
      Context2D(), window, snapshot, RasterMode::kGPU));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName,
      static_cast<int>(CanvasNoiseReason::kAllConditionsMet), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 1);
  histogram_tester.ExpectUniqueSample(
      kCanvasSizeMetricName, CanvasElement().width() * CanvasElement().height(),
      1);
  EXPECT_NE(snapshot_copy, snapshot);
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

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
      Context2D(), window, snapshot, RasterMode::kGPU));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName,
      static_cast<int>(CanvasNoiseReason::kNotEnabledInMode), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  EXPECT_EQ(snapshot_copy, snapshot);
}

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotDoesNotNoiseForCpuCanvas) {
  ScopedCanvasInterventionsOnCpuForTestingForTest scoped_cpu_test_(false);

  base::HistogramTester histogram_tester;
  auto* window = GetFrame().DomWindow();
  // Enable CanvasInterventions.
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();

  DrawSomethingWithTrigger();
  scoped_refptr<StaticBitmapImage> snapshot =
      Context2D()->GetImage(FlushReason::kTesting);
  scoped_refptr<StaticBitmapImage> snapshot_copy = snapshot;

  EXPECT_FALSE(CanvasInterventionsHelper::MaybeNoiseSnapshot(
      Context2D(), window, snapshot, RasterMode::kCPU));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName, static_cast<int>(CanvasNoiseReason::kNoGpu), 1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  EXPECT_EQ(snapshot_copy, snapshot);
}

TEST_F(CanvasNoiseTest, MaybeNoiseSnapshotDifferentNoiseTokenNoiseDiffers) {
  NonThrowableExceptionState exception_state;

  auto* window = GetFrame().DomWindow();
  window->GetRuntimeFeatureStateOverrideContext()
      ->SetCanvasInterventionsForceEnabled();
  DrawSomethingWithTrigger();

  // Save a copy of the image data to reset.
  ImageData* copy_image_data = Context2D()->getImageData(
      0, 0, CanvasElement().width(), CanvasElement().height(), exception_state);
  base::span<uint8_t> original_noised_pixels =
      GetNoisedPixelsWithData(copy_image_data, window);

  // Sanity check to ensure GetNoisedPixelsWithData performs the same noising
  // pattern without changing the noise token.
  EXPECT_EQ(original_noised_pixels,
            GetNoisedPixelsWithData(copy_image_data, window));

  // Now change the noise token.
  CanvasNoiseToken::Set(0xdeadbeef);
  base::span<uint8_t> updated_noised_pixels =
      GetNoisedPixelsWithData(copy_image_data, window);

  EXPECT_NE(original_noised_pixels, updated_noised_pixels);
}

TEST_F(CanvasNoiseTest, NoTriggerOnFillRect) {
  V8TestingScope scope;
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillRect(0, 0, 10, 10);
  ExpectInterventionDidNotHappen();
}

TEST_F(CanvasNoiseTest, TriggerOnShadowBlur) {
  Context2D()->setShadowBlur(10);
  Context2D()->setShadowColor("red");
  Context2D()->fillRect(0, 0, 10, 10);
  ExpectInterventionHappened();
}

TEST_F(CanvasNoiseTest, TriggerOnArc) {
  NonThrowableExceptionState exception_state;
  Context2D()->beginPath();
  Context2D()->arc(10, 10, 10, 0, 6, false, exception_state);
  Context2D()->stroke();
  ExpectInterventionHappened();
}

TEST_F(CanvasNoiseTest, TriggerOnEllipse) {
  NonThrowableExceptionState exception_state;
  Context2D()->beginPath();
  Context2D()->ellipse(10, 10, 5, 7, 3, 0, 3, false, exception_state);
  Context2D()->fill();
  ExpectInterventionHappened();
}

TEST_F(CanvasNoiseTest, TriggerOnSetGlobalCompositeOperation) {
  V8TestingScope scope;
  Context2D()->setGlobalCompositeOperation("multiply");
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillRect(0, 0, 10, 10);
  ExpectInterventionHappened();
}

TEST_F(CanvasNoiseTest, TriggerOnFillText) {
  Context2D()->fillText("CanvasNoiseTest", 0, 0);
  ExpectInterventionHappened();
}

TEST_F(CanvasNoiseTest, TriggerOnStrokeText) {
  Context2D()->strokeText("CanvasNoiseTest", 0, 0);
  ExpectInterventionHappened();
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
      Context2D(), GetFrame().DomWindow(), snapshot, RasterMode::kGPU));
  histogram_tester.ExpectUniqueSample(
      kNoiseReasonMetricName, static_cast<int>(CanvasNoiseReason::kNoTrigger),
      1);
  histogram_tester.ExpectTotalCount(kNoiseDurationMetricName, 0);
  histogram_tester.ExpectTotalCount(kCanvasSizeMetricName, 0);
  ExpectInterventionDidNotHappen();
}

TEST_F(CanvasNoiseTest, TriggerOnFillWithPath2DWithNoise) {
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
  Context2D()->fill(canvas_path);
  ExpectInterventionHappened();
}

TEST_F(CanvasNoiseTest, OffscreenCanvasNoise) {
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), 300, 300);
  OffscreenCanvasRenderingContext2D* context =
      static_cast<OffscreenCanvasRenderingContext2D*>(
          host->GetCanvasRenderingContext(
              scope.GetExecutionContext(),
              CanvasRenderingContext::CanvasRenderingAPI::k2D,
              CanvasContextCreationAttributesCore()));
  PutRandomPixels(context, host->width(), host->height());
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
                          GetPixels(context, host->width(), host->height()));
  EXPECT_GT(num_changed_pixel_values, 0);
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
