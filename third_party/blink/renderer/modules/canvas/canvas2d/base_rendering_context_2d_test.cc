// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/test/paint_op_matchers.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace blink {

class MemoryManagedPaintCanvas;

namespace {

using ::blink_testing::ParseFilter;
using ::blink_testing::RecordedOpsAre;
using ::cc::DrawImageOp;
using ::cc::PaintOpEq;
using ::cc::PaintOpIs;
using ::cc::RestoreOp;
using ::cc::SaveLayerOp;

// Test version of BaseRenderingContext2D. BaseRenderingContext2D can't be
// tested directly because it's an abstract class. This test class essentially
// just gives a definition to all pure virtual method, making it instantiable.
class TestRenderingContext2D final
    : public GarbageCollected<TestRenderingContext2D>,
      public BaseRenderingContext2D {
 public:
  explicit TestRenderingContext2D(
      V8TestingScope& scope,
      const CanvasContextCreationAttributesCore& attrs =
          CanvasContextCreationAttributesCore(),
      const gfx::Size& size = gfx::Size(300, 300))
      : BaseRenderingContext2D(
            MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument()),
            attrs,
            scheduler::GetSingleThreadTaskRunnerForTesting()),
        execution_context_(scope.GetExecutionContext()) {
    HostAsHTMLCanvasElement()->SetSize(size);
    ConfigureRecorder(size, /*is_graphite=*/false);
  }
  ~TestRenderingContext2D() override = default;

  // Returns the content of the paint recorder, leaving it empty.
  cc::PaintRecord FlushRecorder() { return Recorder()->ReleaseMainRecording(); }

  bool OriginClean() const override { return true; }
  void SetOriginTainted() override {}

  int Width() const override { return Host()->width(); }
  int Height() const override { return Host()->height(); }

  bool CanCreateBacking() override { return false; }

  RespectImageOrientationEnum RespectImageOrientation() const override {
    return kRespectImageOrientation;
  }

  Color GetCurrentColor() const override { return Color::kBlack; }

  MemoryManagedPaintCanvas* GetOrCreatePaintCanvas() override {
    return GetPaintCanvas();
  }
  using BaseRenderingContext2D::ConfigureRecorder;
  using BaseRenderingContext2D::CreateSharedImageProvider;
  using BaseRenderingContext2D::CreateSoftwareSurface;
  using BaseRenderingContext2D::FlushIfRecordingLimitExceeded;
  using BaseRenderingContext2D::Snapshot;
  void WillDraw(const gfx::Rect& dirty_rect,
                CanvasPerformanceMonitor::DrawType) override {}

  sk_sp<PaintFilter> StateGetFilter() override {
    return GetState().GetFilterForOffscreenCanvas({}, this);
  }

  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_.Get();
  }

  bool HasAlpha() const override { return true; }

  void SetContextLost(LostContextMode context_lost_mode) {
    context_lost_mode_ = context_lost_mode;
  }

  void SetIsPaintable(bool is_paintable) { is_paintable_ = is_paintable; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    BaseRenderingContext2D::Trace(visitor);
  }

  HTMLCanvasElement* HostAsHTMLCanvasElement() const override {
    return static_cast<HTMLCanvasElement*>(Host());
  }

 private:
  void InitializeForRecording(cc::PaintCanvas* canvas) const override {
    if (restore_matrix_enabled_) {
      RestoreMatrixClipStack(canvas);
    }
  }

  std::optional<cc::PaintRecord> FlushCanvas(FlushReason reason) override {
    if (HasBacking()) {
      return FlushCanvasInternal(reason);
    }
    return Recorder()->ReleaseMainRecording();
  }

  bool ResolveFont(const String& new_font) override {
    if (Host() == nullptr) {
      return false;
    }
    auto* style = CSSParser::ParseFont(new_font, execution_context_);
    if (style == nullptr) {
      return false;
    }
    auto* selector = Host()->GetFontSelector();
    std::optional<FontDescription> maybe_desc =
        FontStyleResolver::ComputeFont(*style, selector->BaseFontSelector());
    if (!maybe_desc.has_value()) {
      return false;
    }
    GetState().SetFont(maybe_desc.value(), selector);
    return true;
  }

  bool InitializeBacking() override { return false; }

  // Implementing pure virtual functions from CanvasRenderingContext.
  scoped_refptr<StaticBitmapImage> GetImage() override { return nullptr; }

  bool IsComposited() const override { return false; }
  bool IsPaintable() const override { return is_paintable_; }
  void Stop() override {}

  Member<ExecutionContext> execution_context_;
  bool restore_matrix_enabled_ = true;
  bool is_paintable_ = true;
};

