// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

#include <optional>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_stretch.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_text_rendering.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_offscreen_rendering_context.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_bitmap_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

class MemoryManagedPaintCanvas;

namespace {
const size_t kHardMaxCachedFonts = 250;
const size_t kMaxCachedFonts = 25;
// Max delay to fire context lost for context in iframes.
static const unsigned kMaxIframeContextLoseDelay = 100;

class OffscreenFontCache {
 public:
  void PruneLocalFontCache(size_t target_size) {
    while (font_lru_list_.size() > target_size) {
      fonts_resolved_.erase(font_lru_list_.back());
      font_lru_list_.pop_back();
    }
  }

  void AddFont(blink::String name, blink::FontDescription& font) {
    fonts_resolved_.insert(name, font);
    auto add_result = font_lru_list_.PrependOrMoveToFirst(name);
    DCHECK(add_result.is_new_entry);
    PruneLocalFontCache(kHardMaxCachedFonts);
  }

  blink::FontDescription* GetFont(blink::String name) {
    auto i = fonts_resolved_.find(name);
    if (i != fonts_resolved_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(name);
      DCHECK(!add_result.is_new_entry);
      return &(i->value);
    }
    return nullptr;
  }

 private:
  blink::HashMap<blink::String, blink::FontDescription> fonts_resolved_;
  blink::LinkedHashSet<blink::String> font_lru_list_;
};

OffscreenFontCache& GetOffscreenFontCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<OffscreenFontCache>,
                                  thread_specific_pool, ());
  return *thread_specific_pool;
}

}  // namespace

CanvasRenderingContext* OffscreenCanvasRenderingContext2D::Factory::Create(
    ExecutionContext*,
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  DCHECK(host->IsOffscreenCanvas());
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<OffscreenCanvasRenderingContext2D>(
          static_cast<OffscreenCanvas*>(host), attrs);
  DCHECK(rendering_context);
  return rendering_context;
}

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() {
  FlushForImageListener::Get()->RemoveObserver(this);
}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : BaseRenderingContext2D(canvas,
                             attrs,
                             canvas->GetTopExecutionContext()->GetTaskRunner(
                                 TaskType::kInternalDefault)) {
  FlushForImageListener::Get()->AddObserver(this);
  ExecutionContext* execution_context = canvas->GetTopExecutionContext();
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (window->GetFrame() && window->GetFrame()->GetSettings() &&
        window->GetFrame()->GetSettings()->GetDisableReadingFromCanvas())
      canvas->SetDisableReadingFromCanvasTrue();
    return;
  }
  WorkerSettings* worker_settings =
      To<WorkerGlobalScope>(execution_context)->GetWorkerSettings();
  if (worker_settings && worker_settings->DisableReadingFromCanvas())
    canvas->SetDisableReadingFromCanvasTrue();
}

void OffscreenCanvasRenderingContext2D::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
}

void OffscreenCanvasRenderingContext2D::FinalizeFrame(FlushReason reason) {
  TRACE_EVENT0("blink", "OffscreenCanvasRenderingContext2D::FinalizeFrame");

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!InitializeResourceProvider()) {
    return;
  }

  FlushCanvas(reason);
  Host()->NotifyCachesOfSwitchingFrame();
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::OriginClean() const {
  return Host()->OriginClean();
}

void OffscreenCanvasRenderingContext2D::SetOriginTainted() {
  Host()->SetOriginTainted();
}

int OffscreenCanvasRenderingContext2D::Width() const {
  return Host()->Size().width();
}

int OffscreenCanvasRenderingContext2D::Height() const {
  return Host()->Size().height();
}

bool OffscreenCanvasRenderingContext2D::CanCreateResourceProvider() {
  const CanvasRenderingContextHost* const host = Host();
  if (host == nullptr || host->Size().IsEmpty()) [[unlikely]] {
    return false;
  }
  return InitializeResourceProvider();
}

