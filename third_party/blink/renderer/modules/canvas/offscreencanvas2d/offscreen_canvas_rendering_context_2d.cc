// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

#include "base/metrics/histogram_functions.h"
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
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() =
    default;

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    OffscreenCanvas* canvas,
    const CanvasContextCreationAttributesCore& attrs)
    : BaseRenderingContext2D(canvas,
                             attrs,
                             canvas->GetTopExecutionContext()->GetTaskRunner(
                                 TaskType::kInternalDefault)) {
  is_valid_size_ = Host()->IsValidImageSize();

  ExecutionContext* execution_context = canvas->GetTopExecutionContext();
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (window->GetFrame() && window->GetFrame()->GetSettings() &&
        window->GetFrame()->GetSettings()->GetDisableReadingFromCanvas())
      canvas->SetDisableReadingFromCanvasTrue();
    return;
  }
  dirty_rect_for_commit_.setEmpty();
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
  if (!GetOrCreateResourceProvider()) {
    return;
  }
  resource_provider_->FlushCanvas(reason);
  if (RuntimeEnabledFeatures::CanvasTextSwitchFrameOnFinalizeEnabled()) {
    Host()->NotifyCachesOfSwitchingFrame();
  }
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
  return !!GetOrCreateResourceProvider();
}

CanvasResourceProvider*
OffscreenCanvasRenderingContext2D::GetOrCreateResourceProvider() {
  DCHECK(Host() && Host()->IsOffscreenCanvas());
  OffscreenCanvas* host = HostAsOffscreenCanvas();
  if (host == nullptr) [[unlikely]] {
    return nullptr;
  }
  if (isContextLost() && !IsContextBeingRestored()) {
    return nullptr;
  }

  if (resource_provider_) {
    if (!resource_provider_->IsValid()) {
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
      return nullptr;
    }
    return resource_provider_.get();
  }

  if (!host->IsValidImageSize() && !host->Size().IsEmpty()) {
    LoseContext(CanvasRenderingContext::kInvalidCanvasSize);
    return nullptr;
  }

  std::unique_ptr<CanvasResourceProvider> provider;
  gfx::Size surface_size(host->width(), host->height());
  const bool can_use_gpu =
      SharedGpuContext::IsGpuCompositingEnabled() &&
      RuntimeEnabledFeatures::Accelerated2dCanvasEnabled() &&
      !(CreationAttributes().will_read_frequently ==
        CanvasContextCreationAttributesCore::WillReadFrequently::kTrue);
  const bool use_shared_image =
      can_use_gpu || (host->HasPlaceholderCanvas() &&
                      SharedGpuContext::IsGpuCompositingEnabled());
  const bool use_scanout =
      use_shared_image && host->HasPlaceholderCanvas() &&
      SharedGpuContext::MaySupportImageChromium() &&
      RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled();

  gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  if (use_scanout) {
    shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  const SkAlphaType alpha_type = GetAlphaType();
  const viz::SharedImageFormat format = GetSharedImageFormat();
  const gfx::ColorSpace color_space = GetColorSpace();
  if (use_shared_image) {
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        host->Size(), format, alpha_type, color_space,
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        SharedGpuContext::ContextProviderWrapper(),
        can_use_gpu ? RasterMode::kGPU : RasterMode::kCPU,
        shared_image_usage_flags, host);
  } else if (host->HasPlaceholderCanvas()) {
    // using the software compositor
    base::WeakPtr<CanvasResourceDispatcher> dispatcher_weakptr =
        host->GetOrCreateResourceDispatcher()->GetWeakPtr();
    provider =
        CanvasResourceProvider::CreateSharedImageProviderForSoftwareCompositor(
            host->Size(), format, alpha_type, color_space,
            CanvasResourceProvider::ShouldInitialize::kCallClear,
            SharedGpuContext::SharedImageInterfaceProvider(), host);
  }

  if (!provider) {
    // Last resort fallback is to use the bitmap provider. Using this
    // path is normal for software-rendered OffscreenCanvases that have no
    // placeholder canvas. If there is a placeholder, its content will not be
    // visible on screen, but at least readbacks will work. Failure to create
    // another type of resource prover above is a sign that the graphics
    // pipeline is in a bad state (e.g. gpu process crashed, out of memory)
    provider = Canvas2DResourceProviderBitmap::Create(
        host->Size(), format, alpha_type, color_space,
        CanvasResourceProvider::ShouldInitialize::kCallClear, host);
  }

  resource_provider_ = std::move(provider);
  Host()->UpdateMemoryUsage();

  if (resource_provider_ && resource_provider_->IsValid()) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              resource_provider_->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  resource_provider_->GetType());
    host->DidDraw();
  }
  return resource_provider_.get();
}

std::unique_ptr<CanvasResourceProvider>
OffscreenCanvasRenderingContext2D::ReplaceResourceProvider(
    std::unique_ptr<CanvasResourceProvider> provider) {
  std::unique_ptr<CanvasResourceProvider> old_resource_provider =
      std::move(resource_provider_);
  resource_provider_ = std::move(provider);
  Host()->UpdateMemoryUsage();
  if (old_resource_provider) {
    old_resource_provider->SetDelegate(nullptr);
  }
  return old_resource_provider;
}

CanvasResourceProvider* OffscreenCanvasRenderingContext2D::GetResourceProvider()
    const {
  return resource_provider_.get();
}

void OffscreenCanvasRenderingContext2D::Reset() {
  resource_provider_ = nullptr;
  Host()->DiscardResources();
  BaseRenderingContext2D::ResetInternal();
  // Because the host may have changed to a zero size
  is_valid_size_ = Host()->IsValidImageSize();
  // We must resize the damage rect to avoid a potentially larger damage than
  // actual canvas size. See: crbug.com/1227165
  dirty_rect_for_commit_ = SkIRect::MakeWH(Width(), Height());
}

