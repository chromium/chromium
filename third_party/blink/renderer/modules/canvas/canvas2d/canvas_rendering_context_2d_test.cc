// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/layers/texture_layer.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_op.h"
#include "cc/test/paint_op_matchers.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
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
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float16array_float32array_uint8clampedarray.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2043)
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_will_read_frequently.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
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
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
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
#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/opacity_mode.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
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
using event_type_names::kContextlost;
using event_type_names::kContextrestored;

class ExecutionContext;

namespace {

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
using PageVisibilityState = ::blink::mojom::blink::PageVisibilityState;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Message;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::SaveArg;

enum BitmapOpacity { kOpaqueBitmap, kTransparentBitmap };

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

class FakeImageSource : public CanvasImageSource {
 public:
  FakeImageSource(gfx::Size, BitmapOpacity);

  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               const gfx::SizeF&) override;

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
    SourceImageStatus* status,
    const gfx::SizeF&) {
  if (status)
    *status = kNormalSourceImageStatus;
  return image_;
}

// Event listener that runs a callback when invoked.
class CallbackEventListener : public NativeEventListener {
 public:
  explicit CallbackEventListener(base::RepeatingClosure callback)
      : callback_(callback) {}
  void Invoke(ExecutionContext*, Event*) override { callback_.Run(); }

 private:
  const base::RepeatingClosure callback_;
};

void SetDocumentVisibility(Document& document, PageVisibilityState visibility) {
  document.GetPage()->SetVisibilityState(visibility,
                                         /*is_initial_state=*/false);
}

void RunIdleTasks() {
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  blink::test::RunPendingTasks();
}

void WaitForHibernation() {
  // Hibernation is posted as an idle task.
  RunIdleTasks();
}

}  // namespace

// Helper class to registers an event listener and wait for it to fire.
class EventWatcher {
 public:
  EventWatcher(Element& element, const AtomicString& event_type) {
    element.addEventListener(
        event_type,
        MakeGarbageCollected<CallbackEventListener>(run_loop_.QuitClosure()));
  }
  void Wait() { run_loop_.Run(); }
  bool WasInvoked() { return run_loop_.AnyQuitCalled(); }

 private:
  base::RunLoop run_loop_;
};

//============================================================================

class CanvasRenderingContext2DTestBase : public ::testing::Test,
                                         public PaintTestConfigurations {
 public:
  CanvasRenderingContext2DTestBase();
  void SetUp() override;
  virtual bool AllowsAcceleration() { return false; }

  virtual void CreateContextProvider(SetIsContextLost set_context_lost) = 0;
  virtual void ConfigureContextProvider(
      viz::TestContextProvider& context_provider) {}

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  cc::PaintCanvas& Canvas() {
    return Context2D()->GetResourceProviderForTesting()->Canvas();
  }
  CanvasRenderingContext2D* Context2D() const {
    return static_cast<CanvasRenderingContext2D*>(
        CanvasElement().RenderingContext());
  }

  CanvasRenderingContext2DState& GetContext2DState() {
    return Context2D()->GetState();
  }

  void DrawSomething() {
    CanvasElement().DidDraw();
    Context2D()->PreFinalizeFrame();
    Context2D()->FinalizeFrame(FlushReason::kOther);
    CanvasElement().PostFinalizeFrame(FlushReason::kOther);
    // Grabbing an image forces a flush
    CanvasElement().Snapshot(kBackBuffer);
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

  void TearDownPage() {
    // Synchronously tears down the page, which causes ContextDestroyed() to be
    // invoked on the canvas element (which in turn causes Stop() to be invoked
    // on the rendering context).
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

  void LoseContext() {
    test_context_provider_->GetTestRasterInterface()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    test::RunPendingTasks();
  }

  // Run a callback in a task and wait for that task to finish. This is needed
  // to run functions drawing in the canvas because these indirectly call
  // `CanvasRenderingContext::DidDraw` which can't be called outside of a task.
  void RunInTask(base::OnceCallback<void()> callback) {
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, std::move(callback));
    test::RunPendingTasks();
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  Persistent<HTMLCanvasElement> canvas_element_;
  ScopedMemoryCacheForTesting scoped_memory_cache_;

 private:
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

class CanvasRenderingContext2DTest : public CanvasRenderingContext2DTestBase {
 public:
  void CreateContextProvider(SetIsContextLost set_context_lost) override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    InitializeSharedGpuContextRaster(test_context_provider_.get(),
                                     /*cache=*/nullptr, set_context_lost);
    ConfigureContextProvider(*test_context_provider_);
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTest);

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
      auto* context =
          static_cast<CanvasRenderingContext2D*>(canvas->RenderingContext());
      context->GetOrCreateResourceProvider();
      // Expect that at least the first 10 are accelerated. The exact number
      // depends on the feature params.
      if (i < 10) {
        EXPECT_TRUE(canvas->IsAccelerated());
      }
      context->DisableAcceleration();
    }
  }

 private:
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(CanvasRenderingContext2DTestAccelerated);

CanvasRenderingContext2DTestBase::CanvasRenderingContext2DTestBase()
    : scoped_memory_cache_(MakeGarbageCollected<MemoryCache>(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting())),
      wrap_gradients_(MakeGarbageCollected<WrapGradients>()),
      opaque_bitmap_(gfx::Size(10, 10), kOpaqueBitmap),
      alpha_bitmap_(gfx::Size(10, 10), kTransparentBitmap) {}

void CanvasRenderingContext2DTestBase::CreateContext(
    OpacityMode opacity_mode,
    LatencyMode latency_mode,
    CanvasContextCreationAttributesCore::WillReadFrequently
        will_read_frequently,
    HTMLCanvasElement* canvas) {
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = opacity_mode == kNonOpaque;
  attributes.desynchronized_specified = kLowLatency;
  attributes.desynchronized = latency_mode == kLowLatency;
  attributes.will_read_frequently = will_read_frequently;
  if (!canvas) {
    canvas = canvas_element_;
  }
  canvas->GetCanvasRenderingContext(GetExecutionContext(), canvas_type,
                                    attributes);
}

void CanvasRenderingContext2DTestBase::SetUp() {
  base::FieldTrialParams auto_flush_params;
  auto_flush_params["max_pinned_image_kb"] =
      base::NumberToString(kMaxPinnedImageKB);
  auto_flush_params["max_recorded_op_kb"] =
      base::NumberToString(kMaxRecordedOpKB);
  feature_list_.InitAndEnableFeatureWithParameters(kCanvas2DAutoFlushParams,
                                                   auto_flush_params);

  // Create a `TestContextProvider` that automatically restores itself after a
  // GPU context loss.
  CreateContextProvider(SetIsContextLost::kSetToFalse);
  allow_accelerated_ =
      std::make_unique<ScopedAccelerated2dCanvasForTest>(AllowsAcceleration());
  web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
  web_view_helper_->Initialize();

  // Default canvas size is 300x150, per the spec:
  // https://www.w3.org/TR/2012/WD-html5-author-20120329/the-canvas-element.html#the-canvas-element
  // This is above the threshold for canvas hibernation, even when small
  // canvases are excluded.
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      String::FromUTF8(
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
}

void CanvasRenderingContext2DTestBase::TearDown() {
  feature_list_.Reset();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  // Tear down WebViewHelper because we override Platform in some tests which
  // must be torn down after WebViewHelper.
  web_view_helper_ = nullptr;

  // Must be torn down after WebViewHelper since its destructor can create a
  // fresh context provider otherwise.
  SharedGpuContext::Reset();

  // Prevent CanvasPerformanceMonitor state from leaking between tests.
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  task_environment_.FastForwardUntilNoTasksRemain();
}

//============================================================================

enum class CompositingMode {
  kDoesNotSupportDirectCompositing,
  kSupportsDirectCompositing
};

class FakeCanvasResourceProvider : public CanvasResourceProviderSharedImage {
 public:
  FakeCanvasResourceProvider(gfx::Size size,
                             RasterModeHint hint,
                             CanvasResourceProvider::Delegate* delegate,
                             CompositingMode compositing_mode)
      : CanvasResourceProviderSharedImage(
            size,
            GetN32FormatForCanvas(),
            kPremul_SkAlphaType,
            gfx::ColorSpace::CreateSRGB(),
            SharedGpuContext::ContextProviderWrapper(),
            /*is_accelerated=*/hint != RasterModeHint::kPreferCPU,
            gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                gpu::SHARED_IMAGE_USAGE_RASTER_WRITE,
            delegate),
        supports_direct_compositing_(
            compositing_mode == CompositingMode::kSupportsDirectCompositing) {
    ON_CALL(*this, Snapshot)
        .WillByDefault([this](ImageOrientation orientation) {
          return UnacceleratedSnapshot(orientation);
        });
  }
  ~FakeCanvasResourceProvider() override = default;
  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason) override {
    return scoped_refptr<CanvasResource>(CanvasResourceSharedImage::Create(
        Size(), GetSharedImageFormat(), GetAlphaType(), GetColorSpace(),
        SharedGpuContext::ContextProviderWrapper(),
        weak_ptr_factory_.GetWeakPtr(), IsAccelerated(),
        GetSharedImageUsageFlags()));
  }
  bool SupportsDirectCompositing() const override {
    return supports_direct_compositing_;
  }
  sk_sp<SkSurface> CreateSkSurface() const override {
    return SkSurfaces::Raster(GetSkImageInfo());
  }

  MOCK_METHOD((void), RasterRecord, (cc::PaintRecord last_recording));

  MOCK_METHOD((scoped_refptr<StaticBitmapImage>),
              Snapshot,
              (ImageOrientation orientation));

  MOCK_METHOD(bool,
              WritePixels,
              (const SkImageInfo& orig_info,
               const void* pixels,
               size_t row_bytes,
               int x,
               int y));

 private:
  bool supports_direct_compositing_;
  base::WeakPtrFactory<FakeCanvasResourceProvider> weak_ptr_factory_{this};
};

// Sets up an accelerated CanvasResourceProvider, accelerated compositing, and a
// CcLayer on the passed-in HTMLCanvasElement and context. Returns false if the
// CcLayer couldn't be created.
bool SetUpFullAccelerationAndCcLayer(HTMLCanvasElement& canvas_element,
                                     CanvasRenderingContext2D* context) {
  // Install a CanvasResourceProvider that is accelerated and supports direct
  // compositing (the latter is necessary for
  // GetOrCreateCcLayerForCanvas2DIfNeeded() to succeed).
  CHECK(context->GetOrCreateResourceProvider());

  // Put the host in GPU compositing mode.
  canvas_element.SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);

  // Create the CcLayer.
  return canvas_element.GetOrCreateCcLayerForCanvas2DIfNeeded() != nullptr;
}

