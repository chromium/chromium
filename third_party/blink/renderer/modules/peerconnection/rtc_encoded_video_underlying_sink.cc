// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedVideoUnderlyingSink::RTCEncodedVideoUnderlyingSink(
    ScriptState* script_state,
    TransformerCallback transformer_callback,
    webrtc::TransformableFrameInterface::Direction expected_direction)
    : transformer_callback_(std::move(transformer_callback)),
      expected_direction_(expected_direction) {
  DCHECK(transformer_callback_);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState&) {
  // No extra setup needed.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  RTCEncodedVideoFrame* encoded_frame =
      V8RTCEncodedVideoFrame::ToImplWithTypeCheck(script_state->GetIsolate(),
                                                  chunk.V8Value());
  if (!encoded_frame) {
    exception_state.ThrowTypeError("Invalid frame");
    return ScriptPromise();
  }

  if (!transformer_callback_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
  }

  auto webrtc_frame = encoded_frame->PassWebRtcFrame();
  if (!webrtc_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Empty frame");
    return ScriptPromise();
  }

  if (webrtc_frame->GetDirection() != expected_direction_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Invalid frame");
    return ScriptPromise();
  }

  RTCEncodedVideoStreamTransformer* transformer = transformer_callback_.Run();
  if (!transformer) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No underlying sink");
    return ScriptPromise();
  }

  transformer->SendFrameToSink(std::move(webrtc_frame));
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::close(ScriptState* script_state,
                                                   ExceptionState&) {
  // Disconnect from the transformer if the sink is closed.
  if (transformer_callback_)
    transformer_callback_.Reset();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // It is not possible to cancel any frames already sent to the WebRTC sink,
  // thus abort() has the same effect as close().
  return close(script_state, exception_state);
}

void RTCEncodedVideoUnderlyingSink::Trace(Visitor* visitor) const {
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
