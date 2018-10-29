// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/third_party/skcms/skcms.h"

using testing::_;
using testing::InSequence;
using testing::Mock;

namespace blink {

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(IntSize, BitmapOpacity);

  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               const FloatSize&) override;

  bool WouldTaintOrigin(
      const SecurityOrigin* destination_security_origin) const override {
    return false;
  }
  FloatSize ElementSize(const FloatSize&) const override {
    return FloatSize(size_);
  }
  bool IsOpaque() const override { return is_opaque_; }
  bool IsAccelerated() const override { return false; }

  ~FakeImageSource() override = default;

 private:
  IntSize size_;
  scoped_refptr<Image> image_;
  bool is_opaque_;
};

FakeImageSource::FakeImageSource(IntSize size, BitmapOpacity opacity)
    : size_(size), is_opaque_(opacity == kOpaqueBitmap) {
  sk_sp<SkSurface> surface(
      SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height()));
  surface->getCanvas()->clear(opacity == kOpaqueBitmap ? SK_ColorWHITE
                                                       : SK_ColorTRANSPARENT);
  image_ = StaticBitmapImage::Create(surface->makeImageSnapshot());
}

scoped_refptr<Image> FakeImageSource::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    const FloatSize&) {
  if (status)
    *status = kNormalSourceImageStatus;
  return image_;
}

//============================================================================

enum LinearPixelMathState { kLinearPixelMathDisabled, kLinearPixelMathEnabled };

class CanvasRenderingContext2DTest : public PageTestBase {
 protected:
  CanvasRenderingContext2DTest();
  void SetUp() override;

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  bool IsCanvasResourceHostSet(Canvas2DLayerBridge* bridge) {
    return !!bridge->resource_host_;
  }
  CanvasRenderingContext2D* Context2d() const {
    return static_cast<CanvasRenderingContext2D*>(
        CanvasElement().RenderingContext());
  }
  intptr_t GetGlobalGPUMemoryUsage() const {
    return HTMLCanvasElement::GetGlobalGPUMemoryUsage();
  }
  unsigned GetGlobalAcceleratedContextCount() const {
    return HTMLCanvasElement::GetGlobalAcceleratedContextCount();
  }
  intptr_t GetCurrentGPUMemoryUsage() const {
    return CanvasElement().GetGPUMemoryUsage();
  }
  void DrawSomething() {
    CanvasElement().DidDraw();
    CanvasElement().FinalizeFrame();
    // Grabbing an image forces a flush
    CanvasElement().Snapshot(kBackBuffer, kPreferAcceleration);
  }

  enum LatencyMode { kNormalLatency, kLowLatency };

  void CreateContext(OpacityMode, LatencyMode = kNormalLatency);
  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(canvas_element_->GetFrame());
  }

  void TearDown() override;
  void UnrefCanvas();
  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(
      const IntSize&,
      Canvas2DLayerBridge::AccelerationMode);

 private:
  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<MemoryCache> global_memory_cache_;

  class WrapGradients final : public GarbageCollectedFinalized<WrapGradients> {
   public:
    static WrapGradients* Create() { return new WrapGradients; }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(opaque_gradient_);
      visitor->Trace(alpha_gradient_);
    }

    StringOrCanvasGradientOrCanvasPattern opaque_gradient_;
    StringOrCanvasGradientOrCanvasPattern alpha_gradient_;
  };

  // TODO(Oilpan): avoid tedious part-object wrapper by supporting on-heap
  // testing::Tests.
  Persistent<WrapGradients> wrap_gradients_;

 protected:
  // Pre-canned objects for testing
  Persistent<ImageData> full_image_data_;
  Persistent<ImageData> partial_image_data_;
  FakeImageSource opaque_bitmap_;
  FakeImageSource alpha_bitmap_;
  FakeGLES2Interface gl_;

  // Set this to override frame settings.
  FrameSettingOverrideFunction override_settings_function_ = nullptr;

  StringOrCanvasGradientOrCanvasPattern& OpaqueGradient() {
    return wrap_gradients_->opaque_gradient_;
  }
  StringOrCanvasGradientOrCanvasPattern& AlphaGradient() {
    return wrap_gradients_->alpha_gradient_;
  }
};

CanvasRenderingContext2DTest::CanvasRenderingContext2DTest()
    : wrap_gradients_(WrapGradients::Create()),
      opaque_bitmap_(IntSize(10, 10), kOpaqueBitmap),
      alpha_bitmap_(IntSize(10, 10), kTransparentBitmap) {}

void CanvasRenderingContext2DTest::CreateContext(OpacityMode opacity_mode,
                                                 LatencyMode latency_mode) {
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = opacity_mode == kNonOpaque;
  attributes.low_latency = latency_mode == kLowLatency;
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
}

