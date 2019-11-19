// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_canvas_element_capture.h"

#include <memory>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_handler.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"

namespace {
const double kDefaultFrameRate = 60.0;
}  // anonymous namespace

namespace blink {

MediaStream* HTMLCanvasElementCapture::captureStream(
    ScriptState* script_state,
    HTMLCanvasElement& element,
    ExceptionState& exception_state) {
  return HTMLCanvasElementCapture::captureStream(script_state, element, false,
                                                 0, exception_state);
}

MediaStream* HTMLCanvasElementCapture::captureStream(
    ScriptState* script_state,
    HTMLCanvasElement& element,
    double frame_rate,
    ExceptionState& exception_state) {
  if (frame_rate < 0.0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Given frame rate is not supported.");
    return nullptr;
  }

  return HTMLCanvasElementCapture::captureStream(script_state, element, true,
                                                 frame_rate, exception_state);
}

MediaStream* HTMLCanvasElementCapture::captureStream(
    ScriptState* script_state,
    HTMLCanvasElement& element,
    bool given_frame_rate,
    double frame_rate,
    ExceptionState& exception_state) {
  if (!element.OriginClean()) {
    exception_state.ThrowSecurityError("Canvas is not origin-clean.");
    return nullptr;
  }

  LocalFrame* frame = ToLocalFrameIfNotDetached(script_state->GetContext());
  WebMediaStreamTrack track;
  const WebSize size(element.width(), element.height());
  std::unique_ptr<CanvasCaptureHandler> handler;
  if (given_frame_rate) {
    handler = CanvasCaptureHandler::CreateCanvasCaptureHandler(
        frame, size, frame_rate, Platform::Current()->GetIOTaskRunner(),
        &track);
  } else {
    handler = CanvasCaptureHandler::CreateCanvasCaptureHandler(
        frame, size, kDefaultFrameRate, Platform::Current()->GetIOTaskRunner(),
        &track);
  }

  if (!handler) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "No CanvasCapture handler can be created.");
    return nullptr;
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);
  CanvasCaptureMediaStreamTrack* canvas_track;
  if (given_frame_rate) {
    canvas_track = MakeGarbageCollected<CanvasCaptureMediaStreamTrack>(
        track, &element, context, std::move(handler), frame_rate);
  } else {
    canvas_track = MakeGarbageCollected<CanvasCaptureMediaStreamTrack>(
        track, &element, context, std::move(handler));
  }
  // We want to capture a frame in the beginning.
  canvas_track->requestFrame();

  MediaStreamTrackVector tracks;
  tracks.push_back(canvas_track);
  return MediaStream::Create(context, tracks);
}

}  // namespace blink
