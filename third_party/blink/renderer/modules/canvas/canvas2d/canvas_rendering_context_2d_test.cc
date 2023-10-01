// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float32array_uint16array_uint8clampedarray.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_will_read_frequently.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/modules/skcms/skcms.h"

using testing::_;
using testing::InSequence;
using testing::Mock;

namespace blink {

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(gfx::Size, BitmapOpacity);

  scoped_refptr<Image> GetSourceImageForCanvas(FlushReason,
                                               SourceImageStatus*,
                                               const gfx::SizeF&,
                                               const AlphaDisposition) override;

  bool WouldTaintOrigin() const override { return false; }
  gfx::SizeF ElementSize(const gfx::SizeF&,
                         const RespectImageOrientationEnum) const override {
    return gfx::SizeF(size_);
  }
  bool IsOpaque() const override { return is_opaque_; }
  bool IsAccelerated() const override { return false; }

  ~FakeImageSource() override = default;

 private:
  gfx::Size size_;
  scoped_refptr<Image> image_;
  bool is_opaque_;
};

FakeImageSource::FakeImageSource(gfx::Size size, BitmapOpacity opacity)
    : size_(size), is_opaque_(opacity == kOpaqueBitmap) {
  sk_sp<SkSurface> surface(SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(size_.width(), size_.height())));
  surface->getCanvas()->clear(opacity == kOpaqueBitmap ? SK_ColorWHITE
                                                       : SK_ColorTRANSPARENT);
  image_ = UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

scoped_refptr<Image> FakeImageSource::GetSourceImageForCanvas(
    FlushReason,
    SourceImageStatus* status,
    const gfx::SizeF&,
    const AlphaDisposition alpha_disposition = kPremultiplyAlpha) {
  // Only cover premultiply alpha cases.
  DCHECK_EQ(alpha_disposition, kPremultiplyAlpha);

  if (status)
    *status = kNormalSourceImageStatus;
  return image_;
}

//============================================================================

class CanvasRenderingContext2DTest : public ::testing::Test,
                                     public PaintTestConfigurations {
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
    Context2D()->FinalizeFrame(FlushReason::kTesting);
    CanvasElement().PostFinalizeFrame(FlushReason::kTesting);
    // Grabbing an image forces a flush
    CanvasElement().Snapshot(FlushReason::kTesting, kBackBuffer);
  }

  enum LatencyMode { kNormalLatency, kLowLatency };

  static constexpr size_t kMaxPinnedImageKB = 1;
  static constexpr size_t kMaxRecordedOpKB = 10;

  void CreateContext(
      OpacityMode,
      LatencyMode = kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently =
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined,
      HTMLCanvasElement* canvas = nullptr);

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(canvas_element_->DomWindow()->GetFrame());
  }

  ExecutionContext* GetExecutionContext() {
    return GetDocument().GetExecutionContext();
  }

  void TearDown() override;
  void UnrefCanvas();

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

    Member<CanvasGradient> opaque_gradient_;
    Member<CanvasGradient> alpha_gradient_;
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

  Member<CanvasGradient>& OpaqueGradient() {
    return wrap_gradients_->opaque_gradient_;
  }
  Member<CanvasGradient>& AlphaGradient() {
    return wrap_gradients_->alpha_gradient_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTest);

CanvasRenderingContext2DTest::CanvasRenderingContext2DTest()
    : wrap_gradients_(MakeGarbageCollected<WrapGradients>()),
      opaque_bitmap_(gfx::Size(10, 10), kOpaqueBitmap),
      alpha_bitmap_(gfx::Size(10, 10), kTransparentBitmap) {}

void CanvasRenderingContext2DTest::CreateContext(
    OpacityMode opacity_mode,
    LatencyMode latency_mode,
    CanvasContextCreationAttributesCore::WillReadFrequently
        will_read_frequently,
    HTMLCanvasElement* canvas) {
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = opacity_mode == kNonOpaque;
  attributes.desynchronized = latency_mode == kLowLatency;
  attributes.will_read_frequently = will_read_frequently;
  if (!canvas) {
    canvas = canvas_element_;
  }
  canvas->GetCanvasRenderingContext(canvas_type, attributes);
}

