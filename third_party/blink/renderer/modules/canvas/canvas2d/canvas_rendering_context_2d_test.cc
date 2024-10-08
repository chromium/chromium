// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_base.h"
#include "base/notreached.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_op.h"
#include "cc/test/paint_op_matchers.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float32array_uint16array_uint8clampedarray.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2043)
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_will_read_frequently.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_status.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_util.h"

// Including "base/time/time.h" triggers a bug in IWYU:
// https://github.com/include-what-you-use/include-what-you-use/issues/1122
// IWYU pragma: no_include "base/numerics/clamped_math.h"

// GoogleTest expectation macros trigger a bug in IWYU:
// https://github.com/include-what-you-use/include-what-you-use/issues/1546
// IWYU pragma: no_include <string>

class GrDirectContext;

namespace blink {
class CanvasResourceHost;
class ExecutionContext;
}  // namespace blink

using ::base::test::ScopedFeatureList;
using ::blink_testing::ClearRectFlags;
using ::blink_testing::FillFlags;
using ::blink_testing::RecordedOpsAre;
using ::cc::ClipRectOp;
using ::cc::DrawColorOp;
using ::cc::DrawImageRectOp;
using ::cc::DrawPathOp;
using ::cc::DrawRectOp;
using ::cc::PaintOpEq;
using ::cc::PaintOpIs;
using ::cc::RestoreOp;
using ::cc::SaveLayerAlphaOp;
using ::cc::SaveLayerOp;
using ::cc::SaveOp;
using ::cc::SetMatrixOp;
using ::cc::TranslateOp;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Message;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::SaveArg;

namespace blink {

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

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
 public:
  CanvasRenderingContext2DTest();
  void SetUp() override;
  virtual bool AllowsAcceleration() { return false; }

  virtual scoped_refptr<viz::TestContextProvider> CreateContextProvider() {
    return viz::TestContextProvider::Create();
  }

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  cc::PaintCanvas& Canvas() {
    return CanvasElement().ResourceProvider()->Canvas();
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

  void TearDownHost() {
    // To tear down the host it is both necessary and sufficient to tear down
    // the document, as the document effectively owns the host.
    web_view_helper_ = nullptr;
  }

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

  test::TaskEnvironment task_environment_;
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
  SharedGpuContext::Reset();

  // Prevent CanvasPerformanceMonitor state from leaking between tests.
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
}

//============================================================================

enum class CompositingMode {
  kDoesNotSupportDirectCompositing,
  kSupportsDirectCompositing
};

class FakeCanvasResourceProvider : public CanvasResourceProvider {
 public:
  FakeCanvasResourceProvider(const SkImageInfo& info,
                             RasterModeHint hint,
                             CanvasResourceHost* resource_host,
                             CompositingMode compositing_mode)
      : CanvasResourceProvider(CanvasResourceProvider::kSharedImage,
                               info,
                               cc::PaintFlags::FilterQuality::kLow,
                               /*is_origin_top_left=*/false,
                               SharedGpuContext::ContextProviderWrapper(),
                               /*resource_dispatcher=*/nullptr,
                               resource_host),
        is_accelerated_(hint != RasterModeHint::kPreferCPU),
        supports_direct_compositing_(
            compositing_mode == CompositingMode::kSupportsDirectCompositing) {
    ON_CALL(*this, Snapshot)
        .WillByDefault(
            [this](FlushReason reason, ImageOrientation orientation) {
              return SnapshotInternal(orientation, reason);
            });
  }
  ~FakeCanvasResourceProvider() override = default;
  bool IsAccelerated() const override { return is_accelerated_; }
  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) override {
    return scoped_refptr<CanvasResource>(CanvasResourceSharedImage::Create(
        GetSkImageInfo(), SharedGpuContext::ContextProviderWrapper(),
        CreateWeakPtr(), cc::PaintFlags::FilterQuality::kLow,
        /*is_origin_top_left=*/true, IsAccelerated(),
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
            gpu::SHARED_IMAGE_USAGE_RASTER_WRITE));
  }
  bool SupportsDirectCompositing() const override {
    return supports_direct_compositing_;
  }
  bool IsValid() const override { return true; }
  sk_sp<SkSurface> CreateSkSurface() const override {
    return SkSurfaces::Raster(GetSkImageInfo());
  }

  MOCK_METHOD((void), RasterRecord, (cc::PaintRecord last_recording));

  MOCK_METHOD((scoped_refptr<StaticBitmapImage>),
              Snapshot,
              (FlushReason reason, ImageOrientation orientation));

  MOCK_METHOD(bool,
              WritePixels,
              (const SkImageInfo& orig_info,
               const void* pixels,
               size_t row_bytes,
               int x,
               int y));

 private:
  bool is_accelerated_;
  bool supports_direct_compositing_;
};

//============================================================================

MATCHER_P(OverdrawOpAreMatcher, expected_overdraw_ops, "") {
  constexpr int last_bucket =
      static_cast<int>(BaseRenderingContext2D::OverdrawOp::kMaxValue);
  for (int bucket = 0; bucket <= last_bucket; ++bucket) {
    SCOPED_TRACE(Message() << "Checking overdraw bucket: " << bucket);
    arg.ExpectBucketCount(
        "Blink.Canvas.OverdrawOp", bucket,
        static_cast<base::HistogramBase::Count>(expected_overdraw_ops.count(
            static_cast<BaseRenderingContext2D::OverdrawOp>(bucket))));
  }
  return true;
}

template <typename... Args>
testing::Matcher<base::HistogramTester> OverdrawOpAre(Args... args) {
  return OverdrawOpAreMatcher(
      std::unordered_set<BaseRenderingContext2D::OverdrawOp>{args...});
}