void CanvasRenderingContext2DTest::SetUp() {
  auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    *gpu_compositing_disabled = false;
    gl->SetIsContextLost(false);
    return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
  };
  SharedGpuContext::SetContextProviderFactoryForTesting(
      WTF::BindRepeating(factory, WTF::Unretained(&gl_)));

  PageTestBase::SetUp();
  // Simulate that we allow scripts, so that HTMLCanvasElement uses
  // LayoutHTMLCanvas.
  GetPage().GetSettings().SetScriptEnabled(true);

  SetHtmlInnerHTML(
      "<body><canvas id='c'></canvas><canvas id='d'></canvas></body>");
  canvas_element_ = ToHTMLCanvasElement(GetElementById("c"));

  full_image_data_ = ImageData::Create(IntSize(10, 10));
  partial_image_data_ = ImageData::Create(IntSize(2, 2));

  NonThrowableExceptionState exception_state;
  CanvasGradient* opaque_gradient =
      CanvasGradient::Create(FloatPoint(0, 0), FloatPoint(10, 0));
  opaque_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  opaque_gradient->addColorStop(1, String("blue"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  this->OpaqueGradient().SetCanvasGradient(opaque_gradient);

  CanvasGradient* alpha_gradient =
      CanvasGradient::Create(FloatPoint(0, 0), FloatPoint(10, 0));
  alpha_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  alpha_gradient->addColorStop(1, String("rgba(0, 0, 255, 0.5)"),
                               exception_state);
  EXPECT_FALSE(exception_state.HadException());
  StringOrCanvasGradientOrCanvasPattern wrapped_alpha_gradient;
  this->AlphaGradient().SetCanvasGradient(alpha_gradient);

  global_memory_cache_ = ReplaceMemoryCacheForTesting(MemoryCache::Create(
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
}

void CanvasRenderingContext2DTest::TearDown() {
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGC);
  ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  SharedGpuContext::ResetForTesting();
}

std::unique_ptr<Canvas2DLayerBridge> CanvasRenderingContext2DTest::MakeBridge(
    const IntSize& size,
    Canvas2DLayerBridge::AccelerationMode acceleration_mode) {
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>(size, acceleration_mode,
                                            CanvasColorParams());
  bridge->SetCanvasResourceHost(canvas_element_);
  return bridge;
}

//============================================================================

class FakeCanvas2DLayerBridge : public Canvas2DLayerBridge {
 public:
  FakeCanvas2DLayerBridge(const IntSize& size,
                          CanvasColorParams color_params,
                          AccelerationHint hint)
      : Canvas2DLayerBridge(size, kDisableAcceleration, color_params),
        is_accelerated_(hint != kPreferNoAcceleration) {}
  ~FakeCanvas2DLayerBridge() override = default;
  bool IsAccelerated() const override { return is_accelerated_; }
  void SetIsAccelerated(bool is_accelerated) {
    if (is_accelerated != is_accelerated_)
      is_accelerated_ = is_accelerated;
  }
  MOCK_METHOD1(DrawFullImage, void(const PaintImage& image));
  MOCK_METHOD1(DidRestoreCanvasMatrixClipStack, void(cc::PaintCanvas*));

 private:
  bool is_accelerated_;
};

//============================================================================

class FakeCanvasResourceProvider : public CanvasResourceProvider {
 public:
  FakeCanvasResourceProvider(const IntSize& size,
                             CanvasColorParams color_params,
                             AccelerationHint hint)
      : CanvasResourceProvider(size, color_params, nullptr, nullptr),
        is_accelerated_(hint != kPreferNoAcceleration) {}
  ~FakeCanvasResourceProvider() override = default;
  bool IsAccelerated() const override { return is_accelerated_; }
  scoped_refptr<CanvasResource> ProduceFrame() override {
    return scoped_refptr<CanvasResource>();
  }
  bool SupportsDirectCompositing() const override { return false; }
  bool IsValid() const override { return false; }
  sk_sp<SkSurface> CreateSkSurface() const override {
    return sk_sp<SkSurface>();
  }

 private:
  bool is_accelerated_;
};

//============================================================================

class MockImageBufferSurfaceForOverwriteTesting : public Canvas2DLayerBridge {
 public:
  MockImageBufferSurfaceForOverwriteTesting(const IntSize& size,
                                            CanvasColorParams color_params)
      : Canvas2DLayerBridge(size,
                            Canvas2DLayerBridge::kDisableAcceleration,
                            color_params) {}
  ~MockImageBufferSurfaceForOverwriteTesting() override = default;
  MOCK_METHOD0(WillOverwriteCanvas, void());
};

//============================================================================

#define TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                                \
  IntSize size(10, 10);                                                        \
  std::unique_ptr<MockImageBufferSurfaceForOverwriteTesting> mock_surface =    \
      std::make_unique<MockImageBufferSurfaceForOverwriteTesting>(             \
          size, CanvasColorParams());                                          \
  MockImageBufferSurfaceForOverwriteTesting* surface_ptr = mock_surface.get(); \
  CanvasElement().SetResourceProviderForTesting(                               \
      nullptr, std::move(mock_surface), size);                                 \
  EXPECT_CALL(*surface_ptr, WillOverwriteCanvas()).Times(EXPECTED_OVERDRAWS);  \
  Context2d()->save();

#define TEST_OVERDRAW_FINALIZE \
  Context2d()->restore();      \
  Mock::VerifyAndClearExpectations(surface_ptr);

#define TEST_OVERDRAW_1(EXPECTED_OVERDRAWS, CALL1) \
  do {                                             \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)        \
    Context2d()->CALL1;                            \
    TEST_OVERDRAW_FINALIZE                         \
  } while (0)

#define TEST_OVERDRAW_2(EXPECTED_OVERDRAWS, CALL1, CALL2) \
  do {                                                    \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)               \
    Context2d()->CALL1;                                   \
    Context2d()->CALL2;                                   \
    TEST_OVERDRAW_FINALIZE                                \
  } while (0)