void CanvasRenderingContext2DTest::SetUp() {
  base::FieldTrialParams auto_flush_params;
  auto_flush_params["max_pinned_image_kb"] =
      base::NumberToString(kMaxPinnedImageKB);
  auto_flush_params["max_recorded_op_kb"] =
      base::NumberToString(kMaxRecordedOpKB);
  feature_list_.InitAndEnableFeatureWithParameters(kCanvas2DAutoFlushParams,
                                                   auto_flush_params);

  test_context_provider_ = CreateContextProvider();
  InitializeSharedGpuContextGLES2(test_context_provider_.get());
  allow_accelerated_ =
      std::make_unique<ScopedAccelerated2dCanvasForTest>(AllowsAcceleration());
  web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
  web_view_helper_->Initialize();

  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(
      "<body><canvas id='c'></canvas><canvas id='d'></canvas></body>"));
  UpdateAllLifecyclePhasesForTest();

  // Simulate that we allow scripts, so that HTMLCanvasElement uses
  // LayoutHTMLCanvas.
  GetDocument().GetPage()->GetSettings().SetScriptEnabled(true);

  canvas_element_ =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));

  ImageDataSettings* settings = ImageDataSettings::Create();
  full_image_data_ = ImageData::Create(10, 10, settings, ASSERT_NO_EXCEPTION);
  partial_image_data_ = ImageData::Create(2, 2, settings, ASSERT_NO_EXCEPTION);

  NonThrowableExceptionState exception_state;
  auto* opaque_gradient = MakeGarbageCollected<CanvasGradient>(
      gfx::PointF(0, 0), gfx::PointF(10, 0));
  opaque_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  opaque_gradient->addColorStop(1, String("blue"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  OpaqueGradient() = opaque_gradient;

  auto* alpha_gradient = MakeGarbageCollected<CanvasGradient>(
      gfx::PointF(0, 0), gfx::PointF(10, 0));
  alpha_gradient->addColorStop(0, String("green"), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  alpha_gradient->addColorStop(1, String("rgba(0, 0, 255, 0.5)"),
                               exception_state);
  EXPECT_FALSE(exception_state.HadException());
  AlphaGradient() = alpha_gradient;

  global_memory_cache_ =
      ReplaceMemoryCacheForTesting(MakeGarbageCollected<MemoryCache>(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
}

void CanvasRenderingContext2DTest::TearDown() {
  feature_list_.Reset();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  ReplaceMemoryCacheForTesting(global_memory_cache_.Release());

  // Tear down WebViewHelper because we override Platform in some tests which
  // must be torn down after WebViewHelper.
  web_view_helper_ = nullptr;

  // Must be torn down after WebViewHelper since its destructor can create a
  // fresh context provider otherwise.
  SharedGpuContext::ResetForTesting();

  // Prevent CanvasPerformanceMonitor state from leaking between tests.
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
}

//============================================================================

class FakeCanvas2DLayerBridge : public Canvas2DLayerBridge {
 public:
  FakeCanvas2DLayerBridge() {}

  MOCK_METHOD1(DrawFullImage, void(const PaintImage& image));
  MOCK_METHOD1(DidRestoreCanvasMatrixClipStack, void(cc::PaintCanvas*));

 private:
};

//============================================================================

class FakeCanvasResourceProvider : public CanvasResourceProvider {
 public:
  FakeCanvasResourceProvider(const SkImageInfo& info,
                             RasterModeHint hint,
                             CanvasResourceHost* resource_host)
      : CanvasResourceProvider(CanvasResourceProvider::kBitmap,
                               info,
                               cc::PaintFlags::FilterQuality::kLow,
                               /*is_origin_top_left=*/false,
                               /*context_provider_wrapper=*/nullptr,
                               /*resource_dispatcher=*/nullptr,
                               resource_host),
        is_accelerated_(hint != RasterModeHint::kPreferCPU) {}
  ~FakeCanvasResourceProvider() override = default;
  bool IsAccelerated() const override { return is_accelerated_; }
  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) override {
    return scoped_refptr<CanvasResource>();
  }
  bool SupportsDirectCompositing() const override { return false; }
  bool IsValid() const override { return false; }
  sk_sp<SkSurface> CreateSkSurface() const override {
    return sk_sp<SkSurface>();
  }
  scoped_refptr<StaticBitmapImage> Snapshot(
      FlushReason reason,
      ImageOrientation orientation) override {
    return SnapshotInternal(orientation, reason);
  }

 private:
  bool is_accelerated_;
};

//============================================================================

class MockImageBufferSurfaceForOverwriteTesting : public Canvas2DLayerBridge {
 public:
  MockImageBufferSurfaceForOverwriteTesting() {}
  MOCK_METHOD0(WillOverwriteCanvas, void());
};

//============================================================================

typedef std::unordered_set<BaseRenderingContext2D::OverdrawOp>
    OverdrawHistogramBuckets;

class CanvasRenderingContext2DOverdrawTest
    : public CanvasRenderingContext2DTest {
 public:
  void ExpectNoOverdraw();
  void ExpectOverdraw(
      std::initializer_list<BaseRenderingContext2D::OverdrawOp>);
  void VerifyExpectations();

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  raw_ptr<MockImageBufferSurfaceForOverwriteTesting, ExperimentalRenderer>
      surface_ptr_;
  OverdrawHistogramBuckets expected_buckets_;
};

void CanvasRenderingContext2DOverdrawTest::SetUp() {
  CanvasRenderingContext2DTest::SetUp();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  std::unique_ptr<MockImageBufferSurfaceForOverwriteTesting> mock_surface =
      std::make_unique<MockImageBufferSurfaceForOverwriteTesting>();
  surface_ptr_ = mock_surface.get();
  CanvasElement().SetResourceProviderForTesting(nullptr,
                                                std::move(mock_surface), size);
  Context2D()->save();
}

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DOverdrawTest);

void CanvasRenderingContext2DOverdrawTest::TearDown() {
  NonThrowableExceptionState exception_state;
  Context2D()->restore(exception_state);

  histogram_tester_.reset();
  surface_ptr_ = nullptr;
  expected_buckets_.clear();

  CanvasRenderingContext2DTest::TearDown();
}

void CanvasRenderingContext2DOverdrawTest::ExpectNoOverdraw() {
  EXPECT_CALL(*surface_ptr_, WillOverwriteCanvas()).Times(0);
}

void CanvasRenderingContext2DOverdrawTest::ExpectOverdraw(
    std::initializer_list<BaseRenderingContext2D::OverdrawOp>
        expected_buckets) {
  EXPECT_CALL(*surface_ptr_, WillOverwriteCanvas()).Times(1);
  expected_buckets_ = expected_buckets;
  EXPECT_FALSE(expected_buckets_.empty());
  // Validate that all buckets are valid (and that kMaxValue is up to date).
  for (auto bucket : expected_buckets_) {
    EXPECT_LE(bucket, BaseRenderingContext2D::OverdrawOp::kMaxValue);
  }
}

void CanvasRenderingContext2DOverdrawTest::VerifyExpectations() {
  // Verify that WillOverwriteCanvas was call the expected number of times.
  Mock::VerifyAndClearExpectations(surface_ptr_);

  // Verify that the expected histogram buckets received hits.
  constexpr int last_bucket =
      static_cast<int>(BaseRenderingContext2D::OverdrawOp::kMaxValue);
  for (int bucket = 0; bucket <= last_bucket; ++bucket) {
    histogram_tester_->ExpectBucketCount(
        "Blink.Canvas.OverdrawOp", bucket,
        static_cast<base::HistogramBase::Count>(expected_buckets_.count(
            static_cast<BaseRenderingContext2D::OverdrawOp>(bucket))));
  }
}

//============================================================================

TEST_P(CanvasRenderingContext2DOverdrawTest, FillRect_FullCoverage) {
  // Fill rect no longer supports overdraw optimizations
  // Reason: low real world incidence not worth the test overhead.
  ExpectNoOverdraw();
  Context2D()->fillRect(-1, -1, 12, 12);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DisableOverdrawOptimization) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kDisableCanvasOverdrawOptimization);
  ExpectNoOverdraw();
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_ExactCoverage) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kClearRect,
  });
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_PartialCoverage) {
  ExpectNoOverdraw();
  Context2D()->clearRect(0, 0, 9, 9);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_GlobalAlpha) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kClearRect,
  });
  Context2D()->setGlobalAlpha(0.5f);
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_TransparentGradient) {
  auto* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kClearRect,
  });
  SetFillStyleHelper(Context2D(), script_state, AlphaGradient().Get());
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_Filter) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kClearRect,
  });
  V8UnionCanvasFilterOrString* filter =
      MakeGarbageCollected<V8UnionCanvasFilterOrString>("blur(4px)");
  Context2D()->setFilter(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         filter);
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest,
       ClearRect_TransformPartialCoverage) {
  ExpectNoOverdraw();
  Context2D()->translate(1, 1);
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest,
       ClearRect_TransformCompleteCoverage) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kClearRect,
      BaseRenderingContext2D::OverdrawOp::kHasTransform,
  });
  Context2D()->translate(1, 1);
  Context2D()->clearRect(-1, -1, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_IgnoreCompositeOp) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kClearRect,
  });
  Context2D()->setGlobalCompositeOperation(String("destination-in"));
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, ClearRect_Clipped) {
  ExpectNoOverdraw();
  Context2D()->rect(0, 0, 5, 5);
  Context2D()->clip();
  Context2D()->clearRect(0, 0, 10, 10);
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_ExactCoverage) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kDrawImage,
  });
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_Magnified) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kDrawImage,
  });
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 1, 1, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_GlobalAlpha) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->setGlobalAlpha(0.5f);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_TransparentBitmap) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&alpha_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_Filter) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  V8UnionCanvasFilterOrString* filter =
      MakeGarbageCollected<V8UnionCanvasFilterOrString>("blur(4px)");
  Context2D()->setFilter(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         filter);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_PartialCoverage1) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_PartialCoverage2) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 9, 9,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_FullCoverage) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kDrawImage,
  });
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 11, 11,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_TransformFullCoverage) {
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kDrawImage,
      BaseRenderingContext2D::OverdrawOp::kHasTransform,
  });
  NonThrowableExceptionState exception_state;
  Context2D()->translate(-1, 0),
      Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                             exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest,
       DrawImage_TransformPartialCoverage) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->translate(-1, 0),
      Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                             exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest,
       DrawImage_TransparenBitmapOpaqueGradient) {
  auto* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  SetFillStyleHelper(Context2D(), GetScriptState(), OpaqueGradient().Get());
  Context2D()->drawImage(&alpha_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest,
       DrawImage_OpaqueBitmapTransparentGradient) {
  auto* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  ExpectOverdraw({
      BaseRenderingContext2D::OverdrawOp::kTotal,
      BaseRenderingContext2D::OverdrawOp::kDrawImage,
  });
  NonThrowableExceptionState exception_state;
  SetFillStyleHelper(Context2D(), GetScriptState(), AlphaGradient().Get());
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_CopyPartialCoverage) {
  // The 'copy' blend mode no longer trigger the overdraw optimization
  // Reason: low real-world incidence, test overhead not justified.
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->setGlobalCompositeOperation(String("copy"));
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest,
       DrawImage_CopyTransformPartialCoverage) {
  // Overdraw optimizations with the 'copy' composite operation are no longer
  // supported. Reason: low real-world incidence, test overhead not justified.
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->setGlobalCompositeOperation(String("copy"));
  Context2D()->translate(1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, DrawImage_Clipped) {
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->rect(0, 0, 5, 5);
  Context2D()->clip();
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, PutImageData_FullCoverage) {
  // PutImageData no longer supports overdraw optimizations.
  // Reason: low real-world incidence, test overhead not justified
  ExpectNoOverdraw();
  NonThrowableExceptionState exception_state;
  Context2D()->putImageData(full_image_data_.Get(), 0, 0, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  VerifyExpectations();
}

TEST_P(CanvasRenderingContext2DOverdrawTest, Path_FullCoverage) {
  // This case is an overdraw but the current detection logic rejects all
  // paths.
  ExpectNoOverdraw();
  Context2D()->rect(-1, -1, 12, 12);
  Context2D()->fill();
  VerifyExpectations();
}

//==============================================================================

TEST_P(CanvasRenderingContext2DTest, ImageResourceLifetime) {
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().CreateRawElement(html_names::kCanvasTag));
  canvas->SetSize(gfx::Size(40, 40));
  ImageBitmap* image_bitmap_derived = nullptr;
  {
    const ImageBitmapOptions* default_options = ImageBitmapOptions::Create();
    absl::optional<gfx::Rect> crop_rect =
        gfx::Rect(0, 0, canvas->width(), canvas->height());
    auto* image_bitmap_from_canvas =
        MakeGarbageCollected<ImageBitmap>(canvas, crop_rect, default_options);
    ASSERT_TRUE(image_bitmap_from_canvas);

    crop_rect = gfx::Rect(0, 0, 20, 20);
    image_bitmap_derived = MakeGarbageCollected<ImageBitmap>(
        image_bitmap_from_canvas, crop_rect, default_options);
    ASSERT_TRUE(image_bitmap_derived);
  }
  CanvasContextCreationAttributesCore attributes;
  CanvasRenderingContext2D* context = static_cast<CanvasRenderingContext2D*>(
      canvas->GetCanvasRenderingContext("2d", attributes));
  DummyExceptionStateForTesting exception_state;
  auto* image_source =
      MakeGarbageCollected<V8CanvasImageSource>(image_bitmap_derived);
  context->drawImage(image_source, 0, 0, exception_state);
}

TEST_P(CanvasRenderingContext2DTest, GPUMemoryUpdateForAcceleratedCanvas) {
  CreateContext(kNonOpaque);

  gfx::Size size(10, 10);
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider =
      std::make_unique<FakeCanvasResourceProvider>(
          SkImageInfo::MakeN32Premul(size.width(), size.height()),
          RasterModeHint::kPreferGPU, &CanvasElement());
  std::unique_ptr<FakeCanvas2DLayerBridge> fake_2d_layer_bridge =
      std::make_unique<FakeCanvas2DLayerBridge>();
  FakeCanvas2DLayerBridge* fake_2d_layer_bridge_ptr =
      fake_2d_layer_bridge.get();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      std::move(fake_resource_provider), std::move(fake_2d_layer_bridge), size);

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required

  // Switching accelerated mode to non-accelerated mode
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferCPU);
  CanvasElement().UpdateMemoryUsage();

  // Switching non-accelerated mode to accelerated mode
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().UpdateMemoryUsage();

  // Creating a different accelerated image buffer
  auto* anotherCanvas =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("d")));
  CanvasContextCreationAttributesCore attributes;
  anotherCanvas->GetCanvasRenderingContext("2d", attributes);
  gfx::Size size2(10, 5);
  std::unique_ptr<FakeCanvas2DLayerBridge> fake_2d_layer_bridge2 =
      std::make_unique<FakeCanvas2DLayerBridge>();
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider2 =
      std::make_unique<FakeCanvasResourceProvider>(
          SkImageInfo::MakeN32Premul(size2.width(), size2.height()),
          RasterModeHint::kPreferGPU, &CanvasElement());
  anotherCanvas->SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  anotherCanvas->SetResourceProviderForTesting(
      std::move(fake_resource_provider2), std::move(fake_2d_layer_bridge2),
      size2);

  // Tear down the first image buffer that resides in current canvas element
  CanvasElement().SetSize(gfx::Size(20, 20));
  Mock::VerifyAndClearExpectations(fake_2d_layer_bridge_ptr);

  // Tear down the second image buffer
  anotherCanvas->SetSize(gfx::Size(20, 20));
}