TEST_P(CanvasRenderingContext2DTest, NonDisplayedCanvasIsNotRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  EXPECT_TRUE(CanvasElement().IsResourceValid());
  CanvasElement().SetIsDisplayed(false);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should not result in rate limiting as the
  // canvas is not displayed.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTest,
       DisplayedNonPaintableCanvasIsNotRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  EXPECT_TRUE(CanvasElement().IsResourceValid());
  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should not result in rate limiting as the
  // canvas is not paintable.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTest,
       DisplayedPaintableNonCompositedCanvasIsNotRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Install a Canvas2DLayerBridge instance to make the canvas paintable and a
  // CanvasResourceProvider that does not support direct compositing.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kDoesNotSupportDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  EXPECT_TRUE(CanvasElement().IsResourceValid());
  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should not result in rate limiting as the
  // canvas is not composited.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTest,
       DisplayedPaintableCompositedCanvasIsRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Install a Canvas2DLayerBridge instance to make the canvas paintable and a
  // CanvasResourceProvider that supports direct compositing to make the canvas
  // composited.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  EXPECT_TRUE(CanvasElement().IsResourceValid());
  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should result in rate limiting.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_TRUE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTest, HidingCanvasTurnsOffRateLimiting) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Install a Canvas2DLayerBridge instance to make the canvas paintable and a
  // CanvasResourceProvider that supports direct compositing to make the canvas
  // composited.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  EXPECT_TRUE(CanvasElement().IsResourceValid());
  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_TRUE(!!CanvasElement().RateLimiter());

  CanvasElement().SetIsDisplayed(false);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTest, GetImageWithAccelerationDisabled) {
  CreateContext(kNonOpaque);

  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferCPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);
  ASSERT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  ASSERT_TRUE(CanvasElement().IsResourceValid());

  EXPECT_FALSE(Context2D()
                   ->GetImage(FlushReason::kTesting)
                   ->PaintImageForCurrentFrame()
                   .IsTextureBacked());

  // The GetImage() call should have preserved the rasterization mode as well as
  // the validity of the resource.
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  EXPECT_TRUE(CanvasElement().IsResourceValid());
}

TEST_P(CanvasRenderingContext2DTest, GetImageAfterContextLoss) {
  // Gpu compositing must be supported for this test to be able to create
  // a CanvasResourceSharedBitmap instance.
  ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>
      accelerated_compositing_scope;

  CreateContext(kNonOpaque);

  // Do initial setup to ensure that CanvasResourceHost will check for the GPU
  // context being lost as part of checking resource validity:

  // * Install a CanvasResourceProvider that is accelerated and supports direct
  //   compositing. The former is necessary as part of ensuring that
  //   CanvasResourceHost::IsResourceValid() checks for context loss, while the
  //   latter is necessary for GetOrCreateCcLayerIfNeeded() to succeed.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  // * Put the host in GPU compositing mode, also necessary to ensure that
  //   IsResourceValid() checks for context loss.
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  // * Finally, create a CC layer as otherwise IsReturnValid() will always
  //   unconditionally return true.
  EXPECT_TRUE(CanvasElement().GetOrCreateCcLayerIfNeeded());

  EXPECT_TRUE(CanvasElement().IsResourceValid());
  EXPECT_TRUE(Context2D()->GetImage(FlushReason::kTesting));

  test_context_provider_->TestContextGL()->set_context_lost(true);

  EXPECT_FALSE(Context2D()->GetImage(FlushReason::kTesting));
}

TEST_P(CanvasRenderingContext2DTest, GetImageWithAcceleration) {
  CreateContext(kNonOpaque);

  // Ensure that the canvas resource host prefers GPU rasterization to be able
  // to check that GetImage() preserves GPU rasterization being used.
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  // Inject a CanvasResourceProviderSharedImage instance to ensure the presence
  // of a CanvasResourceProvider that creates accelerated snapshots.
  gfx::Size size = CanvasElement().Size();
  std::unique_ptr<CanvasResourceProvider> provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          SkImageInfo::MakeN32Premul(size.width(), size.height()),
          cc::PaintFlags::FilterQuality::kMedium,
          CanvasResourceProvider::ShouldInitialize::kCallClear,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
              gpu::SHARED_IMAGE_USAGE_SCANOUT,
          &CanvasElement());
  ASSERT_TRUE(provider);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);
  ASSERT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  ASSERT_TRUE(CanvasElement().IsResourceValid());

  // Verify that CanvasRenderingContext2D::GetImage() creates an accelerated
  // image given that the underlying CanvasResourceProvider does so.
  EXPECT_TRUE(Context2D()
                  ->GetImage(FlushReason::kTesting)
                  ->PaintImageForCurrentFrame()
                  .IsTextureBacked());

  // The GetImage() call should have preserved the rasterization mode as well as
  // the validity of the resource.
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  EXPECT_TRUE(CanvasElement().IsResourceValid());
}

