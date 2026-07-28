// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_elementimage.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"
#include "third_party/blink/renderer/core/html/canvas/element_image.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/ukm_parameters.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/canvas_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_transform.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {
namespace {
class OffscreenCanvasRegistry
    : public GarbageCollected<OffscreenCanvasRegistry>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];
  static OffscreenCanvasRegistry& From(ExecutionContext* context) {
    auto* supplement =
        Supplement<ExecutionContext>::From<OffscreenCanvasRegistry>(context);
    if (!supplement) {
      supplement = MakeGarbageCollected<OffscreenCanvasRegistry>(context);
      Supplement<ExecutionContext>::ProvideTo(*context, supplement);
    }
    return *supplement;
  }
  explicit OffscreenCanvasRegistry(ExecutionContext* context)
      : Supplement<ExecutionContext>(*context) {}
  void Register(DOMNodeId id, OffscreenCanvas* canvas) {
    if (id != kInvalidDOMNodeId) {
      map_.Set(id, canvas);
    }
  }
  OffscreenCanvas* Get(DOMNodeId id) {
    auto it = map_.find(id);
    return it != map_.end() ? it->value.Get() : nullptr;
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(map_);
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  HeapHashMap<DOMNodeId, WeakMember<OffscreenCanvas>> map_;
};
const char OffscreenCanvasRegistry::kSupplementName[] =
    "OffscreenCanvasRegistry";

void RecordRenderedTextOnMainThread(int placeholder_canvas_id,
                                    const String text,
                                    const gfx::RectF bounds,
                                    float font_height) {
  DCHECK(IsMainThread());
  if (auto* placeholder = OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id)) {
    placeholder->RecordRenderedText(text, bounds, font_height);
  }
}

void ClearRenderedTextOnMainThread(int placeholder_canvas_id,
                                   const gfx::RectF rect) {
  DCHECK(IsMainThread());
  if (auto* placeholder = OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id)) {
    placeholder->ClearRenderedText(rect);
  }
}

void ClearAllRenderedTextOnMainThread(int placeholder_canvas_id) {
  DCHECK(IsMainThread());
  if (auto* placeholder = OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id)) {
    placeholder->ClearRenderedText();
  }
}

}  // namespace

OffscreenCanvas::OffscreenCanvas(ExecutionContext* context,
                                 gfx::Size size,
                                 uint32_t client_id,
                                 uint32_t sink_id,
                                 DOMNodeId canvas_id)
    : CanvasRenderingContextHost(
          CanvasRenderingContextHost::HostType::kOffscreenCanvasHost,
          size),
      execution_context_(context),
      placeholder_canvas_id_(canvas_id),
      client_id_(client_id),
      sink_id_(sink_id) {
  // Other code in Blink watches for destruction of the context; be
  // robust here as well.
  if (!context->IsContextDestroyed()) {
    if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
      // Snapshot the text direction. For a offscreen transferred from
      // an element this will be over-written by the value from the element.
      if (window->document()->documentElement()) {
        text_direction_ =
            window->document()->documentElement()->CachedDirectionality();
      }
      // If this OffscreenCanvas is being created in the context of a
      // cross-origin iframe, it should prefer to use the low-power GPU.
      LocalFrame* frame = window->GetFrame();
      if (!(frame && frame->IsCrossOriginToOutermostMainFrame())) {
        AllowHighPerformancePowerPreference();
      }
    } else if (context->IsDedicatedWorkerGlobalScope()) {
      // Per spec, dedicated workers can only load same-origin top-level
      // scripts, so grant them access to the high-performance GPU.
      //
      // TODO(crbug.com/40118181): refine this logic. If the worker was
      // spawned from an iframe, keep track of whether that iframe was
      // itself cross-origin.
      AllowHighPerformancePowerPreference();
    }
  }

  CanvasResourceTracker::For(context->GetIsolate())->Add(this, context);

  OffscreenCanvasRegistry::From(execution_context_).Register(canvas_id, this);

  if (HasPlaceholderCanvas() &&
      execution_context_->IsDedicatedWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<DedicatedWorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    DCHECK(animation_frame_provider);
    if (animation_frame_provider) {
      animation_frame_provider->RegisterOffscreenCanvas(this);
    }
  }

  // TODO(crbug.com/498093320): Try to get this from placeholder canvas.
  should_capture_rendered_text_ =
      base::FeatureList::IsEnabled(::features::kAccessibilityCanvas);
}