TEST_P(CanvasRenderingContext2DTest, CanvasDisposedBeforeContext) {
  CreateContext(kNonOpaque);
  Context2D()->fillRect(0, 0, 1, 1);  // results in task observer registration

  Context2D()->DetachHost();

  // This is the only method that is callable after DetachHost
  // Test passes by not crashing.
  base::PendingTask dummy_pending_task(FROM_HERE, base::OnceClosure());
  Context2D()->DidProcessTask(dummy_pending_task);

  // Test passes by not crashing during teardown
}

TEST_P(CanvasRenderingContext2DTest, ContextDisposedBeforeCanvas) {
  CreateContext(kNonOpaque);

  CanvasElement().DetachContext();
  // Passes by not crashing later during teardown
}

TEST_P(CanvasRenderingContext2DTest,
       NoResourceProviderInCanvas2DBufferInitialization) {
  // This test enforces that there is no eager creation of
  // CanvasResourceProvider for html canvas with 2d context when its
  // Canvas2DLayerBridge is initially set up. This enforcement might be changed
  // in the future refactoring; but change is seriously warned against because
  // certain code paths in canvas 2d (that depend on the existence of
  // CanvasResourceProvider) will be changed too, causing bad regressions.
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  EXPECT_TRUE(CanvasElement().GetCanvas2DLayerBridge());
  EXPECT_FALSE(CanvasElement().ResourceProvider());
}