//============================================================================

MATCHER_P(OverdrawOpAreMatcher, expected_overdraw_ops, "") {
  constexpr int last_bucket =
      static_cast<int>(BaseRenderingContext2D::OverdrawOp::kMaxValue);
  for (int bucket = 0; bucket <= last_bucket; ++bucket) {
    SCOPED_TRACE(Message() << "Checking overdraw bucket: " << bucket);
    arg.ExpectBucketCount(
        "Blink.Canvas.OverdrawOp", bucket,
        static_cast<base::HistogramBase::Count32>(expected_overdraw_ops.count(
            static_cast<BaseRenderingContext2D::OverdrawOp>(bucket))));
  }
  return true;
}

template <typename... Args>
testing::Matcher<base::HistogramTester> OverdrawOpAre(Args... args) {
  return OverdrawOpAreMatcher(
      std::unordered_set<BaseRenderingContext2D::OverdrawOp>{args...});
}

// Matches an object (e.g. ContentResourceProvider) that has `IsValid()` ==
// true.
MATCHER(IsValid, "") {
  return arg.IsValid();
}

TEST_P(CanvasRenderingContext2DTest, NoRecreationOfResourceProviderAfterDraw) {
  CreateContext(kNonOpaque);
  uint32_t gen_id =
      Context2D()->GetOrCreateResourceProvider()->ContentUniqueID();
  Context2D()->fillRect(3, 3, 1, 1);
  EXPECT_EQ(gen_id,
            Context2D()->GetOrCreateResourceProvider()->ContentUniqueID());
}

TEST_P(CanvasRenderingContext2DTest,
       NonDisplayedNonPaintableCanvasIsNotRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  CanvasElement().SetIsDisplayed(false);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should not result in rate limiting as the
  // canvas is not displayed.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTest,
       NonDisplayedPaintableCanvasIsNotRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  CanvasElement().SetIsDisplayed(false);
  Context2D()->GetOrCreateResourceProvider();
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

  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should not result in rate limiting as the
  // canvas is not paintable.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DisplayedPaintableNonCompositedCanvasIsNotRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Install a CanvasResourceProvider that does not support direct compositing.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      size, RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kDoesNotSupportDirectCompositing);
  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);

  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should not result in rate limiting as the
  // canvas is not composited.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DisplayedPaintableCompositedCanvasIsRateLimited) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Install a CanvasResourceProvider that supports direct compositing to make
  // the canvas composited.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      size, RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);

  CanvasElement().SetIsDisplayed(true);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Invoking FinalizeFrame() twice should result in rate limiting.
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  Context2D()->FinalizeFrame(FlushReason::kCanvasPushFrame);
  EXPECT_TRUE(!!CanvasElement().RateLimiter());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       HidingCanvasTurnsOffRateLimiting) {
  CreateContext(kNonOpaque);
  EXPECT_FALSE(!!CanvasElement().RateLimiter());

  // Install a CanvasResourceProvider that supports direct compositing to make
  // the canvas composited.
  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      size, RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);

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
      size, RasterModeHint::kPreferCPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);
  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);

  EXPECT_FALSE(
      Context2D()->GetImage()->PaintImageForCurrentFrame().IsTextureBacked());

  // The GetImage() call should have preserved the rasterization mode as well as
  // the validity of the resource.
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  EXPECT_TRUE(Context2D()->IsResourceProviderValid());
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
      Context2D()->FlushCanvas(FlushReason::kOther),
      Optional(RecordedOpsAre(
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 1, 1), FillFlags()),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(-1, -1, 12, 12),
                                FillFlags()))));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTest, ClearRect_ExactCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->clearRect(0, 0, 10, 10);

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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
      Context2D()->FlushCanvas(FlushReason::kOther),
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
      Context2D()->FlushCanvas(FlushReason::kOther),
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
      Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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
      Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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
      Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
              Optional(RecordedOpsAre(PaintOpIs<ClipRectOp>(),  //
                                      PaintOpIs<DrawRectOp>(),  //
                                      PaintOpIs<DrawImageRectOp>())));
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTestAccelerated, PutImageData_FullCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      size, RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // The recording will be cleared, so nothing will be rastered before
  // `WritePixels` is called.
  InSequence s;
  EXPECT_CALL(*provider, RasterRecord).Times(0);
  EXPECT_CALL(*provider, WritePixels).Times(1);

  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->putImageData(full_image_data_.Get(), 0, 0, exception_state);

  // `putImageData` isn't included in the recording, keeping it empty.
  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther), Eq(std::nullopt));

  // `putImageData` overdraw isn't handled by
  // `BaseRenderingContext2D::CheckOverdraw` like other draw operations, so the
  // histograms aren't updated.
  EXPECT_THAT(histogram_tester, OverdrawOpAre());
}