#define TEST_OVERDRAW_3(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3) \
  do {                                                           \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                      \
    Context2d()->CALL1;                                          \
    Context2d()->CALL2;                                          \
    Context2d()->CALL3;                                          \
    TEST_OVERDRAW_FINALIZE                                       \
  } while (0)

#define TEST_OVERDRAW_4(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3, CALL4) \
  do {                                                                  \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                             \
    Context2d()->CALL1;                                                 \
    Context2d()->CALL2;                                                 \
    Context2d()->CALL3;                                                 \
    Context2d()->CALL4;                                                 \
    TEST_OVERDRAW_FINALIZE                                              \
  } while (0)

//============================================================================

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithFillRect) {
  CreateContext(kNonOpaque);

  TEST_OVERDRAW_1(1, fillRect(-1, -1, 12, 12));
  TEST_OVERDRAW_1(1, fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_1(
      0, strokeRect(0, 0, 10,
                    10));  // stroking instead of filling does not overwrite
  TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_1(0, fillRect(0, 0, 9, 9));
  TEST_OVERDRAW_2(0, translate(1, 1), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, translate(1, 1), fillRect(-1, -1, 10, 10));
  TEST_OVERDRAW_2(1, setFillStyle(OpaqueGradient()), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(0, setFillStyle(AlphaGradient()), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(0, setGlobalAlpha(0.5), setFillStyle(OpaqueGradient()),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 9, 9));
  TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_4(0, rect(0, 0, 5, 5), clip(),
                  setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithClearRect) {
  CreateContext(kNonOpaque);

  TEST_OVERDRAW_1(1, clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_1(0, clearRect(0, 0, 9, 9));
  TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f), clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setFillStyle(AlphaGradient()), clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(0, translate(1, 1), clearRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, translate(1, 1), clearRect(-1, -1, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("destination-in")),
                  clearRect(0, 0, 10, 10));  // composite op ignored
  TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), clearRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithDrawImage) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  TEST_OVERDRAW_1(1, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               0, 0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(1, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 1, 1, 0,
                               0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, drawImage(GetScriptState(), &alpha_bitmap_, 0, 0, 10, 10,
                               0, 0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f),
                  drawImage(GetScriptState(), &alpha_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               1, 0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               0, 0, 9, 9, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(1, drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10,
                               0, 0, 11, 11, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, translate(-1, 0),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 1,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(0, translate(-1, 0),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(
      0, setFillStyle(OpaqueGradient()),
      drawImage(GetScriptState(), &alpha_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                exception_state));  // fillStyle ignored by drawImage
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(
      1, setFillStyle(AlphaGradient()),
      drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                exception_state));  // fillStyle ignored by drawImage
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 1,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(),
                  drawImage(GetScriptState(), &opaque_bitmap_, 0, 0, 10, 10, 0,
                            0, 10, 10, exception_state));
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithPutImageData) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  // Test putImageData
  TEST_OVERDRAW_1(1,
                  putImageData(full_image_data_.Get(), 0, 0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(1, putImageData(full_image_data_.Get(), 0, 0, 0, 0, 10, 10,
                                  exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0, putImageData(full_image_data_.Get(), 0, 0, 1, 1, 8, 8,
                                  exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f),
                  putImageData(full_image_data_.Get(), 0, 0,
                               exception_state));  // alpha has no effect
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(
      0, putImageData(partial_image_data_.Get(), 0, 0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_2(1, translate(1, 1),
                  putImageData(full_image_data_.Get(), 0, 0,
                               exception_state));  // ignores tranforms
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_1(0,
                  putImageData(full_image_data_.Get(), 1, 0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  TEST_OVERDRAW_3(1, rect(0, 0, 5, 5), clip(),
                  putImageData(full_image_data_.Get(), 0, 0,
                               exception_state));  // ignores clip
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithCompositeOperations) {
  CreateContext(kNonOpaque);

  // Test composite operators with an opaque rect that covers the entire canvas
  // Note: all the untested composite operations take the same code path as
  // source-in, which assumes that the destination may not be overwritten
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("clear")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("source-over")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")),
                  fillRect(0, 0, 10, 10));
  // Test composite operators with a transparent rect that covers the entire
  // canvas
  TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("clear")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("source-over")),
                  fillRect(0, 0, 10, 10));
  TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f),
                  setGlobalCompositeOperation(String("source-in")),
                  fillRect(0, 0, 10, 10));
  // Test composite operators with an opaque rect that does not cover the entire
  // canvas
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("clear")),
                  fillRect(0, 0, 5, 5));
  TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")),
                  fillRect(0, 0, 5, 5));
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-over")),
                  fillRect(0, 0, 5, 5));
  TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")),
                  fillRect(0, 0, 5, 5));
}

