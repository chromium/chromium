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

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/workers/worker_animation_frame_provider.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

CanvasRenderingContext::CanvasRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : host_(host),
      color_params_(kSRGBCanvasColorSpace, kRGBA8CanvasPixelFormat, kNonOpaque),
      creation_attributes_(attrs) {
  // Supported color spaces and pixel formats: sRGB in uint8, e-sRGB in f16,
  // linear sRGB and p3 and rec2020 with linear gamma transfer function in f16.
  // For wide gamut color spaces, user must explicitly request half float
  // storage. Otherwise, we fall back to sRGB in uint8. Invalid requests fall
  // back to sRGB in uint8 too.
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    if (creation_attributes_.pixel_format == kF16CanvasPixelFormatName) {
      color_params_.SetCanvasPixelFormat(kF16CanvasPixelFormat);
      if (creation_attributes_.color_space == kLinearRGBCanvasColorSpaceName)
        color_params_.SetCanvasColorSpace(kLinearRGBCanvasColorSpace);
      if (creation_attributes_.color_space == kRec2020CanvasColorSpaceName)
        color_params_.SetCanvasColorSpace(kRec2020CanvasColorSpace);
      else if (creation_attributes_.color_space == kP3CanvasColorSpaceName)
        color_params_.SetCanvasColorSpace(kP3CanvasColorSpace);
    }
  }

  if (!creation_attributes_.alpha)
    color_params_.SetOpacityMode(kOpaque);

  if (!OriginTrials::LowLatencyCanvasEnabled(host->GetTopExecutionContext()))
    creation_attributes_.low_latency = false;

  // Make creation_attributes_ reflect the effective color_space and
  // pixel_format rather than the requested one.
  creation_attributes_.color_space = ColorSpaceAsString();
  creation_attributes_.pixel_format = PixelFormatAsString();
}

WTF::String CanvasRenderingContext::ColorSpaceAsString() const {
  switch (color_params_.ColorSpace()) {
    case kSRGBCanvasColorSpace:
      return kSRGBCanvasColorSpaceName;
    case kLinearRGBCanvasColorSpace:
      return kLinearRGBCanvasColorSpaceName;
    case kRec2020CanvasColorSpace:
      return kRec2020CanvasColorSpaceName;
    case kP3CanvasColorSpace:
      return kP3CanvasColorSpaceName;
  };
  CHECK(false);
  return "";
}

WTF::String CanvasRenderingContext::PixelFormatAsString() const {
  switch (color_params_.PixelFormat()) {
    case kRGBA8CanvasPixelFormat:
      return kRGBA8CanvasPixelFormatName;
    case kF16CanvasPixelFormat:
      return kF16CanvasPixelFormatName;
  };
  CHECK(false);
  return "";
}

void CanvasRenderingContext::Dispose() {
  if (finalize_frame_scheduled_)
    Platform::Current()->CurrentThread()->RemoveTaskObserver(this);

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
  NeedsFinalizeFrame();
}

void CanvasRenderingContext::DidDraw() {
  Host()->DidDraw();
  NeedsFinalizeFrame();
}

void CanvasRenderingContext::NeedsFinalizeFrame() {
  if (!finalize_frame_scheduled_) {
    finalize_frame_scheduled_ = true;
    Platform::Current()->CurrentThread()->AddTaskObserver(this);
  }
}

void CanvasRenderingContext::DidProcessTask(
    const base::PendingTask& pending_task) {
  Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
  finalize_frame_scheduled_ = false;
  // The end of a script task that drew content to the canvas is the point
  // at which the current frame may be considered complete.
  if (Host())
    Host()->FinalizeFrame();
  FinalizeFrame();
}

CanvasRenderingContext::ContextType CanvasRenderingContext::ContextTypeFromId(
    const String& id) {
  if (id == "2d")
    return kContext2d;
  if (id == "experimental-webgl")
    return kContextExperimentalWebgl;
  if (id == "webgl")
    return kContextWebgl;
  if (id == "webgl2")
    return kContextWebgl2;
  if (id == "webgl2-compute" &&
      RuntimeEnabledFeatures::WebGL2ComputeContextEnabled())
    return kContextWebgl2Compute;
  if (id == "bitmaprenderer")
    return kContextImageBitmap;
  if (id == "xrpresent")
    return kContextXRPresent;
  return kContextTypeUnknown;
}

CanvasRenderingContext::ContextType
CanvasRenderingContext::ResolveContextTypeAliases(
    CanvasRenderingContext::ContextType type) {
  if (type == kContextExperimentalWebgl)
    return kContextWebgl;
  return type;
}

bool CanvasRenderingContext::WouldTaintOrigin(
    CanvasImageSource* image_source,
    const SecurityOrigin* destination_security_origin) {
  const KURL& source_url = image_source->SourceURL();
  const bool has_url = (source_url.IsValid() && !source_url.IsAboutBlankURL());

  if (has_url) {
    if (source_url.ProtocolIsData() ||
        clean_urls_.Contains(source_url.GetString())) {
      return false;
    }
    if (dirty_urls_.Contains(source_url.GetString()))
      return true;
  }

  const bool taint_origin =
      image_source->WouldTaintOrigin(destination_security_origin);
  if (has_url) {
    if (taint_origin)
      dirty_urls_.insert(source_url.GetString());
    else
      clean_urls_.insert(source_url.GetString());
  }
  return taint_origin;
}

void CanvasRenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(host_);
  visitor->Trace(creation_attributes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
