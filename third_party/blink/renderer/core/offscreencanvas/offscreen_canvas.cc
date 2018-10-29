// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

OffscreenCanvas::OffscreenCanvas(const IntSize& size) : size_(size) {
  UpdateMemoryUsage();
}

OffscreenCanvas* OffscreenCanvas::Create(unsigned width, unsigned height) {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.NewOffscreenCanvas", true);
  return new OffscreenCanvas(
      IntSize(clampTo<int>(width), clampTo<int>(height)));
}

OffscreenCanvas::~OffscreenCanvas() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -memory_usage_);
}

void OffscreenCanvas::Commit(scoped_refptr<CanvasResource> canvas_resource,
                             const SkIRect& damage_rect) {
  if (!HasPlaceholderCanvas() || !canvas_resource)
    return;

  base::TimeTicks commit_start_time = WTF::CurrentTimeTicks();
  current_frame_damage_rect_.join(damage_rect);
  GetOrCreateResourceDispatcher()->DispatchFrameSync(
      std::move(canvas_resource), commit_start_time, current_frame_damage_rect_,
      !RenderingContext()->IsOriginTopLeft() /* needs_vertical_flip */,
      IsOpaque());
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
}

void OffscreenCanvas::Dispose() {
  if (context_) {
    context_->DetachHost();
    context_ = nullptr;
  }

  if (HasPlaceholderCanvas() && GetTopExecutionContext() &&
      GetTopExecutionContext()->IsWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<WorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    if (animation_frame_provider)
      animation_frame_provider->DeregisterOffscreenCanvas(this);
  }
}

void OffscreenCanvas::SetPlaceholderCanvasId(DOMNodeId canvas_id) {
  placeholder_canvas_id_ = canvas_id;
  if (GetTopExecutionContext() &&
      GetTopExecutionContext()->IsWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<WorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    if (animation_frame_provider)
      animation_frame_provider->RegisterOffscreenCanvas(this);
  }
}

void OffscreenCanvas::setWidth(unsigned width) {
  IntSize new_size = size_;
  new_size.SetWidth(clampTo<int>(width));
  SetSize(new_size);
}

void OffscreenCanvas::setHeight(unsigned height) {
  IntSize new_size = size_;
  new_size.SetHeight(clampTo<int>(height));
  SetSize(new_size);
}

void OffscreenCanvas::SetSize(const IntSize& size) {
  if (context_) {
    if (context_->Is3d()) {
      if (size != size_)
        context_->Reshape(size.Width(), size.Height());
    } else if (context_->Is2d()) {
      context_->Reset();
      origin_clean_ = true;
    }
  }
  if (size != size_) {
    UpdateMemoryUsage();
  }
  size_ = size;
  if (frame_dispatcher_)
    frame_dispatcher_->Reshape(size_);

  current_frame_damage_rect_ = SkIRect::MakeWH(size_.Width(), size_.Height());
  if (context_)
    context_->DidDraw();
}

void OffscreenCanvas::RecordTransfer() {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.Transferred", true);
}

void OffscreenCanvas::SetNeutered() {
  DCHECK(!context_);
  is_neutered_ = true;
  size_.SetWidth(0);
  size_.SetHeight(0);
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (is_neutered_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot transfer an ImageBitmap from a detached OffscreenCanvas");
    return nullptr;
  }
  if (!context_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot transfer an ImageBitmap from an "
                                      "OffscreenCanvas with no context");
    return nullptr;
  }

  ImageBitmap* image = context_->TransferToImageBitmap(script_state);
  if (!image) {
    // Undocumented exception (not in spec)
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "Out of memory");
  }

  return image;
}

scoped_refptr<Image> OffscreenCanvas::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint hint,
    const FloatSize& size) {
  if (!context_) {
    *status = kInvalidSourceImageStatus;
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
    return surface ? StaticBitmapImage::Create(surface->makeImageSnapshot())
                   : nullptr;
  }
  if (!size.Width() || !size.Height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }
  scoped_refptr<Image> image = context_->GetImage(hint);
  if (!image)
    image = CreateTransparentImage(Size());
  *status = image ? kNormalSourceImageStatus : kInvalidSourceImageStatus;
  return image;
}

IntSize OffscreenCanvas::BitmapSourceSize() const {
  return size_;
}

ScriptPromise OffscreenCanvas::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget&,
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options) {
  return ImageBitmapSource::FulfillImageBitmap(
      script_state,
      IsPaintable() ? ImageBitmap::Create(this, crop_rect, options) : nullptr);
}