TEST_P(CanvasRenderingContext2DTestAccelerated, PutImageData_PartialCoverage) {
  base::HistogramTester histogram_tester;
  CreateContext(kNonOpaque);
  CanvasElement().SetSize(gfx::Size(10, 10));

  gfx::Size size = CanvasElement().Size();
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      size, RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // `putImageData` forces a flush, so the `fillRect` will get rasterized before
  // `WritePixels` is called.
  InSequence s;
  EXPECT_CALL(*provider, RasterRecord(RecordedOpsAre(PaintOpIs<DrawRectOp>())))
      .Times(1);
  EXPECT_CALL(*provider, WritePixels).Times(1);

  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);

  // `putImageData` forces a flush, which clears the recording.
  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(3, 3, 1, 1);
  Context2D()->putImageData(partial_image_data_.Get(), 0, 0, exception_state);

  // `putImageData` isn't included in the recording, keeping it empty.
  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther), Eq(std::nullopt));

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

  EXPECT_THAT(Context2D()->FlushCanvas(FlushReason::kOther),
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
  CanvasRenderingContext2D* context =
      static_cast<CanvasRenderingContext2D*>(canvas->GetCanvasRenderingContext(
          GetExecutionContext(), "2d", attributes));
  DummyExceptionStateForTesting exception_state;
  auto* image_source =
      MakeGarbageCollected<V8CanvasImageSource>(image_bitmap_derived);
  context->drawImage(image_source, 0, 0, exception_state);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       GPUMemoryUpdateForAcceleratedCanvas) {
  CreateContext(kNonOpaque);

  gfx::Size size(10, 10);
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider =
      std::make_unique<FakeCanvasResourceProvider>(
          size, RasterModeHint::kPreferGPU, &CanvasElement(),
          CompositingMode::kSupportsDirectCompositing);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  Context2D()->SetCanvas2DResourceProviderForTesting(
      std::move(fake_resource_provider), size);

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
  anotherCanvas->GetCanvasRenderingContext(GetExecutionContext(), "2d",
                                           attributes);
  gfx::Size size2(10, 5);
  std::unique_ptr<FakeCanvasResourceProvider> fake_resource_provider2 =
      std::make_unique<FakeCanvasResourceProvider>(
          size2, RasterModeHint::kPreferGPU, &CanvasElement(),
          CompositingMode::kSupportsDirectCompositing);
  anotherCanvas->SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  auto* second_canvas_context =
      static_cast<CanvasRenderingContext2D*>(anotherCanvas->RenderingContext());
  second_canvas_context->SetCanvas2DResourceProviderForTesting(
      std::move(fake_resource_provider2), size2);

  // Tear down the first image buffer that resides in current canvas element
  CanvasElement().SetSize(gfx::Size(20, 20));

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
       UnacceleratedLowLatencyIsNotSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_FALSE(Context2D()->GetOrCreateResourceProvider()->IsSingleBuffered());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       UnacceleratedIfNormalLatencyWillReadFrequently) {
  CreateContext(kNonOpaque, kNormalLatency,
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       UnacceleratedIfLowLatencyWillReadFrequently) {
  CreateContext(kNonOpaque, kLowLatency,
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       UnacceleratedAfterGetImageDataWithDefaultWillReadFrequently) {
  base::test::ScopedFeatureList feature_list_;
  CreateContext(kNonOpaque, kNormalLatency);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);

  DrawSomething();
  NonThrowableExceptionState exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  int read_count = BaseRenderingContext2D::kFallbackToCPUAfterReadbacks;
  while (read_count--) {
    Context2D()->getImageData(0, 0, 1, 1, settings, exception_state);
  }
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest, AutoFlush) {
  CreateContext(kNonOpaque);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);
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
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);

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
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);

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
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);

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
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);
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

TEST_P(CanvasRenderingContext2DTest,
       SoftwareCanvasIsCompositedIfImageChromium) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);

  // Ensure that native support for BGRA GMBs is present, as otherwise
  // compositing will not occur irrespective of whether
  // `ScopedCanvas2dImageChromium` is enabled.
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     .GetCapabilities())
      .gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);

  CreateContext(kNonOpaque);
  EXPECT_TRUE(Context2D()->GetOrCreateResourceProvider());

  // Draw to the canvas and verify that the canvas is composited.
  Context2D()->fillRect(0, 0, 1, 1);
  EXPECT_TRUE(Context2D()->IsComposited());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest,
       SoftwareCanvasIsNotCompositedIfNotImageChromium) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(false);

  CreateContext(kNonOpaque);
  EXPECT_TRUE(Context2D()->GetOrCreateResourceProvider());

  // Ensure that native support for BGRA GMBs is present, as otherwise
  // compositing will not occur irrespective of whether
  // `ScopedCanvas2dImageChromium` is enabled.
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     .GetCapabilities())
      .gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);

  // Draw to the canvas and verify that the canvas is not composited.
  Context2D()->fillRect(0, 0, 1, 1);
  EXPECT_FALSE(Context2D()->IsComposited());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
}

TEST_P(CanvasRenderingContext2DTest, TextRenderingTest) {
  CreateContext(kNonOpaque, kLowLatency);
  Context2D()->setFont("10px sans-serif");
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kAutoTextRendering);
  // Update the textRendering to "geometricPrecision"
  std::optional<V8CanvasTextRendering> textRendering =
      V8CanvasTextRendering::Create("geometricPrecision");
  Context2D()->setTextRendering(textRendering.value());
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kGeometricPrecision);
  Context2D()->setFont("12px sans-serif");
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kGeometricPrecision);

  // Update the textRendering to "optimizeLegibility"
  textRendering = V8CanvasTextRendering::Create("optimizeLegibility");
  Context2D()->setTextRendering(textRendering.value());
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kOptimizeLegibility);
  Context2D()->setFont("12px sans-serif");
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kOptimizeLegibility);

  // Update the textRendering to "optimizeSpeed"
  textRendering = V8CanvasTextRendering::Create("optimizeSpeed");
  Context2D()->setTextRendering(textRendering.value());
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kOptimizeSpeed);
  Context2D()->setFont("12px sans-serif");
  EXPECT_EQ(GetContext2DState().GetFontDescription().TextRendering(),
            TextRenderingMode::kOptimizeSpeed);
}

