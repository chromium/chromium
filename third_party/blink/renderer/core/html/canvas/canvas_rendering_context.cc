/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"

#include "base/byte_size.h"
#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_elementimage.h"
#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/element_image.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_accessibility_manager.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace blink {
namespace {
BASE_FEATURE(kAllowAcceleratedTexElement, base::FEATURE_ENABLED_BY_DEFAULT);

void RecordAccessibilityUKMForCanvasElement(
    HTMLCanvasElement* canvas_element,
    CanvasRenderingContext::CanvasRenderingAPI canvas_rendering_type,
    bool is_offscreen,
    bool did_draw_text) {
  DCHECK(IsMainThread());
  DCHECK(canvas_element);

  DEFINE_STATIC_LOCAL(const Vector<AtomicString>, keyboard_event_types,
                      ({event_type_names::kKeydown, event_type_names::kKeypress,
                        event_type_names::kKeyup}));
  // Include modern pointer and touch events in addition to legacy mouse
  // events, as interactive canvas applications frequently bind to pointer
  // or touch handlers instead of mouse handlers.
  DEFINE_STATIC_LOCAL(
      const Vector<AtomicString>, interactive_event_types,
      ({event_type_names::kClick, event_type_names::kMousedown,
        event_type_names::kMouseup, event_type_names::kMousemove,
        event_type_names::kMouseover, event_type_names::kMouseout,
        event_type_names::kPointerdown, event_type_names::kPointerup,
        event_type_names::kPointermove, event_type_names::kPointercancel,
        event_type_names::kPointerover, event_type_names::kPointerout,
        event_type_names::kTouchstart, event_type_names::kTouchmove,
        event_type_names::kTouchend, event_type_names::kTouchcancel}));

  bool has_keyboard_listener =
      canvas_element->HasAnyEventListeners(keyboard_event_types);
  bool has_mouse_listener =
      canvas_element->HasAnyEventListeners(interactive_event_types);

  const auto& ukm_params = canvas_element->GetUkmParameters();
  auto ukm_builder = ukm::builders::Accessibility_Canvas(ukm_params.source_id);
  ukm_builder.SetRenderingContext(static_cast<int>(canvas_rendering_type))
      .SetIsOffscreen(is_offscreen)
      .SetHasKeyboardListener(has_keyboard_listener)
      .SetHasMouseListener(has_mouse_listener)
      .SetHasText(did_draw_text);

  if (base::FeatureList::IsEnabled(
          ::features::kEnableCollectAccessibilityHeuristicInCanvasUkm)) {
    ukm_builder.SetNeedsAccessibilitySupport(
        canvas_element->GetNeedsAccessibilitySupportHeuristic());
  }

  ukm_builder.Record(ukm_params.ukm_recorder);
}

void RecordPlaceholderCanvasUKMOnMainThread(
    DOMNodeId placeholder_canvas_id,
    CanvasRenderingContext::CanvasRenderingAPI canvas_rendering_type,
    bool did_draw_text) {
  DCHECK(IsMainThread());
  auto* placeholder = DynamicTo<HTMLCanvasElement>(
      DOMNodeIds::NodeForId(placeholder_canvas_id));
  if (!placeholder || !placeholder->IsDisplayed()) {
    // If the placeholder element is undisplayed or detached, recording is
    // skipped (worker contexts do not reschedule).
    return;
  }
  RecordAccessibilityUKMForCanvasElement(placeholder, canvas_rendering_type,
                                         /*is_offscreen=*/true, did_draw_text);
}

}  // namespace

CanvasRenderingContext::CanvasRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs,
    CanvasRenderingAPI canvas_rendering_API)
    : ActiveScriptWrappable<CanvasRenderingContext>({}),
      host_(host),
      creation_attributes_(attrs),
      canvas_rendering_type_(canvas_rendering_API) {
  // The following check is for investigating crbug.com/1470622
  // If the crash stops happening in CanvasRenderingContext2D::
  // GetOrCreatePaintCanvas(), and starts happening here instead,
  // then we'll know that the bug is related to creation and the
  // new crash reports pointing to this location will provide more
  // actionable feedback on how to fix the issue. If the crash
  // continues to happen at the old location, then we'll know that
  // the problem has to do with a pre-finalizer being called
  // prematurely.
  CHECK(host_);
}