TEST_F(CanvasRenderingContext2DTest, ImageResourceLifetime) {
  auto* canvas =
      ToHTMLCanvasElement(GetDocument().CreateRawElement(HTMLNames::canvasTag));
  canvas->SetSize(IntSize(40, 40));
  ImageBitmap* image_bitmap_derived = nullptr;
  {
    const ImageBitmapOptions default_options;
    base::Optional<IntRect> crop_rect =
        IntRect(0, 0, canvas->width(), canvas->height());
    ImageBitmap* image_bitmap_from_canvas =
        ImageBitmap::Create(canvas, crop_rect, default_options);
    ASSERT_TRUE(image_bitmap_from_canvas);

    crop_rect = IntRect(0, 0, 20, 20);
    image_bitmap_derived = ImageBitmap::Create(image_bitmap_from_canvas,
                                               crop_rect, default_options);
    ASSERT_TRUE(image_bitmap_derived);
  }
  CanvasContextCreationAttributesCore attributes;
  CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(
      canvas->GetCanvasRenderingContext("2d", attributes));
  DummyExceptionStateForTesting exception_state;
  CanvasImageSourceUnion image_source;
  image_source.SetImageBitmap(image_bitmap_derived);
  context->drawImage(GetScriptState(), image_source, 0, 0, exception_state);
}

TEST_F(CanvasRenderingContext2DTest, GPUMemoryUpdateForAcceleratedCanvas) {
  CreateContext(kNonOpaque);

  IntSize size(10, 10);
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider =
      std::make_unique<FakeCanvasResourceProvider>(size, CanvasColorParams(),
                                                   kPreferAcceleration);
  std::unique_ptr<FakeCanvas2DLayerBridge> fake_2d_layer_bridge =
      std::make_unique<FakeCanvas2DLayerBridge>(size, CanvasColorParams(),
                                                kPreferAcceleration);
  FakeCanvas2DLayerBridge* fake_2d_layer_bridge_ptr =
      fake_2d_layer_bridge.get();
  CanvasElement().SetResourceProviderForTesting(
      std::move(fake_resource_provider), std::move(fake_2d_layer_bridge), size);

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(800, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  // Switching accelerated mode to non-accelerated mode
  fake_2d_layer_bridge_ptr->SetIsAccelerated(false);
  CanvasElement().UpdateMemoryUsage();
  EXPECT_EQ(0, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());

  // Switching non-accelerated mode to accelerated mode
  fake_2d_layer_bridge_ptr->SetIsAccelerated(true);
  CanvasElement().UpdateMemoryUsage();
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(800, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  // Creating a different accelerated image buffer
  HTMLCanvasElement* anotherCanvas =
      ToHTMLCanvasElement(GetDocument().getElementById("d"));
  CanvasContextCreationAttributesCore attributes;
  anotherCanvas->GetCanvasRenderingContext("2d", attributes);
  IntSize size2(10, 5);
  std::unique_ptr<FakeCanvas2DLayerBridge> fake_2d_layer_bridge2 =
      std::make_unique<FakeCanvas2DLayerBridge>(size2, CanvasColorParams(),
                                                kPreferAcceleration);
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider2 =
      std::make_unique<FakeCanvasResourceProvider>(size2, CanvasColorParams(),
                                                   kPreferAcceleration);
  anotherCanvas->SetResourceProviderForTesting(
      std::move(fake_resource_provider2), std::move(fake_2d_layer_bridge2),
      size2);
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(1200, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(2u, GetGlobalAcceleratedContextCount());

  // Tear down the first image buffer that resides in current canvas element
  CanvasElement().SetSize(IntSize(20, 20));
  Mock::VerifyAndClearExpectations(fake_2d_layer_bridge_ptr);
  EXPECT_EQ(400, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  // Tear down the second image buffer
  anotherCanvas->SetSize(IntSize(20, 20));
  EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
}

TEST_F(CanvasRenderingContext2DTest, CanvasDisposedBeforeContext) {
  CreateContext(kNonOpaque);
  Context2d()->fillRect(0, 0, 1, 1);  // results in task observer registration

  Context2d()->DetachHost();

  // This is the only method that is callable after DetachHost
  // Test passes by not crashing.
  base::PendingTask dummy_pending_task(FROM_HERE, base::Closure());
  Context2d()->DidProcessTask(dummy_pending_task);

  // Test passes by not crashing during teardown
}

TEST_F(CanvasRenderingContext2DTest, ContextDisposedBeforeCanvas) {
  CreateContext(kNonOpaque);

  CanvasElement().DetachContext();
  // Passes by not crashing later during teardown
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_GetImageDataDisablesAcceleration \
  DISABLED_GetImageDataDisablesAcceleration
#else
#define MAYBE_GetImageDataDisablesAcceleration GetImageDataDisablesAcceleration
#endif

TEST_F(CanvasRenderingContext2DTest, MAYBE_GetImageDataDisablesAcceleration) {
  ScopedCanvas2dFixedRenderingModeForTest canvas_2d_fixed_rendering_mode(false);

  // This Page is not actually being shown by a compositor, but we act like it
  // will in order to test behaviour.
  GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, Canvas2DLayerBridge::kForceAccelerationForTesting);
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);
  DrawSomething();  // Lock-in gpu acceleration
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
  EXPECT_EQ(720000, GetGlobalGPUMemoryUsage());

  DummyExceptionStateForTesting exception_state;
  for (int i = 0;
       i < canvas_heuristic_parameters::kGPUReadbackMinSuccessiveFrames - 1;
       i++) {
    Context2d()->getImageData(0, 0, 1, 1, exception_state);
    CanvasElement().FinalizeFrame();

    EXPECT_FALSE(exception_state.HadException());
    EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
    EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
    EXPECT_EQ(720000, GetGlobalGPUMemoryUsage());
  }

  Context2d()->getImageData(0, 0, 1, 1, exception_state);
  CanvasElement().FinalizeFrame();

  EXPECT_FALSE(exception_state.HadException());
  if (canvas_heuristic_parameters::kGPUReadbackForcesNoAcceleration) {
    EXPECT_FALSE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
    EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
    EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  } else {
    EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
    EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());
    EXPECT_EQ(720000, GetGlobalGPUMemoryUsage());
  }
}

TEST_F(CanvasRenderingContext2DTest,
       NoResourceProviderInCanvas2DBufferInitialization) {
  // This test enforces that there is no eager creation of
  // CanvasResourceProvider for html canvas with 2d context when its
  // Canvas2DLayerBridge is initially set up. This enforcement might be changed
  // in the future refactoring; but change is seriously warned against because
  // certain code paths in canvas 2d (that depend on the existence of
  // CanvasResourceProvider) will be changed too, causing bad regressions.
  CreateContext(kNonOpaque);
  IntSize size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), kPreferAcceleration);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge());
  EXPECT_FALSE(CanvasElement().ResourceProvider());
}

