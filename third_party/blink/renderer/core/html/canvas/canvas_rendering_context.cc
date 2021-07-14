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

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

CanvasRenderingContext::CanvasRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs,
    CanvasRenderingAPI canvas_rendering_API)
    : host_(host),
      color_params_(attrs.color_space, attrs.pixel_format, attrs.alpha),
      creation_attributes_(attrs),
      canvas_rendering_type_(canvas_rendering_API) {}

void CanvasRenderingContext::Dispose() {
  RenderTaskEnded();

  // HTMLCanvasElement and CanvasRenderingContext have a circular reference.
  // When the pair is no longer reachable, their destruction order is non-
  // deterministic, so the first of the two to be destroyed needs to notify
  // the other in order to break the circular reference.  This is to avoid
  // an error when CanvasRenderingContext::DidProcessTask() is invoked
  // after the HTMLCanvasElement is destroyed.
  if (Host()) {
    Host()->DetachContext();
    host_ = nullptr;
  }
}

void CanvasRenderingContext::DidDraw(
    const SkIRect& dirty_rect,
    CanvasPerformanceMonitor::DrawType draw_type) {
  Host()->DidDraw(dirty_rect);

  auto& monitor = GetCanvasPerformanceMonitor();
  monitor.DidDraw(draw_type);
  if (did_draw_in_current_task_)
    return;

  monitor.CurrentTaskDrawsToContext(this);
  did_draw_in_current_task_ = true;
  Thread::Current()->AddTaskObserver(this);
}

void CanvasRenderingContext::DidProcessTask(
    const base::PendingTask& /* pending_task */) {
  RenderTaskEnded();

  // The end of a script task that drew content to the canvas is the point
  // at which the current frame may be considered complete.
  if (Host())
    Host()->PreFinalizeFrame();
  FinalizeFrame();
  if (Host())
    Host()->PostFinalizeFrame();
}

void CanvasRenderingContext::RecordUKMCanvasRenderingAPI() {
  DCHECK(Host());
  const auto& ukm_params = Host()->GetUkmParameters();
  if (Host()->IsOffscreenCanvas()) {
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
  DCHECK(Host());
  const auto& ukm_params = Host()->GetUkmParameters();
  if (Host()->IsOffscreenCanvas()) {
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

CanvasRenderingContext::ContextType CanvasRenderingContext::ContextTypeFromId(
    const String& id,
    const ExecutionContext* execution_context) {
  if (id == "2d")
    return kContext2D;
  if (id == "experimental-webgl")
    return kContextExperimentalWebgl;
  if (id == "webgl")
    return kContextWebgl;
  if (id == "webgl2")
    return kContextWebgl2;
  if (id == "bitmaprenderer")
    return kContextImageBitmap;
  // TODO(crbug.com/1229274): Remove 'gpupresent' type after deprecation period.
  if ((id == "webgpu" || id == "gpupresent") &&
      RuntimeEnabledFeatures::WebGPUEnabled(execution_context))
    return kContextWebGPU;
  return kContextTypeUnknown;
}

CanvasRenderingContext::ContextType
CanvasRenderingContext::ResolveContextTypeAliases(
    CanvasRenderingContext::ContextType type) {
  if (type == kContextExperimentalWebgl)
    return kContextWebgl;
  return type;
}

bool CanvasRenderingContext::WouldTaintOrigin(CanvasImageSource* image_source) {
  // Don't taint the canvas on data URLs. This special case is needed here
  // because CanvasImageSource::WouldTaintOrigin() can return false for data
  // URLs due to restrictions on SVG foreignObject nodes as described in
  // https://crbug.com/294129.
  // TODO(crbug.com/294129): Remove the restriction on foreignObject nodes, then
  // this logic isn't needed, CanvasImageSource::SourceURL() isn't needed, and
  // this function can just be image_source->WouldTaintOrigin().
  const KURL& source_url = image_source->SourceURL();
  const bool has_url = (source_url.IsValid() && !source_url.IsAboutBlankURL());
  if (has_url && source_url.ProtocolIsData())
    return false;

  return image_source->WouldTaintOrigin();
}

void CanvasRenderingContext::Trace(Visitor* visitor) const {
  visitor->Trace(host_);
  ScriptWrappable::Trace(visitor);
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