TEST_P(CanvasRenderingContext2DTest, FallbackToSoftwareOnFailedTextureAlloc) {
  CreateContext(kNonOpaque);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  // As no CanvasResourceProvider has yet been created, the host should default
  // to the raster mode that has been set as preferred.
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  // This will cause SkSurface_Gpu creation to fail without
  // Canvas2DLayerBridge otherwise detecting that anything was disabled.
  SharedGpuContext::ContextProviderWrapper()
      ->ContextProvider()
      ->GetGrContext()
      ->abandonContext();

  // Drawing to the canvas should cause a CanvasResourceProvider to be created.
  // It is not possible to create a valid CanvasResourceProviderSharedImage
  // instance without a GrContext as creating an SkSurface will fail, so the
  // created provider should be unaccelerated (and hence downgrade the raster
  // mode to CPU).
  Context2D()->fillRect(3, 3, 1, 1);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);

  // Without GPU rasterization, snapshots should not be texture-backed.
  EXPECT_FALSE(Context2D()
                   ->GetImage(FlushReason::kTesting)
                   ->PaintImageForCurrentFrame()
                   .IsTextureBacked());

  // Verify that taking the snapshot did not alter the raster mode.
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       PrepareMailboxWhenContextIsLostWithFailedRestore) {
  // Gpu compositing must be supported for this test to be able to create
  // a CanvasResourceSharedBitmap instance.
  ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>
      accelerated_compositing_scope;

  CreateContext(kNonOpaque);

  // Do initial setup to ensure that CanvasResourceHost will check for the GPU
  // context being lost as part of checking resource validity:

  // * Install a CanvasResourceProvider that is accelerated and supports direct
  //   compositing. The former is necessary to ensure that
  //   CanvasResourceHost::IsResourceValid() checks for context loss, while the
  //   latter is necessary for GetOrCreateCcLayerIfNeeded() to succeed.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  // * The host must also be in GPU compositing mode for IsResourceValid()
  //   to check for context loss.
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  // * Finally, IsResourceValid() always returns true in the absence of a CC
  //   layer.
  EXPECT_TRUE(CanvasElement().GetOrCreateCcLayerIfNeeded());

  // The resource should start off valid.
  EXPECT_TRUE(CanvasElement().IsResourceValid());

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_TRUE(CanvasElement().PrepareTransferableResource(nullptr, &resource,
                                                          &release_callback));

  // Losing the context should result in the resource becoming invalid and the
  // host being unable to produce a TransferableResource from it.
  test_context_provider_->TestContextGL()->set_context_lost(true);
  EXPECT_FALSE(CanvasElement().IsResourceValid());
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));

  // Restoration of the context should fail because
  // Platform::createSharedOffscreenGraphicsContext3DProvider() is stubbed in
  // unit tests. This simulates what would happen when attempting to restore
  // while the GPU process is down.
  Context2D()->TryRestoreContextEvent(/*timer=*/nullptr);
  EXPECT_FALSE(CanvasElement().IsResourceValid());
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
}