TEST_P(CanvasRenderingContext2DTestAccelerated, GetImage) {
  CreateContext(kNonOpaque);

  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  // Verify that CanvasRenderingContext2D::GetImage() creates an accelerated
  // image given that the underlying CanvasResourceProvider does so.
  EXPECT_TRUE(
      Context2D()->GetImage()->PaintImageForCurrentFrame().IsTextureBacked());

  // The GetImage() call should have preserved the rasterization mode as well as
  // the validity of the resource.
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_TRUE(Context2D()->IsResourceProviderValid());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ReleaseLostTransferableResource) {
  CreateContext(kNonOpaque);

  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // is present.
  ASSERT_TRUE(CanvasElement().GetOrCreateCcLayerForCanvas2DIfNeeded());

  Context2D()->fillRect(3, 3, 1, 1);

  // Prepare a TransferableResource, then report the resource as lost.
  // This test passes by not crashing and not triggering assertions.
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resource,
                                                          &release_callback));
  bool lost_resource = true;
  std::move(release_callback).Run(gpu::SyncToken(), lost_resource);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       NoRegenerationOfTransferableResourceWhenAlreadyInCcLayer) {
  CreateContext(kNonOpaque);

  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // is present.
  ASSERT_TRUE(CanvasElement().GetOrCreateCcLayerForCanvas2DIfNeeded());

  Context2D()->fillRect(3, 3, 1, 1);

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resource,
                                                          &release_callback));

  // Put the resource in the Cc layer and then make a second call to prepare a
  // TransferableResource without modifying the canvas in between. This new call
  // should not generate a new TransferableResource as the canvas' resource is
  // already present in the CC layer.
  CanvasElement().GetCanvas2DCcLayerForTesting()->SetTransferableResource(
      resource, std::move(release_callback));
  viz::ReleaseCallback release_callback2;
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(&resource,
                                                           &release_callback2));
  EXPECT_FALSE(release_callback2);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ContextLostAndRestoredEventsAreEmittedAfterGPUContextLost) {
  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), Pointee(IsValid()));

  // Lose the GPU context.
  EventWatcher lost_event(CanvasElement(), kContextlost);
  LoseContext();
  EXPECT_TRUE(Context2D()->IsContextLost());
  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), IsNull());
  lost_event.Wait();

  // Wait for context to be restored.
  {
    EventWatcher restored_event(CanvasElement(), kContextrestored);
    task_environment_.FastForwardBy(
        BaseRenderingContext2D::kTryRestoreContextInterval);
    EXPECT_TRUE(restored_event.WasInvoked());
    EXPECT_FALSE(Context2D()->IsContextLost());
    EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
    EXPECT_THAT(Context2D()->GetResourceProviderForTesting(),
                Pointee(IsValid()));
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ContextRestorationAbortsAfterRetry) {
  // Configure context provider to stay lost after context losses.
  CreateContextProvider(SetIsContextLost::kNotModifyValue);

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), Pointee(IsValid()));

  EventWatcher lost_event(CanvasElement(), event_type_names::kContextlost);
  LoseContext();
  EXPECT_TRUE(Context2D()->IsContextLost());
  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), IsNull());
  lost_event.Wait();

  // Context restoration will fail, wait for the context to give up.
  {
    base::RunLoop run_loop;
    Context2D()->SetRestoreFailedCallbackForTesting(run_loop.QuitClosure());
    task_environment_.FastForwardBy(
        (BaseRenderingContext2D::kMaxTryRestoreContextAttempts + 1) *
        BaseRenderingContext2D::kTryRestoreContextInterval);
    run_loop.Run();
    EXPECT_TRUE(Context2D()->IsContextLost());
    EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), IsNull());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       GetResourceProviderAfterContextLoss) {
  CreateContext(kNonOpaque);

  EXPECT_TRUE(Context2D()->GetOrCreateResourceProvider());

  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);
  EXPECT_EQ(nullptr, Context2D()->GetOrCreateResourceProvider());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PrepareTransferableResourceAfterContextLoss) {
  CreateContext(kNonOpaque);

  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // is present.
  ASSERT_TRUE(CanvasElement().GetOrCreateCcLayerForCanvas2DIfNeeded());

  EXPECT_TRUE(CanvasElement().GetRasterModeForCanvas2D() == RasterMode::kGPU);

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_TRUE(CanvasElement().PrepareTransferableResource(&resource,
                                                          &release_callback));

  // When the context is lost we are not sure if we should still be producing
  // GL frames for the compositor or not, so fail to generate frames.
  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(&resource,
                                                           &release_callback));
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ReleaseLostTransferableResourceWithLostContext) {
  CreateContext(kNonOpaque);

  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // is present.
  ASSERT_TRUE(CanvasElement().GetOrCreateCcLayerForCanvas2DIfNeeded());

  EXPECT_TRUE(CanvasElement().GetRasterModeForCanvas2D() == RasterMode::kGPU);

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_TRUE(CanvasElement().PrepareTransferableResource(&resource,
                                                          &release_callback));

  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);

  // Get a new context provider so that the WeakPtr to the old one is null.
  // This verifies that ReleaseFrameResources() handles null
  // context_provider_wrapper properly.
  SharedGpuContext::ContextProviderWrapper();
  std::move(release_callback).Run(gpu::SyncToken(), /*lost_resource=*/true);
  SharedGpuContext::Reset();
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       FallbackToSoftwareIfContextLost) {
  // Configure context provider to stay lost after context losses.
  CreateContextProvider(SetIsContextLost::kNotModifyValue);

  CreateContext(kNonOpaque);

  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);

  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());

  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  EXPECT_TRUE(Context2D()->IsResourceProviderValid());
}

TEST_P(CanvasRenderingContext2DTestAccelerated, GetImageAfterContextLoss) {
  CreateContext(kNonOpaque);

  // For CanvasRenderingContextHost to check for the GPU context being
  // lost as part of checking resource validity, it is necessary to have both
  // accelerated raster/compositing and a CC layer.
  ASSERT_TRUE(SetUpFullAccelerationAndCcLayer(CanvasElement(), Context2D()));

  EXPECT_TRUE(Context2D()->IsResourceProviderValid());
  EXPECT_TRUE(Context2D()->GetImage());

  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);

  EXPECT_FALSE(Context2D()->GetImage());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PrepareMailboxWhenContextIsLostWithFailedRestore) {
  CreateContext(kNonOpaque);

  // For CanvasRenderingContextHost to check for the GPU context being
  // lost as part of checking resource validity, it is necessary to have both
  // accelerated raster/compositing and a CC layer.
  ASSERT_TRUE(SetUpFullAccelerationAndCcLayer(CanvasElement(), Context2D()));

  // The resource should start off valid.
  EXPECT_TRUE(Context2D()->IsResourceProviderValid());

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_TRUE(CanvasElement().PrepareTransferableResource(&resource,
                                                          &release_callback));

  // Losing the context should result in the resource becoming invalid and the
  // host being unable to produce a TransferableResource from it.
  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(&resource,
                                                           &release_callback));

  // Restoration of the context should fail because
  // Platform::createSharedOffscreenGraphicsContext3DProvider() is stubbed in
  // unit tests. This simulates what would happen when attempting to restore
  // while the GPU process is down.
  Context2D()->TryRestoreContextEvent(/*timer=*/nullptr);
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(&resource,
                                                           &release_callback));
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       RemainAcceleratedAfterGetImageDataWithWillNotReadFrequently) {
  base::test::ScopedFeatureList feature_list_;
  CreateContext(
      kNonOpaque, kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently::kFalse);
  gfx::Size size(10, 10);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);

  DrawSomething();
  NonThrowableExceptionState exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  Context2D()->getImageData(0, 0, 1, 1, settings, exception_state);
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       GetImageDataDoesntEndHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  ASSERT_FALSE(handler.IsHibernating());

  GetDocument().GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);

  WaitForHibernation();
  ASSERT_TRUE(handler.IsHibernating());

  NonThrowableExceptionState exception_state;
  Context2D()->getImageData(0, 0, 1, 1, exception_state);

  EXPECT_TRUE(handler.IsHibernating());
}

// https://crbug.com/708445: When the canvas hibernates or wakes up from
// hibernation, the compositing reasons for the canvas element may change. In
// these cases, the element should request a compositing update.
TEST_P(CanvasRenderingContext2DTestAccelerated,
       ElementRequestsCompositingUpdateOnHibernateAndWakeUp) {
  CreateContext(kNonOpaque);
  gfx::Size size(300, 300);
  CanvasElement().SetPreferred2DRasterMode(RasterModeHint::kPreferGPU);
  CanvasElement().SetSize(size);

  // Trigger resource provider creation.
  Context2D()->fillRect(3, 3, 1, 1);
  EXPECT_TRUE(!!Context2D()->GetResourceProviderForTesting());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto* box = CanvasElement().GetLayoutBoxModelObject();
  EXPECT_TRUE(box);
  PaintLayer* painting_layer = box->PaintingLayer();
  EXPECT_TRUE(painting_layer);
  UpdateAllLifecyclePhasesForTest();

  // Hide element to trigger hibernation (if enabled).
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

  // The above fillRect() call caused the CanvasPerformanceMonitor to start
  // observing tasks. Reset this task observation before running pending tasks
  // to avoid a CHECK that goes off in its WillProcessTask() method due to its
  // not being expected to be called at this point in the flow.
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();

  WaitForHibernation();
  // If enabled, hibernation should cause repaint of the painting layer.
  EXPECT_FALSE(box->NeedsPaintPropertyUpdate());
  EXPECT_EQ(features::IsCanvas2DHibernationEnabled(),
            painting_layer->SelfNeedsRepaint());
  EXPECT_EQ(features::IsCanvas2DHibernationEnabled(),
            !Context2D()->GetResourceProviderForTesting());

  // The page is hidden so it doesn't make sense to paint, and doing so will
  // DCHECK. Update all other lifecycle phases.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  // Wake up again, which should request repaint of the painting layer.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
  EXPECT_FALSE(box->NeedsPaintPropertyUpdate());
  EXPECT_EQ(features::IsCanvas2DHibernationEnabled(),
            painting_layer->SelfNeedsRepaint());
}