BeginLayerOptions* FilterOption(blink::V8TestingScope& scope,
                                const std::string& filter) {
  BeginLayerOptions* options = BeginLayerOptions::Create();
  options->setFilter(ParseFilter(scope, filter));
  return options;
}

TEST(BaseRenderingContextLayerTests, ContextLost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->SetContextLost(CanvasRenderingContext::kRealLostContext);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->FlushRecorder(), RecordedOpsAre());
}

TEST(BaseRenderingContextLayerTests, putImageDataThrowsInLayer) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  NonThrowableExceptionState no_exception;
  ImageData* image =
      context->createImageData(/*sw=*/10, /*sh=*/10, no_exception);
  // `putImageData` shouldn't throw on it's own.
  context->putImageData(image, /*dx=*/0, /*dy=*/0, no_exception);
  // Make sure the exception isn't caused by calling the function twice.
  context->putImageData(image, /*dx=*/0, /*dy=*/0, no_exception);
  // Calling again inside a layer should throw.
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);
  context->putImageData(image, /*dx=*/0, /*dy=*/0, scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(BaseRenderingContextLayersCSSTests,
     FilterOperationsWithStyleResolutionHost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->setFont("10px sans-serif");
  NonThrowableExceptionState exception_state;
  context->beginLayer(scope.GetScriptState(),
                      FilterOption(scope, "'blur(1em)'"), exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(10.0f, 10.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContext2DTest, GetPaintCanvasRequiresIsPaintable) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  ASSERT_NE(context->Recorder(), nullptr);
  EXPECT_NE(context->GetPaintCanvas(), nullptr);

  context->SetIsPaintable(false);
  EXPECT_NE(context->Recorder(), nullptr);
  EXPECT_EQ(context->GetPaintCanvas(), nullptr);

  context->SetIsPaintable(true);
  EXPECT_NE(context->GetPaintCanvas(), nullptr);
}

TEST(BaseRenderingContext2DTest, RecordingLimits) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  const MemoryManagedPaintRecorder* initial_recorder = context->Recorder();
  EXPECT_NE(initial_recorder, nullptr);
  EXPECT_EQ(context->max_recorded_op_bytes(),
            static_cast<size_t>(features::kMaxRecordedOpKB.Get()) * 1024);
  EXPECT_EQ(context->max_pinned_image_bytes(),
            static_cast<size_t>(features::kMaxPinnedImageKB.Get()) * 1024);

  context->ConfigureRecorder(gfx::Size(300, 300), /*is_graphite=*/true);
  EXPECT_EQ(context->Recorder(), initial_recorder);
  EXPECT_EQ(
      context->max_recorded_op_bytes(),
      static_cast<size_t>(features::kMaxRecordedOpGraphiteKB.Get()) * 1024);
  EXPECT_EQ(context->max_pinned_image_bytes(),
            static_cast<size_t>(features::kMaxPinnedImageKB.Get()) * 1024);

  context->ConfigureRecorder(gfx::Size(300, 300), /*is_graphite=*/false);
  EXPECT_EQ(context->Recorder(), initial_recorder);
  EXPECT_EQ(context->max_recorded_op_bytes(),
            static_cast<size_t>(features::kMaxRecordedOpKB.Get()) * 1024);
}

TEST(BaseRenderingContext2DTest, FlushIfRecordingLimitExceeded) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  const size_t initial_op_count = context->Recorder()->TotalOpCount();

  // Under limit: no flush.
  context->fillRect(0, 0, 1, 1);
  EXPECT_GT(context->Recorder()->TotalOpCount(), initial_op_count);
  const size_t op_count_under_limit = context->Recorder()->TotalOpCount();
  context->FlushIfRecordingLimitExceeded();
  EXPECT_EQ(context->Recorder()->TotalOpCount(), op_count_under_limit);

  // Exceed op budget: FlushIfRecordingLimitExceeded flushes.
  while (context->Recorder()->TotalOpBytesUsed() <=
         context->max_recorded_op_bytes()) {
    context->fillRect(0, 0, 1, 1);
  }
  context->FlushIfRecordingLimitExceeded();
  EXPECT_EQ(context->Recorder()->TotalOpCount(), initial_op_count);

  // When printing and clear_frame() is true, no flush occurs even if over
  // limit.
  scope.GetDocument().SetPrinting(Document::kPrinting);
  while (context->Recorder()->TotalOpBytesUsed() <=
         context->max_recorded_op_bytes()) {
    context->fillRect(0, 0, 1, 1);
  }
  EXPECT_TRUE(context->clear_frame());
  const size_t printing_op_count = context->Recorder()->TotalOpCount();
  EXPECT_GT(printing_op_count, initial_op_count);
  context->FlushIfRecordingLimitExceeded();
  EXPECT_EQ(context->Recorder()->TotalOpCount(), printing_op_count);

  // When printing but clear_frame() is false, flushing does occur when over
  // limit.
  context->set_clear_frame(false);
  context->FlushIfRecordingLimitExceeded();
  EXPECT_EQ(context->Recorder()->TotalOpCount(), initial_op_count);
}