bool OffscreenCanvasRenderingContext2D::Is2DCanvasAccelerated() const {
  if (shared_image_provider_) {
    return shared_image_provider_->IsAccelerated();
  }
  if (bitmap_provider_) {
    return false;
  }
  if (!Host()) {
    return false;
  }
  return Host()->ShouldTryToUseGpuRaster();
}

bool OffscreenCanvasRenderingContext2D::InitializeResourceProvider() {
  DCHECK(Host() && Host()->IsOffscreenCanvas());
  OffscreenCanvas* host = HostAsOffscreenCanvas();
  if (host == nullptr) [[unlikely]] {
    return false;
  }
  if (isContextLost() && !IsContextBeingRestored()) {
    return false;
  }

  if (shared_image_provider_) {
    if (!shared_image_provider_->IsValid()) {
      // The canvas context is not lost but the provider is invalid. This
      // happens if the GPU process dies in the middle of a render task. The
      // canvas is notified of GPU context losses via the `NotifyGpuContextLost`
      // callback and restoration happens in `TryRestoreContextEvent`. Both
      // callbacks are executed in their own separate task. If the GPU context
      // goes invalid in the middle of a render task, the canvas won't
      // immediately know about it and canvas APIs will continue using the
      // provider that is now invalid. We can early return here, trying to
      // re-create the provider right away would just fail. We need to let
      // `TryRestoreContextEvent` wait for the GPU process to up again.
      return false;
    }
    return true;
  }
  if (bitmap_provider_) {
    if (!bitmap_provider_->IsValid()) {
      return false;
    }
    return true;
  }

  if (!host->IsValidImageSize() && !host->Size().IsEmpty()) {
    LoseContext(CanvasRenderingContext::kInvalidCanvasSize);
    return false;
  }

  gfx::Size surface_size(host->width(), host->height());
  const bool use_gpu_raster =
      SharedGpuContext::IsGpuCompositingEnabled() &&
      RuntimeEnabledFeatures::Accelerated2dCanvasEnabled() &&
      !(CreationAttributes().will_read_frequently ==
        CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);

  // TODO(crbug.com/479561824): The computation of whether it's possible to use
  // a SharedImage provider with software raster here is not correct.
  // Using a SharedImage provider with software raster and GPU compositing
  // requires that the SharedImage can be mapped into software for raster and
  // read out into GPU memory for display. This is true if native mappable
  // buffers are supported (e.g., IOSurface), but is not generically true on all
  // platforms. CanvasRenderingContext2D handles this by using a SharedImage
  // provider with SW raster/GPU compositing *only if* native mappable buffers
  // are provided. However, in that case the fallback usage of a bitmap provider
  // is still able to display to the screen, which is not the case here. We need
  // to determine a proper fix for this use case.
  const bool use_shared_image =
      use_gpu_raster || (host->HasPlaceholderCanvas() &&
                         SharedGpuContext::IsGpuCompositingEnabled());
  const SkAlphaType alpha_type = color_params_.GetAlphaType();
  const viz::SharedImageFormat format = color_params_.GetSharedImageFormat();
  const gfx::ColorSpace color_space = color_params_.GetGfxColorSpace();
  const gfx::HDRMetadata hdr_metadata = color_params_.GetGfxHdrMetadata();
  if (use_shared_image) {
    gpu::SharedImageUsageSet shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    if (host->HasPlaceholderCanvas() && UseOverlaysForCanvas2D()) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }

    shared_image_provider_ = Canvas2DResourceProvider::CreateWithClear(
        host->Size(), format, alpha_type, color_space, hdr_metadata,
        SharedGpuContext::ContextProviderWrapper(),
        use_gpu_raster ? RasterMode::kGPU : RasterMode::kCPU,
        shared_image_usage_flags, host);
  } else if (host->HasPlaceholderCanvas()) {
    // using the software compositor
    host->GetOrCreateResourceDispatcher();
    shared_image_provider_ =
        Canvas2DResourceProvider::CreateWithClearForSoftwareCompositor(
            host->Size(), format, alpha_type, color_space, hdr_metadata,
            SharedGpuContext::SharedImageInterfaceProvider(), host);
  }

  if (!shared_image_provider_) {
    // Last resort fallback is to use the bitmap provider. Using this
    // path is normal for software-rendered OffscreenCanvases that have no
    // placeholder canvas. If there is a placeholder, its content will not be
    // visible on screen, but at least readbacks will work. Failure to create
    // another type of resource prover above is a sign that the graphics
    // pipeline is in a bad state (e.g. gpu process crashed, out of memory)
    bitmap_provider_ = Canvas2DBitmapProvider::CreateWithClear(
        host->Size(), format, alpha_type, color_space, hdr_metadata, host);
  }

  Host()->UpdateMemoryUsage();

  if (shared_image_provider_) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              shared_image_provider_->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  CanvasResourceProviderType::kSharedImage);
    host->DidDraw();
    return true;
  }
  if (bitmap_provider_) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              false);
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  CanvasResourceProviderType::kBitmap);

    host->DidDraw();
    return true;
  }
  return false;
}

