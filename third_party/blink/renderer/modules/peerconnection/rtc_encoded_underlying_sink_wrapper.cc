// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_underlying_sink_wrapper.h"

#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedUnderlyingSinkWrapper::RTCEncodedUnderlyingSinkWrapper(
    ScriptState* script_state)
    : script_state_(script_state) {}

void RTCEncodedUnderlyingSinkWrapper::CreateAudioUnderlyingSink(
    scoped_refptr<RTCEncodedAudioStreamTransformer::Broker>
        encoded_audio_transformer,
    base::UnguessableToken owner_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!video_to_packetizer_underlying_sink_);
  audio_to_packetizer_underlying_sink_ =
      MakeGarbageCollected<RTCEncodedAudioUnderlyingSink>(
          script_state_, std::move(encoded_audio_transformer),
          /*detach_frame_data_on_write=*/true,
          base::FeatureList::IsEnabled(
              kWebRtcRtpScriptTransformerFrameRestrictions),
          owner_id);
}

void RTCEncodedUnderlyingSinkWrapper::CreateVideoUnderlyingSink(
    scoped_refptr<RTCEncodedVideoStreamTransformer::Broker>
        encoded_video_transformer,
    base::UnguessableToken owner_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!audio_to_packetizer_underlying_sink_);
  video_to_packetizer_underlying_sink_ =
      MakeGarbageCollected<RTCEncodedVideoUnderlyingSink>(
          script_state_, std::move(encoded_video_transformer),
          /*detach_frame_data_on_write=*/true,
          base::FeatureList::IsEnabled(
              kWebRtcRtpScriptTransformerFrameRestrictions),
          owner_id);
}

ScriptPromise<IDLUndefined> RTCEncodedUnderlyingSinkWrapper::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No extra setup needed.
  return ToResolvedUndefinedPromise(script_state);
}

// It is possible that the application calls |write| before the audio or video
// underlying source are set, and the write will fail. In practice, this
// scenario is not an issue because the specification mandates that only
// previously read frames can be written.
ScriptPromise<IDLUndefined> RTCEncodedUnderlyingSinkWrapper::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_to_packetizer_underlying_sink_) {
    return video_to_packetizer_underlying_sink_->write(
        script_state, chunk, controller, exception_state);
  }
  if (audio_to_packetizer_underlying_sink_) {
    return audio_to_packetizer_underlying_sink_->write(
        script_state, chunk, controller, exception_state);
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Invalid state.");
  return ScriptPromise<IDLUndefined>();
}

ScriptPromise<IDLUndefined> RTCEncodedUnderlyingSinkWrapper::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_to_packetizer_underlying_sink_) {
    return video_to_packetizer_underlying_sink_->close(script_state,
                                                       exception_state);
  }
  if (audio_to_packetizer_underlying_sink_) {
    return audio_to_packetizer_underlying_sink_->close(script_state,
                                                       exception_state);
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Invalid state.");
  return ScriptPromise<IDLUndefined>();
}

ScriptPromise<IDLUndefined> RTCEncodedUnderlyingSinkWrapper::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_to_packetizer_underlying_sink_) {
    return video_to_packetizer_underlying_sink_->abort(script_state, reason,
                                                       exception_state);
  }
  if (audio_to_packetizer_underlying_sink_) {
    return audio_to_packetizer_underlying_sink_->abort(script_state, reason,
                                                       exception_state);
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Invalid state.");
  return ScriptPromise<IDLUndefined>();
}

void RTCEncodedUnderlyingSinkWrapper::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_to_packetizer_underlying_sink_) {
    video_to_packetizer_underlying_sink_->ResetTransformerCallback();
    video_to_packetizer_underlying_sink_ = nullptr;
  }
  if (audio_to_packetizer_underlying_sink_) {
    audio_to_packetizer_underlying_sink_->ResetTransformerCallback();
    audio_to_packetizer_underlying_sink_ = nullptr;
  }
}

void RTCEncodedUnderlyingSinkWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(audio_to_packetizer_underlying_sink_);
  visitor->Trace(video_to_packetizer_underlying_sink_);
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