TEST_F(CanvasRenderingContext2DTest,
       DISABLED_DisableAcceleration_UpdateGPUMemoryUsage) {
  CreateContext(kNonOpaque);

  IntSize size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), kPreferAcceleration);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);
  CanvasRenderingContext2D* context = Context2d();

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required
  EXPECT_EQ(800, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(800, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(1u, GetGlobalAcceleratedContextCount());

  context->fillRect(10, 10, 100, 100);
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());

  CanvasElement().DisableAcceleration();
  EXPECT_FALSE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());

  context->fillRect(10, 10, 100, 100);

  EXPECT_EQ(0, GetCurrentGPUMemoryUsage());
  EXPECT_EQ(0, GetGlobalGPUMemoryUsage());
  EXPECT_EQ(0u, GetGlobalAcceleratedContextCount());
}

TEST_F(CanvasRenderingContext2DTest,
       DisableAcceleration_RestoreCanvasMatrixClipStack) {
  // This tests verifies whether the RestoreCanvasMatrixClipStack happens after
  // PaintCanvas is drawn from old 2d bridge to new 2d bridge.
  InSequence s;

  CreateContext(kNonOpaque);
  IntSize size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), kPreferAcceleration);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  FakeCanvasResourceHost host(size);
  auto fake_deaccelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), kPreferNoAcceleration);
  fake_deaccelerate_surface->SetCanvasResourceHost(&host);

  cc::PaintCanvas* paint_canvas_ptr = fake_deaccelerate_surface->Canvas();
  FakeCanvas2DLayerBridge* surface_ptr = fake_deaccelerate_surface.get();

  EXPECT_CALL(*fake_deaccelerate_surface, DrawFullImage(_)).Times(1);
  EXPECT_CALL(*fake_deaccelerate_surface,
              DidRestoreCanvasMatrixClipStack(paint_canvas_ptr))
      .Times(1);

  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  EXPECT_TRUE(
      IsCanvasResourceHostSet(CanvasElement().GetCanvas2DLayerBridge()));

  CanvasElement().DisableAcceleration(std::move(fake_deaccelerate_surface));
  EXPECT_FALSE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  EXPECT_TRUE(
      IsCanvasResourceHostSet(CanvasElement().GetCanvas2DLayerBridge()));

  Mock::VerifyAndClearExpectations(surface_ptr);
}

static void TestDrawSingleHighBitDepthPNGOnCanvas(
    String filepath,
    CanvasRenderingContext2D* context,
    Document& document,
    ScriptState* script_state) {
  scoped_refptr<SharedBuffer> pixel_buffer = test::ReadFromFile(filepath);
  ASSERT_EQ(false, pixel_buffer->IsEmpty());

  ImageResourceContent* resource_content =
      ImageResourceContent::CreateNotStarted();
  const bool all_data_received = true;
  const bool is_multipart = false;
  ImageResourceContent::UpdateImageResult update_result =
      resource_content->UpdateImage(
          pixel_buffer, ResourceStatus::kPending,
          ImageResourceContent::UpdateImageOption::kUpdateImage,
          all_data_received, is_multipart);
  ASSERT_EQ(ImageResourceContent::UpdateImageResult::kNoDecodeError,
            update_result);

  HTMLImageElement* image_element = HTMLImageElement::Create(document);
  image_element->SetImageForTest(resource_content);

  context->clearRect(0, 0, 2, 2);
  NonThrowableExceptionState exception_state;
  CanvasImageSourceUnion image_union;
  image_union.SetHTMLImageElement(image_element);
  context->drawImage(script_state, image_union, 0, 0, exception_state);

  ImageData* image_data = context->getImageData(0, 0, 2, 2, exception_state);
  ImageDataArray data_array = image_data->dataUnion();
  ASSERT_TRUE(data_array.IsFloat32Array());
  DOMArrayBufferView* buffer_view = data_array.GetAsFloat32Array().View();
  ASSERT_EQ(16u, buffer_view->byteLength() / buffer_view->TypeSize());
  float* actual_pixels = static_cast<float*>(buffer_view->BaseAddress());

  sk_sp<SkImage> decoded_image =
      resource_content->GetImage()->PaintImageForCurrentFrame().GetSkImage();
  ASSERT_EQ(kRGBA_F16_SkColorType, decoded_image->colorType());
  sk_sp<SkImage> color_converted_image = decoded_image->makeColorSpace(
      context->ColorParamsForTest().GetSkColorSpaceForSkSurfaces());
  float expected_pixels[16];
  SkImageInfo expected_info_no_color_space = SkImageInfo::Make(
      2, 2, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType, nullptr);
  color_converted_image->readPixels(
      expected_info_no_color_space, expected_pixels,
      expected_info_no_color_space.minRowBytes(), 0, 0);
  ColorCorrectionTestUtils::CompareColorCorrectedPixels(
      actual_pixels, expected_pixels, 4, kPixelFormat_ffff);
}

