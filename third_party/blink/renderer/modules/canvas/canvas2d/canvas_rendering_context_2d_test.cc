// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

using testing::_;
using testing::InSequence;
using testing::Mock;

namespace blink {

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(IntSize, BitmapOpacity);

  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               const FloatSize&) override;

  bool WouldTaintOrigin() const override { return false; }
  FloatSize ElementSize(const FloatSize&,
                        const RespectImageOrientationEnum) const override {
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
  image_ = UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

scoped_refptr<Image> FakeImageSource::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const FloatSize&) {
  if (status)
    *status = kNormalSourceImageStatus;
  return image_;
}

//============================================================================

class CanvasRenderingContext2DTest : public ::testing::Test {
 protected:
  CanvasRenderingContext2DTest();
  void SetUp() override;
  virtual bool AllowsAcceleration() { return false; }

  virtual scoped_refptr<viz::TestContextProvider> CreateContextProvider() {
    return viz::TestContextProvider::Create();
  }

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  bool IsCanvasResourceHostSet(Canvas2DLayerBridge* bridge) {
    return !!bridge->resource_host_;
  }
  CanvasRenderingContext2D* Context2D() const {
    return static_cast<CanvasRenderingContext2D*>(
        CanvasElement().RenderingContext());
  }
  void DrawSomething() {
    CanvasElement().DidDraw();
    CanvasElement().PreFinalizeFrame();
    Context2D()->FinalizeFrame();
    CanvasElement().PostFinalizeFrame();
    // Grabbing an image forces a flush
    CanvasElement().Snapshot(kBackBuffer);
  }

  enum LatencyMode { kNormalLatency, kLowLatency };

  enum class ReadFrequencyMode { kWillReadFrequency, kWillNotReadFrequency };