base::ByteSize OffscreenCanvasRenderingContext2D::AllocatedBufferSize() const {
  if (shared_image_provider_) {
    return shared_image_provider_->EstimatedSizeInBytes();
  }
  if (bitmap_provider_) {
    return bitmap_provider_->EstimatedSizeInBytes();
  }
  return base::ByteSize();
}

void OffscreenCanvasRenderingContext2D::Reset() {
  shared_image_provider_ = nullptr;
  bitmap_provider_ = nullptr;
  Host()->DiscardResources();
  BaseRenderingContext2D::ResetInternal();
}

scoped_refptr<CanvasResource>
OffscreenCanvasRenderingContext2D::ProduceCanvasResource(FlushReason reason) {
  if (!InitializeResourceProvider() || !shared_image_provider_) {
    return nullptr;
  }

  FlushCanvas(reason);
  scoped_refptr<CanvasResource> frame =
      shared_image_provider_->ProduceCanvasResource();
  if (!frame)
    return nullptr;

  frame->SetOriginClean(OriginClean());
  return frame;
}

scoped_refptr<CanvasResource>
OffscreenCanvasRenderingContext2D::GetResourceForPushFrame(
    bool& should_call_push_frame) {
  should_call_push_frame = true;
  FinalizeFrame(FlushReason::kOther);
  scoped_refptr<CanvasResource> resource =
      ProduceCanvasResource(FlushReason::kOther);
  GetOffscreenFontCache().PruneLocalFontCache(kMaxCachedFonts);
  return resource;
}

CanvasRenderingContextHost*
OffscreenCanvasRenderingContext2D::GetCanvasRenderingContextHost() const {
  return Host();
}
ExecutionContext* OffscreenCanvasRenderingContext2D::GetTopExecutionContext()
    const {
  return Host()->GetTopExecutionContext();
}

ImageBitmap* OffscreenCanvasRenderingContext2D::TransferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  WebFeature feature = WebFeature::kOffscreenCanvasTransferToImageBitmap2D;
  UseCounter::Count(ExecutionContext::From(script_state), feature);

  if (layer_count_ != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`transferToImageBitmap()` cannot be called while layers are opened.");
    return nullptr;
  }

  if (!InitializeResourceProvider()) {
    return nullptr;
  }
  scoped_refptr<StaticBitmapImage> image = GetImage();
  if (!image)
    return nullptr;
  image->SetOriginClean(OriginClean());

  shared_image_provider_ = nullptr;
  bitmap_provider_ = nullptr;
  Host()->DiscardResources();

  return MakeGarbageCollected<ImageBitmap>(std::move(image));
}