OffscreenCanvas* OffscreenCanvas::Create(ScriptState* script_state,
                                         unsigned width,
                                         unsigned height,
                                         uint32_t client_id,
                                         uint32_t sink_id,
                                         DOMNodeId canvas_id) {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.NewOffscreenCanvas", true);
  return MakeGarbageCollected<OffscreenCanvas>(
      ExecutionContext::From(script_state),
      gfx::Size(ClampTo<int>(width), ClampTo<int>(height)), client_id, sink_id,
      canvas_id);
}

void OffscreenCanvas::Dispose() {
  // We need to drop frame dispatcher, to prevent mojo calls from completing.
  disposing_ = true;
  placeholder_client_ = nullptr;
  frame_dispatcher_ = nullptr;
  DiscardResources();

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

OffscreenCanvas* OffscreenCanvas::FromPlaceholderId(ExecutionContext* context,
                                                    DOMNodeId canvas_id) {
  if (!context || canvas_id == kInvalidDOMNodeId) {
    return nullptr;
  }
  return OffscreenCanvasRegistry::From(context).Get(canvas_id);
}

void OffscreenCanvas::setWidth(unsigned width) {
  gfx::Size new_size = Size();
  new_size.set_width(ClampTo<int>(width));
  SetSize(new_size);
}

void OffscreenCanvas::setHeight(unsigned height) {
  gfx::Size new_size = Size();
  new_size.set_height(ClampTo<int>(height));
  SetSize(new_size);
}

void OffscreenCanvas::SetSize(gfx::Size size) {
  // Setting size of a canvas also resets it.
  if (size == Size()) {
    if (context_ && context_->IsRenderingContext2D()) {
      context_->Reset();
      origin_clean_ = true;
      // We need to trigger the draw, because we did reset the context.
      context_->DidDraw(CanvasPerformanceMonitor::DrawType::kOther);
    }
    return;
  }

  size_ = size;

  if (context_ && context_->isContextLost()) {
    context_->RestoreFromInvalidSizeIfNeeded();
  }

  if (frame_dispatcher_)
    frame_dispatcher_->Reshape(Size());
  if (context_) {
    if (context_->IsWebGL() || IsWebGPU()) {
      context_->Reshape(Size().width(), Size().height());
    } else if (context_->IsRenderingContext2D() ||
               context_->IsImageBitmapRenderingContext()) {
      context_->Reset();
      if (context_->IsRenderingContext2D()) {
        origin_clean_ = true;
      }
    }
    context_->DidDraw(CanvasPerformanceMonitor::DrawType::kOther);
  }
}

void OffscreenCanvas::RecordTransfer() {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.Transferred", true);
}

void OffscreenCanvas::SetNeutered() {
  DCHECK(!context_);
  is_neutered_ = true;
  SetSize(gfx::Size(0, 0));
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
  if (ContextHasOpenLayers(context_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`transferToImageBitmap()` cannot be called with open layers.");
    return nullptr;
  }

  ImageBitmap* image =
      context_->TransferToImageBitmap(script_state, exception_state);
  if (exception_state.HadException()) [[unlikely]] {
    return nullptr;
  }

  if (!image) {
    // Undocumented exception (not in spec).
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "ImageBitmap construction failed");
  }

  return image;
}

scoped_refptr<Image> OffscreenCanvas::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const gfx::SizeF& size) {
  if (!context_) {
    *status = kInvalidSourceImageStatus;
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(Size().width(), Size().height()));
    return surface ? UnacceleratedStaticBitmapImage::Create(
                         surface->makeImageSnapshot())
                   : nullptr;
  }
  if (ContextHasOpenLayers(context_)) {
    *status = kLayersOpenInCanvasSource;
    return nullptr;
  }
  if (!size.width() || !size.height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }
  scoped_refptr<StaticBitmapImage> image;
  if (IsWebGL() || IsWebGPU()) {
    // Because WebGL/WebGPU sources always require copying the back buffer,
    // we use PaintRenderingResultsToSnapshot instead of GetImage in order to
    // keep a cached copy of the backing in the canvas's resource provider.
    image = RenderingContext()->PaintRenderingResultsToSnapshot(kBackBuffer);
  } else {
    image = RenderingContext()->GetImage();
  }
  if (!image) {
    image = CreateTransparentImage();
  }
  *status = image ? kNormalSourceImageStatus : kInvalidSourceImageStatus;

  return image;
}