static void TestDrawSingleHighBitDepthPNGOnCanvas(
    String filepath,
    CanvasRenderingContext2D* context,
    PredefinedColorSpace context_color_space,
    Document& document,
    ImageDataSettings* color_setting,
    ScriptState* script_state) {
  scoped_refptr<SharedBuffer> pixel_buffer = test::ReadFromFile(filepath);
  ASSERT_EQ(false, pixel_buffer->empty());

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
  auto* image_union = MakeGarbageCollected<V8CanvasImageSource>(image_element);
  context->drawImage(image_union, 0, 0, exception_state);

  ImageData* image_data =
      context->getImageData(0, 0, 2, 2, color_setting, exception_state);
  const V8ImageDataArray* data_array = image_data->data();
  ASSERT_TRUE(data_array->IsFloat32Array());
  DOMArrayBufferView* buffer_view = data_array->GetAsFloat32Array().Get();
  ASSERT_EQ(16u, buffer_view->byteLength() / buffer_view->TypeSize());
  float* actual_pixels = static_cast<float*>(buffer_view->BaseAddress());

  sk_sp<SkImage> decoded_image =
      resource_content->GetImage()->PaintImageForCurrentFrame().GetSwSkImage();
  ASSERT_EQ(kRGBA_F16_SkColorType, decoded_image->colorType());
  sk_sp<SkImage> color_converted_image = decoded_image->makeColorSpace(
      static_cast<GrDirectContext*>(nullptr),
      PredefinedColorSpaceToSkColorSpace(context_color_space));
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
    PredefinedColorSpace color_space,
    Document& document,
    Persistent<HTMLCanvasElement> canvas,
    ScriptState* script_state) {
  // Prepare the wide gamut context with the given color space.
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  attributes.color_space = color_space;
  attributes.pixel_format = CanvasPixelFormat::kF16;
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
  ImageDataSettings* color_setting = ImageDataSettings::Create();
  color_setting->setStorageFormat(
      ImageDataStorageFormatName(ImageDataStorageFormat::kFloat32));
  color_setting->setColorSpace(PredefinedColorSpaceName(color_space));
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
                                              color_space, document,
                                              color_setting, script_state);
      }
    }
  }
}