TEST(BaseRenderingContext2DTest, HdrMetadata) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const gfx::Size kSize(10, 10);
  gfx::HDRMetadata hdr_metadata;
  hdr_metadata.SetExtendedRangeWithHeadroom(2.f);

  CanvasContextCreationAttributesCore attrs;
  attrs.hdr_metadata = hdr_metadata;
  auto* context =
      MakeGarbageCollected<TestRenderingContext2D>(scope, attrs, kSize);
  context->CreateSoftwareSurface();

  ASSERT_TRUE(context->HasBacking());
  scoped_refptr<StaticBitmapImage> snapshot = context->Snapshot();
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->GetHdrMetadata(), hdr_metadata);
}

TEST(BaseRenderingContext2DTest, AllocatedBufferSizeSoftware) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  constexpr gfx::Size kSize(10, 10);

  auto* context = MakeGarbageCollected<TestRenderingContext2D>(
      scope, CanvasContextCreationAttributesCore(), kSize);
  context->CreateSoftwareSurface();

  ASSERT_TRUE(context->HasBacking());
  EXPECT_EQ(context->AllocatedBufferSize(),
            base::ByteSize(kSize.width() * kSize.height() * 4));
}

constexpr int kMaxTextureSize = 1024;

class BaseRenderingContext2DSharedImageTest : public testing::Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    auto* test_raster = test_context_provider_->UnboundTestRasterInterface();
    test_raster->set_max_texture_size(kMaxTextureSize);
    test_raster->set_texture_format_bgra8888(true);
    test_raster->set_texture_half_float_linear(true);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_scanout_shared_images = true;
#if BUILDFLAG(IS_WIN)
    shared_image_caps.shared_image_swap_chain = true;
#endif
    test_context_provider_->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

    InitializeSharedGpuContext(test_context_provider_.get());
  }

  void TearDown() override { SharedGpuContext::Reset(); }

 protected:
  static constexpr gfx::Size kSize{10, 10};

  TestRenderingContext2D* CreateBackedContext(
      V8TestingScope& scope,
      RasterMode raster_mode = RasterMode::kGPU) {
    const gpu::SharedImageUsageSet shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    auto* context = MakeGarbageCollected<TestRenderingContext2D>(
        scope, CanvasContextCreationAttributesCore(), kSize);
    context->CreateSharedImageProvider(raster_mode, shared_image_usage_flags);
    return context;
  }

  const gpu::SyncToken& GetSyncToken(const ExportedCanvasResource* resource) {
    return resource->GetResourceForTesting()->sync_token();
  }

  void EnsureResourceRecycled(
      scoped_refptr<ExportedCanvasResource>&& resource) {
    auto transferable_resource = resource->PrepareTransferableResource();
    resource.reset();
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  ScopedTestingPlatformSupport<GpuCompositingTestPlatform> platform_;
};