  void CreateContext(
      OpacityMode,
      LatencyMode = kNormalLatency,
      ReadFrequencyMode = ReadFrequencyMode::kWillNotReadFrequency);
  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(canvas_element_->DomWindow()->GetFrame());
  }

  void TearDown() override;
  void UnrefCanvas();
  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(const IntSize&, RasterMode);

  Document& GetDocument() const {
    return *web_view_helper_->GetWebView()
                ->MainFrameImpl()
                ->GetFrame()
                ->DomWindow()
                ->document();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  Persistent<HTMLCanvasElement> canvas_element_;

 private:
  Persistent<MemoryCache> global_memory_cache_;
  std::unique_ptr<ScopedAccelerated2dCanvasForTest> allow_accelerated_;

  class WrapGradients final : public GarbageCollected<WrapGradients> {
   public:
    void Trace(Visitor* visitor) const {
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
  scoped_refptr<viz::TestContextProvider> test_context_provider_;

  StringOrCanvasGradientOrCanvasPattern& OpaqueGradient() {
    return wrap_gradients_->opaque_gradient_;
  }
  StringOrCanvasGradientOrCanvasPattern& AlphaGradient() {
    return wrap_gradients_->alpha_gradient_;
  }
};

CanvasRenderingContext2DTest::CanvasRenderingContext2DTest()
    : wrap_gradients_(MakeGarbageCollected<WrapGradients>()),
      opaque_bitmap_(IntSize(10, 10), kOpaqueBitmap),
      alpha_bitmap_(IntSize(10, 10), kTransparentBitmap) {}

void CanvasRenderingContext2DTest::CreateContext(
    OpacityMode opacity_mode,
    LatencyMode latency_mode,
    ReadFrequencyMode read_frequency_mode) {
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = opacity_mode == kNonOpaque;
  attributes.desynchronized = latency_mode == kLowLatency;
  attributes.will_read_frequently =
      read_frequency_mode == ReadFrequencyMode::kWillReadFrequency;
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
}

void CanvasRenderingContext2DTest::SetUp() {
  test_context_provider_ = CreateContextProvider();
  InitializeSharedGpuContext(test_context_provider_.get());
  allow_accelerated_.reset(
      new ScopedAccelerated2dCanvasForTest(AllowsAcceleration()));
  web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
  web_view_helper_->Initialize();

  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(
      "<body><canvas id='c'></canvas><canvas id='d'></canvas></body>"));
  UpdateAllLifecyclePhasesForTest();

  // Simulate that we allow scripts, so that HTMLCanvasElement uses
  // LayoutHTMLCanvas.
  GetDocument().GetPage()->GetSettings().SetScriptEnabled(true);

  canvas_element_ = To<HTMLCanvasElement>(GetDocument().getElementById("c"));

  full_image_data_ = ImageData::Create(IntSize(10, 10));
  partial_image_data_ = ImageData::Create(IntSize(2, 2));

  NonThrowableExceptionState exception_state;
  auto* opaque_gradient =
      MakeGarbageCollected<CanvasGradient>(FloatPoint(0, 0), FloatPoint(10, 0));
  opaque_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  opaque_gradient->addColorStop(1, String("blue"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  this->OpaqueGradient().SetCanvasGradient(opaque_gradient);

  auto* alpha_gradient =
      MakeGarbageCollected<CanvasGradient>(FloatPoint(0, 0), FloatPoint(10, 0));
  alpha_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  alpha_gradient->addColorStop(1, String("rgba(0, 0, 255, 0.5)"),
                               exception_state);
  EXPECT_FALSE(exception_state.HadException());
  StringOrCanvasGradientOrCanvasPattern wrapped_alpha_gradient;
  this->AlphaGradient().SetCanvasGradient(alpha_gradient);

  global_memory_cache_ =
      ReplaceMemoryCacheForTesting(MakeGarbageCollected<MemoryCache>(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
}

void CanvasRenderingContext2DTest::TearDown() {
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);

  ReplaceMemoryCacheForTesting(global_memory_cache_.Release());

  // Tear down WebViewHelper because we override Platform in some tests which
  // must be torn down after WebViewHelper.
  web_view_helper_ = nullptr;

  // Must be torn down after WebViewHelper since its destructor can create a
  // fresh context provider otherwise.
  SharedGpuContext::ResetForTesting();
}

std::unique_ptr<Canvas2DLayerBridge> CanvasRenderingContext2DTest::MakeBridge(
    const IntSize& size,
    RasterMode raster_mode) {
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>(size, raster_mode,
                                            CanvasColorParams());
  bridge->SetCanvasResourceHost(canvas_element_);
  return bridge;
}

//============================================================================

class FakeCanvas2DLayerBridge : public Canvas2DLayerBridge {
 public:
  FakeCanvas2DLayerBridge(const IntSize& size,
                          CanvasColorParams color_params,
                          RasterModeHint hint)
      : Canvas2DLayerBridge(size, RasterMode::kCPU, color_params),
        is_accelerated_(hint != RasterModeHint::kPreferCPU) {}
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
                             RasterModeHint hint)
      : CanvasResourceProvider(CanvasResourceProvider::kBitmap,
                               size,
                               kLow_SkFilterQuality,
                               color_params,
                               /*is_origin_top_left=*/false,
                               nullptr,
                               nullptr),
        is_accelerated_(hint != RasterModeHint::kPreferCPU) {}
  ~FakeCanvasResourceProvider() override = default;
  bool IsAccelerated() const override { return is_accelerated_; }
  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    return scoped_refptr<CanvasResource>();
  }
  bool SupportsDirectCompositing() const override { return false; }
  bool IsValid() const override { return false; }
  sk_sp<SkSurface> CreateSkSurface() const override {
    return sk_sp<SkSurface>();
  }
  scoped_refptr<StaticBitmapImage> Snapshot(
      const ImageOrientation& orientation) override {
    return SnapshotInternal(orientation);
  }

 private:
  bool is_accelerated_;
};

//============================================================================

class MockImageBufferSurfaceForOverwriteTesting : public Canvas2DLayerBridge {
 public:
  MockImageBufferSurfaceForOverwriteTesting(const IntSize& size,
                                            CanvasColorParams color_params)
      : Canvas2DLayerBridge(size, RasterMode::kCPU, color_params) {}
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
  Context2D()->save();

#define TEST_OVERDRAW_FINALIZE \
  Context2D()->restore();      \
  Mock::VerifyAndClearExpectations(surface_ptr);

#define TEST_OVERDRAW_1(EXPECTED_OVERDRAWS, CALL1) \
  do {                                             \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)        \
    Context2D()->CALL1;                            \
    TEST_OVERDRAW_FINALIZE                         \
  } while (0)

#define TEST_OVERDRAW_2(EXPECTED_OVERDRAWS, CALL1, CALL2) \
  do {                                                    \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)               \
    Context2D()->CALL1;                                   \
    Context2D()->CALL2;                                   \
    TEST_OVERDRAW_FINALIZE                                \
  } while (0)

#define TEST_OVERDRAW_3(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3) \
  do {                                                           \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                      \
    Context2D()->CALL1;                                          \
    Context2D()->CALL2;                                          \
    Context2D()->CALL3;                                          \
    TEST_OVERDRAW_FINALIZE                                       \
  } while (0)

#define TEST_OVERDRAW_4(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3, CALL4) \
  do {                                                                  \
    TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS)                             \
    Context2D()->CALL1;                                                 \
    Context2D()->CALL2;                                                 \
    Context2D()->CALL3;                                                 \
    Context2D()->CALL4;                                                 \
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
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().CreateRawElement(html_names::kCanvasTag));
  canvas->SetSize(IntSize(40, 40));
  ImageBitmap* image_bitmap_derived = nullptr;
  {
    const ImageBitmapOptions* default_options = ImageBitmapOptions::Create();
    base::Optional<IntRect> crop_rect =
        IntRect(0, 0, canvas->width(), canvas->height());
    auto* image_bitmap_from_canvas =
        MakeGarbageCollected<ImageBitmap>(canvas, crop_rect, default_options);
    ASSERT_TRUE(image_bitmap_from_canvas);

    crop_rect = IntRect(0, 0, 20, 20);
    image_bitmap_derived = MakeGarbageCollected<ImageBitmap>(
        image_bitmap_from_canvas, crop_rect, default_options);
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
                                                   RasterModeHint::kPreferGPU);
  std::unique_ptr<FakeCanvas2DLayerBridge> fake_2d_layer_bridge =
      std::make_unique<FakeCanvas2DLayerBridge>(size, CanvasColorParams(),
                                                RasterModeHint::kPreferGPU);
  FakeCanvas2DLayerBridge* fake_2d_layer_bridge_ptr =
      fake_2d_layer_bridge.get();
  CanvasElement().SetResourceProviderForTesting(
      std::move(fake_resource_provider), std::move(fake_2d_layer_bridge), size);

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required

  // Switching accelerated mode to non-accelerated mode
  fake_2d_layer_bridge_ptr->SetIsAccelerated(false);
  CanvasElement().UpdateMemoryUsage();

  // Switching non-accelerated mode to accelerated mode
  fake_2d_layer_bridge_ptr->SetIsAccelerated(true);
  CanvasElement().UpdateMemoryUsage();

  // Creating a different accelerated image buffer
  auto* anotherCanvas =
      To<HTMLCanvasElement>(GetDocument().getElementById("d"));
  CanvasContextCreationAttributesCore attributes;
  anotherCanvas->GetCanvasRenderingContext("2d", attributes);
  IntSize size2(10, 5);
  std::unique_ptr<FakeCanvas2DLayerBridge> fake_2d_layer_bridge2 =
      std::make_unique<FakeCanvas2DLayerBridge>(size2, CanvasColorParams(),
                                                RasterModeHint::kPreferGPU);
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider2 =
      std::make_unique<FakeCanvasResourceProvider>(size2, CanvasColorParams(),
                                                   RasterModeHint::kPreferGPU);
  anotherCanvas->SetResourceProviderForTesting(
      std::move(fake_resource_provider2), std::move(fake_2d_layer_bridge2),
      size2);

  // Tear down the first image buffer that resides in current canvas element
  CanvasElement().SetSize(IntSize(20, 20));
  Mock::VerifyAndClearExpectations(fake_2d_layer_bridge_ptr);

  // Tear down the second image buffer
  anotherCanvas->SetSize(IntSize(20, 20));
}