TEST_P(CanvasRenderingContext2DTest, DrawHighBitDepthPngOnP3Canvas) {
  TestDrawHighBitDepthPNGsOnWideGamutCanvas(
      PredefinedColorSpace::kP3, GetDocument(),
      Persistent<HTMLCanvasElement>(CanvasElement()), GetScriptState());
}

TEST_P(CanvasRenderingContext2DTest, DrawHighBitDepthPngOnRec2020Canvas) {
  TestDrawHighBitDepthPNGsOnWideGamutCanvas(
      PredefinedColorSpace::kRec2020, GetDocument(),
      Persistent<HTMLCanvasElement>(CanvasElement()), GetScriptState());
}

// The color settings of the surface of the canvas always remaines loyal to the
// first created context 2D. Therefore, we have to test different canvas color
// space settings for CanvasRenderingContext2D::putImageData() in different
// tests.
enum class PredefinedColorSpaceSettings : uint8_t {
  CANVAS_SRGB = 0,
  CANVAS_REC2020 = 1,
  CANVAS_P3 = 2,

  LAST = CANVAS_P3
};

// This test verifies the correct behavior of putImageData member function in
// color managed mode.
void TestPutImageDataOnCanvasWithColorSpaceSettings(
    HTMLCanvasElement& canvas_element,
    PredefinedColorSpaceSettings canvas_colorspace_setting) {
  unsigned num_image_data_color_spaces = 3;
  PredefinedColorSpace image_data_color_spaces[] = {
      PredefinedColorSpace::kSRGB,
      PredefinedColorSpace::kRec2020,
      PredefinedColorSpace::kP3,
  };

  unsigned num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      ImageDataStorageFormat::kUint8,
      ImageDataStorageFormat::kUint16,
      ImageDataStorageFormat::kFloat32,
  };

  PredefinedColorSpace predefined_color_spaces[] = {
      PredefinedColorSpace::kSRGB,
      PredefinedColorSpace::kSRGB,
      PredefinedColorSpace::kRec2020,
      PredefinedColorSpace::kP3,
  };

  CanvasPixelFormat canvas_pixel_formats[] = {
      CanvasPixelFormat::kUint8,
      CanvasPixelFormat::kF16,
      CanvasPixelFormat::kF16,
      CanvasPixelFormat::kF16,
  };

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
  size_t num_pixels = data_length / 4;

  // At most four bytes are needed for Float32 output per color component.
  std::unique_ptr<uint8_t[]> pixels_converted_manually(
      new uint8_t[data_length * 4]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < num_image_data_color_spaces; i++) {
    for (unsigned j = 0; j < num_image_data_storage_formats; j++) {
      NotShared<DOMArrayBufferView> data_array;
      switch (image_data_storage_formats[j]) {
        case ImageDataStorageFormat::kUint8:
          data_array = data_u8;
          break;
        case ImageDataStorageFormat::kUint16:
          data_array = data_u16;
          break;
        case ImageDataStorageFormat::kFloat32:
          data_array = data_f32;
          break;
        default:
          NOTREACHED();
      }

      image_data = ImageData::CreateForTest(gfx::Size(2, 2), data_array,
                                            image_data_color_spaces[i],
                                            image_data_storage_formats[j]);
      unsigned k = static_cast<unsigned>(canvas_colorspace_setting);
      ImageDataSettings* canvas_color_setting = ImageDataSettings::Create();
      canvas_color_setting->setColorSpace(
          PredefinedColorSpaceName(predefined_color_spaces[k]));
      switch (canvas_pixel_formats[k]) {
        case CanvasPixelFormat::kUint8:
          canvas_color_setting->setStorageFormat(
              ImageDataStorageFormatName(ImageDataStorageFormat::kUint8));
          break;
        case CanvasPixelFormat::kF16:
          canvas_color_setting->setStorageFormat(
              ImageDataStorageFormatName(ImageDataStorageFormat::kFloat32));
          break;
        default:
          NOTREACHED();
      }

      // Convert the original data used to create ImageData to the
      // canvas color space and canvas pixel format.
      EXPECT_TRUE(
          ColorCorrectionTestUtils::
              ConvertPixelsToColorSpaceAndPixelFormatForTest(
                  data_array->BaseAddress(), data_length,
                  image_data_color_spaces[i], image_data_storage_formats[j],
                  predefined_color_spaces[k], canvas_pixel_formats[k],
                  pixels_converted_manually, kPixelFormat_ffff));

      // Create a canvas and call putImageData and getImageData to make sure
      // the conversion is done correctly.
      CanvasContextCreationAttributesCore attributes;
      attributes.alpha = true;
      attributes.color_space = predefined_color_spaces[k];
      attributes.pixel_format = canvas_pixel_formats[k];
      CanvasRenderingContext2D* context =
          static_cast<CanvasRenderingContext2D*>(
              canvas_element.GetCanvasRenderingContext("2d", attributes));
      NonThrowableExceptionState exception_state;
      context->putImageData(image_data, 0, 0, exception_state);

      const void* pixels_from_get_image_data =
          context
              ->getImageData(0, 0, 2, 2, canvas_color_setting, exception_state)
              ->GetSkPixmap()
              .addr();
      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          pixels_from_get_image_data, pixels_converted_manually.get(),
          num_pixels,
          (canvas_pixel_formats[k] == CanvasPixelFormat::kUint8)
              ? kPixelFormat_8888
              : kPixelFormat_ffff,
          kAlphaUnmultiplied, kUnpremulRoundTripTolerance);
    }
  }
  delete[] u16_pixels;
  delete[] f32_pixels;
}

// Test disabled due to crbug.com/780925
TEST_P(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnSRGBCanvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), PredefinedColorSpaceSettings::CANVAS_SRGB);
}