TEST_P(CanvasRenderingContext2DTestAccelerated, NoHibernationForSmallCanvas) {
  base::test::ScopedFeatureList enable_hibernation{
      features::kCanvas2DHibernation};
  CreateContext(kNonOpaque);
  canvas_element_->SetSize(gfx::Size(64, 64));
  Context2D()->GetOrCreateResourceProvider();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kCanvas2DHibernationNoSmallCanvas);
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
    WaitForHibernation();
    EXPECT_TRUE(handler.IsHibernating());
  }

  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
  EXPECT_FALSE(handler.IsHibernating());

  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kCanvas2DHibernationNoSmallCanvas};
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
    WaitForHibernation();
    EXPECT_FALSE(handler.IsHibernating());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, AlwaysHibernateLargeCanvas) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kCanvas2DHibernation,
       features::kCanvas2DHibernationNoSmallCanvas},
      {});
  CreateContext(kNonOpaque);
  canvas_element_->SetSize(gfx::Size(200, 200));
  Context2D()->GetOrCreateResourceProvider();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PageComingToForegroundEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  WaitForHibernation();

  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());

  // Verify that coming to the foreground ends hibernation synchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedNormally,
        1);
    EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(Context2D()->IsResourceProviderValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ResourcesAreDiscardedAggressivelyOnlyDuringHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});
  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  CanvasHibernationHandler* handler = Context2D()->GetHibernationHandler();
  viz::TestContextSupport* context_support = test_context_provider_->support();

  EXPECT_FALSE(handler->IsHibernating());
  EXPECT_FALSE(context_support->GetAggressivelyFreeResources());

  // Hide the page, queuing hibernation in an idle task.
  // Resource should start being aggressively freed immediately.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  EXPECT_TRUE(context_support->GetAggressivelyFreeResources());
  EXPECT_FALSE(handler->IsHibernating());

  WaitForHibernation();
  EXPECT_TRUE(context_support->GetAggressivelyFreeResources());
  EXPECT_TRUE(handler->IsHibernating());

  // Show the page, resources should no longer be freed aggressively.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
  EXPECT_FALSE(handler->IsHibernating());
  EXPECT_FALSE(context_support->GetAggressivelyFreeResources());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       AggressiveResourceDiscardingCanBeStoppedEvenWhenContextIsLost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});
  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  CanvasHibernationHandler* handler = Context2D()->GetHibernationHandler();
  viz::TestContextSupport* context_support = test_context_provider_->support();

  EXPECT_FALSE(handler->IsHibernating());
  EXPECT_FALSE(context_support->GetAggressivelyFreeResources());

  // Hide the page, queuing hibernation in an idle task.
  // Resource should start being aggressively freed immediately.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  EXPECT_TRUE(context_support->GetAggressivelyFreeResources());
  EXPECT_FALSE(handler->IsHibernating());

  // Simulate GPU context loss while the canvas is hibernated.
  EventWatcher restored_event(CanvasElement(), kContextrestored);
  EXPECT_FALSE(Context2D()->IsContextLost());
  LoseContext();
  EXPECT_TRUE(Context2D()->IsContextLost());
  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), IsNull());

  WaitForHibernation();
  EXPECT_TRUE(context_support->GetAggressivelyFreeResources());
  EXPECT_FALSE(handler->IsHibernating());

  // Show the page, resources should no longer be freed aggressively.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
  EXPECT_FALSE(handler->IsHibernating());
  EXPECT_FALSE(context_support->GetAggressivelyFreeResources());

  // Wait for context to be restored.
  task_environment_.FastForwardBy(
      BaseRenderingContext2D::kTryRestoreContextInterval);
  restored_event.Wait();
  EXPECT_FALSE(Context2D()->IsContextLost());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), Pointee(IsValid()));
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       HibernationPostForegroundBackgroundToggleOccursNormally) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Toggle visibility to foreground before the task that enters hibernation
  // gets a chance to run.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);

  // Move the page to the background again and verify that hibernation is not
  // newly scheduled, as the hibernation scheduled on the first backgrounding is
  // still pending.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 0);
    EXPECT_FALSE(handler.IsHibernating());
  }

  WaitForHibernation();

  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());

  // Verify that coming to the foreground ends hibernation synchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedNormally,
        1);
    EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(Context2D()->IsResourceProviderValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, TeardownEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  WaitForHibernation();

  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());

  // Verify that tearing down the page ends hibernation synchronously.
  {
    base::HistogramTester histogram_tester;
    TearDownPage();
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationEndedWithTeardown,
        1);
  }
}

// Tests that tearing down the page causes a pending hibernation to be aborted
// because the page teardown causes the resource provider to be discarded.
TEST_P(CanvasRenderingContext2DTestAccelerated,
       TeardownWhileHibernationIsPendingAbortsHibernationDueToSurfaceLoss) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Tear down the page while hibernation is pending.
  TearDownPage();

  // Verify that running the hibernation task aborts hibernation (and doesn't
  // crash by calling into the destroyed state).
  {
    base::HistogramTester histogram_tester;

    WaitForHibernation();

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationAbortedBecauseNoSurface,
        1);
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DisablingAccelerationWhileHibernationIsPendingAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  ASSERT_FALSE(Context2D()->IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    ASSERT_FALSE(Context2D()->IsHibernating());
  }

  Context2D()->DisableAcceleration();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);

  // Verify that running the hibernation task aborts hibernation due to the
  // switch to software rendering.
  {
    base::HistogramTester histogram_tester;

    WaitForHibernation();

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationAbortedDueToSwitchToUnacceleratedRendering,
        1);
    EXPECT_FALSE(Context2D()->IsHibernating());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DisablingAccelerationWhileHibernatingLogsAnHistogram) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Disable acceleration on this canvas and a large number of other canvases,
  // to permanently disable acceleration on this document.
  Context2D()->DisableAcceleration();
  CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled();

  // Waking up after acceleration was disabled logs an UMA histogram.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationEndedWithFallbackToSW,
        1);
    EXPECT_TRUE(Context2D()->IsResourceProviderValid());
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  }
}