TEST_F(CanvasRenderingContext2DTest, CanvasDisposedBeforeContext) {
  CreateContext(kNonOpaque);
  Context2D()->fillRect(0, 0, 1, 1);  // results in task observer registration

  Context2D()->DetachHost();

  // This is the only method that is callable after DetachHost
  // Test passes by not crashing.
  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  Context2D()->DidProcessTask(dummy_pending_task);

  // Test passes by not crashing during teardown
}

TEST_F(CanvasRenderingContext2DTest, ContextDisposedBeforeCanvas) {
  CreateContext(kNonOpaque);

  CanvasElement().DetachContext();
  // Passes by not crashing later during teardown
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
      size, CanvasColorParams(), RasterModeHint::kPreferGPU);
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
      size, CanvasColorParams(), RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);
  CanvasRenderingContext2D* context = Context2D();

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required

  context->fillRect(10, 10, 100, 100);
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());

  CanvasElement().DisableAcceleration();
  EXPECT_FALSE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());

  context->fillRect(10, 10, 100, 100);

}

TEST_F(CanvasRenderingContext2DTest,
       DisableAcceleration_RestoreCanvasMatrixClipStack) {
  // This tests verifies whether the RestoreCanvasMatrixClipStack happens after
  // PaintCanvas is drawn from old 2d bridge to new 2d bridge.
  InSequence s;

  CreateContext(kNonOpaque);
  IntSize size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  FakeCanvasResourceHost host(size);
  auto fake_deaccelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), RasterModeHint::kPreferCPU);
  fake_deaccelerate_surface->SetCanvasResourceHost(&host);

  FakeCanvas2DLayerBridge* surface_ptr = fake_deaccelerate_surface.get();

  EXPECT_CALL(*fake_deaccelerate_surface, DrawFullImage(_)).Times(1);
  EXPECT_CALL(*fake_deaccelerate_surface, DidRestoreCanvasMatrixClipStack(_))
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

  auto* image_element = MakeGarbageCollected<HTMLImageElement>(document);
  image_element->SetImageForTest(resource_content);

  context->clearRect(0, 0, 2, 2);
  NonThrowableExceptionState exception_state;
  CanvasImageSourceUnion image_union;
  image_union.SetHTMLImageElement(image_element);
  context->drawImage(script_state, image_union, 0, 0, exception_state);

  ImageData* image_data = context->getImageData(0, 0, 2, 2, exception_state);
  ImageDataArray data_array = image_data->data();
  ASSERT_TRUE(data_array.IsFloat32Array());
  DOMArrayBufferView* buffer_view = data_array.GetAsFloat32Array().View();
  ASSERT_EQ(16u, buffer_view->byteLength() / buffer_view->TypeSize());
  float* actual_pixels = static_cast<float*>(buffer_view->BaseAddress());

  sk_sp<SkImage> decoded_image =
      resource_content->GetImage()->PaintImageForCurrentFrame().GetSwSkImage();
  ASSERT_EQ(kRGBA_F16_SkColorType, decoded_image->colorType());
  sk_sp<SkImage> color_converted_image = decoded_image->makeColorSpace(
      context->ColorParamsForTest().GetSkColorSpace());
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
  Vector<String> interlace_status = {"", "_interlaced"};
  Vector<String> color_profiles = {"_sRGB",      "_e-sRGB",   "_AdobeRGB",
                                   "_DisplayP3", "_ProPhoto", "_Rec2020"};
  Vector<String> alpha_status = {"_opaque", "_transparent"};

  StringBuilder path;
  path.Append(test::CoreTestDataPath());
  path.Append("/png-16bit/");
  for (auto interlace : interlace_status) {
    for (auto color_profile : color_profiles) {
      for (auto alpha : alpha_status) {
        StringBuilder full_path;
        full_path.Append(path);
        full_path.Append("2x2_16bit");
        full_path.Append(interlace);
        full_path.Append(color_profile);
        full_path.Append(alpha);
        full_path.Append(".png");
        TestDrawSingleHighBitDepthPNGOnCanvas(full_path.ToString(), context,
                                              document, script_state);
      }
    }
  }
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