TEST_P(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnRec2020Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), PredefinedColorSpaceSettings::CANVAS_REC2020);
}

TEST_P(CanvasRenderingContext2DTest, ColorManagedPutImageDataOnP3Canvas) {
  TestPutImageDataOnCanvasWithColorSpaceSettings(
      CanvasElement(), PredefinedColorSpaceSettings::CANVAS_P3);
}

TEST_P(CanvasRenderingContext2DTest,
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
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       UnacceleratedIfNormalLatencyWillReadFrequently) {
  CreateContext(kNonOpaque, kNormalLatency,
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  DrawSomething();
  EXPECT_EQ(Context2D()->getContextAttributes()->willReadFrequently(),
            V8CanvasWillReadFrequently::Enum::kTrue);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       UnacceleratedIfLowLatencyWillReadFrequently) {
  CreateContext(kNonOpaque, kLowLatency,
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_EQ(Context2D()->getContextAttributes()->willReadFrequently(),
            V8CanvasWillReadFrequently::Enum::kTrue);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       UnacceleratedAfterGetImageDataWithDefaultWillReadFrequently) {
  base::test::ScopedFeatureList feature_list_;
  CreateContext(kNonOpaque, kNormalLatency);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  DrawSomething();
  NonThrowableExceptionState exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  int read_count = BaseRenderingContext2D::kFallbackToCPUAfterReadbacks;
  while (read_count--) {
    Context2D()->getImageData(0, 0, 1, 1, settings, exception_state);
  }
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest, AutoFlush) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);
  Context2D()->fillRect(0, 0, 1, 1);  // Ensure resource provider is created.
  const size_t initial_op_count =
      CanvasElement().ResourceProvider()->TotalOpCount();

  while (CanvasElement().ResourceProvider()->TotalOpBytesUsed() <=
         kMaxRecordedOpKB * 1024) {
    Context2D()->fillRect(0, 0, 1, 1);
    // Verify that auto-flush did not happen
    ASSERT_GT(CanvasElement().ResourceProvider()->TotalOpCount(),
              initial_op_count);
  }
  Context2D()->fillRect(0, 0, 1, 1);
  // Verify that auto-flush happened
  ASSERT_EQ(CanvasElement().ResourceProvider()->TotalOpCount(),
            initial_op_count);
}

TEST_P(CanvasRenderingContext2DTest, AutoFlushPinnedImages) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  Context2D()->fillRect(0, 0, 1, 1);  // Ensure resource provider is created.

  constexpr unsigned int kImageSize = 10;
  constexpr unsigned int kBytesPerImage = 400;

  const size_t initial_op_count =
      CanvasElement().ResourceProvider()->TotalOpCount();

  // We repeat the test twice to verify that the state was properly
  // reset by the Flush.
  for (int repeat = 0; repeat < 2; ++repeat) {
    size_t expected_op_count = initial_op_count;
    for (size_t pinned_bytes = 0; pinned_bytes <= kMaxPinnedImageKB * 1024;
         pinned_bytes += kBytesPerImage) {
      FakeImageSource unique_image(gfx::Size(kImageSize, kImageSize),
                                   kOpaqueBitmap);
      NonThrowableExceptionState exception_state;
      Context2D()->drawImage(&unique_image, 0, 0, 1, 1, 0, 0, 1, 1,
                             exception_state);
      EXPECT_FALSE(exception_state.HadException());
      ++expected_op_count;
      ASSERT_EQ(CanvasElement().ResourceProvider()->TotalOpCount(),
                expected_op_count);
    }
    Context2D()->fillRect(0, 0, 1, 1);  // Trigger flush due to memory limit
    ASSERT_EQ(CanvasElement().ResourceProvider()->TotalOpCount(),
              initial_op_count);
  }
}

TEST_P(CanvasRenderingContext2DTest, OverdrawResetsPinnedImageBytes) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  constexpr unsigned int kImageSize = 10;
  constexpr unsigned int kBytesPerImage = 400;

  FakeImageSource unique_image(gfx::Size(kImageSize, kImageSize),
                               kOpaqueBitmap);
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&unique_image, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  size_t initial_op_count = CanvasElement().ResourceProvider()->TotalOpCount();
  ASSERT_EQ(CanvasElement().ResourceProvider()->TotalPinnedImageBytes(),
            kBytesPerImage);

  Context2D()->clearRect(0, 0, 10, 10);  // Overdraw
  ASSERT_EQ(CanvasElement().ResourceProvider()->TotalOpCount(),
            initial_op_count);
  ASSERT_EQ(CanvasElement().ResourceProvider()->TotalPinnedImageBytes(), 0u);
}

TEST_P(CanvasRenderingContext2DTest, AutoFlushSameImage) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  Context2D()->fillRect(0, 0, 1, 1);  // Ensure resource provider is created.
  size_t expected_op_count = CanvasElement().ResourceProvider()->TotalOpCount();

  constexpr unsigned int kImageSize = 10;
  constexpr unsigned int kBytesPerImage = 400;

  FakeImageSource image(gfx::Size(kImageSize, kImageSize), kOpaqueBitmap);

  for (size_t pinned_bytes = 0; pinned_bytes <= 2 * kMaxPinnedImageKB * 1024;
       pinned_bytes += kBytesPerImage) {
    NonThrowableExceptionState exception_state;
    Context2D()->drawImage(&image, 0, 0, 1, 1, 0, 0, 1, 1, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    ++expected_op_count;
    ASSERT_EQ(CanvasElement().ResourceProvider()->TotalOpCount(),
              expected_op_count);
  }
}

