// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/ukm_parameters.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

OffscreenCanvas::OffscreenCanvas(ExecutionContext* context, const IntSize& size)
    : CanvasRenderingContextHost(
          CanvasRenderingContextHost::HostType::kOffscreenCanvasHost,
          {context->UkmRecorder(), context->UkmSourceID()}),
      execution_context_(context),
      size_(size) {
  // Other code in Blink watches for destruction of the context; be
  // robust here as well.
  if (!context->IsContextDestroyed()) {
    if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
      // If this OffscreenCanvas is being created in the context of a
      // cross-origin iframe, it should prefer to use the low-power GPU.
      LocalFrame* frame = window->GetFrame();
      if (!(frame && frame->IsCrossOriginToMainFrame())) {
        AllowHighPerformancePowerPreference();
      }
    } else if (context->IsDedicatedWorkerGlobalScope()) {
      // Per spec, dedicated workers can only load same-origin top-level
      // scripts, so grant them access to the high-performance GPU.
      //
      // TODO(crbug.com/1050739): refine this logic. If the worker was
      // spawned from an iframe, keep track of whether that iframe was
      // itself cross-origin.
      AllowHighPerformancePowerPreference();
    }
  }

  UpdateMemoryUsage();
}

OffscreenCanvas* OffscreenCanvas::Create(ExecutionContext* context,
                                         unsigned width,
                                         unsigned height) {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.NewOffscreenCanvas", true);
  return MakeGarbageCollected<OffscreenCanvas>(
      context, IntSize(clampTo<int>(width), clampTo<int>(height)));
}

OffscreenCanvas::~OffscreenCanvas() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -memory_usage_);
}

void OffscreenCanvas::Commit(scoped_refptr<CanvasResource> canvas_resource,
                             const SkIRect& damage_rect) {
  if (!HasPlaceholderCanvas() || !canvas_resource)
    return;
  RecordCanvasSizeToUMA(Size());

  base::TimeTicks commit_start_time = base::TimeTicks::Now();
  current_frame_damage_rect_.join(damage_rect);
  GetOrCreateResourceDispatcher()->DispatchFrameSync(
      std::move(canvas_resource), commit_start_time, current_frame_damage_rect_,
      !RenderingContext()->IsOriginTopLeft() /* needs_vertical_flip */,
      IsOpaque());
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
}

void OffscreenCanvas::Dispose() {
  // We need to drop frame dispatcher, to prevent mojo calls from completing.
  frame_dispatcher_ = nullptr;
  DiscardResourceProvider();

  if (context_) {
    context_->DetachHost();
    context_ = nullptr;
  }
}

void OffscreenCanvas::DeregisterFromAnimationFrameProvider() {
  if (HasPlaceholderCanvas() && GetTopExecutionContext() &&
      GetTopExecutionContext()->IsDedicatedWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<DedicatedWorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    if (animation_frame_provider)
      animation_frame_provider->DeregisterOffscreenCanvas(this);
  }
}

void OffscreenCanvas::SetPlaceholderCanvasId(DOMNodeId canvas_id) {
  placeholder_canvas_id_ = canvas_id;
  if (GetTopExecutionContext() &&
      GetTopExecutionContext()->IsDedicatedWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<DedicatedWorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    DCHECK(animation_frame_provider);
    if (animation_frame_provider)
      animation_frame_provider->RegisterOffscreenCanvas(this);
  }
  if (frame_dispatcher_) {
    frame_dispatcher_->SetPlaceholderCanvasDispatcher(placeholder_canvas_id_);
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
  // Setting size of a canvas also resets it.
  if (size == size_) {
    if (context_ && context_->IsRenderingContext2D()) {
      context_->Reset();
      origin_clean_ = true;
    }
    return;
  }

  size_ = size;
  UpdateMemoryUsage();
  current_frame_damage_rect_ = SkIRect::MakeWH(size_.Width(), size_.Height());

  if (frame_dispatcher_)
    frame_dispatcher_->Reshape(size_);
  if (context_) {
    if (context_->Is3d()) {
      context_->Reshape(size_.Width(), size_.Height());
    } else if (context_->IsRenderingContext2D()) {
      context_->Reset();
      origin_clean_ = true;
    }
    context_->DidDraw();
  }
}

ScriptPromise OffscreenCanvas::convertToBlob(ScriptState* script_state,
                                             const ImageEncodeOptions* options,
                                             ExceptionState& exception_state) {
  return CanvasRenderingContextHost::convertToBlob(script_state, options,
                                                   exception_state, context_);
}

void OffscreenCanvas::RecordTransfer() {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.Transferred", true);
}

void OffscreenCanvas::SetNeutered() {
  DCHECK(!context_);
  is_neutered_ = true;
  size_.SetWidth(0);
  size_.SetHeight(0);
  DeregisterFromAnimationFrameProvider();
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
    // Undocumented exception (not in spec).
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "ImageBitmap construction failed");
  }

  RecordIdentifiabilityMetric(
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kCanvasReadback,
          context_ ? context_->GetContextType()
                   : CanvasRenderingContext::kContextTypeUnknown),
      0);

  return image;
}