static void TestDrawHighBitDepthPNGsOnWideGamutCanvas(
    String canvas_color_space,
    Document& document,
    Persistent<HTMLCanvasElement> canvas,
    ScriptState* script_state) {
  // Prepare the wide gamut context with the given color space.
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  attributes.color_space = canvas_color_space;
  attributes.pixel_format = "float16";
  CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(
      canvas->GetCanvasRenderingContext("2d", attributes));

  // Prepare the png file path and call the test routine
  std::vector<String> interlace_status = {"", "_interlaced"};
  std::vector<String> color_profiles = {"_sRGB",      "_e-sRGB",   "_AdobeRGB",
                                        "_DisplayP3", "_ProPhoto", "_Rec2020"};
  std::vector<String> alpha_status = {"_opaque", "_transparent"};

  String path = test::CoreTestDataPath();
  path.append("/png-16bit/");
  for (auto interlace : interlace_status) {
    for (auto color_profile : color_profiles) {
      for (auto alpha : alpha_status) {
        String filename = "2x2_16bit";
        filename.append(interlace);
        filename.append(color_profile);
        filename.append(alpha);
        filename.append(".png");
        String full_path = path;
        full_path.append(filename);
        TestDrawSingleHighBitDepthPNGOnCanvas(full_path, context, document,
                                              script_state);
      }
    }
  }
}

TEST_F(CanvasRenderingContext2DTest, DrawHighBitDepthPngOnLinearRGBCanvas) {
  TestDrawHighBitDepthPNGsOnWideGamutCanvas(
      "linear-rgb", GetDocument(),
      Persistent<HTMLCanvasElement>(CanvasElement()), GetScriptState());
}

TEST_F(CanvasRenderingContext2DTest, DrawHighBitDepthPngOnP3Canvas) {
  TestDrawHighBitDepthPNGsOnWideGamutCanvas(
      "p3", GetDocument(), Persistent<HTMLCanvasElement>(CanvasElement()),
      GetScriptState());
}

TEST_F(CanvasRenderingContext2DTest, DrawHighBitDepthPngOnRec2020Canvas) {
  TestDrawHighBitDepthPNGsOnWideGamutCanvas(
      "rec2020", GetDocument(), Persistent<HTMLCanvasElement>(CanvasElement()),
      GetScriptState());
}

TEST_F(CanvasRenderingContext2DTest, ImageBitmapColorSpaceConversion) {
  Persistent<HTMLCanvasElement> canvas =
      Persistent<HTMLCanvasElement>(CanvasElement());
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  attributes.color_space = "srgb";
  CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(
      canvas->GetCanvasRenderingContext("2d", attributes));
  StringOrCanvasGradientOrCanvasPattern fill_style;
  fill_style.SetString("#FFC08040");  // 255,192,128,64
  context->setFillStyle(fill_style);
  context->fillRect(0, 0, 1, 1);
  scoped_refptr<StaticBitmapImage> snapshot =
      canvas->Snapshot(kFrontBuffer, kPreferNoAcceleration);
  ASSERT_TRUE(snapshot);
  sk_sp<SkImage> source_image =
      snapshot->PaintImageForCurrentFrame().GetSkImage();
  SkPixmap source_pixmap;
  source_image->peekPixels(&source_pixmap);

  // Create and test the ImageBitmap objects.
  base::Optional<IntRect> crop_rect = IntRect(0, 0, 1, 1);
  for (int conversion_iterator = kColorSpaceConversion_Default;
       conversion_iterator <= kColorSpaceConversion_Last;
       conversion_iterator++) {
    // Color convert using ImageBitmap
    ImageBitmapOptions options;
    options.setColorSpaceConversion(
        ColorCorrectionTestUtils::ColorSpaceConversionToString(
            static_cast<ColorSpaceConversion>(conversion_iterator)));
    ImageBitmap* image_bitmap = ImageBitmap::Create(canvas, crop_rect, options);
    ASSERT_TRUE(image_bitmap);
    sk_sp<SkImage> converted_image =
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    ASSERT_TRUE(converted_image);
    SkPixmap converted_pixmap;
    converted_image->peekPixels(&converted_pixmap);

    // Manual color convert for testing
    sk_sp<SkColorSpace> color_space =
        ColorCorrectionTestUtils::ColorSpaceConversionToSkColorSpace(
            static_cast<ColorSpaceConversion>(conversion_iterator));
    if (conversion_iterator == kColorSpaceConversion_Preserve)
      color_space = SkColorSpace::MakeSRGB();

    // TODO: crbug.com/768855: Remove if statement when CanvasResourceProvider
    // does not use SkColorSpaceXformCanvas (which rips off sRGB from
    // ImageBitmap).
    if (!color_space->isSRGB()) {
      EXPECT_TRUE(SkColorSpace::Equals(color_space.get(),
                                       converted_image->colorSpace()));
    }

    SkColorType color_type = SkColorType::kN32_SkColorType;
    if (color_space && color_space->gammaIsLinear())
      color_type = kRGBA_F16_SkColorType;
    SkImageInfo image_info = SkImageInfo::Make(
        1, 1, color_type, SkAlphaType::kPremul_SkAlphaType, color_space);
    SkBitmap manual_converted_bitmap;
    EXPECT_TRUE(manual_converted_bitmap.tryAllocPixels(image_info));
    source_pixmap.readPixels(manual_converted_bitmap.pixmap(), 0, 0);

    ColorCorrectionTestUtils::CompareColorCorrectedPixels(
        converted_pixmap.addr(), manual_converted_bitmap.pixmap().addr(), 1,
        (color_type == kN32_SkColorType) ? kPixelFormat_8888
                                         : kPixelFormat_hhhh,
        kAlphaMultiplied, kNoUnpremulRoundTripTolerance);
  }
}