// The color settings of the surface of the canvas always remaines loyal to the
// first created context 2D. Therefore, we have to test different canvas color
// space settings for CanvasRenderingContext2D::putImageData() in different
// tests.
enum class CanvasColorSpaceSettings : uint8_t {
  CANVAS_SRGB = 0,
  CANVAS_REC2020 = 1,
  CANVAS_P3 = 2,

  LAST = CANVAS_P3
};

// This test verifies the correct behavior of putImageData member function in
// color managed mode.
void TestPutImageDataOnCanvasWithColorSpaceSettings(
    HTMLCanvasElement& canvas_element,
    CanvasColorSpaceSettings canvas_colorspace_setting) {
  unsigned num_image_data_color_spaces = 3;
  CanvasColorSpace image_data_color_spaces[] = {
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kRec2020,
      CanvasColorSpace::kP3,
  };

  unsigned num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };

  CanvasColorSpace canvas_color_spaces[] = {
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kRec2020,
      CanvasColorSpace::kP3,
  };

  String canvas_color_space_names[] = {
      kSRGBCanvasColorSpaceName, kSRGBCanvasColorSpaceName,
      kRec2020CanvasColorSpaceName, kP3CanvasColorSpaceName};

  CanvasPixelFormat canvas_pixel_formats[] = {
      CanvasPixelFormat::kRGBA8,
      CanvasPixelFormat::kF16,
      CanvasPixelFormat::kF16,
      CanvasPixelFormat::kF16,
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
  size_t data_length = 16;

  uint16_t* u16_pixels = new uint16_t[data_length];
  for (size_t i = 0; i < data_length; i++)
    u16_pixels[i] = u8_pixels[i] * 257;

  float* f32_pixels = new float[data_length];
  for (size_t i = 0; i < data_length; i++)
    f32_pixels[i] = u8_pixels[i] / 255.0;

  NotShared<DOMUint8ClampedArray> data_u8(
      DOMUint8ClampedArray::Create(u8_pixels, data_length));
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->length());
  NotShared<DOMUint16Array> data_u16(
      DOMUint16Array::Create(u16_pixels, data_length));
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->length());
  NotShared<DOMFloat32Array> data_f32(
      DOMFloat32Array::Create(f32_pixels, data_length));
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->length());

  ImageData* image_data = nullptr;
  ImageDataColorSettings* color_settings = ImageDataColorSettings::Create();
  int num_pixels = data_length / 4;

  // At most four bytes are needed for Float32 output per color component.
  std::unique_ptr<uint8_t[]> pixels_converted_manually(
      new uint8_t[data_length * 4]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < num_image_data_color_spaces; i++) {
    color_settings->setColorSpace(
        ImageData::CanvasColorSpaceName(image_data_color_spaces[i]));

    for (unsigned j = 0; j < num_image_data_storage_formats; j++) {
      NotShared<DOMArrayBufferView> data_array;
      switch (image_data_storage_formats[j]) {
        case kUint8ClampedArrayStorageFormat:
          data_array = data_u8;
          color_settings->setStorageFormat(kUint8ClampedArrayStorageFormatName);
          break;
        case kUint16ArrayStorageFormat:
          data_array = data_u16;
          color_settings->setStorageFormat(kUint16ArrayStorageFormatName);
          break;
        case kFloat32ArrayStorageFormat:
          data_array = data_f32;
          color_settings->setStorageFormat(kFloat32ArrayStorageFormatName);
          break;
        default:
          NOTREACHED();
      }

      image_data =
          ImageData::CreateForTest(IntSize(2, 2), data_array, color_settings);

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
          (canvas_pixel_formats[k] == CanvasPixelFormat::kRGBA8)
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

TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnRec2020Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_REC2020);
}