ScriptPromise<ImageBitmap> OffscreenCanvas::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (ContextHasOpenLayers(context_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`createImageBitmap()` cannot be called with open layers.");
    return EmptyPromise();
  }
  if (context_) {
    context_->FinalizeFrame(FlushReason::kOther);
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state,
      IsPaintable()
          ? MakeGarbageCollected<ImageBitmap>(this, crop_rect, options)
          : nullptr,
      options, exception_state);
}

DOMMatrix* OffscreenCanvas::getElementTransform(
    const V8UnionElementOrElementImage* element_or_image,
    DOMMatrix* draw_transform,
    ExceptionState& exception_state) {
  if (element_or_image->IsElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Elements cannot be drawn into an OffscreenCanvas.");
    return nullptr;
  }

  if (element_or_image->IsElementImage()) {
    const auto& paint_record =
        element_or_image->GetAsElementImage()->PaintRecord();
    if (!paint_record) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The ElementImage has been closed.");
      return nullptr;
    }
    if (paint_record->paint_state.canvas_node_id == kInvalidDOMNodeId ||
        paint_record->paint_state.canvas_node_id != PlaceholderCanvasId()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The ElementImage was captured from a different canvas.");
      return nullptr;
    }
    gfx::Transform transform = GetElementTransform(
        paint_record->paint_state, Size(), draw_transform->Matrix());
    return MakeGarbageCollected<DOMMatrix>(transform,
                                           transform.Is2dTransform());
  }

  return DOMMatrix::Create();
}

ScriptPromise<Blob> OffscreenCanvas::convertToBlob(
    ScriptState* script_state,
    const ImageEncodeOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsOffscreenCanvas());
  if (is_neutered_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached.");
    return EmptyPromise();
  }

  if (ContextHasOpenLayers(context_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`convertToBlob()` cannot be called while layers are opened.");
    return EmptyPromise();
  }

  if (!OriginClean()) {
    exception_state.ThrowSecurityError(
        "A tainted OffscreenCanvas may not be exported.");
    return EmptyPromise();
  }

  // It's possible that there are recorded commands that have not been resolved
  // Finalize frame will be called in GetImage, but if there's no
  // resourceProvider yet then the IsPaintable check will fail
  if (context_) {
    context_->FinalizeFrame(FlushReason::kOther);
  }

  if (!IsPaintable() || Size().IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The size of the OffscreenCanvas is zero.");
    return EmptyPromise();
  }

  if (!context_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The OffscreenCanvas has no rendering context.");
    return EmptyPromise();
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  scoped_refptr<StaticBitmapImage> image_bitmap = context_->GetImage();
  if (image_bitmap) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<Blob>>(
        script_state, exception_state.GetContext());
    CanvasAsyncBlobCreator::ToBlobFunctionType function_type =
        CanvasAsyncBlobCreator::kOffscreenCanvasConvertToBlobPromise;
    auto* execution_context = ExecutionContext::From(script_state);
    auto* async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options, function_type, start_time, execution_context,
        resolver);
    async_creator->ScheduleAsyncBlobCreation(options->quality());
    return resolver->Promise();
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                    "Readback of the source image has failed.");
  return EmptyPromise();
}

bool OffscreenCanvas::IsOpaque() const {
  return RenderingContext() && RenderingContext()->IsOpaque();
}

