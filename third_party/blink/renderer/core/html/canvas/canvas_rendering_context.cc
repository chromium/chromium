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

#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_element_hit_test_region.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_hit_test_rect.h"
#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

// A default size used for canvas memory allocation when canvas size is greater
// than 2^20.
constexpr int kMaximumCanvasSize = 2 << 20;

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

intptr_t CanvasRenderingContext::AllocatedBufferSize() const {
  if (!Host() || isContextLost()) {
    return 0;
  }
  int buffer_count = AllocatedBufferCountPerPixel();

  // NOTE: All formats used by canvas are either 8-bit or 16-bit.
  const int bytes_per_pixel = GetSharedImageFormat().BitsPerPixel() / 8;

  // Recomputation of externally memory usage computation is carried out
  // in all cases.
  base::CheckedNumeric<intptr_t> checked_usage = buffer_count * bytes_per_pixel;
  gfx::Size canvas_size = DrawingBufferSize();
  checked_usage *= std::min(kMaximumCanvasSize, canvas_size.width());
  checked_usage *= std::min(kMaximumCanvasSize, canvas_size.height());
  return checked_usage.ValueOrDefault(std::numeric_limits<intptr_t>::max());
}

void CanvasRenderingContext::Dispose() {
  RenderTaskEnded();

  // HTMLCanvasElement and CanvasRenderingContext have a circular reference.
  // When the pair is no longer reachable, their destruction order is non-
  // deterministic, so the first of the two to be destroyed needs to notify
  // the other in order to break the circular reference.  This is to avoid
  // an error when CanvasRenderingContext::DidProcessTask() is invoked
  // after the HTMLCanvasElement is destroyed.
  if (CanvasRenderingContextHost* host = Host()) [[likely]] {
    host->DetachContext();
    host_ = nullptr;
  }
}

bool CanvasRenderingContext::IsDrawElementImageEligible(
    Element* element,
    const String& func_name,
    ExceptionState& exception_state) {
  if (!Host() || Host()->IsOffscreenCanvas()) {
    return false;
  }

  HTMLCanvasElement* canvas_element = static_cast<HTMLCanvasElement*>(Host());
  if (!canvas_element || !canvas_element->GetDocument().View()) {
    return false;
  }

  auto build_error = [&func_name](const char* format) {
    StringBuilder builder;
    builder.AppendFormat(format, func_name.Utf8().c_str());
    return builder.ToString();
  };

  if (!RuntimeEnabledFeatures::CanvasDrawElementInSubtreeEnabled()) {
    if (element->parentElement() != canvas_element) {
      exception_state.ThrowTypeError(
          build_error("Only immediate children of the <canvas> element can be "
                      "passed to %s."));
      return false;
    }
  } else {
    if (!element->IsDescendantOf(canvas_element)) {
      exception_state.ThrowTypeError(build_error(
          "Only descendants of the <canvas> element can be passed to %s."));
      return false;
    }
    // TODO(pdr): Update these checks to point to the updated spec. These are
    // currently copied from element capture, which has similar paint reqs:
    // https://screen-share.github.io/element-capture/#elements-eligible-for-restriction
    auto* object = element->GetLayoutObject();
    if (!object || !object->IsStackingContext() || !object->CreatesGroup() ||
        !object->IsBox() ||
        To<LayoutBox>(object)->PhysicalFragmentCount() > 1) {
      exception_state.ThrowTypeError(
          build_error("Only elements with certain requirements (stacking "
                      "context, etc) can be passed to %s."));
      return false;
    }
  }

  if (!canvas_element->layoutSubtree()) {
    exception_state.ThrowTypeError(build_error(
        "<canvas> elements without layoutsubtree do not support %s."));
    return false;
  }

  if (!element->GetLayoutObject()) {
    exception_state.ThrowTypeError(build_error(
        "The canvas and element used with %s must have been laid "
        "out. Detached canvases are not supported, nor canvas or children that "
        "are `display: none`."));
    return false;
  }

  return true;
}