TEST_P(CanvasRenderingContext2DTest, FillRect_FullCoverage) {
  // Fill rect no longer supports overdraw optimizations
  // Reason: low real world incidence not worth the test overhead.
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->fillRect(-1, -1, 12, 12);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 1, 1), FillFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(-1, -1, 12, 12),
                                FillFlags()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DisableOverdrawOptimization) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kDisableCanvasOverdrawOptimization);

  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 1, 1), FillFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                ClearRectFlags()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_ExactCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, 10, 10), ClearRectFlags()))));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kClearRect));
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_PartialCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 9, 9);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 1, 1), FillFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 9, 9),
                                ClearRectFlags()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_InsideLayer) {
  // Overdraw is not currently implemented when layers are opened.
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState no_exception;
  Context2D()->fillRect(1, 1, 1, 1);
  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);
  Context2D()->fillRect(2, 2, 2, 2);
  Context2D()->clearRect(0, 0, 10, 10);
  Context2D()->fillRect(3, 3, 3, 3);
  Context2D()->endLayer(no_exception);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 1, 1), FillFlags()),
          DrawRecordOpEq(
              PaintOpEq<SaveLayerAlphaOp>(1.0f),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(2, 2, 2, 2), FillFlags()),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                    ClearRectFlags()),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 3, 3), FillFlags()),
              PaintOpEq<RestoreOp>()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_InsideNestedLayer) {
  // Overdraw is not currently implemented when layers are opened.
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState no_exception;
  Context2D()->fillRect(1, 1, 1, 1);
  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);
  Context2D()->fillRect(2, 2, 2, 2);
  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);
  Context2D()->fillRect(3, 3, 3, 3);
  Context2D()->clearRect(0, 0, 10, 10);
  Context2D()->fillRect(4, 4, 4, 4);
  Context2D()->endLayer(no_exception);
  Context2D()->endLayer(no_exception);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 1, 1), FillFlags()),
          DrawRecordOpEq(
              PaintOpEq<SaveLayerAlphaOp>(1.0f),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(2, 2, 2, 2), FillFlags()),
              PaintOpEq<SaveLayerAlphaOp>(1.0f),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 3, 3), FillFlags()),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                    ClearRectFlags()),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(4, 4, 4, 4), FillFlags()),
              PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_GlobalAlpha) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->setGlobalAlpha(0.5f);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, 10, 10), ClearRectFlags()))));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kClearRect));
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_TransparentGradient) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  auto* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  SetFillStyleHelper(Context2D(), script_state, AlphaGradient().Get());
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, 10, 10), ClearRectFlags()))));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kClearRect));
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_Filter) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  V8UnionCanvasFilterOrString* filter =
      MakeGarbageCollected<V8UnionCanvasFilterOrString>("blur(4px)");
  Context2D()->setFilter(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         filter);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, 10, 10), ClearRectFlags()))));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kClearRect));
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_TransformPartialCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->translate(1, 1);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpIs<TranslateOp>(),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 1, 1), FillFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                ClearRectFlags()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_TransformCompleteCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->translate(1, 1);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(-1, -1, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(
                  PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 1,  //
                                               0, 1, 0, 1,  //
                                               0, 0, 1, 0,  //
                                               0, 0, 0, 1)),
                  PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(-1, -1, 10, 10),
                                        ClearRectFlags()))));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kClearRect,
                            BaseRenderingContext2D::OverdrawOp::kHasTransform));
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_IgnoreCompositeOp) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->setGlobalCompositeOperation(String("destination-in"));
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, 10, 10), ClearRectFlags()))));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kClearRect));
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_Clipped) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->rect(0, 0, 5, 5);
  Context2D()->clip();
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpIs<ClipRectOp>(),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 1, 1), FillFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 10, 10),
                                ClearRectFlags()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_ExactCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kDrawImage));
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_Magnified) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 1, 1, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kDrawImage));
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_GlobalAlpha) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->setGlobalAlpha(0.5f);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawRectOp>(),
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_TransparentBitmap) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&alpha_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawRectOp>(),
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_Filter) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  V8UnionCanvasFilterOrString* filter =
      MakeGarbageCollected<V8UnionCanvasFilterOrString>("blur(4px)");
  Context2D()->setFilter(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                         filter);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(
                  // Composited DrawRectOp:
                  PaintOpIs<SetMatrixOp>(), PaintOpIs<SaveLayerOp>(),
                  PaintOpIs<SetMatrixOp>(), PaintOpIs<DrawRectOp>(),
                  PaintOpIs<RestoreOp>(), PaintOpIs<SetMatrixOp>(),
                  // Composited DrawImageRectOp:
                  PaintOpIs<SetMatrixOp>(), PaintOpIs<SaveLayerOp>(),
                  PaintOpIs<SetMatrixOp>(), PaintOpIs<DrawImageRectOp>(),
                  PaintOpIs<RestoreOp>(), PaintOpIs<SetMatrixOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_PartialCoverage1) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawRectOp>(),
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_FALSE(exception_state.HadException());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_PartialCoverage2) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 9, 9,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawRectOp>(),
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_FullCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 11, 11,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kDrawImage));
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_TransformFullCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->translate(-1, 0);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<SetMatrixOp>(),
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kDrawImage,
                            BaseRenderingContext2D::OverdrawOp::kHasTransform));
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_TransformPartialCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->translate(-1, 0);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<TranslateOp>(),  //
                                      PaintOpIs<DrawRectOp>(),   //
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_TransparenBitmapOpaqueGradient) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  auto* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  NonThrowableExceptionState exception_state;
  SetFillStyleHelper(Context2D(), GetScriptState(), OpaqueGradient().Get());
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&alpha_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawRectOp>(),
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest,
       DrawImage_OpaqueBitmapTransparentGradient) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  auto* script_state = GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  NonThrowableExceptionState exception_state;
  SetFillStyleHelper(Context2D(), GetScriptState(), AlphaGradient().Get());
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester,
              OverdrawOpAre(BaseRenderingContext2D::OverdrawOp::kTotal,
                            BaseRenderingContext2D::OverdrawOp::kDrawImage));
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_CopyPartialCoverage) {
  // The 'copy' blend mode no longer trigger the overdraw optimization
  // Reason: low real-world incidence, test overhead not justified.
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->setGlobalCompositeOperation(String("copy"));
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(
                  // Copy composite op clears the frame before each draw ops.
                  PaintOpIs<DrawColorOp>(), PaintOpIs<DrawRectOp>(),
                  PaintOpIs<DrawColorOp>(), PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_CopyTransformPartialCoverage) {
  // Overdraw optimizations with the 'copy' composite operation are no longer
  // supported. Reason: low real-world incidence, test overhead not justified.
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->setGlobalCompositeOperation(String("copy"));
  Context2D()->translate(1, 1);
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 1, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(
                  PaintOpIs<TranslateOp>(),
                  // Copy composite op clears the frame before each draw ops.
                  PaintOpIs<DrawColorOp>(), PaintOpIs<DrawRectOp>(),
                  PaintOpIs<DrawColorOp>(), PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, DrawImage_Clipped) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  NonThrowableExceptionState exception_state;
  Context2D()->rect(0, 0, 5, 5);
  Context2D()->clip();
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->drawImage(&opaque_bitmap_, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<ClipRectOp>(),  //
                                      PaintOpIs<DrawRectOp>(),  //
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, PutImageData_FullCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // The recording will be cleared, so nothing will be rastered before
  // `WritePixels` is called.
  InSequence s;
  EXPECT_CALL(*provider, RasterRecord).Times(0);
  EXPECT_CALL(*provider, WritePixels).Times(1);

  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->putImageData(full_image_data_.Get(), 0, 0, exception_state);

  // `putImageData` isn't included in the recording, keeping it empty.
  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Eq(std::nullopt));

  // `putImageData` overdraw isn't handled by
  // `BaseRenderingContext2D::CheckOverdraw` like other draw operations, so the
  // histograms aren't updated.
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, PutImageData_PartialCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // `putImageData` forces a flush, so the `fillRect` will get rasterized before
  // `WritePixels` is called.
  InSequence s;
  EXPECT_CALL(*provider, RasterRecord(RecordedOpsAre(PaintOpIs<DrawRectOp>())))
      .Times(1);
  EXPECT_CALL(*provider, WritePixels).Times(1);

  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  // `putImageData` forces a flush, which clears the recording.
  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->putImageData(partial_image_data_.Get(), 0, 0, exception_state);

  // `putImageData` isn't included in the recording, keeping it empty.
  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Eq(std::nullopt));

  // `putImageData` overdraw isn't handled by
  // `BaseRenderingContext2D::CheckOverdraw` like other draw operations, so the
  // histograms aren't updated.
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, Path_FullCoverage) {
  // This case is an overdraw but the current detection logic rejects all
  // paths.
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->rect(-1, -1, 12, 12);
  Context2D()->fill();

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kTesting),
              Optional(RecordedOpsAre(PaintOpIs<DrawRectOp>(),
                                      PaintOpIs<DrawPathOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

//==============================================================================

TEST_P(CanvasRenderingContext2DTest, ImageResourceLifetime) {
  auto* canvas = To<HTMLCanvasElement>(
      GetDocument().CreateRawElement(html_names::kCanvasTag));
  canvas->SetSize(gfx::Size(40, 40));
  ImageBitmap* image_bitmap_derived = nullptr;
  {
    const ImageBitmapOptions* default_options = ImageBitmapOptions::Create();
    std::optional<gfx::Rect> crop_rect =
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
          RasterModeHint::kPreferGPU, &CanvasElement(),
          CompositingMode::kSupportsDirectCompositing);
  auto bridge = std::make_unique<Canvas2DLayerBridge>(&CanvasElement());
  Canvas2DLayerBridge* bridge_ptr = bridge.get();
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      std::move(fake_resource_provider), std::move(bridge), size);

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
  auto bridge2 = std::make_unique<Canvas2DLayerBridge>(&CanvasElement());
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider2 =
      std::make_unique<FakeCanvasResourceProvider>(
          SkImageInfo::MakeN32Premul(size2.width(), size2.height()),
          RasterModeHint::kPreferGPU, &CanvasElement(),
          CompositingMode::kSupportsDirectCompositing);
  anotherCanvas->SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  anotherCanvas->SetResourceProviderForTesting(
      std::move(fake_resource_provider2), std::move(bridge2), size2);

  // Tear down the first image buffer that resides in current canvas element
  CanvasElement().SetSize(gfx::Size(20, 20));
  Mock::VerifyAndClearExpectations(bridge_ptr);

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
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

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
  std::optional<Vector<char>> pixel_buffer_data = test::ReadFromFile(filepath);
  ASSERT_TRUE(pixel_buffer_data);
  scoped_refptr<SharedBuffer> pixel_buffer =
      SharedBuffer::Create(std::move(*pixel_buffer_data));

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
  constexpr size_t data_length = std::size(u8_pixels);

  std::array<uint16_t, data_length> u16_pixels;
  for (size_t i = 0; i < data_length; i++)
    u16_pixels[i] = u8_pixels[i] * 257;

  std::array<float, data_length> f32_pixels;
  for (size_t i = 0; i < data_length; i++)
    f32_pixels[i] = u8_pixels[i] / 255.0;

  NotShared<DOMUint8ClampedArray> data_u8(
      DOMUint8ClampedArray::Create(u8_pixels));
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->length());
  NotShared<DOMUint16Array> data_u16(DOMUint16Array::Create(u16_pixels));
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->length());
  NotShared<DOMFloat32Array> data_f32(DOMFloat32Array::Create(f32_pixels));
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
          NOTREACHED_IN_MIGRATION();
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
          NOTREACHED_IN_MIGRATION();
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
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

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
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);
  Context2D()->fillRect(0, 0, 1, 1);  // Ensure resource provider is created.
  const size_t initial_op_count = Context2D()->Recorder()->TotalOpCount();

  while (Context2D()->Recorder()->TotalOpBytesUsed() <=
         kMaxRecordedOpKB * 1024) {
    Context2D()->fillRect(0, 0, 1, 1);
    // Verify that auto-flush did not happen
    ASSERT_GT(Context2D()->Recorder()->TotalOpCount(), initial_op_count);
  }
  Context2D()->fillRect(0, 0, 1, 1);
  // Verify that auto-flush happened
  ASSERT_EQ(Context2D()->Recorder()->TotalOpCount(), initial_op_count);
}

