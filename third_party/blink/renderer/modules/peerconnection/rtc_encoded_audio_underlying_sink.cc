// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_sink.h"

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedAudioUnderlyingSink::RTCEncodedAudioUnderlyingSink(
    ScriptState* script_state,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        transformer_broker,
    bool detach_frame_data_on_write)
    : RTCEncodedAudioUnderlyingSink(script_state,
                                    std::move(transformer_broker),
                                    detach_frame_data_on_write,
                                    /*enable_frame_restrictions=*/false,
                                    base::UnguessableToken::Null()) {}

RTCEncodedAudioUnderlyingSink::RTCEncodedAudioUnderlyingSink(
    ScriptState* script_state,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        transformer_broker,
    bool detach_frame_data_on_write,
    bool enable_frame_restrictions,
    base::UnguessableToken owner_id)
    : transformer_broker_(std::move(transformer_broker)),
      detach_frame_data_on_write_(detach_frame_data_on_write),
      enable_frame_restrictions_(enable_frame_restrictions),
      owner_id_(owner_id) {
  DCHECK(transformer_broker_);
}

ScriptPromise<IDLUndefined> RTCEncodedAudioUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState&) {
  // No extra setup needed.
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> RTCEncodedAudioUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RTCEncodedAudioFrame* encoded_frame = V8RTCEncodedAudioFrame::ToWrappable(
      script_state->GetIsolate(), chunk.V8Value());
  if (!encoded_frame) {
    exception_state.ThrowTypeError("Invalid frame");
    return EmptyPromise();
  }

  if (enable_frame_restrictions_ &&
      (encoded_frame->OwnerId() != owner_id_ ||
       encoded_frame->Counter() <= last_received_frame_counter_)) {
    return EmptyPromise();
  }

  last_received_frame_counter_ = encoded_frame->Counter();

  if (!transformer_broker_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return EmptyPromise();
  }

  auto webrtc_frame = encoded_frame->PassWebRtcFrame(
      script_state->GetIsolate(), detach_frame_data_on_write_);
  if (!webrtc_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Empty frame");
    return EmptyPromise();
  }

  transformer_broker_->SendFrameToSink(std::move(webrtc_frame));
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> RTCEncodedAudioUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState&) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Disconnect from the transformer if the sink is closed.
  if (transformer_broker_)
    transformer_broker_.reset();
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> RTCEncodedAudioUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // It is not possible to cancel any frames already sent to the WebRTC sink,
  // thus abort() has the same effect as close().
  return close(script_state, exception_state);
}

void RTCEncodedAudioUnderlyingSink::ResetTransformerCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_broker_->ResetTransformerCallback();
}

void RTCEncodedAudioUnderlyingSink::Trace(Visitor* visitor) const {
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