TEST_F(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnP3Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), CanvasColorSpaceSettings::CANVAS_P3);
}

TEST_F(CanvasRenderingContext2DTest,
       UnacceleratedLowLatencyIsNotSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_FALSE(
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferCPU)
          ->SupportsSingleBuffering());
  EXPECT_FALSE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
}

TEST_F(CanvasRenderingContext2DTest,
       UnacceleratedIfNormalLatencyWillReadFrequently) {
  RuntimeEnabledFeatures::SetNewCanvas2DAPIEnabled(true);
  CreateContext(kNonOpaque, kNormalLatency,
                ReadFrequencyMode::kWillReadFrequency);
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_FALSE(
      CanvasElement().GetOrCreateCanvas2DLayerBridge()->IsAccelerated());
}

TEST_F(CanvasRenderingContext2DTest,
       UnacceleratedIfLowLatencyWillReadFrequently) {
  RuntimeEnabledFeatures::SetNewCanvas2DAPIEnabled(true);
  CreateContext(kNonOpaque, kLowLatency, ReadFrequencyMode::kWillReadFrequency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_FALSE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
}

TEST_F(CanvasRenderingContext2DTest, RemainAcceleratedAfterGetImageData) {
  RuntimeEnabledFeatures::SetNewCanvas2DAPIEnabled(true);
  CreateContext(kNonOpaque);
  IntSize size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>(
      size, CanvasColorParams(), RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  DrawSomething();
  NonThrowableExceptionState exception_state;
  Context2D()->getImageData(0, 0, 1, 1, exception_state);
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
}

class CanvasRenderingContext2DTestAccelerated
    : public CanvasRenderingContext2DTest {
 protected:
  bool AllowsAcceleration() override { return true; }
};

// https://crbug.com/708445: When the Canvas2DLayerBridge hibernates or wakes up
// from hibernation, the compositing reasons for the canvas element may change.
// In these cases, the element should request a compositing update.
TEST_F(CanvasRenderingContext2DTestAccelerated,
       ElementRequestsCompositingUpdateOnHibernateAndWakeUp) {
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>(size, RasterMode::kGPU,
                                            CanvasColorParams());
  // Force hibernatation to occur in an immediate task.
  bridge->DontUseIdleSchedulingForTesting();
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);
  CanvasElement().GetCanvas2DLayerBridge()->SetCanvasResourceHost(
      canvas_element_);

  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  // Take a snapshot to trigger lazy resource provider creation
  CanvasElement().GetCanvas2DLayerBridge()->NewImageSnapshot();
  EXPECT_TRUE(!!CanvasElement().ResourceProvider());
  EXPECT_TRUE(CanvasElement().ResourceProvider()->IsAccelerated());
  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  PaintLayer* layer = CanvasElement().GetLayoutBoxModelObject()->Layer();
  EXPECT_TRUE(layer);
  UpdateAllLifecyclePhasesForTest();

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);
  blink::test::RunPendingTasks();  // Run hibernation task.
  // If enabled, hibernation should cause compositing update.
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            layer->NeedsCompositingInputsUpdate());
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            !CanvasElement().ResourceProvider());

  // The page is hidden so it doesn't make sense to paint, and doing so will
  // DCHECK. Update all other lifecycle phases.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());

  // Wake up again, which should request a compositing update synchronously.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kVisible,
      /*is_initial_state=*/false);
  EXPECT_EQ(!!CANVAS2D_HIBERNATION_ENABLED,
            layer->NeedsCompositingInputsUpdate());
}