bool OffscreenCanvas::IsOpaque() const {
  return context_ ? !context_->CreationAttributes().alpha : false;
}

CanvasRenderingContext* OffscreenCanvas::GetCanvasRenderingContext(
    ExecutionContext* execution_context,
    const String& id,
    const CanvasContextCreationAttributesCore& attributes) {
  execution_context_ = execution_context;

  CanvasRenderingContext::ContextType context_type =
      CanvasRenderingContext::ContextTypeFromId(id);

  // Unknown type.
  if (context_type == CanvasRenderingContext::kContextTypeUnknown ||
      (context_type == CanvasRenderingContext::kContextXRPresent &&
       !OriginTrials::WebXREnabled(execution_context))) {
    return nullptr;
  }

  // Log the aliased context type used.
  if (!context_) {
    UMA_HISTOGRAM_ENUMERATION("Blink.OffscreenCanvas.ContextType",
                              context_type);
  }

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(context_type);
  if (!factory)
    return nullptr;

  if (context_) {
    if (context_->GetContextType() != context_type) {
      factory->OnError(
          this, "OffscreenCanvas has an existing context of a different type");
      return nullptr;
    }
  } else {
    context_ = factory->Create(this, attributes);
  }

  return context_.Get();
}

OffscreenCanvas::ContextFactoryVector&
OffscreenCanvas::RenderingContextFactories() {
  DEFINE_STATIC_LOCAL(ContextFactoryVector, context_factories,
                      (CanvasRenderingContext::kMaxValue));
  return context_factories;
}

CanvasRenderingContextFactory* OffscreenCanvas::GetRenderingContextFactory(
    int type) {
  DCHECK_LE(type, CanvasRenderingContext::kMaxValue);
  return RenderingContextFactories()[type].get();
}

void OffscreenCanvas::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::ContextType type =
      rendering_context_factory->GetContextType();
  DCHECK_LE(type, CanvasRenderingContext::kMaxValue);
  DCHECK(!RenderingContextFactories()[type]);
  RenderingContextFactories()[type] = std::move(rendering_context_factory);
}

bool OffscreenCanvas::OriginClean() const {
  return origin_clean_ && !disable_reading_from_canvas_;
}

bool OffscreenCanvas::IsAccelerated() const {
  return context_ && context_->IsAccelerated();
}

bool OffscreenCanvas::HasPlaceholderCanvas() const {
  return placeholder_canvas_id_ != kInvalidDOMNodeId;
}

CanvasResourceDispatcher* OffscreenCanvas::GetOrCreateResourceDispatcher() {
  DCHECK(HasPlaceholderCanvas());
  // If we don't have a valid placeholder_canvas_id_, then this is a standalone
  // OffscreenCanvas, and it should not have a placeholder.
  if (!frame_dispatcher_) {
    // The frame dispatcher connects the current thread of OffscreenCanvas
    // (either main or worker) to the browser process and remains unchanged
    // throughout the lifetime of this OffscreenCanvas.
    frame_dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        this, client_id_, sink_id_, placeholder_canvas_id_, size_);
  }
  return frame_dispatcher_.get();
}

void OffscreenCanvas::DiscardResourceProvider() {
  CanvasResourceHost::DiscardResourceProvider();
  needs_matrix_clip_restore_ = true;
}

