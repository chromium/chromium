// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_stretch.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_text_rendering.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

  void AddFont(String name, blink::FontDescription font) {
    fonts_resolved_.insert(name, font);
    auto add_result = font_lru_list_.PrependOrMoveToFirst(name);
    DCHECK(add_result.is_new_entry);
    PruneLocalFontCache(kHardMaxCachedFonts);
  }

  blink::FontDescription* GetFont(String name) {
    auto i = fonts_resolved_.find(name);
    if (i != fonts_resolved_.end()) {
      auto add_result = font_lru_list_.PrependOrMoveToFirst(name);
      DCHECK(!add_result.is_new_entry);
      return &(i->value);
    }
    return nullptr;
  }

 private:
  HashMap<String, blink::FontDescription> fonts_resolved_;
  LinkedHashSet<String> font_lru_list_;
};

OffscreenFontCache& GetOffscreenFontCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<OffscreenFontCache>,
                                  thread_specific_pool, ());
  return *thread_specific_pool;
}

}  // namespace

namespace blink {

CanvasRenderingContext* OffscreenCanvasRenderingContext2D::Factory::Create(
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
    : CanvasRenderingContext(canvas, attrs, CanvasRenderingAPI::k2D),
      BaseRenderingContext2D(canvas->GetTopExecutionContext()->GetTaskRunner(
          TaskType::kInternalDefault)),
      color_params_(attrs.color_space, attrs.pixel_format, attrs.alpha) {
  identifiability_study_helper_.SetExecutionContext(
      canvas->GetTopExecutionContext());
  is_valid_size_ = IsValidImageSize(Host()->Size());

  // Clear the background transparent or opaque.
  if (IsCanvas2DBufferValid())
    DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

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
  CanvasRenderingContext::Trace(visitor);
  BaseRenderingContext2D::Trace(visitor);
}

void OffscreenCanvasRenderingContext2D::commit() {
  // TODO(fserb): consolidate this with PushFrame
  SkIRect damage_rect(dirty_rect_for_commit_);
  dirty_rect_for_commit_.setEmpty();
  FinalizeFrame(FlushReason::kOffscreenCanvasCommit);
  Host()->Commit(ProduceCanvasResource(FlushReason::kOffscreenCanvasCommit),
                 damage_rect);
  GetOffscreenFontCache().PruneLocalFontCache(kMaxCachedFonts);
}

void OffscreenCanvasRenderingContext2D::FlushRecording(FlushReason reason) {
  if (!GetCanvasResourceProvider() ||
      !GetCanvasResourceProvider()->HasRecordedDrawOps())
    return;

  GetCanvasResourceProvider()->FlushCanvas(reason);
  GetCanvasResourceProvider()->ReleaseLockedImages();
}

void OffscreenCanvasRenderingContext2D::FinalizeFrame(FlushReason reason) {
  TRACE_EVENT0("blink", "OffscreenCanvasRenderingContext2D::FinalizeFrame");

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!GetOrCreateCanvasResourceProvider())
    return;
  FlushRecording(reason);
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

bool OffscreenCanvasRenderingContext2D::CanCreateCanvas2dResourceProvider()
    const {
  if (!Host() || Host()->Size().IsEmpty())
    return false;
  return !!GetOrCreateCanvasResourceProvider();
}

CanvasResourceProvider*
OffscreenCanvasRenderingContext2D::GetOrCreateCanvasResourceProvider() const {
  DCHECK(Host() && Host()->IsOffscreenCanvas());
  return static_cast<OffscreenCanvas*>(Host())->GetOrCreateResourceProvider();
}

CanvasResourceProvider*
OffscreenCanvasRenderingContext2D::GetCanvasResourceProvider() const {
  return Host()->ResourceProvider();
}

void OffscreenCanvasRenderingContext2D::Reset() {
  Host()->DiscardResourceProvider();
  BaseRenderingContext2D::ResetInternal();
  // Because the host may have changed to a zero size
  is_valid_size_ = IsValidImageSize(Host()->Size());
  // We must resize the damage rect to avoid a potentially larger damage than
  // actual canvas size. See: crbug.com/1227165
  dirty_rect_for_commit_ = SkIRect::MakeWH(Width(), Height());
}

scoped_refptr<CanvasResource>
OffscreenCanvasRenderingContext2D::ProduceCanvasResource(FlushReason reason) {
  if (!GetOrCreateCanvasResourceProvider())
    return nullptr;
  scoped_refptr<CanvasResource> frame =
      GetCanvasResourceProvider()->ProduceCanvasResource(reason);
  if (!frame)
    return nullptr;

  frame->SetOriginClean(OriginClean());
  return frame;
}

bool OffscreenCanvasRenderingContext2D::PushFrame() {
  if (dirty_rect_for_commit_.isEmpty())
    return false;

  SkIRect damage_rect(dirty_rect_for_commit_);
  FinalizeFrame(FlushReason::kOffscreenCanvasPushFrame);
  bool ret = Host()->PushFrame(
      ProduceCanvasResource(FlushReason::kOffscreenCanvasPushFrame),
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

  if (!GetOrCreateCanvasResourceProvider())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image = GetImage(FlushReason::kTransfer);
  if (!image)
    return nullptr;
  image->SetOriginClean(OriginClean());
  // Before discarding the image resource, we need to flush pending render ops
  // to fully resolve the snapshot.
  image->PaintImageForCurrentFrame().FlushPendingSkiaOps();

  Host()->DiscardResourceProvider();

  return MakeGarbageCollected<ImageBitmap>(std::move(image));
}

scoped_refptr<StaticBitmapImage> OffscreenCanvasRenderingContext2D::GetImage(
    FlushReason reason) {
  FinalizeFrame(reason);
  if (!IsPaintable())
    return nullptr;
  scoped_refptr<StaticBitmapImage> image =
      GetCanvasResourceProvider()->Snapshot(reason);

  return image;
}

NoAllocDirectCallHost*
OffscreenCanvasRenderingContext2D::AsNoAllocDirectCallHost() {
  return this;
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

cc::PaintCanvas* OffscreenCanvasRenderingContext2D::GetOrCreatePaintCanvas() {
  if (UNLIKELY(!is_valid_size_ || isContextLost() ||
               !GetOrCreateCanvasResourceProvider())) {
    return nullptr;
  }
  return GetPaintCanvas();
}

cc::PaintCanvas* OffscreenCanvasRenderingContext2D::GetPaintCanvas() {
  if (UNLIKELY(!is_valid_size_ || isContextLost() ||
               !GetCanvasResourceProvider())) {
    return nullptr;
  }
  return GetCanvasResourceProvider()->Canvas();
}

void OffscreenCanvasRenderingContext2D::WillDraw(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  // Call sites should ensure GetPaintCanvas() returns non-null before calling
  // this.
  DCHECK(GetPaintCanvas());
  dirty_rect_for_commit_.join(dirty_rect);
  GetCanvasPerformanceMonitor().DidDraw(draw_type);
  Host()->DidDraw(dirty_rect_for_commit_);
  if (!layer_count_) {
    // TODO(crbug.com/1246486): Make auto-flushing layer friendly.
    GetCanvasResourceProvider()->FlushIfRecordingLimitExceeded();
  }
}

sk_sp<PaintFilter> OffscreenCanvasRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(Host()->Size(), this);
}

void OffscreenCanvasRenderingContext2D::LoseContext(LostContextMode lost_mode) {
  if (context_lost_mode_ != kNotLostContext)
    return;
  context_lost_mode_ = lost_mode;
  if (context_lost_mode_ == kSyntheticLostContext && Host()) {
    Host()->DiscardResourceProvider();
  }
  uint32_t delay = base::RandInt(1, kMaxIframeContextLoseDelay);
  dispatch_context_lost_event_timer_.StartOneShot(base::Milliseconds(delay),
                                                  FROM_HERE);
}

bool OffscreenCanvasRenderingContext2D::IsPaintable() const {
  return Host()->ResourceProvider();
}

bool OffscreenCanvasRenderingContext2D::WritePixels(
    const SkImageInfo& orig_info,
    const void* pixels,
    size_t row_bytes,
    int x,
    int y) {
  if (!GetOrCreateCanvasResourceProvider())
    return false;

  DCHECK(IsPaintable());
  FinalizeFrame(FlushReason::kWritePixels);

  return offscreenCanvasForBinding()->ResourceProvider()->WritePixels(
      orig_info, pixels, row_bytes, x, y);
}

void OffscreenCanvasRenderingContext2D::WillOverwriteCanvas() {
  GetCanvasResourceProvider()->SkipQueuedDrawCommands();
}

bool OffscreenCanvasRenderingContext2D::ResolveFont(const String& new_font) {
  OffscreenFontCache& font_cache = GetOffscreenFontCache();
  FontDescription* cached_font = font_cache.GetFont(new_font);
  if (cached_font) {
    GetState().SetFont(*cached_font, Host()->GetFontSelector());
  } else {
    auto* style =
        CSSParser::ParseFont(new_font, Host()->GetTopExecutionContext());
    if (!style) {
      return false;
    }

    FontDescription desc =
        FontStyleResolver::ComputeFont(*style, Host()->GetFontSelector());

    font_cache.AddFont(new_font, desc);
    GetState().SetFont(desc, Host()->GetFontSelector());
  }
  return true;
}

bool OffscreenCanvasRenderingContext2D::IsCanvas2DBufferValid() const {
  if (IsPaintable())
    return GetCanvasResourceProvider()->IsValid();
  return false;
}

void OffscreenCanvasRenderingContext2D::DispatchContextLostEvent(
    TimerBase* time) {
  PostDeferrableAction(WTF::BindOnce(
      [](BaseRenderingContext2D* context) { context->ResetInternal(); },
      WrapPersistent(this)));
  BaseRenderingContext2D::DispatchContextLostEvent(time);
}

void OffscreenCanvasRenderingContext2D::TryRestoreContextEvent(
    TimerBase* timer) {
  if (context_lost_mode_ == kNotLostContext) {
    // Canvas was already restored (possibly thanks to a resize), so stop
    // trying.
    try_restore_context_event_timer_.Stop();
    return;
  }

  DCHECK(context_lost_mode_ != kWebGLLoseContextLostContext);

  // If lost mode is |kSyntheticLostContext| and |context_restorable_| is set to
  // true, it means context is forced to be lost for testing purpose. Restore
  // the context.
  if (context_lost_mode_ == kSyntheticLostContext &&
      GetOrCreateCanvasResourceProvider() &&
      GetCanvasResourceProvider()->Canvas()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
    return;
  }

  // If lost mode is |kRealLostContext|, it means the context was not lost due
  // to surface failure but rather due to a an eviction, which means image
  // buffer exists.
  if (context_lost_mode_ == kRealLostContext &&
      GetOrCreateCanvasResourceProvider() &&
      GetCanvasResourceProvider()->Canvas()) {
    try_restore_context_event_timer_.Stop();
    DispatchContextRestoredEvent(nullptr);
    return;
  }

  // It gets here if lost mode is |kRealLostContext| and it fails to create a
  // new PaintCanvas. Discard the old resource and allocating a new one here.
  if (++try_restore_context_attempt_count_ > kMaxTryRestoreContextAttempts) {
    if (Host()) {
      Host()->DiscardResourceProvider();
    }
    try_restore_context_event_timer_.Stop();
    if (GetOrCreateCanvasResourceProvider() &&
        GetCanvasResourceProvider()->Canvas()) {
      DispatchContextRestoredEvent(nullptr);
    }
  }
}

void OffscreenCanvasRenderingContext2D::FlushCanvas(FlushReason reason) {
  if (GetCanvasResourceProvider()) {
    GetCanvasResourceProvider()->FlushCanvas(reason);
  }
}

OffscreenCanvas* OffscreenCanvasRenderingContext2D::HostAsOffscreenCanvas()
    const {
  return static_cast<OffscreenCanvas*>(Host());
}

FontSelector* OffscreenCanvasRenderingContext2D::GetFontSelector() const {
  return Host()->GetFontSelector();
}

}  // namespace blink