void OffscreenCanvas::RecordIdentifiabilityMetric(
    const blink::IdentifiableSurface& surface,
    const IdentifiableToken& token) const {
  if (!IdentifiabilityStudySettings::Get()->ShouldSample(surface))
    return;
  blink::IdentifiabilityMetricBuilder(GetExecutionContext()->UkmSourceID())
      .Set(surface, token)
      .Record(GetExecutionContext()->UkmRecorder());
}

scoped_refptr<Image> OffscreenCanvas::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const FloatSize& size) {
  if (!context_) {
    *status = kInvalidSourceImageStatus;
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
    return surface ? UnacceleratedStaticBitmapImage::Create(
                         surface->makeImageSnapshot())
                   : nullptr;
  }
  if (!size.Width() || !size.Height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }
  scoped_refptr<Image> image = context_->GetImage();
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
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (context_)
    context_->FinalizeFrame();
  return ImageBitmapSource::FulfillImageBitmap(
      script_state,
      IsPaintable()
          ? MakeGarbageCollected<ImageBitmap>(this, crop_rect, options)
          : nullptr,
      exception_state);
}

bool OffscreenCanvas::IsOpaque() const {
  return context_ ? !context_->CreationAttributes().alpha : false;
}

CanvasRenderingContext* OffscreenCanvas::GetCanvasRenderingContext(
    ExecutionContext* execution_context,
    const String& id,
    const CanvasContextCreationAttributesCore& attributes) {
  DCHECK_EQ(execution_context, GetTopExecutionContext());
  CanvasRenderingContext::ContextType context_type =
      CanvasRenderingContext::ContextTypeFromId(id);

  // Unknown type.
  if (context_type == CanvasRenderingContext::kContextTypeUnknown ||
      (context_type == CanvasRenderingContext::kContextXRPresent &&
       !RuntimeEnabledFeatures::WebXREnabled(execution_context))) {
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
    CanvasContextCreationAttributesCore recomputed_attributes = attributes;
    if (!allow_high_performance_power_preference_)
      recomputed_attributes.power_preference = "low-power";

    context_ = factory->Create(this, recomputed_attributes);
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

    if (HasPlaceholderCanvas())
      frame_dispatcher_->SetPlaceholderCanvasDispatcher(placeholder_canvas_id_);
  }
  return frame_dispatcher_.get();
}