CanvasRenderingContext* OffscreenCanvas::GetCanvasRenderingContext(
    ExecutionContext* execution_context,
    CanvasRenderingContext::CanvasRenderingAPI rendering_api,
    const CanvasContextCreationAttributesCore& attributes) {
  DCHECK_EQ(execution_context, GetTopExecutionContext());

  if (execution_context->IsContextDestroyed())
    return nullptr;

  // Unknown type.
  if (rendering_api == CanvasRenderingContext::CanvasRenderingAPI::kUnknown)
    return nullptr;

  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    if (attributes.color_space != PredefinedColorSpace::kSRGB)
      UseCounter::Count(window->document(), WebFeature::kCanvasUseColorSpace);
  }

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(static_cast<int>(rendering_api));
  if (!factory)
    return nullptr;

  if (context_) {
    if (context_->GetRenderingAPI() != rendering_api) {
      factory->OnError(
          this, "OffscreenCanvas has an existing context of a different type");
      return nullptr;
    }
  } else {
    // Tell the debugger about the attempt to create an offscreen
    // canvas context even if it will fail, to ease debugging.
    probe::DidCreateOffscreenCanvasContext(this);

    CanvasContextCreationAttributesCore recomputed_attributes = attributes;
    if (!allow_high_performance_power_preference_) {
      recomputed_attributes.power_preference =
          CanvasContextCreationAttributesCore::PowerPreference::kLowPower;
    }

    context_ = factory->Create(execution_context, this, recomputed_attributes);
    if (context_) {
      context_->RecordUKMCanvasRenderingAPI();
      context_->RecordUMACanvasRenderingAPI();
    }
  }

  return context_.Get();
}

OffscreenCanvas::ContextFactoryVector&
OffscreenCanvas::RenderingContextFactories() {
  DEFINE_STATIC_LOCAL(
      ContextFactoryVector, context_factories,
      (static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::kMaxValue) +
       1));
  return context_factories;
}

CanvasRenderingContextFactory* OffscreenCanvas::GetRenderingContextFactory(
    int type) {
  DCHECK_LE(type, static_cast<int>(
                      CanvasRenderingContext::CanvasRenderingAPI::kMaxValue));
  return RenderingContextFactories()[type].get();
}

void OffscreenCanvas::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::CanvasRenderingAPI rendering_api =
      rendering_context_factory->GetRenderingAPI();
  DCHECK_LE(rendering_api,
            CanvasRenderingContext::CanvasRenderingAPI::kMaxValue);
  DCHECK(!RenderingContextFactories()[static_cast<int>(rendering_api)]);
  RenderingContextFactories()[static_cast<int>(rendering_api)] =
      std::move(rendering_context_factory);
}

bool OffscreenCanvas::OriginClean() const {
  return origin_clean_ && !disable_reading_from_canvas_;
}

void OffscreenCanvas::DiscardResources() {
  UpdateMemoryUsage();
}

bool OffscreenCanvas::HasPlaceholderCanvas() const {
  return placeholder_canvas_id_ != kInvalidDOMNodeId;
}

CanvasResourceDispatcher* OffscreenCanvas::GetOrCreateResourceDispatcher() {
  CHECK(HasPlaceholderCanvas());
  // If we don't have a valid placeholder_canvas_id_, then this is a standalone
  // OffscreenCanvas, and it should not have a placeholder.
  if (frame_dispatcher_ == nullptr) {
    scoped_refptr<base::SingleThreadTaskRunner>
        agent_group_scheduler_compositor_task_runner;
    scoped_refptr<base::SingleThreadTaskRunner> dispatcher_task_runner;
    if (auto* top_execution_context = GetTopExecutionContext()) {
      agent_group_scheduler_compositor_task_runner =
          top_execution_context->GetAgentGroupSchedulerCompositorTaskRunner();

      dispatcher_task_runner =
          top_execution_context->GetTaskRunner(TaskType::kInternalDefault);
    }

    // The frame dispatcher connects the current thread of OffscreenCanvas
    // (either main or worker) to the GPU process and will have to be recreated
    // if the GPU channel is lost.
    frame_dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        this, dispatcher_task_runner, client_id_, sink_id_, Size());

    if (agent_group_scheduler_compositor_task_runner) {
      placeholder_client_ =
          std::make_unique<OffscreenCanvasPlaceholder::Client>(
              placeholder_canvas_id_,
              std::move(agent_group_scheduler_compositor_task_runner),
              std::move(dispatcher_task_runner),
              blink::BindRepeating(
                  [](OffscreenCanvas* canvas) {
                    if (canvas->frame_dispatcher_) {
                      canvas->frame_dispatcher_->SetAnimationState(
                          canvas->placeholder_client_->GetAnimationState());
                    }
                  },
                  WrapWeakPersistent(this)));
    }
  }
  return frame_dispatcher_.get();
}