TEST_P(
    CanvasRenderingContext2DTestAccelerated,
    DisablingThenReenablingAccelerationWhileHibernationIsPendingDoesntAbortHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  ASSERT_FALSE(Context2D()->IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    ASSERT_FALSE(Context2D()->IsHibernating());
  }

  Context2D()->DisableAcceleration();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  Context2D()->EnableAccelerationIfPossible();
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  WaitForHibernation();
  EXPECT_TRUE(Context2D()->IsHibernating());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ForegroundingWhileHibernationIsPendingAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Foreground the page while hibernation is pending.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);

  // Verify that running the hibernation task aborts hibernation due to the
  // page having been foregrounded.
  {
    base::HistogramTester histogram_tester;

    WaitForHibernation();

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationAbortedDueToVisibilityChange,
        1);
    EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(Context2D()->IsResourceProviderValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ContextLossAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);

  // For CanvasRenderingContextHost to check for the GPU context being
  // lost as part of checking resource validity, it is necessary to have both
  // accelerated raster/compositing and a CC layer.
  ASSERT_TRUE(SetUpFullAccelerationAndCcLayer(CanvasElement(), Context2D()));

  EXPECT_TRUE(Context2D()->IsResourceProviderValid());

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Simulate GPU context loss.
  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  // Verify that running the hibernation task aborts hibernation due to the
  // GPU context having been lost.
  {
    base::HistogramTester histogram_tester;

    WaitForHibernation();

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationAbortedDueGpuContextLoss,
        1);
    EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_FALSE(Context2D()->IsResourceProviderValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       CanvasCanDehibernateIfGpuContextIsLostAndRestored) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Lose GPU context. Because the canvas is hibernating, it has no GPU
  // resources and is therefore not impacted by the context loss.
  LoseContext();
  EXPECT_FALSE(Context2D()->IsContextLost());
  EXPECT_TRUE(handler.IsHibernating());

  // The GPU context is automatically restored and the canvas can correctly
  // de-hibernate.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedNormally,
        1);
  }

  EXPECT_FALSE(Context2D()->IsContextLost());
  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_TRUE(Context2D()->IsResourceProviderValid());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       MakingPageVisibleDowngradesHibernatedCanvasToCpuIfContextIsLost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  // Configure context provider to stay lost after context losses.
  CreateContextProvider(SetIsContextLost::kNotModifyValue);

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Lose GPU context. Because the canvas is hibernating, it has no GPU
  // resources and is therefore not impacted by the context loss.
  LoseContext();
  EXPECT_FALSE(Context2D()->IsContextLost());
  EXPECT_TRUE(handler.IsHibernating());

  // Since the GPU context is still lost, waking from hibernation will cause the
  // canvas to downgrade to CPU.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationEndedWithFallbackToSW,
        1);
  }

  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  // Detection of GPU context losses isn't currently supported while hibernated.
  EXPECT_FALSE(Context2D()->IsContextLost());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       BackgroundRenderingDowngradesHibernatedCanvasToCpuIfContextIsLost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  // Configure context provider to stay lost after context losses.
  CreateContextProvider(SetIsContextLost::kNotModifyValue);

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Lose GPU context. Because the canvas is hibernating, it has no GPU
  // resources and is therefore not impacted by the context loss.
  LoseContext();
  EXPECT_FALSE(Context2D()->IsContextLost());
  EXPECT_TRUE(handler.IsHibernating());

  // Since the GPU context is still lost, background rendering will wake up the
  // canvas in CPU mode.
  {
    base::HistogramTester histogram_tester;
    RunInTask(base::BindLambdaForTesting(
        [this] { Context2D()->fillRect(0, 0, 10, 10); }));
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationEndedWithSwitchToBackgroundRendering,
        1);
  }

  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  // Detection of GPU context losses isn't currently supported while hibernated.
  EXPECT_FALSE(Context2D()->IsContextLost());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       ResetDowngradesHibernatedCanvasToCpuIfContextIsLost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  // Configure context provider to stay lost after context losses.
  CreateContextProvider(SetIsContextLost::kNotModifyValue);

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Lose GPU context. Because the canvas is hibernating, it has no GPU
  // resources and is therefore not impacted by the context loss.
  LoseContext();
  EXPECT_FALSE(Context2D()->IsContextLost());

  // Since the GPU context is still lost, resetting the canvas will end
  // hibernation in CPU mode.
  {
    base::HistogramTester histogram_tester;
    RunInTask(base::BindLambdaForTesting(
        [this] { CanvasElement().SetSize(gfx::Size(10, 10)); }));
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedOnReset,
        1);
  }

  EXPECT_FALSE(handler.IsHibernating());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
  // Detection of GPU context losses isn't currently supported while hibernated.
  EXPECT_FALSE(Context2D()->IsContextLost());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       BackgroundRenderingEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());

  // Recreate a provider to simulate background rendering.
  {
    base::HistogramTester histogram_tester;
    Context2D()->GetOrCreateResourceProvider();
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationEndedWithSwitchToBackgroundRendering,
        1);
    EXPECT_FALSE(handler.IsHibernating());
    EXPECT_TRUE(Context2D()->IsResourceProviderValid());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ResizeEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Resize the canvas, resetting it and ending hibernation.
  {
    base::HistogramTester histogram_tester;
    RunInTask(base::BindLambdaForTesting(
        [this] { CanvasElement().SetSize(gfx::Size(10, 10)); }));
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedOnReset,
        1);
    EXPECT_FALSE(handler.IsHibernating());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ResetEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  RunIdleTasks();
  EXPECT_TRUE(handler.IsHibernating());

  // Reset the canvas, ending hibernation.
  {
    base::HistogramTester histogram_tester;
    RunInTask(base::BindLambdaForTesting([this] { Context2D()->reset(); }));
    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationEndedOnReset,
        1);
    EXPECT_FALSE(handler.IsHibernating());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ResizeAbortsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

  // Resize the canvas before the hibernation task runs. Resizing the canvas
  // discards the resource provider. There's nothing to hibernate anymore, so
  // hibernation is cancelled.
  {
    base::HistogramTester histogram_tester;
    RunInTask(base::BindLambdaForTesting(
        [this] { CanvasElement().SetSize(gfx::Size(10, 10)); }));

    // Run hibernation task. Hibernation aborts since there's no more resources.
    WaitForHibernation();

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::
            kHibernationAbortedBecauseNoSurface,
        1);
    EXPECT_FALSE(handler.IsHibernating());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ResetDoesntAbortHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  // Hide the page and run hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

  // Reset the canvas before the hibernation task runs. Resetting the canvas
  // clears the canvas, but preserves the resource provider. Hibernation can
  // proceed normally, with a blank canvas.
  {
    base::HistogramTester histogram_tester;
    RunInTask(base::BindLambdaForTesting([this] { Context2D()->reset(); }));

    // Run hibernation task. Hibernation aborts since there's no more resources.
    RunIdleTasks();

    EXPECT_TRUE(handler.IsHibernating());
  }
}