scoped_refptr<CanvasResource>
OffscreenCanvasRenderingContext2D::ProduceCanvasResource(FlushReason reason) {
  CanvasResourceProvider* provider = GetOrCreateResourceProvider();
  if (!provider) {
    return nullptr;
  }

  // Only CRPSI can produce CanvasResources.
  CanvasResourceProviderSharedImage* si_provider =
      provider->AsSharedImageProvider();
  if (!si_provider) {
    return nullptr;
  }

  scoped_refptr<CanvasResource> frame =
      si_provider->ProduceCanvasResource(reason);
  if (!frame)
    return nullptr;

  frame->SetOriginClean(OriginClean());
  return frame;
}

bool OffscreenCanvasRenderingContext2D::PushFrame() {
  if (dirty_rect_for_commit_.isEmpty())
    return false;

  SkIRect damage_rect(dirty_rect_for_commit_);
  FinalizeFrame(FlushReason::kOther);
  bool ret = Host()->PushFrame(ProduceCanvasResource(FlushReason::kOther),
                               damage_rect);
  dirty_rect_for_commit_.setEmpty();
  GetOffscreenFontCache().PruneLocalFontCache(kMaxCachedFonts);
  return ret;
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

  if (!GetOrCreateResourceProvider()) {
    return nullptr;
  }
  scoped_refptr<StaticBitmapImage> image = GetImage();
  if (!image)
    return nullptr;
  image->SetOriginClean(OriginClean());

  resource_provider_ = nullptr;
  Host()->DiscardResources();

  return MakeGarbageCollected<ImageBitmap>(std::move(image));
}

scoped_refptr<StaticBitmapImage> OffscreenCanvasRenderingContext2D::GetImage() {
  FinalizeFrame(FlushReason::kOther);
  if (!IsPaintable())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image = resource_provider_->Snapshot();

  return image;
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
  if (!is_valid_size_ || isContextLost() || !GetOrCreateResourceProvider())
      [[unlikely]] {
    return nullptr;
  }
  return GetPaintCanvas();
}

const MemoryManagedPaintCanvas*
OffscreenCanvasRenderingContext2D::GetPaintCanvas() const {
  if (!is_valid_size_ || isContextLost()) [[unlikely]] {
    return nullptr;
  }
  return resource_provider_ ? &resource_provider_->Canvas() : nullptr;
}

const MemoryManagedPaintRecorder* OffscreenCanvasRenderingContext2D::Recorder()
    const {
  return resource_provider_ ? &resource_provider_->Recorder() : nullptr;
}

void OffscreenCanvasRenderingContext2D::WillDraw(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  dirty_rect_for_commit_.join(dirty_rect);
  GetCanvasPerformanceMonitor().DidDraw(draw_type);
  if (GetState().ShouldAntialias()) {
    SkIRect inflated_dirty_rect = dirty_rect_for_commit_.makeOutset(1, 1);
    Host()->DidDraw(inflated_dirty_rect);
  } else {
    Host()->DidDraw(dirty_rect_for_commit_);
  }
  if (layer_count_ == 0 && resource_provider_ != nullptr) [[likely]] {
    // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
    resource_provider_->FlushIfRecordingLimitExceeded();
  }
}

sk_sp<PaintFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(Host()->Size(), this);
}

void OffscreenCanvasRenderingContext2D::Dispose() {
  resource_provider_.reset();
  CanvasRenderingContext::Dispose();
}

void OffscreenCanvasRenderingContext2D::LoseContext(LostContextMode lost_mode) {
  if (context_lost_mode_ != kNotLostContext)
    return;
  context_lost_mode_ = lost_mode;
  ResetInternal();
  if (CanvasRenderingContextHost* host = Host()) [[likely]] {
    resource_provider_ = nullptr;
    host->DiscardResources();
    host->DiscardResourceDispatcher();
  }
  uint32_t delay = base::RandInt(1, kMaxIframeContextLoseDelay);
  dispatch_context_lost_event_timer_.StartOneShot(base::Milliseconds(delay),
                                                  FROM_HERE);
}

bool OffscreenCanvasRenderingContext2D::IsPaintable() const {
  return !!resource_provider_;
}

bool OffscreenCanvasRenderingContext2D::WritePixels(
    const SkImageInfo& orig_info,
    const void* pixels,
    size_t row_bytes,
    int x,
    int y) {
  if (!resource_provider_ || !resource_provider_->IsValid()) {
    return false;
  }

  resource_provider_->FlushCanvas();

  // Short-circuit out if an error occurred while flushing the recording.
  if (!resource_provider_->IsValid()) {
    return false;
  }

  return resource_provider_->WritePixels(orig_info, pixels, row_bytes, x, y);
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
    FontDescription desc = FontStyleResolver::ComputeFont(
        *style, host->GetFontSelector()->BaseFontSelector());
    desc.SetLocale(locale);
    font_cache.AddFont(new_font, desc);
    GetState().SetFont(desc, host->GetFontSelector());
  }
  return true;
}

std::optional<cc::PaintRecord> OffscreenCanvasRenderingContext2D::FlushCanvas(
    FlushReason reason) {
  return resource_provider_ ? resource_provider_->FlushCanvas(reason)
                            : std::nullopt;
}

OffscreenCanvas* OffscreenCanvasRenderingContext2D::HostAsOffscreenCanvas()
    const {
  return static_cast<OffscreenCanvas*>(Host());
}

UniqueFontSelector* OffscreenCanvasRenderingContext2D::GetFontSelector() const {
  return Host()->GetFontSelector();
}

}  // namespace blink