TEST_F(CanvasRenderingContext2DTestAccelerated,
       NoHibernationIfNoResourceProvider) {
  CreateContext(kNonOpaque);
  IntSize size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>(size, RasterMode::kGPU,
                                            CanvasColorParams());
  // Force hibernatation to occur in an immediate task.
  bridge->DontUseIdleSchedulingForTesting();
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);
  CanvasElement().GetCanvas2DLayerBridge()->SetCanvasResourceHost(
      canvas_element_);
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());

  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  PaintLayer* layer = CanvasElement().GetLayoutBoxModelObject()->Layer();
  EXPECT_TRUE(layer);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());

  // The resource provider gets lazily created. Force it to be dropped.
  canvas_element_->ReplaceResourceProvider(nullptr);

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);
  blink::test::RunPendingTasks();  // Run hibernation task.

  // Never hibernate a canvas with no resource provider
  EXPECT_FALSE(layer->NeedsCompositingInputsUpdate());
}

TEST_F(CanvasRenderingContext2DTestAccelerated, LowLatencyIsNotSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_FALSE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_FALSE(
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->SupportsSingleBuffering());
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
}

class CanvasRenderingContext2DTestImageChromium
    : public CanvasRenderingContext2DTestAccelerated {
 protected:
  CanvasRenderingContext2DTestImageChromium()
      : CanvasRenderingContext2DTestAccelerated() {
    // This test relies on overlays being supported and enabled for low latency
    // canvas.  The latter is true only on ChromeOS in production.
    feature_list_.InitAndEnableFeature(
        features::kLowLatencyCanvas2dImageChromium);
  }

  scoped_refptr<viz::TestContextProvider> CreateContextProvider() override {
    auto context_provider = viz::TestContextProvider::Create();
    auto* test_gl = context_provider->UnboundTestContextGL();
    test_gl->set_max_texture_size(1024);
    test_gl->set_support_texture_storage_image(true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::BGRA_8888,
                                                   true);
    return context_provider;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasRenderingContext2DTestImageChromium, LowLatencyIsSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_FALSE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  EXPECT_TRUE(CanvasElement()
                  .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
                  ->SupportsSingleBuffering());
  auto frame1_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource();
  EXPECT_TRUE(frame1_resource);
  DrawSomething();
  auto frame2_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource();
  EXPECT_TRUE(frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}

class CanvasRenderingContext2DTestSwapChain
    : public CanvasRenderingContext2DTestAccelerated {
 protected:
  CanvasRenderingContext2DTestSwapChain()
      : CanvasRenderingContext2DTestAccelerated() {
    feature_list_.InitAndEnableFeature(features::kLowLatencyCanvas2dSwapChain);
  }

  scoped_refptr<viz::TestContextProvider> CreateContextProvider() override {
    auto context_provider = viz::TestContextProvider::Create();
    auto* test_gl = context_provider->UnboundTestContextGL();
    test_gl->set_max_texture_size(1024);
    test_gl->set_supports_shared_image_swap_chain(true);
    return context_provider;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CanvasRenderingContext2DTestSwapChain, LowLatencyIsSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_FALSE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge()->IsAccelerated());
  EXPECT_TRUE(CanvasElement()
                  .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
                  ->SupportsSingleBuffering());
  auto frame1_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource();
  EXPECT_TRUE(frame1_resource);
  DrawSomething();
  auto frame2_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource();
  EXPECT_TRUE(frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}

}  // namespace blink