void CanvasRenderingContext::Dispose() {
  RenderTaskEnded();

  if (did_schedule_accessibility_ukm_recording_) {
    RecordUKMCanvasAccessibility();
  }

  // HTMLCanvasElement and CanvasRenderingContext have a circular reference.
  // When the pair is no longer reachable, their destruction order is non-
  // deterministic, so the first of the two to be destroyed needs to notify
  // the other in order to break the circular reference.  This is to avoid
  // an error when CanvasRenderingContext::DidProcessTask() is invoked
  // after the HTMLCanvasElement is destroyed.
  if (CanvasRenderingContextHost* host = Host()) [[likely]] {
    if (host->RenderingContext() == this) {
      host->DetachContext();
    }
    host_ = nullptr;
  }
}

// static
CanvasRenderingContext*
CanvasRenderingContext::GetEnclosingContextForDrawElement(
    Element* element,
    const String& func_name,
    ExceptionState& exception_state) {
  auto* canvas = element->CanvasForDrawing();
  if (!canvas) {
    exception_state.ThrowTypeError(
        StrCat({"Only drawable descendants of the <canvas> element can be "
                "passed to ",
                func_name, "."}));
    return nullptr;
  }
  CanvasRenderingContext* context = canvas->RenderingContext();
  if (!context) {
    exception_state.ThrowTypeError(StrCat(
        {func_name, ": containing canvas does not have a rendering context."}));
    return nullptr;
  }
  if (!context->IsDrawElementImageEligible(element, func_name,
                                           exception_state)) {
    return nullptr;
  }
  return context;
}

bool CanvasRenderingContext::IsDrawElementImageEligible(
    Element* element,
    const String& func_name,
    ExceptionState& exception_state) {
  if (!Host() || Host()->IsOffscreenCanvas()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Elements cannot be drawn into an OffscreenCanvas.");
    return false;
  }

  HTMLCanvasElement* canvas_element = static_cast<HTMLCanvasElement*>(Host());
  if (!canvas_element || !canvas_element->GetDocument().View()) {
    return false;
  }

  return canvas_element->VerifyDrawElementImageEligibility(element, func_name,
                                                           exception_state);
}

bool CanvasRenderingContext::IsDrawElementImageEligible(
    const V8UnionElementOrElementImage* element_or_image,
    const String& func_name,
    ExceptionState& exception_state) {
  if (element_or_image->IsElement()) {
    return IsDrawElementImageEligible(element_or_image->GetAsElement(),
                                      func_name, exception_state);
  }

  const auto& record = element_or_image->GetAsElementImage()->PaintRecord();
  if (!record) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The ElementImage has been closed.");
    return false;
  }

  DOMNodeId current_canvas_node_id = kInvalidDOMNodeId;
  if (Host()) {
    if (!Host()->IsOffscreenCanvas()) {
      current_canvas_node_id =
          static_cast<HTMLCanvasElement*>(Host())->GetDomNodeId();
    } else {
      current_canvas_node_id =
          static_cast<OffscreenCanvas*>(Host())->PlaceholderCanvasId();
    }
  }

  if (current_canvas_node_id == kInvalidDOMNodeId ||
      record->paint_state.canvas_node_id != current_canvas_node_id) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The source was captured from a different canvas.");
    return false;
  }
  return true;
}

std::optional<CanvasDrawablePaintRecord>
CanvasRenderingContext::GetDrawablePaintRecord(Element* element) {
  return Host()->GetCanvasDrawablePaintRecord(element->GetDomNodeId());
}