TEST_P(CanvasRenderingContext2DTest, AutoFlushDelayedByLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);
  NonThrowableExceptionState exception_state;
  Context2D()->beginLayer(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                          BeginLayerOptions::Create(), exception_state);
  const size_t initial_op_count =
      CanvasElement().ResourceProvider()->TotalOpCount();
  while (CanvasElement().ResourceProvider()->TotalOpBytesUsed() <=
         kMaxRecordedOpKB * 1024 * 2) {
    Context2D()->fillRect(0, 0, 1, 1);
    ASSERT_GT(CanvasElement().ResourceProvider()->TotalOpCount(),
              initial_op_count);
  }
  // Closing the layer means next op can trigger auto flush
  Context2D()->endLayer(exception_state);
  Context2D()->fillRect(0, 0, 1, 1);
  ASSERT_EQ(CanvasElement().ResourceProvider()->TotalOpCount(),
            initial_op_count);
}

class CanvasRenderingContext2DTestAccelerated
    : public CanvasRenderingContext2DTest {
 protected:
  bool AllowsAcceleration() override { return true; }

  void CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled() {
    for (int i = 0; i < 200; ++i) {
      auto* canvas = MakeGarbageCollected<HTMLCanvasElement>(GetDocument());
      CreateContext(
          kNonOpaque, kNormalLatency,
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined,
          canvas);
      canvas->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
      // Expect that at least the first 10 are accelerated. The exact number
      // depends on the feature params.
      if (i < 10) {
        EXPECT_TRUE(canvas->IsAccelerated());
      }
      canvas->DisableAcceleration();
    }
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTestAccelerated);

TEST_P(CanvasRenderingContext2DTestAccelerated,
       RemainAcceleratedAfterGetImageDataWithWillNotReadFrequently) {
  base::test::ScopedFeatureList feature_list_;
  CreateContext(
      kNonOpaque, kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently::kFalse);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  DrawSomething();
  NonThrowableExceptionState exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  Context2D()->getImageData(0, 0, 1, 1, settings, exception_state);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
}

// https://crbug.com/708445: When the Canvas2DLayerBridge hibernates or wakes up
// from hibernation, the compositing reasons for the canvas element may change.
// In these cases, the element should request a compositing update.
TEST_P(CanvasRenderingContext2DTestAccelerated,
       ElementRequestsCompositingUpdateOnHibernateAndWakeUp) {
  CreateContext(kNonOpaque);
  gfx::Size size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);
  CanvasElement().GetCanvas2DLayerBridge()->SetCanvasResourceHost(
      canvas_element_);

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  // Take a snapshot to trigger lazy resource provider creation
  CanvasElement().GetCanvas2DLayerBridge()->NewImageSnapshot(
      FlushReason::kTesting);
  EXPECT_TRUE(!!CanvasElement().ResourceProvider());
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  auto* box = CanvasElement().GetLayoutBoxModelObject();
  EXPECT_TRUE(box);
  PaintLayer* painting_layer = box->PaintingLayer();
  EXPECT_TRUE(painting_layer);
  UpdateAllLifecyclePhasesForTest();

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);
  // Run hibernation task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();
  // If enabled, hibernation should cause repaint of the painting layer.
  EXPECT_FALSE(box->NeedsPaintPropertyUpdate());
  EXPECT_EQ(Canvas2DLayerBridge::IsHibernationEnabled(),
            painting_layer->SelfNeedsRepaint());
  EXPECT_EQ(Canvas2DLayerBridge::IsHibernationEnabled(),
            !CanvasElement().ResourceProvider());

  // The page is hidden so it doesn't make sense to paint, and doing so will
  // DCHECK. Update all other lifecycle phases.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  // Wake up again, which should request repaint of the painting layer.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kVisible,
      /*is_initial_state=*/false);
  EXPECT_FALSE(box->NeedsPaintPropertyUpdate());
  EXPECT_EQ(Canvas2DLayerBridge::IsHibernationEnabled(),
            painting_layer->SelfNeedsRepaint());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       NoHibernationIfNoResourceProvider) {
  CreateContext(kNonOpaque);
  gfx::Size size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(nullptr, std::move(bridge),
                                                size);
  CanvasElement().GetCanvas2DLayerBridge()->SetCanvasResourceHost(
      canvas_element_);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  auto* box = CanvasElement().GetLayoutBoxModelObject();
  EXPECT_TRUE(box);
  PaintLayer* painting_layer = box->PaintingLayer();
  EXPECT_TRUE(painting_layer);
  UpdateAllLifecyclePhasesForTest();

  // The resource provider gets lazily created. Force it to be dropped.
  canvas_element_->ReplaceResourceProvider(nullptr);

  // Hide element to trigger hibernation (if enabled).
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);
  // Run hibernation task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();

  // Never hibernate a canvas with no resource provider.
  EXPECT_FALSE(box->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(painting_layer->SelfNeedsRepaint());
}

TEST_P(CanvasRenderingContext2DTestAccelerated, LowLatencyIsNotSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_EQ(Context2D()->getContextAttributes()->willReadFrequently(),
            V8CanvasWillReadFrequently::Enum::kUndefined);
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_FALSE(
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->SupportsSingleBuffering());
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
}

