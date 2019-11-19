// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

void HTMLCanvasElementModule::getContext(
    HTMLCanvasElement& canvas,
    const String& type,
    const CanvasContextCreationAttributesModule* attributes,
    ExceptionState& exception_state,
    RenderingContext& result) {
  if (canvas.SurfaceLayerBridge() && !canvas.LowLatencyEnabled()) {
    // The existence of canvas surfaceLayerBridge indicates that
    // HTMLCanvasElement.transferControlToOffscreen() has been called.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot get context from a canvas that "
                                      "has transferred its control to "
                                      "offscreen.");
    return;
  }

  CanvasRenderingContext* context = canvas.GetCanvasRenderingContext(
      type, ToCanvasContextCreationAttributes(attributes));
  if (context)
    context->SetCanvasGetContextResult(result);
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreen(
    ExecutionContext* execution_context,
    HTMLCanvasElement& canvas,
    ExceptionState& exception_state) {
  OffscreenCanvas* offscreen_canvas = nullptr;
  if (canvas.SurfaceLayerBridge()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot transfer control from a canvas for more than one time.");
  } else {
    canvas.CreateLayer();
    offscreen_canvas = TransferControlToOffscreenInternal(
        execution_context, canvas, exception_state);
  }

  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.TransferControlToOffscreen",
                        bool(offscreen_canvas));
  return offscreen_canvas;
}

OffscreenCanvas* HTMLCanvasElementModule::TransferControlToOffscreenInternal(
    ExecutionContext* execution_context,
    HTMLCanvasElement& canvas,
    ExceptionState& exception_state) {
  if (canvas.RenderingContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot transfer control from a canvas that has a rendering context.");
    return nullptr;
  }
  OffscreenCanvas* offscreen_canvas = OffscreenCanvas::Create(
      execution_context, canvas.width(), canvas.height());
  offscreen_canvas->SetFilterQuality(canvas.FilterQuality());

  // If this canvas is cross-origin, then the associated offscreen canvas
  // should prefer using the low-power GPU.
  LocalFrame* frame = canvas.GetDocument().GetFrame();
  if (!(frame && frame->IsCrossOriginSubframe()))
    offscreen_canvas->AllowHighPerformancePowerPreference();

  DOMNodeId canvas_id = DOMNodeIds::IdForNode(&canvas);
  canvas.RegisterPlaceholderCanvas(static_cast<int>(canvas_id));
  offscreen_canvas->SetPlaceholderCanvasId(canvas_id);

  SurfaceLayerBridge* bridge = canvas.SurfaceLayerBridge();
  if (bridge) {
    offscreen_canvas->SetFrameSinkId(bridge->GetFrameSinkId().client_id(),
                                     bridge->GetFrameSinkId().sink_id());
  }
  return offscreen_canvas;
}
}  // namespace blink