scoped_refptr<StaticBitmapImage> CanvasRenderingContext::GetElementImage(
    const V8UnionElementOrElementImage* element,
    std::optional<float> sx,
    std::optional<float> sy,
    std::optional<float> swidth,
    std::optional<float> sheight,
    std::optional<uint32_t> width,
    std::optional<uint32_t> height,
    gpu::SharedImageUsageSet usage,
    const String& func_name,
    ExceptionState& exception_state) {
  if (!IsDrawElementImageEligible(element, func_name, exception_state)) {
    return nullptr;
  }

  std::optional<CanvasDrawablePaintRecord> drawable_paint_record;
  if (element->IsElement()) {
    drawable_paint_record = GetDrawablePaintRecord(element->GetAsElement());
  } else {
    if (const auto& record = element->GetAsElementImage()->PaintRecord()) {
      drawable_paint_record = *record;
    }
  }

  if (!drawable_paint_record) {
    if (element->IsElementImage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The ElementImage has been closed.");
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "No cached paint record for element.");
    }
    return nullptr;
  }

  // Element size in physical coordinates.
  gfx::RectF src_rect(drawable_paint_record->paint_state.box_size);
  if (sx && sy && swidth && sheight) {
    if (!CanvasRenderingContext::ValidateRectForCanvas(*sx, *sy, *swidth,
                                                       *sheight)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The source rect is empty or not finite");
      return nullptr;
    }
    AdjustRectForCanvas(*sx, *sy, *swidth, *sheight);
    float dpr = drawable_paint_record->paint_state.effective_zoom;
    src_rect = gfx::RectF(*sx * dpr, *sy * dpr, *swidth * dpr, *sheight * dpr);
  }

  // The default destination size for GetElementImage is the source content
  // size scaled to canvas grid coordinates. This causes the element to have
  // the same proportions when appearing inside the canvas as it would have
  // were it painted outside the canvas.
  gfx::SizeF intrinsic_size(src_rect.size());
  gfx::Vector2dF canvas_scale = GetCanvasGridScaleFactor(
      drawable_paint_record->paint_state, Host()->Size());
  intrinsic_size.Scale(canvas_scale.x(), canvas_scale.y());
  gfx::Size intrinsic_dest_size = gfx::ToCeiledSize(intrinsic_size);
  gfx::Size dest_size(intrinsic_dest_size);
  if (width && height) {
    dest_size = gfx::Size(width.value(), height.value());
    canvas_scale.Scale(
        static_cast<float>(dest_size.width()) / intrinsic_dest_size.width(),
        static_cast<float>(dest_size.height()) / intrinsic_dest_size.height());
  }
  if (dest_size.IsEmpty()) {
    return nullptr;
  }

  auto draw_to_canvas = [&](cc::PaintCanvas& canvas) {
    canvas.scale(canvas_scale.x(), canvas_scale.y());
    canvas.translate(-src_rect.x(), -src_rect.y());
    canvas.drawPicture(drawable_paint_record->record);
  };

  if (base::FeatureList::IsEnabled(kAllowAcceleratedTexElement) &&
      SharedGpuContext::IsGpuCompositingEnabled()) {
    if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
      return AcceleratedStaticBitmapImage::CreateFromRaster(
          dest_size, GetN32FormatForCanvas(), kPremul_SkAlphaType,
          gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(), wrapper, usage,
          draw_to_canvas,
          drawable_paint_record->paint_state.animated_image_frame_index_map);
    }
  }

  return UnacceleratedStaticBitmapImage::CreateFromRaster(
      dest_size, draw_to_canvas,
      drawable_paint_record->paint_state.animated_image_frame_index_map);
}

void CanvasRenderingContext::DidDraw(
    const gfx::Rect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  CanvasRenderingContextHost* const host = Host();
  host->DidDraw(dirty_rect);

  did_draw_text_ |= (draw_type == CanvasPerformanceMonitor::DrawType::kText);

  auto& monitor = GetCanvasPerformanceMonitor();
  monitor.DidDraw(draw_type);
  if (did_draw_in_current_task_)
    return;

  monitor.CurrentTaskDrawsToContext(this);
  did_draw_in_current_task_ = true;
  // We need to store whether the document is being printed because the
  // document may exit printing state by the time DidProcessTask is called.
  // This is an issue with beforeprint event listeners.
  did_print_in_current_task_ |= host->IsPrinting();
  Thread::Current()->AddTaskObserver(this);
}

void CanvasRenderingContext::DidProcessTask(
    const base::PendingTask& /* pending_task */) {
  RenderTaskEnded();

  // The end of a script task that drew content to the canvas is the point
  // at which the current frame may be considered complete.
  FlushReason reason = did_print_in_current_task_
                           ? FlushReason::kCanvasPushFrameWhilePrinting
                           : FlushReason::kCanvasPushFrame;
  FinalizeFrame(reason);
  did_print_in_current_task_ = false;
  if (CanvasRenderingContextHost* host = Host()) [[likely]] {
    host->PostFinalizeFrame(reason);
  }

  did_process_task_ = true;
  MaybeRecordUKMCanvasAccessibility();
}