TEST_P(CanvasRenderingContext2DTest, AutoFlushPinnedImages) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  Context2D()->fillRect(0, 0, 1, 1);  // Ensure resource provider is created.

  constexpr unsigned int kImageSize = 10;
  constexpr unsigned int kBytesPerImage = 400;

  const size_t initial_op_count = Context2D()->Recorder()->TotalOpCount();

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
      ASSERT_EQ(Context2D()->Recorder()->TotalOpCount(), expected_op_count);
    }
    Context2D()->fillRect(0, 0, 1, 1);  // Trigger flush due to memory limit
    ASSERT_EQ(Context2D()->Recorder()->TotalOpCount(), initial_op_count);
  }
}

TEST_P(CanvasRenderingContext2DTest, OverdrawResetsPinnedImageBytes) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  constexpr unsigned int kImageSize = 10;
  constexpr unsigned int kBytesPerImage = 400;

  FakeImageSource unique_image(gfx::Size(kImageSize, kImageSize),
                               kOpaqueBitmap);
  NonThrowableExceptionState exception_state;
  Context2D()->drawImage(&unique_image, 0, 0, 10, 10, 0, 0, 10, 10,
                         exception_state);
  size_t initial_op_count = Context2D()->Recorder()->TotalOpCount();
  ASSERT_EQ(Context2D()->Recorder()->ReleasableImageBytesUsed(),
            kBytesPerImage);

  Context2D()->clearRect(0, 0, 10, 10);  // Overdraw
  ASSERT_EQ(Context2D()->Recorder()->TotalOpCount(), initial_op_count);
  ASSERT_EQ(Context2D()->Recorder()->ReleasableImageBytesUsed(), 0u);
}

TEST_P(CanvasRenderingContext2DTest, AutoFlushSameImage) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  Context2D()->fillRect(0, 0, 1, 1);  // Ensure resource provider is created.
  size_t expected_op_count = Context2D()->Recorder()->TotalOpCount();

  constexpr unsigned int kImageSize = 10;
  constexpr unsigned int kBytesPerImage = 400;

  FakeImageSource image(gfx::Size(kImageSize, kImageSize), kOpaqueBitmap);

  for (size_t pinned_bytes = 0; pinned_bytes <= 2 * kMaxPinnedImageKB * 1024;
       pinned_bytes += kBytesPerImage) {
    NonThrowableExceptionState exception_state;
    Context2D()->drawImage(&image, 0, 0, 1, 1, 0, 0, 1, 1, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    ++expected_op_count;
    ASSERT_EQ(Context2D()->Recorder()->TotalOpCount(), expected_op_count);
  }
}

