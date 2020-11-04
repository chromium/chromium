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
    const CanvasContextCreationAttributesCore& attrs)
    : host_(host),
      color_params_(CanvasColorSpace::kSRGB,
                    CanvasColorParams::GetNativeCanvasPixelFormat(),
                    kNonOpaque),
      creation_attributes_(attrs) {
  if (creation_attributes_.pixel_format == kF16CanvasPixelFormatName)
    color_params_.SetCanvasPixelFormat(CanvasPixelFormat::kF16);

  if (creation_attributes_.color_space == kRec2020CanvasColorSpaceName)
    color_params_.SetCanvasColorSpace(CanvasColorSpace::kRec2020);
  else if (creation_attributes_.color_space == kP3CanvasColorSpaceName)
    color_params_.SetCanvasColorSpace(CanvasColorSpace::kP3);

  if (!creation_attributes_.alpha)
    color_params_.SetOpacityMode(kOpaque);

  // Make creation_attributes_ reflect the effective color_space and
  // pixel_format rather than the requested one.
  creation_attributes_.color_space = ColorSpaceAsString();
  creation_attributes_.pixel_format = PixelFormatAsString();
}

WTF::String CanvasRenderingContext::ColorSpaceAsString() const {
  switch (color_params_.ColorSpace()) {
    case CanvasColorSpace::kSRGB:
      return kSRGBCanvasColorSpaceName;
    case CanvasColorSpace::kRec2020:
      return kRec2020CanvasColorSpaceName;
    case CanvasColorSpace::kP3:
      return kP3CanvasColorSpaceName;
  };
  CHECK(false);
  return "";
}

WTF::String CanvasRenderingContext::PixelFormatAsString() const {
  switch (color_params_.PixelFormat()) {
    case CanvasPixelFormat::kRGBA8:
      return kRGBA8CanvasPixelFormatName;
    case CanvasPixelFormat::kF16:
      return kF16CanvasPixelFormatName;
    case CanvasPixelFormat::kBGRA8:
      return kBGRA8CanvasPixelFormatName;
  };
  CHECK(false);
  return "";
}

void CanvasRenderingContext::Dispose() {
  StopListeningForDidProcessTask();

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

void CanvasRenderingContext::DidDraw(const SkIRect& dirty_rect) {
  Host()->DidDraw(SkRect::Make(dirty_rect));
  StartListeningForDidProcessTask();
}

void CanvasRenderingContext::DidDraw() {
  Host()->DidDraw();
  StartListeningForDidProcessTask();
}

void CanvasRenderingContext::DidProcessTask(
    const base::PendingTask& /* pending_task */) {
  StopListeningForDidProcessTask();

  // The end of a script task that drew content to the canvas is the point
  // at which the current frame may be considered complete.
  if (Host())
    Host()->PreFinalizeFrame();
  FinalizeFrame();
  if (Host())
    Host()->PostFinalizeFrame();
}

void CanvasRenderingContext::RecordUKMCanvasRenderingAPI(
    CanvasRenderingAPI canvasRenderingAPI) {
  DCHECK(Host());
  const auto& ukm_params = Host()->GetUkmParameters();
  if (Host()->IsOffscreenCanvas()) {
    ukm::builders::ClientRenderingAPI(ukm_params.source_id)
        .SetOffscreenCanvas_RenderingContext(
            static_cast<int>(canvasRenderingAPI))
        .Record(ukm_params.ukm_recorder);
  } else {
    ukm::builders::ClientRenderingAPI(ukm_params.source_id)
        .SetCanvas_RenderingContext(static_cast<int>(canvasRenderingAPI))
        .Record(ukm_params.ukm_recorder);
  }
}

CanvasRenderingContext::ContextType CanvasRenderingContext::ContextTypeFromId(
    const String& id) {
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
  if (id == "gpupresent" && RuntimeEnabledFeatures::WebGPUEnabled())
    return kContextGPUPresent;
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
}

void CanvasRenderingContext::StartListeningForDidProcessTask() {
  if (listening_for_did_process_task_)
    return;

  listening_for_did_process_task_ = true;
  Thread::Current()->AddTaskObserver(this);
}

void CanvasRenderingContext::StopListeningForDidProcessTask() {
  if (!listening_for_did_process_task_)
    return;

  Thread::Current()->RemoveTaskObserver(this);
  listening_for_did_process_task_ = false;
}

}  // namespace blink