TEST_P(CanvasRenderingContext2DTestAccelerated, ResourceRecycling) {
  CreateContext(kNonOpaque);

  viz::TransferableResource resources[3];
  viz::ReleaseCallback callbacks[3];
  cc::PaintFlags flags;

  Context2D()->fillRect(3, 3, 1, 1);

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // be present.
  CanvasElement().GetOrCreateCcLayerForCanvas2DIfNeeded();

  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resources[0],
                                                          &callbacks[0]));

  Context2D()->fillRect(3, 3, 1, 1);

  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resources[1],
                                                          &callbacks[1]));
  EXPECT_NE(resources[0].mailbox(), resources[1].mailbox());

  // Now release the first resource and draw again. It should be reused due to
  // recycling.
  std::move(callbacks[0]).Run(gpu::SyncToken(), false);

  Context2D()->fillRect(3, 3, 1, 1);

  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resources[2],
                                                          &callbacks[2]));
  EXPECT_EQ(resources[0].mailbox(), resources[2].mailbox());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       NoResourceRecyclingWhenPageHidden) {
  CreateContext(kNonOpaque);

  EXPECT_THAT(Context2D()->GetResourceProviderForTesting(), IsNull());

  Context2D()->fillRect(3, 3, 1, 1);

  const CanvasResourceProviderSharedImage* provider =
      Context2D()->GetResourceProviderForTesting()->AsSharedImageProvider();
  ASSERT_THAT(provider, NotNull());
  EXPECT_EQ(provider->NumInflightResourcesForTesting(), 1);

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // be present.
  CanvasElement().GetOrCreateCcLayerForCanvas2DIfNeeded();

  viz::TransferableResource resources[2];
  viz::ReleaseCallback callbacks[2];

  // Emulate sending the canvas' resource to the display compositor.
  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resources[0],
                                                          &callbacks[0]));

  // Write to the canvas.
  Context2D()->fillRect(3, 3, 1, 1);

  // Note that the write did not in of itself trigger copy-on-write since
  // rasterization has not occurred yet.
  EXPECT_EQ(provider->NumInflightResourcesForTesting(), 1);

  // Emulate sending the canvas' resource to the display compositor, which
  // forces copy-on-write before rasterization as the display compositor has a
  // read ref on the first resource.
  ASSERT_TRUE(CanvasElement().PrepareTransferableResource(&resources[1],
                                                          &callbacks[1]));
  EXPECT_NE(resources[0].mailbox(), resources[1].mailbox());
  EXPECT_EQ(provider->NumInflightResourcesForTesting(), 2);

  // Emulate the display compositor releasing the first resource. The released
  // resource should be saved for recycling (i.e., it should not be dropped).
  std::move(callbacks[0]).Run(gpu::SyncToken(), false);
  EXPECT_EQ(provider->NumInflightResourcesForTesting(), 2);

  // Move the page to the background. This should cause resource recycling to be
  // disabled and the previously-released resource to now be dropped.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  EXPECT_EQ(provider->NumInflightResourcesForTesting(), 1);

  // Emulate the display compositor releasing the second resource. The resource
  // should not be dropped because it's the current render target for the canvas
  // and so the canvas itself still has a reference on this resource. This
  // resource should be dropped only if the canvas is hibernated.
  std::move(callbacks[1]).Run(gpu::SyncToken(), false);
  EXPECT_EQ(provider->NumInflightResourcesForTesting(), 1);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PushPropertiesAfterVisibilityChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kClearCanvasResourcesInBackground},
                                {features::kCanvas2DHibernation});

  CreateContext(kNonOpaque);

  ASSERT_TRUE(SetUpFullAccelerationAndCcLayer(CanvasElement(), Context2D()));

  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  EXPECT_FALSE(CanvasElement()
                   .GetCanvas2DCcLayerForTesting()
                   ->needs_set_resource_for_testing());

  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);
  EXPECT_TRUE(CanvasElement()
                  .GetCanvas2DCcLayerForTesting()
                  ->needs_set_resource_for_testing());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       PrepareTransferableResourceFailsWhileHibernating) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);

  // Invoking PrepareTransferableResource() has a precondition that a CC layer
  // is present, while GPU compositing is necessary for hibernation to succeed.
  ASSERT_TRUE(SetUpFullAccelerationAndCcLayer(CanvasElement(), Context2D()));

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  EXPECT_FALSE(handler.IsHibernating());

  // Verify that going to the background triggers hibernation asynchronously.
  {
    base::HistogramTester histogram_tester;
    SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

    histogram_tester.ExpectUniqueSample(
        kCanvasHibernationEventHistogramName,
        CanvasHibernationHandler::HibernationEvent::kHibernationScheduled, 1);
    EXPECT_FALSE(handler.IsHibernating());
  }

  WaitForHibernation();
  EXPECT_TRUE(handler.IsHibernating());

  // Verify that PrepareTransferableResource() fails while hibernating.
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_FALSE(CanvasElement().PrepareTransferableResource(&resource,
                                                           &release_callback));
  EXPECT_TRUE(handler.IsHibernating());
  EXPECT_FALSE(Context2D()->IsResourceProviderValid());
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       CanvasDrawInBackgroundEndsHibernation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kCanvas2DHibernation}, {});

  CreateContext(kNonOpaque);
  Context2D()->GetOrCreateResourceProvider();

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());

  EXPECT_FALSE(handler.is_encoded());
  EXPECT_FALSE(handler.IsHibernating());

  // Hide the page to trigger the hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

  // Hibernation is triggered asynchronously.
  EXPECT_FALSE(handler.is_encoded());
  EXPECT_FALSE(handler.IsHibernating());

  WaitForHibernation();
  EXPECT_FALSE(Context2D()->GetResourceProviderForTesting());
  EXPECT_TRUE(handler.IsHibernating());

  // Wait for encoding to complete on a background thread.
  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
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
  ASSERT_TRUE(Context2D()->GetOrCreateResourceProvider());
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  auto& handler = CHECK_DEREF(Context2D()->GetHibernationHandler());
  ASSERT_FALSE(handler.is_encoded());
  ASSERT_FALSE(handler.IsHibernating());

  // Hide the page to trigger the hibernation task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);

  // Hibernation is triggered asynchronously.
  ASSERT_FALSE(handler.is_encoded());
  ASSERT_FALSE(handler.IsHibernating());

  WaitForHibernation();

  ASSERT_FALSE(Context2D()->GetResourceProviderForTesting());
  ASSERT_TRUE(handler.IsHibernating());
  ASSERT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);

  task_environment_.FastForwardBy(
      CanvasHibernationHandler::kBeforeCompressionDelay);
  ASSERT_TRUE(handler.is_encoded());

  // Taking a snapshot of the canvas while hibernating should produce an
  // unaccelerated image.
  EXPECT_FALSE(Context2D()->GetImage()->IsTextureBacked());

  // The action of taking the snapshot should not have impacted the state of
  // hibernation.
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);
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

  gfx::Size size(200, 200);
  auto provider = std::make_unique<FakeCanvasResourceProvider>(
      size, RasterModeHint::kPreferGPU, &CanvasElement(),
      CompositingMode::kSupportsDirectCompositing);

  // Recorded draw ops are resterized on hibernation. The provider gets replaced
  // when getting out of hibernation, so this mock will not see the later calls
  // to `RasterRecord`.
  cc::PaintRecord hibernation_raster;
  EXPECT_CALL(*provider, Snapshot(_)).Times(1);
  EXPECT_CALL(*provider, RasterRecord)
      .Times(1)
      .WillOnce(SaveArg<0>(&hibernation_raster));

  Context2D()->SetCanvas2DResourceProviderForTesting(std::move(provider), size);

  ThreadScheduler::Current()->PostIdleTask(
      FROM_HERE, BindOnce(
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
                     Unretained(this)));
  blink::test::RunPendingTasks();

  // Hibernate the canvas. Hibernation is handled in a idle task.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();

  // Hibernating should have rastered paint ops preceding `beginLayer`.
  EXPECT_THAT(hibernation_raster,
              RecordedOpsAre(PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 1, 1),
                                                   FillFlags())));

  // Wake up from hibernation.
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kVisible);

  NonThrowableExceptionState exception_state;
  Context2D()->endLayer(exception_state);

  // Post hibernation recording now holds the layer content.
  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kOther),
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
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  EXPECT_TRUE(CanvasElement().GetLayoutBoxModelObject());
  auto* box = CanvasElement().GetLayoutBoxModelObject();
  EXPECT_TRUE(box);
  PaintLayer* painting_layer = box->PaintingLayer();
  EXPECT_TRUE(painting_layer);
  UpdateAllLifecyclePhasesForTest();

  // The resource provider gets lazily created. Force it to be dropped.
  Context2D()->ResetResourceProvider();

  // Hide element to trigger hibernation (if enabled).
  SetDocumentVisibility(GetDocument(), PageVisibilityState::kHidden);
  WaitForHibernation();

  // Never hibernate a canvas with no resource provider.
  EXPECT_FALSE(box->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(painting_layer->SelfNeedsRepaint());
}

TEST_P(CanvasRenderingContext2DTestAccelerated, LowLatencyIsNotSingleBuffered) {
  CreateContext(kNonOpaque, kLowLatency);
  // No need to set-up the layer bridge when testing low latency mode.
  DrawSomething();
  EXPECT_TRUE(Context2D()->getContextAttributes()->desynchronized());
  EXPECT_FALSE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_FALSE(Context2D()->GetOrCreateResourceProvider()->IsSingleBuffered());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
}