TEST_P(CanvasRenderingContext2DTest, AutoFlushDelayedByLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);
  NonThrowableExceptionState exception_state;
  Context2D()->beginLayer(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                          BeginLayerOptions::Create(), exception_state);
  const size_t initial_op_count = Context2D()->Recorder()->TotalOpCount();
  while (Context2D()->Recorder()->TotalOpBytesUsed() <=
         kMaxRecordedOpKB * 1024 * 2) {
    Context2D()->fillRect(0, 0, 1, 1);
    ASSERT_GT(Context2D()->Recorder()->TotalOpCount(), initial_op_count);
  }
  // Closing the layer means next op can trigger auto flush
  Context2D()->endLayer(exception_state);
  Context2D()->fillRect(0, 0, 1, 1);
  ASSERT_EQ(Context2D()->Recorder()->TotalOpCount(), initial_op_count);
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

 private:
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTestAccelerated);

TEST_P(CanvasRenderingContext2DTestAccelerated,
       RemainAcceleratedAfterGetImageDataWithWillNotReadFrequently) {
  base::test::ScopedFeatureList feature_list_;
  CreateContext(
      kNonOpaque, kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently::kFalse);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

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
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  // Take a snapshot to trigger lazy resource provider creation
  Context2D()->GetImage(FlushReason::kTesting);
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
  EXPECT_EQ(features::IsCanvas2DHibernationEnabled(),
            painting_layer->SelfNeedsRepaint());
  EXPECT_EQ(features::IsCanvas2DHibernationEnabled(),
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
  EXPECT_EQ(features::IsCanvas2DHibernationEnabled(),
            painting_layer->SelfNeedsRepaint());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PageComingToForegroundEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();
  base::RunLoop run_loop;

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Run the task that initiates hibernation, which has been posted as an idle
  // task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(CanvasElement().IsResourceValid());

  // Verify that coming to the foreground ends hibernation synchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kVisible,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationEndedNormally, 1);
    EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(CanvasElement().IsResourceValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       HibernationPostForegroundBackgroundToggleOccursNormally) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Toggle visibility to foreground before the task that enters hibernation
  // gets a chance to run.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kVisible,
      /*is_initial_state=*/false);

  // Move the page to the background again and verify that hibernation is not
  // newly scheduled, as the hibernation scheduled on the first backgrounding is
  // still pending.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 0);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Run the task that initiates hibernation and verify that hibernation
  // triggers.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(CanvasElement().IsResourceValid());

  // Verify that coming to the foreground ends hibernation synchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kVisible,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationEndedNormally, 1);
    EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(CanvasElement().IsResourceValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, TeardownEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Run the task that initiates hibernation, which has been posted as an idle
  // task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(CanvasElement().IsResourceValid());

  // Verify that tearing down the host ends hibernation synchronously.
  {
    base::HistogramTester histogram_tester;
    TearDownHost();
    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationEndedWithTeardown,
        1);
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       TeardownWhileHibernationIsPendingAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Tear down the host while hibernation is pending.
  TearDownHost();

  // Verify that running the hibernation task aborts hibernation (and doesn't
  // crash by calling into the destroyed state).
  {
    base::HistogramTester histogram_tester;

    // Run the task that initiates hibernation, which has been posted as an idle
    // task.
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    blink::test::RunPendingTasks();

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::
            kHibernationAbortedDueToDestructionWhileHibernatePending,
        1);
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ForegroundingWhileHibernationIsPendingAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Foreground the page while hibernation is pending.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kVisible,
      /*is_initial_state=*/false);

  // Verify that running the hibernation task aborts hibernation due to the
  // page having been foregrounded.
  {
    base::HistogramTester histogram_tester;

    // Run the task that initiates hibernation, which has been posted as an idle
    // task.
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    blink::test::RunPendingTasks();

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::
            kHibernationAbortedDueToVisibilityChange,
        1);
    EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(CanvasElement().IsResourceValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ContextLossAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);

  // Do initial setup to ensure that CanvasResourceHost will check for the GPU
  // context being lost as part of checking resource validity:

  // * Install a CanvasResourceProvider that is accelerated and supports direct
  //   compositing. The former is necessary as part of ensuring that
  //   CanvasResourceHost::IsResourceValid() checks for context loss, while the
  //   latter is necessary for GetOrCreateCcLayerIfNeeded() to succeed.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  // * Put the host in GPU compositing mode, also necessary to ensure that
  //   IsResourceValid() checks for context loss.
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  // * Finally, create a CC layer as otherwise IsResourceValid() will always
  //   unconditionally return true.
  EXPECT_TRUE(CanvasElement().GetOrCreateCcLayerIfNeeded());

  EXPECT_TRUE(CanvasElement().IsResourceValid());

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Simulate GPU context loss.
  test_context_provider_->TestContextGL()->set_context_lost(true);

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Verify that running the hibernation task aborts hibernation due to the
  // GPU context having been lost.
  {
    base::HistogramTester histogram_tester;

    // Run the task that initiates hibernation, which has been posted as an idle
    // task.
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    blink::test::RunPendingTasks();

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::
            kHibernationAbortedDueGpuContextLoss,
        1);
    EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_FALSE(CanvasElement().IsResourceValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PrepareTransferableResourceFailsWhileHibernating) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);

  // Create a CC layer, as a CC layer being present is a necessary precondition
  // of calling PrepareTransferableResource().

  // First install a CanvasResourceProvider that supports direct compositing,
  // as this is necessary for GetOrCreateCcLayerIfNeeded() to succeed.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  EXPECT_TRUE(CanvasElement().GetOrCreateCcLayerIfNeeded());
  EXPECT_TRUE(CanvasElement().IsResourceValid());

  // Put the host in GPU compositing mode to ensure that hibernation succeeds.
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    GetDocument().GetPage()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden,
        /*is_initial_state=*/false);

    histogram_tester.ExpectUniqueSample(
        "Blink.Canvas.HibernationEvents",
        Canvas2DLayerBridge::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Run the task that initiates hibernation, which has been posted as an idle
  // task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();
  EXPECT_TRUE(handler.IsHibernating());

  // Verify that PrepareTransferableResource() fails while hibernating.
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(CanvasElement().IsResourceValid());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       CanvasDrawInBackgroundEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();
  base::RunLoop run_loop;

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  // NOTE: It is necessary to install the quit closure before running tasks
  // below in order to avoid test flake, as it is possible that encoding occurs
  // on a background thread *before* we run the run loop to wait for encoding.
  // As long as the quit closure has been invoked as part of encoding in that
  // case, the run loop will immediately exit out when Run() is invoked
  // (otherwise it would spin until timing out).
  handler.SetOnEncodedCallbackForTesting(run_loop.QuitClosure());

  EXPECT_FALSE(handler.is_encoded());
  EXPECT_FALSE(handler.IsHibernating());

  // Hide the page to trigger the hibernation task.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);

  // Hibernation is triggered asynchronously.
  EXPECT_FALSE(handler.is_encoded());
  EXPECT_FALSE(handler.IsHibernating());

  // Run the task that initiates hibernation, which has been posted as an idle
  // task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();
  EXPECT_FALSE(CanvasElement().ResourceProvider());
  EXPECT_TRUE(handler.IsHibernating());

  // Wait for encoding to complete on a background thread.
  run_loop.Run();
  EXPECT_TRUE(handler.is_encoded());

  // Draw into the canvas while the page is backgrounded.
  Context2D()->fillRect(0, 0, 1, 1);

  // That draw should have caused hibernation to end and the encoded canvas to
  // be dropped.
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_FALSE(handler.is_encoded());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       CanvasSnapshotWhileHibernating) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  ASSERT_TRUE(CanvasElement().GetOrCreateCanvasResourceProvider(
      RasterModeHint::kPreferGPU));
  ASSERT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);

  auto* bridge = CanvasElement().GetCanvas2DLayerBridge();
  auto& handler = bridge->GetHibernationHandler();
  base::RunLoop run_loop;

  // Install a minimal delay for testing to ensure that the test remains fast
  // to execute.
  handler.SetBeforeCompressionDelayForTesting(base::Microseconds(10));

  // NOTE: It is necessary to install the quit closure before running tasks
  // below in order to avoid test flake, as it is possible that encoding occurs
  // on a background thread *before* we run the run loop to wait for encoding.
  // As long as the quit closure has been invoked as part of encoding in that
  // case, the run loop will immediately exit out when Run() is invoked
  // (otherwise it would spin until timing out).
  handler.SetOnEncodedCallbackForTesting(run_loop.QuitClosure());

  ASSERT_FALSE(handler.is_encoded());
  ASSERT_FALSE(handler.IsHibernating());

  // Hide the page to trigger the hibernation task.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);

  // Hibernation is triggered asynchronously.
  ASSERT_FALSE(handler.is_encoded());
  ASSERT_FALSE(handler.IsHibernating());

  // Run the task that initiates hibernation, which has been posted as an idle
  // task.
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();

  ASSERT_FALSE(CanvasElement().ResourceProvider());
  ASSERT_TRUE(handler.IsHibernating());
  ASSERT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);

  // Wait for encoding to complete on a background thread.
  run_loop.Run();
  ASSERT_TRUE(handler.is_encoded());

  // Taking a snapshot of the canvas while hibernating should produce an
  // unaccelerated image.
  EXPECT_FALSE(Context2D()->GetImage(FlushReason::kTesting)->IsTextureBacked());

  // The action of taking the snapshot should not have impacted the state of
  // hibernation.
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_TRUE(handler.is_encoded());
}