TEST_F(BaseRenderingContext2DSharedImageTest, UnacceleratedOverlay) {
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kCPU);
  ASSERT_TRUE(context->IsBackingValid());
  EXPECT_FALSE(context->Is2DCanvasAccelerated());

  auto resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                           FlushReason::kOther);
  ASSERT_TRUE(resource);
  auto shared_image = resource->GetSharedImageForTesting();
  ASSERT_TRUE(shared_image);
  EXPECT_EQ(shared_image->size(), kSize);
  EXPECT_EQ(shared_image->format(), viz::SharedImageFormat::N32Format());
  EXPECT_EQ(shared_image->alpha_type(), kPremul_SkAlphaType);
  EXPECT_EQ(shared_image->color_space(), gfx::ColorSpace::CreateSRGB());
  // We do not support single buffering for unaccelerated low latency canvas.
  EXPECT_FALSE(
      shared_image->usage().Has(gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE));
}

TEST_F(BaseRenderingContext2DSharedImageTest, ResourceRecyclingViaSnapshot) {
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kGPU);
  ASSERT_TRUE(context->IsBackingValid());

  // Same resource returned until the canvas is updated.
  auto image = context->PaintRenderingResultsToSnapshot(kBackBuffer);
  ASSERT_TRUE(image);
  auto new_image = context->PaintRenderingResultsToSnapshot(kBackBuffer);
  EXPECT_EQ(image->GetSharedImage(), new_image->GetSharedImage());
  EXPECT_EQ(
      context->PaintRenderingResultsToResource(kBackBuffer, FlushReason::kOther)
          ->GetSharedImageForTesting(),
      image->GetSharedImage());

  // Resource updated after draw.
  context->fillRect(0, 0, 1, 1);
  new_image = context->PaintRenderingResultsToSnapshot(kBackBuffer);
  EXPECT_NE(new_image->GetSharedImage(), image->GetSharedImage());

  // Resource recycled.
  auto original_shared_image = image->GetSharedImage();
  image.reset();
  context->fillRect(0, 0, 1, 1);
  EXPECT_EQ(
      original_shared_image,
      context->PaintRenderingResultsToSnapshot(kBackBuffer)->GetSharedImage());
}

TEST_F(BaseRenderingContext2DSharedImageTest, AllocatedBufferSizeAccelerated) {
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kGPU);
  ASSERT_TRUE(context->IsBackingValid());
  EXPECT_TRUE(context->Is2DCanvasAccelerated());
  EXPECT_EQ(context->AllocatedBufferSize(),
            base::ByteSize(kSize.width() * kSize.height() * 4));
}

class BaseRenderingContext2DSharedImageSyncTokenTest
    : public BaseRenderingContext2DSharedImageTest,
      public testing::WithParamInterface<bool> {
 public:
  BaseRenderingContext2DSharedImageSyncTokenTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          ::features::kUseAutomaticSyncTokenManagement);
    } else {
      feature_list_.InitAndDisableFeature(
          ::features::kUseAutomaticSyncTokenManagement);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BaseRenderingContext2DSharedImageSyncTokenTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "AutomaticSyncTokens"
                                             : "ManualSyncTokens";
                         });

TEST_P(BaseRenderingContext2DSharedImageSyncTokenTest,
       ResourceRecyclingViaCanvasResource) {
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kGPU);
  ASSERT_TRUE(context->IsBackingValid());
  EXPECT_TRUE(context->Is2DCanvasAccelerated());

  // Same resource and sync token if we query again without updating.
  auto resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                           FlushReason::kOther);
  auto sync_token = GetSyncToken(resource.get());
  ASSERT_TRUE(resource);
  auto shared_image = resource->GetSharedImageForTesting();
  ASSERT_TRUE(shared_image);
  EXPECT_EQ(shared_image->size(), kSize);
  EXPECT_EQ(shared_image->alpha_type(), kPremul_SkAlphaType);
  EXPECT_EQ(shared_image->color_space(), gfx::ColorSpace::CreateSRGB());
  EXPECT_FALSE(
      shared_image->usage().Has(gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE));
  // As it is an accelerated canvas, it will internally force it to RGBA8, or
  // BGRA8 on MacOS.
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(shared_image->format(), viz::SinglePlaneFormat::kBGRA_8888);
#else
  EXPECT_EQ(shared_image->format(), viz::SinglePlaneFormat::kRGBA_8888);