TEST_P(CanvasRenderingContext2DTestAccelerated, DrawImage_Video_Flush) {
  V8TestingScope scope;

  CreateContext(kNonOpaque);
  // No need to set-up the layer bridge when testing low latency mode.
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  gfx::Size visible_size(10, 10);
  scoped_refptr<media::VideoFrame> media_frame =
      media::VideoFrame::WrapVideoFrame(
          media::VideoFrame::CreateBlackFrame(/*size=*/gfx::Size(16, 16)),
          media::PIXEL_FORMAT_I420,
          /*visible_rect=*/gfx::Rect(visible_size),
          /*natural_size=*/visible_size);
  media_frame->set_timestamp(base::Microseconds(1000));
  VideoFrame* frame = MakeGarbageCollected<VideoFrame>(std::move(media_frame),
                                                       GetExecutionContext());
  NonThrowableExceptionState exception_state;

  Context2D()->fillRect(0, 0, 5, 5);
  EXPECT_TRUE(CanvasElement().ResourceProvider()->HasRecordedDrawOps());

  Context2D()->drawImage(frame, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  // The drawImage Operation is supposed to trigger a flush, which means that
  // There should not be any Recorded ops at this point.
  EXPECT_FALSE(CanvasElement().ResourceProvider()->HasRecordedDrawOps());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DISABLED_DisableAcceleration_UpdateGPUMemoryUsage) {
  CreateContext(kNonOpaque);

  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);
  CanvasRenderingContext2D* context = Context2D();

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required

  context->fillRect(10, 10, 100, 100);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  CanvasElement().DisableAcceleration();
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);

  context->fillRect(10, 10, 100, 100);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DisableAcceleration_RestoreCanvasMatrixClipStack) {
  // This tests verifies whether the RestoreCanvasMatrixClipStack happens after
  // PaintCanvas is drawn from old 2d bridge to new 2d bridge.
  InSequence s;

  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  auto fake_accelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      nullptr, std::move(fake_accelerate_surface), size);

  FakeCanvasResourceHost host(size);
  auto fake_deaccelerate_surface = std::make_unique<FakeCanvas2DLayerBridge>();
  host.SetPreferred2DRasterMode(RasterModeHint::kPreferCPU);
  fake_deaccelerate_surface->SetCanvasResourceHost(&host);

  FakeCanvas2DLayerBridge* surface_ptr = fake_deaccelerate_surface.get();

  EXPECT_CALL(*fake_deaccelerate_surface, DrawFullImage(_)).Times(1);
  EXPECT_CALL(*fake_deaccelerate_surface, DidRestoreCanvasMatrixClipStack(_))
      .Times(1);

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  EXPECT_TRUE(
      IsCanvasResourceHostSet(CanvasElement().GetCanvas2DLayerBridge()));

  CanvasElement().DisableAcceleration(std::move(fake_deaccelerate_surface));
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  EXPECT_TRUE(
      IsCanvasResourceHostSet(CanvasElement().GetCanvas2DLayerBridge()));

  Mock::VerifyAndClearExpectations(surface_ptr);
}

class CanvasRenderingContext2DTestAcceleratedMultipleDisables
    : public CanvasRenderingContext2DTest {
 protected:
  void SetUp() override {
    base::FieldTrialParams params;
    params["canvas-disable-acceleration-threshold"] = "10";
    params["canvas-disable-acceleration-percent"] = "80";
    feature_list_.InitAndEnableFeatureWithParameters(
        kStartCanvasWithAccelerationDisabled, params);
    CanvasRenderingContext2DTest::SetUp();
  }

  bool AllowsAcceleration() override { return true; }

  void CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled() {
    for (int i = 0; i < 10; ++i) {
      auto* canvas = MakeGarbageCollected<HTMLCanvasElement>(GetDocument());
      CreateContext(
          kNonOpaque, kNormalLatency,
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined,
          canvas);
      canvas->GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
      EXPECT_TRUE(canvas->IsAccelerated());
      canvas->DisableAcceleration();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(
    CanvasRenderingContext2DTestAcceleratedMultipleDisables);

TEST_P(CanvasRenderingContext2DTestAcceleratedMultipleDisables,
       ReadFrequentlyUndefined) {
  CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled();
  CreateContext(
      kNonOpaque, kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  // Because a bunch of canvases had acceleration explicitly disabled, canvases
  // created with `kUndefined` should start with acceleration disabled.
  EXPECT_FALSE(CanvasElement().IsAccelerated());
}

TEST_P(CanvasRenderingContext2DTestAcceleratedMultipleDisables,
       ReadFrequentlyFalse) {
  CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled();
  CreateContext(
      kNonOpaque, kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently::kFalse);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  // Canvases created with `kFalse` should always start with acceleration
  // enabled regardless of how many canvases had acceleration disabled.
  EXPECT_TRUE(CanvasElement().IsAccelerated());
}

TEST_P(CanvasRenderingContext2DTestAcceleratedMultipleDisables,
       ReadFrequentlyTrue) {
  CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled();
  CreateContext(kNonOpaque, kNormalLatency,
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  // Canvases created with `kTrue` should always start with acceleration
  // disabled regardless of how many canvases had acceleration explicitly
  // disabled.
  EXPECT_FALSE(CanvasElement().IsAccelerated());
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
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::BGRA_8888,
                                                   true);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_scanout_shared_images = true;
    context_provider->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

    return context_provider;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTestImageChromium);

TEST_P(CanvasRenderingContext2DTestImageChromium, LowLatencyIsSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_EQ(Context2D()->getContextAttributes()->willReadFrequently(),
            V8CanvasWillReadFrequently::Enum::kUndefined);
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  EXPECT_TRUE(CanvasElement()
                  .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
                  ->SupportsSingleBuffering());
  auto frame1_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource(FlushReason::kTesting);
  EXPECT_TRUE(frame1_resource);
  DrawSomething();
  auto frame2_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource(FlushReason::kTesting);
  EXPECT_TRUE(frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}

class CanvasRenderingContext2DTestSwapChain
    : public CanvasRenderingContext2DTestAccelerated {
 protected:
  CanvasRenderingContext2DTestSwapChain()
      : CanvasRenderingContext2DTestAccelerated() {}

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

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTestSwapChain);

TEST_P(CanvasRenderingContext2DTestSwapChain, LowLatencyIsSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_EQ(Context2D()->getContextAttributes()->willReadFrequently(),
            V8CanvasWillReadFrequently::Enum::kUndefined);
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  EXPECT_TRUE(CanvasElement()
                  .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
                  ->SupportsSingleBuffering());
  auto frame1_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource(FlushReason::kTesting);
  EXPECT_TRUE(frame1_resource);
  DrawSomething();
  auto frame2_resource =
      CanvasElement()
          .GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU)
          ->ProduceCanvasResource(FlushReason::kTesting);
  EXPECT_TRUE(frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}

}  // namespace blink