CanvasResourceProvider* OffscreenCanvas::GetOrCreateResourceProvider() {
  if (!ResourceProvider()) {
    bool can_use_gpu = false;
    CanvasResourceProvider::PresentationMode presentation_mode =
        CanvasResourceProvider::kDefaultPresentationMode;
    if (Is2d()) {
      if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
        presentation_mode =
            CanvasResourceProvider::kAllowImageChromiumPresentationMode;
      }
      if (SharedGpuContext::IsGpuCompositingEnabled() &&
          RuntimeEnabledFeatures::Accelerated2dCanvasEnabled()) {
        can_use_gpu = true;
      }
    } else if (Is3d()) {
      if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
        presentation_mode =
            CanvasResourceProvider::kAllowImageChromiumPresentationMode;
      }
      can_use_gpu = SharedGpuContext::IsGpuCompositingEnabled();
    }

    IntSize surface_size(width(), height());
    CanvasResourceProvider::ResourceUsage usage;
    if (can_use_gpu) {
      if (HasPlaceholderCanvas())
        usage = CanvasResourceProvider::kAcceleratedCompositedResourceUsage;
      else
        usage = CanvasResourceProvider::kAcceleratedResourceUsage;
    } else {
      if (HasPlaceholderCanvas())
        usage = CanvasResourceProvider::kSoftwareCompositedResourceUsage;
      else
        usage = CanvasResourceProvider::kSoftwareResourceUsage;
    }

    base::WeakPtr<CanvasResourceDispatcher> dispatcher_weakptr =
        HasPlaceholderCanvas() ? GetOrCreateResourceDispatcher()->GetWeakPtr()
                               : nullptr;

    ReplaceResourceProvider(CanvasResourceProvider::Create(
        surface_size, usage, SharedGpuContext::ContextProviderWrapper(), 0,
        context_->ColorParams(), presentation_mode,
        std::move(dispatcher_weakptr), false /* is_origin_top_left */));

    // The fallback chain for k*CompositedResourceUsage should never fall
    // all the way through to BitmapResourceProvider, except in unit tests.
    // In non unit-test scenarios, it should always be possible to at least
    // get a ResourceProviderSharedBitmap as a last resort.
    // This CHECK verifies that we did indeed get a resource provider that
    // supports compositing when one is required.
    CHECK(!ResourceProvider() || !HasPlaceholderCanvas() ||
          ResourceProvider()->SupportsDirectCompositing());

    if (ResourceProvider() && ResourceProvider()->IsValid()) {
      ResourceProvider()->Clear();
      // Always save an initial frame, to support resetting the top level matrix
      // and clip.
      ResourceProvider()->Canvas()->save();

      if (needs_matrix_clip_restore_) {
        needs_matrix_clip_restore_ = false;
        context_->RestoreCanvasMatrixClipStack(ResourceProvider()->Canvas());
      }
    }
  }
  return ResourceProvider();
}

void OffscreenCanvas::DidDraw() {
  DidDraw(FloatRect(0, 0, Size().Width(), Size().Height()));
}

void OffscreenCanvas::DidDraw(const FloatRect& rect) {
  if (rect.IsEmpty())
    return;

  if (HasPlaceholderCanvas()) {
    needs_push_frame_ = true;
    // TODO(fserb): perhaps we could avoid requesting begin frames here in cases
    // where the draw is call from within a worker rAF?
    GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(true);
  }
}

void OffscreenCanvas::BeginFrame() {
  DCHECK(HasPlaceholderCanvas());
  PushFrameIfNeeded();
  GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(false);
}

void OffscreenCanvas::PushFrameIfNeeded() {
  if (needs_push_frame_ && context_) {
    context_->PushFrame();
  }
}

bool OffscreenCanvas::ShouldAccelerate2dContext() const {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         context_provider_wrapper->Utils()->Accelerated2DCanvasFeatureEnabled();
}

void OffscreenCanvas::PushFrame(scoped_refptr<CanvasResource> canvas_resource,
                                const SkIRect& damage_rect) {
  DCHECK(needs_push_frame_);
  needs_push_frame_ = false;
  current_frame_damage_rect_.join(damage_rect);
  if (current_frame_damage_rect_.isEmpty() || !canvas_resource)
    return;
  const base::TimeTicks commit_start_time = WTF::CurrentTimeTicks();
  GetOrCreateResourceDispatcher()->DispatchFrame(
      std::move(canvas_resource), commit_start_time, current_frame_damage_rect_,
      !RenderingContext()->IsOriginTopLeft() /* needs_vertical_flip */,
      IsOpaque());
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
}

FontSelector* OffscreenCanvas::GetFontSelector() {
  if (auto* document = DynamicTo<Document>(GetExecutionContext())) {
    return document->GetStyleEngine().GetFontSelector();
  }
  return To<WorkerGlobalScope>(GetExecutionContext())->GetFontSelector();
}

void OffscreenCanvas::UpdateMemoryUsage() {
  CanvasRenderingContextHost::RecordCanvasSizeToUMA(
      Size().Width(), Size().Height(), true /* OffscreenCanvas */);

  int bytes_per_pixel = ColorParams().BytesPerPixel();

  base::CheckedNumeric<int32_t> memory_usage_checked = bytes_per_pixel;
  memory_usage_checked *= Size().Width();
  memory_usage_checked *= Size().Height();
  int32_t new_memory_usage =
      memory_usage_checked.ValueOrDefault(std::numeric_limits<int32_t>::max());

  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      new_memory_usage - memory_usage_);
  memory_usage_ = new_memory_usage;
}

void OffscreenCanvas::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(execution_context_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