#endif
  EXPECT_EQ(
      resource->GetSharedImageForTesting(),
      context->PaintRenderingResultsToResource(kBackBuffer, FlushReason::kOther)
          ->GetSharedImageForTesting());
  EXPECT_EQ(sync_token, GetSyncToken(resource.get()));

  context->fillRect(0, 0, 1, 1);
  auto new_resource = context->PaintRenderingResultsToResource(
      kBackBuffer, FlushReason::kOther);
  EXPECT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          ::features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    EXPECT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }
  auto old_shared_image = resource->GetSharedImageForTesting();

  EnsureResourceRecycled(std::move(resource));

  context->fillRect(0, 0, 1, 1);
  auto resource_again = context->PaintRenderingResultsToResource(
      kBackBuffer, FlushReason::kOther);
  EXPECT_EQ(old_shared_image.get(), resource_again->GetSharedImageForTesting());
  if (base::FeatureList::IsEnabled(
          ::features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource_again.get()).HasData());
  } else {
    EXPECT_NE(sync_token, GetSyncToken(resource_again.get()));
  }
}

TEST_P(BaseRenderingContext2DSharedImageSyncTokenTest, UnusedResources) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kGPU);
  auto* provider = context->GetSharedImageProviderForTesting();

  auto resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                           FlushReason::kOther);
  context->fillRect(0, 0, 1, 1);
  auto new_resource = context->PaintRenderingResultsToResource(
      kBackBuffer, FlushReason::kOther);
  ASSERT_NE(resource, new_resource);

  if (base::FeatureList::IsEnabled(
          ::features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }

  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  // The reclaim task has been posted.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  // There is a ready-to-reuse resource.
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(
      Canvas2DResourceProvider::kUnusedResourceExpirationTime);
  // The resource is freed, don't repost the task.
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_P(BaseRenderingContext2DSharedImageSyncTokenTest,
       DontReclaimUnusedResourcesWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCanvas2DReclaimUnusedResources);
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kGPU);
  auto* provider = context->GetSharedImageProviderForTesting();

  auto resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                           FlushReason::kOther);
  context->fillRect(0, 0, 1, 1);
  auto new_resource = context->PaintRenderingResultsToResource(
      kBackBuffer, FlushReason::kOther);
  ASSERT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          ::features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  // There is a ready-to-reuse resource.
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // No task posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_P(BaseRenderingContext2DSharedImageSyncTokenTest,
       UnusedResourcesAreNotCollectedWhenYoung) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};
  V8TestingScope scope;
  auto* context = CreateBackedContext(scope, RasterMode::kGPU);
  auto* provider = context->GetSharedImageProviderForTesting();

  auto resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                           FlushReason::kOther);
  context->fillRect(0, 0, 1, 1);
  auto new_resource = context->PaintRenderingResultsToResource(
      kBackBuffer, FlushReason::kOther);
  ASSERT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          ::features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  // There is a ready-to-reuse resource.
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(
      Canvas2DResourceProvider::kUnusedResourceExpirationTime -
      base::Seconds(1));
  // The reclaim task hasn't run yet.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  context->fillRect(0, 0, 1, 1);
  resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                      FlushReason::kOther);
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  context->fillRect(0, 0, 1, 1);
  new_resource = context->PaintRenderingResultsToResource(kBackBuffer,
                                                          FlushReason::kOther);
  ASSERT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          ::features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }

  EnsureResourceRecycled(std::move(resource));
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));

  // Too young, no release yet.
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // But re-post the task to free it.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  task_environment_.FastForwardBy(
      Canvas2DResourceProvider::kUnusedResourceExpirationTime);
  // Now it's collected.
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  // And no new task is posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

}  // namespace
}  // namespace blink