TEST_P(CanvasRenderingContext2DTestAccelerated, DrawImage_Video_Flush) {
  V8TestingScope scope;

  CreateContext(kNonOpaque);
  // No need to set-up the layer bridge when testing low latency mode.
  Context2D()->GetOrCreateResourceProvider();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

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
  EXPECT_TRUE(Context2D()
                  ->GetResourceProviderForTesting()
                  ->Recorder()
                  .HasRecordedDrawOps());

  Context2D()->drawImage(frame, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  // The drawImage Operation is supposed to trigger a flush, which means that
  // There should not be any Recorded ops at this point.
  EXPECT_FALSE(Context2D()
                   ->GetResourceProviderForTesting()
                   ->Recorder()
                   .HasRecordedDrawOps());
}

TEST_P(CanvasRenderingContext2DTest, FlushRestoresClipStack) {
  CreateContext(kNonOpaque);

  // Ensure that the ResourceProvider and canvas are created.
  Context2D()->GetOrCreateResourceProvider();

  // Set a transform.
  Context2D()->translate(5, 0);
  EXPECT_EQ(Canvas().getLocalToDevice().rc(0, 3), 5);

  // Draw something so that there is something to flush.
  cc::PaintFlags flags;
  Canvas().drawLine(0, 0, 2, 2, flags);

  // Flush the canvas and verify that a new drawing canvas is created that has
  // the transform restored.
  EXPECT_TRUE(Context2D()->FlushCanvas(FlushReason::kOther));
  EXPECT_EQ(Canvas().getLocalToDevice().rc(0, 3), 5);
}

TEST_P(CanvasRenderingContext2DTest, PutImageDataRestoresClipStack) {
  CreateContext(kNonOpaque);

  // Ensure that the ResourceProvider and canvas are created.
  Context2D()->GetOrCreateResourceProvider();

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
  Context2D()->SetCanvas2DResourceProviderForTesting(
      /*provider=*/nullptr, size);
  CanvasRenderingContext2D* context = Context2D();

  // 800 = 10 * 10 * 4 * 2 where 10*10 is canvas size, 4 is num of bytes per
  // pixel per buffer, and 2 is an estimate of num of gpu buffers required

  context->fillRect(10, 10, 100, 100);
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);

  Context2D()->DisableAcceleration();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);

  context->fillRect(10, 10, 100, 100);
}

TEST_P(CanvasRenderingContext2DTestAccelerated,
       DisableAccelerationPreservesRecording) {
  ScopedCanvas2dLayersForTest layer_feature{/*enabled=*/true};
  CreateContext(kNonOpaque);

  Context2D()->GetOrCreateResourceProvider();

  NonThrowableExceptionState exception_state;
  Context2D()->fillRect(10, 10, 20, 20);
  Context2D()->save();
  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          exception_state);
  Context2D()->fillRect(10, 20, 30, 40);

  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  Context2D()->DisableAcceleration();
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kCPU);

  Context2D()->endLayer(exception_state);
  Context2D()->restore(exception_state);

  // Disabling acceleration caused pending paint ops to be rasterized. The
  // resulting raster is drawn into the the new CPU surface. We are only left
  // with the paint ops that could not be rasterized.
  EXPECT_THAT(
      Context2D()->FlushCanvas(FlushReason::kOther),
      Optional(RecordedOpsAre(
          PaintOpEq<SaveOp>(),
          DrawRecordOpEq(PaintOpEq<SaveLayerAlphaOp>(1.0f),
                         PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(10, 20, 30, 40),
                                               FillFlags()),
                         PaintOpEq<RestoreOp>()),
          PaintOpEq<RestoreOp>())));
}

class CanvasRenderingContext2DTestAcceleratedMultipleDisables
    : public CanvasRenderingContext2DTestAccelerated {
 protected:
  void CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled() {
    for (int i = 0; i < 100; ++i) {
      auto* canvas = MakeGarbageCollected<HTMLCanvasElement>(GetDocument());
      CreateContext(
          kNonOpaque, kNormalLatency,
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined,
          canvas);
      auto* context =
          static_cast<CanvasRenderingContext2D*>(canvas->RenderingContext());
      context->GetOrCreateResourceProvider();
      EXPECT_TRUE(canvas->IsAccelerated());
      context->DisableAcceleration();
    }
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(
    CanvasRenderingContext2DTestAcceleratedMultipleDisables);

TEST_P(CanvasRenderingContext2DTestAcceleratedMultipleDisables,
       ReadFrequentlyUndefined) {
  CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled();
  CreateContext(
      kNonOpaque, kNormalLatency,
      CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined);
  Context2D()->GetOrCreateResourceProvider();
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
  Context2D()->GetOrCreateResourceProvider();
  // Canvases created with `kFalse` should always start with acceleration
  // enabled regardless of how many canvases had acceleration disabled.
  EXPECT_TRUE(CanvasElement().IsAccelerated());
}

TEST_P(CanvasRenderingContext2DTestAcceleratedMultipleDisables,
       ReadFrequentlyTrue) {
  CreateAlotOfCanvasesWithAccelerationExplicitlyDisabled();
  CreateContext(kNonOpaque, kNormalLatency,
                CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  Context2D()->GetOrCreateResourceProvider();
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

  void ConfigureContextProvider(
      viz::TestContextProvider& context_provider) override {
    auto* test_raster = context_provider.GetTestRasterInterface();
    test_raster->set_max_texture_size(1024);
    test_raster->set_supports_gpu_memory_buffer_format(
        gfx::BufferFormat::BGRA_8888, true);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_scanout_shared_images = true;
    context_provider.SharedImageInterface()->SetCapabilities(shared_image_caps);
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
  EXPECT_FALSE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_TRUE(Context2D()->GetOrCreateResourceProvider()->IsSingleBuffered());
  auto frame1_resource =
      Context2D()->GetOrCreateResourceProvider()->ProduceCanvasResource(
          FlushReason::kOther);
  EXPECT_TRUE(frame1_resource);
  DrawSomething();
  auto frame2_resource =
      Context2D()->GetOrCreateResourceProvider()->ProduceCanvasResource(
          FlushReason::kOther);
  EXPECT_TRUE(frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}

#if BUILDFLAG(IS_WIN)
class CanvasRenderingContext2DTestSwapChain
    : public CanvasRenderingContext2DTestAccelerated {
 protected:
  CanvasRenderingContext2DTestSwapChain()
      : CanvasRenderingContext2DTestAccelerated() {}

  void ConfigureContextProvider(
      viz::TestContextProvider& context_provider) override {
    auto* test_raster = context_provider.GetTestRasterInterface();
    test_raster->set_max_texture_size(1024);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.shared_image_swap_chain = true;
    context_provider.SharedImageInterface()->SetCapabilities(shared_image_caps);
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
  EXPECT_FALSE(Context2D()->getContextAttributes()->willReadFrequently());
  EXPECT_TRUE(CanvasElement().LowLatencyEnabled());
  EXPECT_EQ(CanvasElement().GetRasterModeForCanvas2D(), RasterMode::kGPU);
  EXPECT_TRUE(Context2D()->GetOrCreateResourceProvider()->IsSingleBuffered());
  auto frame1_resource =
      Context2D()->GetOrCreateResourceProvider()->ProduceCanvasResource(
          FlushReason::kOther);
  EXPECT_TRUE(frame1_resource);
  DrawSomething();
  auto frame2_resource =
      Context2D()->GetOrCreateResourceProvider()->ProduceCanvasResource(
          FlushReason::kOther);
  EXPECT_TRUE(frame2_resource);
  EXPECT_EQ(frame1_resource.get(), frame2_resource.get());
}
#endif

}  // namespace blink