void CanvasRenderingContext::MaybeRecordUKMCanvasAccessibility() {
  if (!should_record_accessibility_ukm_ || !did_process_task_) {
    return;
  }

  if (did_schedule_accessibility_ukm_recording_) {
    return;
  }
  CanvasRenderingContextHost* const host = Host();
  if (!host) {
    return;
  }
  if (host->IsOffscreenCanvas() &&
      !static_cast<OffscreenCanvas*>(host)->HasPlaceholderCanvas()) {
    should_record_accessibility_ukm_ = false;
    return;
  }
  ExecutionContext* execution_context = host->GetTopExecutionContext();
  if (!execution_context) {
    return;
  }
  did_schedule_accessibility_ukm_recording_ = true;
  execution_context->GetTaskRunner(TaskType::kInternalDefault)
      ->PostDelayedTask(
          FROM_HERE,
          blink::BindOnce(&CanvasRenderingContext::RecordUKMCanvasAccessibility,
                          WrapWeakPersistent(this)),
          kAccessibilityUkmRecordingDelay);
}

void CanvasRenderingContext::RecordUKMCanvasAccessibility() {
  if (!should_record_accessibility_ukm_ || !did_process_task_) {
    return;
  }

  CanvasRenderingContextHost* const host = Host();
  if (!host) {
    return;
  }

  bool is_offscreen = host->IsOffscreenCanvas();
  if (is_offscreen) {
    auto* offscreen_canvas = static_cast<OffscreenCanvas*>(host);
    if (!offscreen_canvas->HasPlaceholderCanvas()) {
      should_record_accessibility_ukm_ = false;
      return;
    }
    DOMNodeId placeholder_id = offscreen_canvas->PlaceholderCanvasId();
    if (IsMainThread()) {
      auto* placeholder =
          DynamicTo<HTMLCanvasElement>(DOMNodeIds::NodeForId(placeholder_id));
      if (!placeholder) {
        // Placeholder DOM node has been destroyed. Fail closed to prevent
        // infinite rescheduling loops.
        should_record_accessibility_ukm_ = false;
        return;
      }
      if (!placeholder->IsDisplayed()) {
        // Reset the scheduling flag so that when the placeholder becomes
        // displayed later, UKM recording will be rescheduled.
        did_schedule_accessibility_ukm_recording_ = false;
        return;
      }
      RecordAccessibilityUKMForCanvasElement(
          placeholder, canvas_rendering_type_,
          /*is_offscreen=*/true, did_draw_text_);
      should_record_accessibility_ukm_ = false;
    } else {
      // Worker thread: post a cross-thread task to query the placeholder on
      // the main thread and record the UKM event there.
      if (auto* context = host->GetTopExecutionContext()) {
        if (auto task_runner =
                context->GetAgentGroupSchedulerCompositorTaskRunner()) {
          // Note: UKM recording for worker offscreen canvases is attempted
          // once. If the placeholder element is not displayed when this task
          // executes, no metrics are emitted and recording is not rescheduled.
          PostCrossThreadTask(
              *task_runner, FROM_HERE,
              CrossThreadBindOnce(&RecordPlaceholderCanvasUKMOnMainThread,
                                  placeholder_id, canvas_rendering_type_,
                                  did_draw_text_));
        }
      }
      should_record_accessibility_ukm_ = false;
    }
    return;
  }

  HTMLCanvasElement* canvas_element = static_cast<HTMLCanvasElement*>(host);
  // Non offscreen canvases are only collected if they are visible.
  if (!canvas_element->IsDisplayed()) {
    // Reset the scheduling flag so that when the canvas becomes displayed
    // later, UKM recording will be rescheduled.
    did_schedule_accessibility_ukm_recording_ = false;
    return;
  }

  RecordAccessibilityUKMForCanvasElement(canvas_element, canvas_rendering_type_,
                                         /*is_offscreen=*/false,
                                         did_draw_text_);

  should_record_accessibility_ukm_ = false;
}