scoped_refptr<StaticBitmapImage> OffscreenCanvasRenderingContext2D::GetImage() {
  FinalizeFrame(FlushReason::kOther);
  if (!IsPaintable())
    return nullptr;
  if (shared_image_provider_) {
    return shared_image_provider_->Snapshot();
  }
  return bitmap_provider_->Snapshot();
}

scoped_refptr<StaticBitmapImage>
OffscreenCanvasRenderingContext2D::PaintRenderingResultsToSnapshot(
    SourceDrawingBuffer source_buffer) {
  if (!IsResourceProviderValid()) {
    return nullptr;
  }
  FlushCanvas(FlushReason::kOther);
  if (shared_image_provider_) {
    return shared_image_provider_->Snapshot();
  }
  return bitmap_provider_->Snapshot();
}

V8RenderingContext* OffscreenCanvasRenderingContext2D::AsV8RenderingContext() {
  return nullptr;
}

V8OffscreenRenderingContext*
OffscreenCanvasRenderingContext2D::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

Color OffscreenCanvasRenderingContext2D::GetCurrentColor() const {
  return Color::kBlack;
}

MemoryManagedPaintCanvas*
OffscreenCanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (isContextLost() || !InitializeResourceProvider()) [[unlikely]] {
    return nullptr;
  }
  return GetPaintCanvas();
}

const MemoryManagedPaintCanvas*
OffscreenCanvasRenderingContext2D::GetPaintCanvas() const {
  if (isContextLost()) [[unlikely]] {
    return nullptr;
  }
  auto* recorder = Recorder();
  return recorder ? &recorder->getRecordingCanvas() : nullptr;
}

const MemoryManagedPaintRecorder* OffscreenCanvasRenderingContext2D::Recorder()
    const {
  if (shared_image_provider_) {
    return &shared_image_provider_->Recorder();
  }
  if (bitmap_provider_) {
    return &bitmap_provider_->Recorder();
  }
  return nullptr;
}

void OffscreenCanvasRenderingContext2D::WillDraw(
    const gfx::Rect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  gfx::Rect adjusted_dirty_rect = dirty_rect;
  if (GetState().ShouldAntialias()) {
    adjusted_dirty_rect.Outset(1);
  }

  GetCanvasPerformanceMonitor().DidDraw(draw_type);
  Host()->DidDraw(adjusted_dirty_rect);

  if (layer_count_ == 0) [[likely]] {
    // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
    FlushIfRecordingLimitExceeded();
  }
}

void OffscreenCanvasRenderingContext2D::FlushIfRecordingLimitExceeded() {
  if (shared_image_provider_) {
    if (shared_image_provider_->IsPrinting() &&
        shared_image_provider_->clear_frame()) {
      return;
    }
    const MemoryManagedPaintRecorder* recorder = Recorder();
    CHECK(recorder);
    if (recorder->ReleasableOpBytesUsed() >
            shared_image_provider_->max_recorded_op_bytes() ||
        recorder->ReleasableImageBytesUsed() >
            shared_image_provider_->max_pinned_image_bytes()) [[unlikely]] {
      FlushCanvas(FlushReason::kOther);
    }
  } else if (bitmap_provider_) {
    if (bitmap_provider_->IsPrinting() && bitmap_provider_->clear_frame()) {
      return;
    }
    const MemoryManagedPaintRecorder* recorder = Recorder();
    CHECK(recorder);
    if (recorder->ReleasableOpBytesUsed() >
            bitmap_provider_->max_recorded_op_bytes() ||
        recorder->ReleasableImageBytesUsed() >
            bitmap_provider_->max_pinned_image_bytes()) [[unlikely]] {
      FlushCanvas(FlushReason::kOther);
    }
  }
}

sk_sp<PaintFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(Host()->Size(), this);
}

void OffscreenCanvasRenderingContext2D::Dispose() {
  shared_image_provider_.reset();
  bitmap_provider_.reset();
  CanvasRenderingContext::Dispose();
}