void OffscreenCanvas::DidDraw(const gfx::Rect& rect) {
  if (rect.IsEmpty()) {
    return;
  }

  current_frame_damage_rect_.Union(rect);

  if (HasPlaceholderCanvas()) {
    needs_push_frame_ = true;
    if (!inside_worker_raf_) {
      // If the offscreencanvas is in the same tread as the canvas, and we are
      // trying for a second time to request the being frame, and we are in a
      // capture_stream scenario, we will call a BeginFrame right away. So
      // Offscreen Canvas can behave in a more synchronous way when it's on the
      // main thread.
      if (GetOrCreateResourceDispatcher()->NeedsBeginFrame() &&
          IsMainThread()) {
        if (placeholder_canvas_id_ != kInvalidDOMNodeId) {
          OffscreenCanvasPlaceholder* placeholder_canvas =
              OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
                  placeholder_canvas_id_);
          if (placeholder_canvas &&
              placeholder_canvas->IsOffscreenCanvasRegistered() &&
              placeholder_canvas->HasCanvasCapture()) {
            GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(false);
            PushFrameIfNeeded();
          }
        }
      }

      GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(true);
    }
  }
}

bool OffscreenCanvas::BeginFrame() {
  CHECK(frame_dispatcher_);
  frame_dispatcher_->SetNeedsBeginFrame(false);
  return PushFrameIfNeeded();
}

bool OffscreenCanvas::PushFrameIfNeeded() {
  if (needs_push_frame_ && context_) {
    bool should_call_push_frame = false;
    scoped_refptr<CanvasResource> canvas_resource =
        context_->GetResourceForPushFrame(should_call_push_frame);
    if (should_call_push_frame) {
      return PushFrame(std::move(canvas_resource));
    }
  }
  return false;
}

bool OffscreenCanvas::PushFrame(
    scoped_refptr<CanvasResource>&& canvas_resource) {
  TRACE_EVENT0("blink", "OffscreenCanvas::PushFrame");
  DCHECK(needs_push_frame_);
  needs_push_frame_ = false;

  if (!canvas_resource) {
    return false;
  }
  canvas_resource->SetOriginClean(OriginClean());
  current_frame_damage_rect_.Intersect(gfx::Rect(Size()));

  // We can't submit empty CompositorFrames, but contexts not always obeys it
  // (e.g webgl clamps it's size to 1x1) and can still produce the resource.
  if (Size().IsEmpty()) {
    return false;
  }

  auto exported_resource =
      base::MakeRefCounted<ExportedCanvasResource>(std::move(canvas_resource));

  auto* dispatcher = GetOrCreateResourceDispatcher();
  if (placeholder_client_) {
    placeholder_client_->DispatchFrame(exported_resource);
  }
  dispatcher->DispatchFrame(std::move(exported_resource),
                            current_frame_damage_rect_, IsOpaque());
  current_frame_damage_rect_ = gfx::Rect();

  return true;
}

bool OffscreenCanvas::ShouldAccelerate2dContext() const {
  return Accelerated2DCanvasFeatureEnabled(
      SharedGpuContext::ContextProviderWrapper().get());
}

UkmParameters OffscreenCanvas::GetUkmParameters() {
  auto* context = GetExecutionContext();
  return {context->UkmRecorder(), context->UkmSourceID()};
}

void OffscreenCanvas::NotifyGpuContextLost() {
  if (context_ && !context_->isContextLost()) {
    // This code path is used only by 2D canvas, where NotifyGpuContextLost is
    // called by OffscreenCanvas itself rather than the rendering context.
    DCHECK(context_->IsRenderingContext2D());
    context_->LoseContext(CanvasRenderingContext::kRealLostContext);
  }
  if (context_->IsWebGL()) {
    // We'll need to recreate a new frame dispatcher once the context is
    // restored in order to reestablish the compositor frame sink mojo
    // channel.
    placeholder_client_ = nullptr;
    frame_dispatcher_ = nullptr;
  }
}

TextDirection OffscreenCanvas::GetTextDirection(const ComputedStyle*) {
  return text_direction_.value_or(TextDirection::kLtr);
}

void OffscreenCanvas::SetLocale(scoped_refptr<const LayoutLocale> locale) {
  locale_ = std::move(locale);
}