CanvasResourceProvider* OffscreenCanvas::GetOrCreateResourceProvider() {
  if (ResourceProvider())
    return ResourceProvider();

  std::unique_ptr<CanvasResourceProvider> provider;
  IntSize surface_size(width(), height());
  const bool can_use_gpu =
      SharedGpuContext::IsGpuCompositingEnabled() &&
      (Is3d() || (RuntimeEnabledFeatures::Accelerated2dCanvasEnabled() &&
                  !context_->CreationAttributes().will_read_frequently));
  const bool composited_mode =
      (Is3d() ? RuntimeEnabledFeatures::WebGLImageChromiumEnabled()
              : RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled());

  // If this context has a placeholder, the resource needs to be optimized for
  // displaying on screen. In the case we are hardware compositing, we also
  // try to enable the usage of the image as scanout buffer (overlay).
  uint32_t shared_image_usage_flags = 0u;
  if (HasPlaceholderCanvas()) {
    shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_DISPLAY;
    if (composited_mode)
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  if (can_use_gpu) {
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        surface_size, FilterQuality(),
        context_->CanvasRenderingContextColorParams(),
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        false /*is_origin_top_left*/, shared_image_usage_flags);
  } else if (HasPlaceholderCanvas() && composited_mode) {
    // Only try a SoftwareComposited SharedImage if the context has Placeholder
    // canvas and the composited mode is enabled.
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        surface_size, FilterQuality(),
        context_->CanvasRenderingContextColorParams(),
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kCPU,
        false /*is_origin_top_left*/, shared_image_usage_flags);
  }

  if (!provider && HasPlaceholderCanvas()) {
    // If this context has a Placerholder - which means that we have to display
    // this resource - and the SharedImage Provider creation above failed, we
    // try a SharedBitmap Provider before falling back to a Bitmap Provider.
    base::WeakPtr<CanvasResourceDispatcher> dispatcher_weakptr =
        GetOrCreateResourceDispatcher()->GetWeakPtr();
    provider = CanvasResourceProvider::CreateSharedBitmapProvider(
        surface_size, FilterQuality(),
        context_->CanvasRenderingContextColorParams(),
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        std::move(dispatcher_weakptr));
  }

  if (!provider) {
    // If any of the above Create was able to create a valid provider, a
    // BitmapProvider will be created here.
    provider = CanvasResourceProvider::CreateBitmapProvider(
        surface_size, FilterQuality(),
        context_->CanvasRenderingContextColorParams(),
        CanvasResourceProvider::ShouldInitialize::kCallClear);
  }

  ReplaceResourceProvider(std::move(provider));

  if (ResourceProvider() && ResourceProvider()->IsValid()) {
    // todo(crbug/1064363)  Add a separate UMA for Offscreen Canvas usage and
    // understand if the if (ResourceProvider() &&
    // ResourceProvider()->IsValid()) is really needed.
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              ResourceProvider()->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  ResourceProvider()->GetType());
    DidDraw();

    if (needs_matrix_clip_restore_) {
      needs_matrix_clip_restore_ = false;
      context_->RestoreCanvasMatrixClipStack(ResourceProvider()->Canvas());
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
    if (!inside_worker_raf_)
      GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(true);
  }
}

bool OffscreenCanvas::BeginFrame() {
  DCHECK(HasPlaceholderCanvas());
  GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(false);
  return PushFrameIfNeeded();
}

void OffscreenCanvas::SetFilterQualityInResource(
    SkFilterQuality filter_quality) {
  if (filter_quality_ == filter_quality)
    return;

  filter_quality_ = filter_quality;
  if (ResourceProvider())
    GetOrCreateResourceProvider()->SetFilterQuality(filter_quality);
}

bool OffscreenCanvas::PushFrameIfNeeded() {
  if (needs_push_frame_ && context_) {
    return context_->PushFrame();
  }
  return false;
}

bool OffscreenCanvas::PushFrame(scoped_refptr<CanvasResource> canvas_resource,
                                const SkIRect& damage_rect) {
  TRACE_EVENT0("blink", "OffscreenCanvas::PushFrame");
  DCHECK(needs_push_frame_);
  needs_push_frame_ = false;
  current_frame_damage_rect_.join(damage_rect);
  if (current_frame_damage_rect_.isEmpty() || !canvas_resource)
    return false;
  const base::TimeTicks commit_start_time = base::TimeTicks::Now();
  GetOrCreateResourceDispatcher()->DispatchFrame(
      std::move(canvas_resource), commit_start_time, current_frame_damage_rect_,
      !RenderingContext()->IsOriginTopLeft() /* needs_vertical_flip */,
      IsOpaque());
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
  return true;
}

bool OffscreenCanvas::ShouldAccelerate2dContext() const {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         context_provider_wrapper->Utils()->Accelerated2DCanvasFeatureEnabled();
}

FontSelector* OffscreenCanvas::GetFontSelector() {
  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    return window->document()->GetStyleEngine().GetFontSelector();
  }
  return To<WorkerGlobalScope>(GetExecutionContext())->GetFontSelector();
}

void OffscreenCanvas::UpdateMemoryUsage() {
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

void OffscreenCanvas::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(execution_context_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