void CanvasRenderingContext::RecordUMACanvasRenderingAPI() {
  const CanvasRenderingContextHost* const host = Host();
  if (auto* window =
          DynamicTo<LocalDOMWindow>(host->GetTopExecutionContext())) {
    WebFeature feature;
    if (host->IsOffscreenCanvas()) {
      switch (canvas_rendering_type_) {
        case CanvasRenderingContext::CanvasRenderingAPI::k2D:
          feature = WebFeature::kOffscreenCanvas_2D;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kWebgl:
          feature = WebFeature::kOffscreenCanvas_WebGL;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kWebgl2:
          feature = WebFeature::kOffscreenCanvas_WebGL2;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer:
          feature = WebFeature::kOffscreenCanvas_BitmapRenderer;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kWebgpu:
          feature = WebFeature::kOffscreenCanvas_WebGPU;
          break;
        default:
          NOTREACHED();
      }
    } else {
      switch (canvas_rendering_type_) {
        case CanvasRenderingContext::CanvasRenderingAPI::k2D:
          feature = WebFeature::kHTMLCanvasElement_2D;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kWebgl:
          feature = WebFeature::kHTMLCanvasElement_WebGL;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kWebgl2:
          feature = WebFeature::kHTMLCanvasElement_WebGL2;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer:
          feature = WebFeature::kHTMLCanvasElement_BitmapRenderer;
          break;
        case CanvasRenderingContext::CanvasRenderingAPI::kWebgpu:
          feature = WebFeature::kHTMLCanvasElement_WebGPU;
          break;
        default:
          NOTREACHED();
      }
    }
    UseCounter::Count(window->document(), feature);
  }
}

void CanvasRenderingContext::RecordUKMCanvasRenderingAPI() {
  CanvasRenderingContextHost* const host = Host();
  DCHECK(host);
  const auto& ukm_params = host->GetUkmParameters();
  if (host->IsOffscreenCanvas()) {
    ukm::builders::ClientRenderingAPI(ukm_params.source_id)
        .SetOffscreenCanvas_RenderingContext(
            static_cast<int>(canvas_rendering_type_))
        .Record(ukm_params.ukm_recorder);
  } else {
    ukm::builders::ClientRenderingAPI(ukm_params.source_id)
        .SetCanvas_RenderingContext(static_cast<int>(canvas_rendering_type_))
        .Record(ukm_params.ukm_recorder);
  }
}

void CanvasRenderingContext::RecordUKMCanvasDrawnToRenderingAPI() {
  CanvasRenderingContextHost* const host = Host();
  DCHECK(host);
  const auto& ukm_params = host->GetUkmParameters();
  if (host->IsOffscreenCanvas()) {
    ukm::builders::ClientRenderingAPI(ukm_params.source_id)
        .SetOffscreenCanvas_RenderingContextDrawnTo(
            static_cast<int>(canvas_rendering_type_))
        .Record(ukm_params.ukm_recorder);
  } else {
    ukm::builders::ClientRenderingAPI(ukm_params.source_id)
        .SetCanvas_RenderingContextDrawnTo(
            static_cast<int>(canvas_rendering_type_))
        .Record(ukm_params.ukm_recorder);
  }
}

CanvasRenderingContext::CanvasRenderingAPI
CanvasRenderingContext::RenderingAPIFromId(const String& id) {
  if (id == "2d") {
    return CanvasRenderingAPI::k2D;
  }
  if (id == "experimental-webgl") {
    return CanvasRenderingAPI::kWebgl;
  }
  if (id == "webgl") {
    return CanvasRenderingAPI::kWebgl;
  }
  if (id == "webgl2") {
    return CanvasRenderingAPI::kWebgl2;
  }
  if (id == "bitmaprenderer") {
    return CanvasRenderingAPI::kBitmaprenderer;
  }
  if (id == "webgpu") {
    return CanvasRenderingAPI::kWebgpu;
  }
  return CanvasRenderingAPI::kUnknown;
}

void CanvasRenderingContext::Trace(Visitor* visitor) const {
  visitor->Trace(host_);
  ActiveScriptWrappable::Trace(visitor);
}

void CanvasRenderingContext::RenderTaskEnded() {
  if (!did_draw_in_current_task_)
    return;

  Thread::Current()->RemoveTaskObserver(this);
  did_draw_in_current_task_ = false;
}

CanvasPerformanceMonitor&
CanvasRenderingContext::GetCanvasPerformanceMonitor() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<CanvasPerformanceMonitor>,
                                  monitor, ());
  return *monitor;
}

}  // namespace blink