// The color settings of the surface of the canvas always remaines loyal to the
// first created context 2D. Therefore, we have to test different canvas color
// space settings for CanvasRenderingContext2D::putImageData() in different
// tests.
enum class CanvasColorSpaceSettings : uint8_t {
  CANVAS_SRGB = 0,
  CANVAS_LINEARSRGB = 1,
  CANVAS_REC2020 = 2,
  CANVAS_P3 = 3,

  LAST = CANVAS_P3
};

// This test verifies the correct behavior of putImageData member function in
// color managed mode.
void TestPutImageDataOnCanvasWithColorSpaceSettings(
    HTMLCanvasElement& canvas_element,
    CanvasColorSpaceSettings canvas_colorspace_setting) {
  unsigned num_image_data_color_spaces = 4;
  CanvasColorSpace image_data_color_spaces[] = {
      kSRGBCanvasColorSpace, kLinearRGBCanvasColorSpace,
      kRec2020CanvasColorSpace, kP3CanvasColorSpace,
  };

  unsigned num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };

  CanvasColorSpace canvas_color_spaces[] = {
      kSRGBCanvasColorSpace,      kSRGBCanvasColorSpace,
      kLinearRGBCanvasColorSpace, kRec2020CanvasColorSpace,
      kP3CanvasColorSpace,
  };

  String canvas_color_space_names[] = {
      kSRGBCanvasColorSpaceName, kSRGBCanvasColorSpaceName,
      kLinearRGBCanvasColorSpaceName, kRec2020CanvasColorSpaceName,
      kP3CanvasColorSpaceName};

  CanvasPixelFormat canvas_pixel_formats[] = {
      kRGBA8CanvasPixelFormat, kF16CanvasPixelFormat, kF16CanvasPixelFormat,
      kF16CanvasPixelFormat,   kF16CanvasPixelFormat,
  };

  String canvas_pixel_format_names[] = {
      kRGBA8CanvasPixelFormatName, kF16CanvasPixelFormatName,
      kF16CanvasPixelFormatName, kF16CanvasPixelFormatName,
      kF16CanvasPixelFormatName};

  // Source pixels in RGBA32
  uint8_t u8_pixels[] = {255, 0,   0,   255,  // Red
                         0,   0,   0,   0,    // Transparent
                         255, 192, 128, 64,   // Decreasing values
                         93,  117, 205, 41};  // Random values
  unsigned data_length = 16;

  uint16_t* u16_pixels = new uint16_t[data_length];
  for (unsigned i = 0; i < data_length; i++)
    u16_pixels[i] = u8_pixels[i] * 257;

  float* f32_pixels = new float[data_length];
  for (unsigned i = 0; i < data_length; i++)
    f32_pixels[i] = u8_pixels[i] / 255.0;

  DOMArrayBufferView* data_array = nullptr;

  DOMUint8ClampedArray* data_u8 =
      DOMUint8ClampedArray::Create(u8_pixels, data_length);
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->length());
  DOMUint16Array* data_u16 = DOMUint16Array::Create(u16_pixels, data_length);
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->length());
  DOMFloat32Array* data_f32 = DOMFloat32Array::Create(f32_pixels, data_length);
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->length());

  ImageData* image_data = nullptr;
  ImageDataColorSettings color_settings;
  int num_pixels = data_length / 4;

  // At most four bytes are needed for Float32 output per color component.
  std::unique_ptr<uint8_t[]> pixels_converted_manually(
      new uint8_t[data_length * 4]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < num_image_data_color_spaces; i++) {
    color_settings.setColorSpace(
        ImageData::CanvasColorSpaceName(image_data_color_spaces[i]));

    for (unsigned j = 0; j < num_image_data_storage_formats; j++) {
      switch (image_data_storage_formats[j]) {
        case kUint8ClampedArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_u8);
          color_settings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
          break;
        case kUint16ArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_u16);
          color_settings.setStorageFormat(kUint16ArrayStorageFormatName);
          break;
        case kFloat32ArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_f32);
          color_settings.setStorageFormat(kFloat32ArrayStorageFormatName);
          break;
        default:
          NOTREACHED();
      }

      image_data =
          ImageData::CreateForTest(IntSize(2, 2), data_array, &color_settings);

      unsigned k = (unsigned)(canvas_colorspace_setting);
      // Convert the original data used to create ImageData to the
      // canvas color space and canvas pixel format.
      EXPECT_TRUE(
          ColorCorrectionTestUtils::
              ConvertPixelsToColorSpaceAndPixelFormatForTest(
                  data_array->BaseAddress(), data_length,
                  image_data_color_spaces[i], image_data_storage_formats[j],
                  canvas_color_spaces[k], canvas_pixel_formats[k],
                  pixels_converted_manually, kPixelFormat_ffff));

      // Create a canvas and call putImageData and getImageData to make sure
      // the conversion is done correctly.
      CanvasContextCreationAttributesCore attributes;
      attributes.alpha = true;
      attributes.color_space = canvas_color_space_names[k];
      attributes.pixel_format = canvas_pixel_format_names[k];
      CanvasRenderingContext2D* context =
          static_cast<CanvasRenderingContext2D*>(
              canvas_element.GetCanvasRenderingContext("2d", attributes));
      NonThrowableExceptionState exception_state;
      context->putImageData(image_data, 0, 0, exception_state);

      void* pixels_from_get_image_data =
          context->getImageData(0, 0, 2, 2, exception_state)
              ->BufferBase()
              ->Data();
      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          pixels_from_get_image_data, pixels_converted_manually.get(),
          num_pixels,
          (canvas_pixel_formats[k] == kRGBA8CanvasPixelFormat)
              ? kPixelFormat_8888
              : kPixelFormat_ffff,
          kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
    }
  }
  delete[] u16_pixels;
  delete[] f32_pixels;
}

