// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"

namespace blink {

V8RenderingContext* HTMLCanvasElementModule::getContext(
    ScriptState* script_state,
    HTMLCanvasElement& canvas,
    const String& context_id,
    const CanvasContextCreationAttributesModule* attributes,
    ExceptionState& exception_state) {
  if (canvas.IsOffscreenCanvasRegistered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot get context from a canvas that "
                                      "has transferred its control to "
                                      "offscreen.");
    return nullptr;
  }

  CanvasContextCreationAttributesCore canvas_context_creation_attributes;
  if (!ToCanvasContextCreationAttributes(
          attributes, canvas_context_creation_attributes, exception_state)) {
    return nullptr;
  }
  CanvasRenderingContext* context = canvas.GetCanvasRenderingContext(
      ExecutionContext::From(script_state), context_id,
      canvas_context_creation_attributes);
  if (!context)
    return nullptr;
  return context->AsV8RenderingContext();
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreen(
    ScriptState* script_state,
    HTMLCanvasElement& canvas,
    ExceptionState& exception_state) {
  if (canvas.RenderingContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot transfer control from a canvas that has a rendering context.");
    return nullptr;
  }

  OffscreenCanvas* offscreen_canvas = nullptr;
  if (canvas.IsOffscreenCanvasRegistered()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot transfer control from a canvas for more than one time.");
  } else {
    canvas.CreateLayer();
    offscreen_canvas = TransferControlToOffscreenInternal(script_state, canvas);
  }

  base::UmaHistogramBoolean("Blink.OffscreenCanvas.TransferControlToOffscreen",
                            !!offscreen_canvas);
  return offscreen_canvas;
}

OffscreenCanvas* HTMLCanvasElementModule::TransferControlToOffscreenInternal(
    ScriptState* script_state,
    HTMLCanvasElement& canvas) {
  OffscreenCanvas* offscreen_canvas =
      OffscreenCanvas::Create(script_state, canvas.width(), canvas.height());

  DOMNodeId canvas_id = canvas.GetDomNodeId();
  canvas.RegisterPlaceholderCanvas(static_cast<int>(canvas_id));
  offscreen_canvas->SetPlaceholderCanvasId(canvas_id);
  offscreen_canvas->SetTextDirection(canvas.GetTextDirection(nullptr));
  offscreen_canvas->SetLocale(canvas.GetLocale());

  SurfaceLayerBridge* bridge = canvas.SurfaceLayerBridge();
  if (bridge) {
    offscreen_canvas->SetFrameSinkId(bridge->GetFrameSinkId().client_id(),
                                     bridge->GetFrameSinkId().sink_id());
  }
  return offscreen_canvas;
}

}  // namespace blink