const LayoutLocale* OffscreenCanvas::GetLocale() const {
  if (locale_) {
    return locale_.get();
  }
  if (const auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    const Element* document_element = window->document()->documentElement();
    if (document_element) {
      return &LayoutLocale::ValueOrDefault(
          LayoutLocale::Get(document_element->ComputeInheritedLanguage()));
    }
  }
  return &LayoutLocale::GetDefault();
}

UniqueFontSelector* OffscreenCanvas::GetFontSelector() {
  if (UniqueFontSelector* unique_font_selector = unique_font_selector_) {
    return unique_font_selector;
  }
  FontSelector* base_selector = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    base_selector = window->document()->GetStyleEngine().GetFontSelector();
  } else {
    // TODO(crbug.com/40059901): Temporary mitigation.  Remove the following
    // CHECK once a more comprehensive solution has been implemented.
    CHECK(GetExecutionContext()->IsWorkerGlobalScope());
    base_selector =
        To<WorkerGlobalScope>(GetExecutionContext())->GetFontSelector();
  }
  auto* unique_font_selector =
      MakeGarbageCollected<UniqueFontSelector>(base_selector);
  unique_font_selector_ = unique_font_selector;
  return unique_font_selector;
}

bool OffscreenCanvas::IsPageVisible() const {
  if (base::FeatureList::IsEnabled(
          blink::features::kOffscreenCanvasPropagateVisibility)) {
    return is_parent_visible_;
  }
  return true;
}

void OffscreenCanvas::SetParentVisibility(bool visible) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kOffscreenCanvasPropagateVisibility)) {
    return;
  }
  if (is_parent_visible_ == visible) {
    return;
  }
  is_parent_visible_ = visible;
  if (context_) {
    context_->PageVisibilityChanged();
  }
}

void OffscreenCanvas::RecordRenderedText(const String& text,
                                         const gfx::RectF& bounds,
                                         float font_height) {
  if (placeholder_canvas_id_ == kInvalidDOMNodeId) {
    return;
  }

  if (IsMainThread()) {
    RecordRenderedTextOnMainThread(placeholder_canvas_id_, text, bounds,
                                   font_height);
  } else {
    // Accessibility only runs on the main thread. If this OffscreenCanvas is
    // running on a worker thread, we must post a task to the main thread to
    // record the rendered text on the placeholder canvas.
    if (auto* context = GetTopExecutionContext()) {
      if (auto task_runner =
              context->GetAgentGroupSchedulerCompositorTaskRunner()) {
        PostCrossThreadTask(*task_runner, FROM_HERE,
                            CrossThreadBindOnce(&RecordRenderedTextOnMainThread,
                                                placeholder_canvas_id_, text,
                                                bounds, font_height));
      }
    }
  }
}

void OffscreenCanvas::ClearRenderedText(const gfx::RectF& rect) {
  if (placeholder_canvas_id_ == kInvalidDOMNodeId) {
    return;
  }

  if (IsMainThread()) {
    ClearRenderedTextOnMainThread(placeholder_canvas_id_, rect);
  } else {
    if (auto* context = GetTopExecutionContext()) {
      if (auto task_runner =
              context->GetAgentGroupSchedulerCompositorTaskRunner()) {
        PostCrossThreadTask(*task_runner, FROM_HERE,
                            CrossThreadBindOnce(&ClearRenderedTextOnMainThread,
                                                placeholder_canvas_id_, rect));
      }
    }
  }
}

void OffscreenCanvas::ClearRenderedText() {
  if (placeholder_canvas_id_ == kInvalidDOMNodeId) {
    return;
  }

  if (IsMainThread()) {
    ClearAllRenderedTextOnMainThread(placeholder_canvas_id_);
  } else {
    if (auto* context = GetTopExecutionContext()) {
      if (auto task_runner =
              context->GetAgentGroupSchedulerCompositorTaskRunner()) {
        PostCrossThreadTask(
            *task_runner, FROM_HERE,
            CrossThreadBindOnce(&ClearAllRenderedTextOnMainThread,
                                placeholder_canvas_id_));
      }
    }
  }
}

void OffscreenCanvas::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(execution_context_);
  CanvasRenderingContextHost::Trace(visitor);
  EventTarget::Trace(visitor);
  CanvasRenderingContextHost::Trace(visitor);
}

}  // namespace blink