sk_sp<SkImage> CreateSkImage(int width, int height, SkColor color) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  surface->getCanvas()->clear(color);
  return surface->makeImageSnapshot();
}

ImageBitmap* CreateImageBitmap(int width, int height, SkColor color) {
  return MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(
          CreateSkImage(width, height, color)));
}

MATCHER_P(DrawImageRectOpIs, sk_image, "") {
  if (!ExplainMatchResult(PaintOpIs<DrawImageRectOp>(), arg, result_listener)) {
    return false;
  }
  const auto& draw_op = static_cast<const DrawImageRectOp&>(arg);
  SkBitmap lhs, rhs;
  draw_op.image.GetSwSkImage()->asLegacyBitmap(&lhs);
  sk_image->asLegacyBitmap(&rhs);
  if (!gfx::BitmapsAreEqual(lhs, rhs)) {
    *result_listener << "DrawImageRectOp has an unexpected image content";
    return false;
  }
  return true;
}

TEST_P(CanvasRenderingContext2DTestAccelerated, HibernationWithUnclosedLayer) {
  ScopedCanvas2dLayersForTest layer_feature{/*enabled=*/true};
  ScopedFeatureList scoped_feature_list(features::kCanvas2DHibernation);
  CreateContext(kNonOpaque);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  gfx::Size size(100, 100);
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // Recorded draw ops are resterized on hibernation. The provider gets replaced
  // when getting out of hibernation, so this mock will not see the later calls
  // to `RasterRecord`.
  cc::PaintRecord hibernation_raster;
  EXPECT_CALL(*provider, Snapshot(FlushReason::kHibernating, _)).Times(1);
  EXPECT_CALL(*provider, RasterRecord)
      .Times(1)
      .WillOnce(SaveArg<0>(&hibernation_raster));

  CanvasElement().SetResourceProviderForTesting(
      std::move(provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  ThreadScheduler::Current()->PostIdleTask(
      FROM_HERE, WTF::BindOnce(
                     [](CanvasRenderingContext2DTestAccelerated* fixture,
                        base::TimeTicks /*idleDeadline*/) {
                       NonThrowableExceptionState exception_state;

                       // Will be rasterized on hibernation.
                       fixture->Context2D()->fillRect(0, 0, 1, 1);

                       fixture->Context2D()->beginLayer(
                           fixture->GetScriptState(),
                           BeginLayerOptions::Create(), exception_state);

                       // Will be preserved as a paint op in hibernation.
                       fixture->Context2D()->fillRect(1, 1, 1, 1);

                       // Referred image should survive hibernation.
                       fixture->Context2D()->drawImage(
                           CreateImageBitmap(/*width=*/1, /*height=*/1,
                                             SK_ColorRED),          //
                           /*sx=*/0, /*sy=*/0, /*sw=*/1, /*sh*/ 1,  //
                           /*dx=*/0, /*dy=*/0, /*dw=*/1, /*dh=*/1,  //
                           exception_state);
                     },
                     WTF::Unretained(this)));
  blink::test::RunPendingTasks();

  // Hibernate the canvas. Hibernation is handled in a idle task.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden, /*is_initial_state=*/false);
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();

  // Hibernating should have rastered paint ops preceding `beginLayer`.
  EXPECT_THAT(hibernation_raster,
              RecordedOpsAre(PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 1, 1),
                                                   FillFlags())));

  // Wake up from hibernation.
  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kVisible, /*is_initial_state=*/false);

  NonThrowableExceptionState exception_state;
  Context2D()->endLayer(exception_state);

  // Post hibernation recording now holds the layer content.
  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerAlphaOp>(1.0f),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 1, 1), FillFlags()),
          DrawImageRectOpIs(
              CreateSkImage(/*width=*/1, /*height=*/1, SK_ColorRED)),
          PaintOpEq<RestoreOp>()))));
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       NoHibernationIfNoResourceProvider) {
  CreateContext(kNonOpaque);
  gfx::Size size(300, 300);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);
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
  EXPECT_TRUE(
      CanvasElement().ResourceProvider()->Recorder().HasRecordedDrawOps());

  Context2D()->drawImage(frame, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  // The drawImage Operation is supposed to trigger a flush, which means that
  // There should not be any Recorded ops at this point.
  EXPECT_FALSE(
      CanvasElement().ResourceProvider()->Recorder().HasRecordedDrawOps());
}