std::optional<cc::PaintRecord> CanvasRenderingContext::GetElementImage(
    Element* element,
    const String& func_name,
    ExceptionState& exception_state) {
  element->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kCanvasDrawElementImage);
  if (!IsDrawElementImageEligible(element, func_name, exception_state)) {
    return std::nullopt;
  }

  PaintRecordBuilder builder;
  LayoutBox* layout_box = element->GetLayoutBox();
  // All drawn elements should have their own stacking contexts.
  CHECK(layout_box->HasLayer());
  CHECK(layout_box->IsStacked());
  PaintLayer* layer = layout_box->EnclosingLayer();

  auto box_rect =
      gfx::Rect(ToCeiledSize(layer->GetLayoutBox()->StitchedSize()));
  // TODO(https://issues.chromium.org/379143301): Figure out the actual painted
  // rect of the element plus its descendants, and use that instead of the
  // box's size.
  OverriddenCullRectScope cull_rect_scope(*layer, CullRect(box_rect),
                                          /*disable_expansion*/ true);

  PaintLayerPainter paint_layer_painter = PaintLayerPainter(*layer);
  paint_layer_painter.Paint(
      builder.Context(),
      PaintFlag::kPrivacyPreserving | PaintFlag::kOmitCompositingInfo);

  // Use the drawn element's local property tree state to start drawing, but
  // then modify this to include effects and clips between the drawn element
  // and the canvas element. This will exclude transforms above the local
  // border box state (e.g., css transform is ignored), but will include effects
  // (e.g., css filter is not ignored).
  PropertyTreeState property_tree_state = layer->GetLayoutBox()
                                              ->FirstFragment()
                                              .LocalBorderBoxProperties()
                                              .Unalias();
  HTMLCanvasElement* canvas_element = static_cast<HTMLCanvasElement*>(Host());
  const auto& canvas_fragment = canvas_element->GetLayoutBox()->FirstFragment();
  property_tree_state.SetEffect(canvas_fragment.ContentsEffect().Unalias());
  property_tree_state.SetClip(canvas_fragment.ContentsClip().Unalias());

  cc::PaintRecord paint_record = builder.EndRecording(property_tree_state);
  return paint_record;
}

bool CanvasRenderingContext::ConvertHitTestRegionsToHTMLCanvasRegions(
    const HeapVector<Member<CanvasElementHitTestRegion>>& hit_test_regions,
    VectorOf<HTMLCanvasElement::ElementHitTestRegion>& result,
    const String& func_name,
    ExceptionState& exception_state) {
  for (const auto& region : hit_test_regions) {
    if (!IsDrawElementImageEligible(region->element(), func_name,
                                    exception_state)) {
      return false;
    }

    double width = [&]() -> double {
      if (region->rect()->hasWidth()) {
        return *region->rect()->width();
      }
      gfx::RectF bounds =
          region->element()->GetBoundingClientRectNoLifecycleUpdate();
      return bounds.width();
    }();

    double height = [&]() -> double {
      if (region->rect()->hasHeight()) {
        return *region->rect()->height();
      }
      gfx::RectF bounds =
          region->element()->GetBoundingClientRectNoLifecycleUpdate();
      return bounds.height();
    }();

    result.push_back(
        MakeGarbageCollected<HTMLCanvasElement::ElementHitTestRegion>(
            region->element(), gfx::RectF(region->rect()->x(),
                                          region->rect()->y(), width, height)));
  }
  return true;
}

void CanvasRenderingContext::DidDraw(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  CanvasRenderingContextHost* const host = Host();
  host->DidDraw(dirty_rect);

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
  PreFinalizeFrame();
  FlushReason reason = did_print_in_current_task_
                           ? FlushReason::kCanvasPushFrameWhilePrinting
                           : FlushReason::kCanvasPushFrame;
  FinalizeFrame(reason);
  did_print_in_current_task_ = false;
  if (CanvasRenderingContextHost* host = Host()) [[likely]] {
    host->PostFinalizeFrame(reason);
  }
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

CanvasRenderingContext::ElementHitTestRegion::ElementHitTestRegion(
    Element* element,
    const gfx::RectF& rect)
    : element_(element), rect_(rect) {}

void CanvasRenderingContext::ElementHitTestRegion::Trace(
    Visitor* visitor) const {
  visitor->Trace(element_);
}

}  // namespace blink