// Test disabled due to crbug.com/780925
TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnSRGBCanvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_SRGB);
}

TEST_F(CanvasRenderingContext2DTest,
       ColorManagedPutImageDataOnLinearSRGBCanvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_LINEARSRGB);
}

TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnRec2020Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_REC2020);
}

TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnP3Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_P3);
}

class CanvasRenderingContext2DTestWithTestingPlatform
    : public CanvasRenderingContext2DTest {
 protected:
  CanvasRenderingContext2DTestWithTestingPlatform() {
    EnablePlatform();
    platform()->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings.
  }

  void SetUp() override {
    CanvasRenderingContext2DTest::SetUp();
    GetDocument().View()->UpdateLayout();
  }

  void RunUntilIdle() { platform()->RunUntilIdle(); }
};

// https://crbug.com/708445: When the Canvas2DLayerBridge hibernates or wakes up
// from hibernation, the compositing reasons for the canvas element may change.
// In these cases, the element should request a compositing update.
TEST_F(CanvasRenderingContext2DTestWithTestingPlatform,
       ElementRequestsCompositingUpdateOnHibernateAndWakeUp) {
  // This Page is not actually being shown by a compositor, but we act like it
  // will in order to test behaviour.
  GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, Canvas2DLayerBridge::kEnableAcceleration);
  // Force hibernatation to occur in an immediate task.
  bridge->DontUseIdleSchedulingForTesting();
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);

  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  // Take a snapshot to trigger lazy resource provider creation
  CanvasElement().GetCanvas2DLayerBridge()->NewImageSnapshot(
      kPreferAcceleration);
  EXPECT_TRUE(!!CanvasElement().ResourceProvider());
  EXPECT_TRUE(CanvasElement().ResourceProvider()->IsAccelerated());
  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  PaintLayer* layer = CanvasElement().GetLayoutBoxModelObject()->Layer();
  EXPECT_TRUE(layer);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::PageVisibilityState::kHidden, false);
  RunUntilIdle();  // Run hibernation task.
  // If enabled, hibernation should cause compositing update.
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            layer->NeedsCompositingInputsUpdate());
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            !CanvasElement().ResourceProvider());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());

  // Wake up again, which should request a compositing update synchronously.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::PageVisibilityState::kVisible, false);
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            layer->NeedsCompositingInputsUpdate());
  RunUntilIdle();  // Clear task queue.
}

TEST_F(CanvasRenderingContext2DTestWithTestingPlatform,
       NoHibernationIfNoResourceProvider) {
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, Canvas2DLayerBridge::kEnableAcceleration);
  // Force hibernatation to occur in an immediate task.
  bridge->DontUseIdleSchedulingForTesting();
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);

  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());

  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  PaintLayer* layer = CanvasElement().GetLayoutBoxModelObject()->Layer();
  EXPECT_TRUE(layer);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::PageVisibilityState::kHidden, false);
  RunUntilIdle();  // Run hibernation task.

  // Never hibernate a canvas with no resource provider
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());
}

TEST_F(CanvasRenderingContext2DTest, LowLatencyIsSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  auto frame1_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(kPreferNoAcceleration)
          ->ProduceFrame();
  EXPECT_TRUE(!!frame1_resource);
  DrawSomething();
  auto frame2_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(kPreferNoAcceleration)
          ->ProduceFrame();
  EXPECT_TRUE(!!frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}

}  // namespace blink