void OffscreenCanvasRenderingContext2D::LoseContext(LostContextMode lost_mode) {
  if (context_lost_mode_ != kNotLostContext)
    return;
  context_lost_mode_ = lost_mode;
  ResetInternal();
  if (CanvasRenderingContextHost* host = Host()) [[likely]] {
    shared_image_provider_ = nullptr;
    bitmap_provider_ = nullptr;
    host->DiscardResources();
    host->DiscardResourceDispatcher();
  }
  uint32_t delay = base::RandIntInclusive(1, kMaxIframeContextLoseDelay);
  dispatch_context_lost_event_timer_.StartOneShot(base::Milliseconds(delay),
                                                  FROM_HERE);
}

bool OffscreenCanvasRenderingContext2D::IsPaintable() const {
  return shared_image_provider_ != nullptr || bitmap_provider_ != nullptr;
}

bool OffscreenCanvasRenderingContext2D::WritePixels(
    const SkImageInfo& orig_info,
    const void* pixels,
    size_t row_bytes,
    int x,
    int y) {
  if (!IsResourceProviderValid()) {
    return false;
  }
  FlushCanvas(FlushReason::kOther);
  if (!IsResourceProviderValid()) {
    return false;
  }
  if (shared_image_provider_) {
    return shared_image_provider_->WritePixels(orig_info, pixels, row_bytes, x,
                                               y);
  }
  return bitmap_provider_->WritePixels(orig_info, pixels, row_bytes, x, y);
}

bool OffscreenCanvasRenderingContext2D::ResolveFont(const String& new_font) {
  OffscreenFontCache& font_cache = GetOffscreenFontCache();
  FontDescription* cached_font = font_cache.GetFont(new_font);
  CanvasRenderingContextHost* const host = Host();
  const LayoutLocale* locale = LocaleFromLang();

  if (cached_font) {
    if (locale != cached_font->Locale()) {
      cached_font->SetLocale(locale);
    }
    GetState().SetFont(*cached_font, host->GetFontSelector());
  } else {
    auto* style =
        CSSParser::ParseFont(new_font, host->GetTopExecutionContext());
    if (!style) {
      return false;
    }
    std::optional<FontDescription> maybe_desc = FontStyleResolver::ComputeFont(
        *style, host->GetFontSelector()->BaseFontSelector());
    if (!maybe_desc.has_value()) {
      return false;
    }
    FontDescription desc = maybe_desc.value();
    desc.SetLocale(locale);
    font_cache.AddFont(new_font, desc);
    GetState().SetFont(desc, host->GetFontSelector());
  }
  return true;
}

std::optional<cc::PaintRecord> OffscreenCanvasRenderingContext2D::FlushCanvas(
    FlushReason reason) {
  if (shared_image_provider_) {
    return shared_image_provider_->Flush(reason);
  }
  if (bitmap_provider_) {
    return bitmap_provider_->Flush(reason);
  }
  return std::nullopt;
}

void OffscreenCanvasRenderingContext2D::OnFlushForImage(
    cc::PaintImage::ContentId content_id) {
  if (shared_image_provider_ && !shared_image_provider_->IsSoftware()) {
    if (shared_image_provider_->Recorder().getRecordingCanvas().IsCachingImage(
            content_id)) {
      FlushCanvas(FlushReason::kOther);
    }
    shared_image_provider_->OnFlushForImage(content_id);
  }
}

bool OffscreenCanvasRenderingContext2D::IsResourceProviderValid() const {
  if (shared_image_provider_) {
    return shared_image_provider_->IsValid();
  }
  if (bitmap_provider_) {
    return bitmap_provider_->IsValid();
  }
  return false;
}

OffscreenCanvas* OffscreenCanvasRenderingContext2D::HostAsOffscreenCanvas()
    const {
  return static_cast<OffscreenCanvas*>(Host());
}

UniqueFontSelector* OffscreenCanvasRenderingContext2D::GetFontSelector() const {
  return Host()->GetFontSelector();
}

}  // namespace blink