TEST_P(CanvasRenderingContext2DTest, FlushRestoresClipStack) {
  CreateContext(kNonOpaque);

  // Ensure that the ResourceProvider and canvas are created.
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferCPU);

  // Set a transform.
  Context2D()->translate(5, 0);
  EXPECT_EQ(Canvas().getLocalToDevice().rc(0, 3), 5);

  // Draw something so that there is something to flush.
  cc::PaintFlags flags;
  Canvas().drawLine(0, 0, 2, 2, flags);

  // Flush the canvas and verify that a new drawing canvas is created that has
  // the transform restored.
  EXPECT_TRUE(Context2D()->FlushCanvas(FlushReason::kTesting));
  EXPECT_EQ(Canvas().getLocalToDevice().rc(0, 3), 5);
}

TEST_P(CanvasRenderingContext2DTest, PutImageDataRestoresClipStack) {
  CreateContext(kNonOpaque);

  // Ensure that the ResourceProvider and canvas are created.
  CanvasElement().GetOrCreateCanvasResourceProvider(RasterModeHint::kPreferCPU);

  // Set a transform.
  Context2D()->translate(5, 0);
  EXPECT_EQ(Canvas().getLocalToDevice().rc(0, 3), 5);

  // Invoke putImageData(). This forces a flush, after which the transform
  // should be restored.
  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->putImageData(full_image_data_.Get(), 0, 0, exception_state);

  EXPECT_EQ(Canvas().getLocalToDevice().rc(0, 3), 5);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DISABLED_DisableAcceleration_UpdateGPUMemoryUsage) {
  CreateContext(kNonOpaque);

  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      /*provider=*/nullptr,
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);
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
       DisableAccelerationPreservesRasterAndRecording) {
  ScopedCanvas2dLayersForTest layer_feature{/*enabled=*/true};
  CreateContext(kNonOpaque);

  gfx::Size size(100, 100);
  auto gpu_provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  auto cpu_provider = std::make_unique<FakeCanvasResourceProvider>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      RasterModeHint::kPreferCPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // When disabling acceleration, the raster content is read from the
  // accelerated provider and written to the unaccelerated provider.
  InSequence s;
  EXPECT_CALL(*gpu_provider, Snapshot(FlushReason::kReplaceLayerBridge, _))
      .Times(1);
  EXPECT_CALL(*cpu_provider, WritePixels).Times(1);

  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetResourceProviderForTesting(
      std::move(gpu_provider),
      std::make_unique<Canvas2DLayerBridge>(&CanvasElement()), size);

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(10, 10, 20, 20);
  Context2D()->save();
  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          exception_state);
  Context2D()->fillRect(10, 20, 30, 40);

  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kGPU);
  CanvasElement().DisableAcceleration(std::move(cpu_provider));
  EXPECT_EQ(CanvasElement().GetRasterMode(), RasterMode::kCPU);

  Context2D()->endLayer(exception_state);
  Context2D()->restore(exception_state);

  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kTesting),
      Optional(RecordedOpsAre(
          PaintOpEq<SaveOp>(),
          DrawRecordOpEq(PaintOpEq<SaveLayerAlphaOp>(1.0f),
                         PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(10, 20, 30, 40),
                                               FillFlags()),
                         PaintOpEq<RestoreOp>()),
          PaintOpEq<RestoreOp>())));
}

class CanvasRenderingContext2DTestAcceleratedMultipleDisables
    : public CanvasRenderingContext2DTest {
 protected:
  bool AllowsAcceleration() override { return true; }

  void CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled() {
    for (int i = 0; i < 100; ++i) {
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
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
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

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.shared_image_swap_chain = true;
    context_provider->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

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
