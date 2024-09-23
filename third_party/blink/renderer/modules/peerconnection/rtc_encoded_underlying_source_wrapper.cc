// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_underlying_source_wrapper.h"

#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedUnderlyingSourceWrapper::RTCEncodedUnderlyingSourceWrapper(
    ScriptState* script_state,
    WTF::CrossThreadOnceClosure disconnect_callback)
    : UnderlyingSourceBase(script_state), script_state_(script_state) {}

void RTCEncodedUnderlyingSourceWrapper::CreateAudioUnderlyingSource(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    base::UnguessableToken owner_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!video_from_encoder_underlying_source_);
  audio_from_encoder_underlying_source_ =
      MakeGarbageCollected<RTCEncodedAudioUnderlyingSource>(
          script_state_, std::move(disconnect_callback_source),
          base::FeatureList::IsEnabled(
              kWebRtcRtpScriptTransformerFrameRestrictions),
          owner_id, Controller());
}

void RTCEncodedUnderlyingSourceWrapper::CreateVideoUnderlyingSource(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    base::UnguessableToken owner_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!audio_from_encoder_underlying_source_);
  video_from_encoder_underlying_source_ =
      MakeGarbageCollected<RTCEncodedVideoUnderlyingSource>(
          script_state_, std::move(disconnect_callback_source),
          base::FeatureList::IsEnabled(
              kWebRtcRtpScriptTransformerFrameRestrictions),
          owner_id, Controller());
}

RTCEncodedUnderlyingSourceWrapper::VideoTransformer
RTCEncodedUnderlyingSourceWrapper::GetVideoTransformer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return WTF::CrossThreadBindRepeating(
      &RTCEncodedVideoUnderlyingSource::OnFrameFromSource,
      WrapCrossThreadPersistent(video_from_encoder_underlying_source_.Get()));
}

RTCEncodedUnderlyingSourceWrapper::AudioTransformer
RTCEncodedUnderlyingSourceWrapper::GetAudioTransformer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return WTF::CrossThreadBindRepeating(
      &RTCEncodedAudioUnderlyingSource::OnFrameFromSource,
      WrapCrossThreadPersistent(audio_from_encoder_underlying_source_.Get()));
}

ScriptPromiseUntyped RTCEncodedUnderlyingSourceWrapper::Pull(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_from_encoder_underlying_source_) {
    return audio_from_encoder_underlying_source_->Pull(script_state,
                                                       exception_state);
  }
  if (video_from_encoder_underlying_source_) {
    return video_from_encoder_underlying_source_->Pull(script_state,
                                                       exception_state);
  }
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromiseUntyped RTCEncodedUnderlyingSourceWrapper::Cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_from_encoder_underlying_source_) {
    return audio_from_encoder_underlying_source_->Cancel(script_state, reason,
                                                         exception_state);
  }
  if (video_from_encoder_underlying_source_) {
    return video_from_encoder_underlying_source_->Cancel(script_state, reason,
                                                         exception_state);
  }
  return ToResolvedUndefinedPromise(script_state);
}

void RTCEncodedUnderlyingSourceWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(audio_from_encoder_underlying_source_);
  visitor->Trace(video_from_encoder_underlying_source_);
  UnderlyingSourceBase::Trace(visitor);
}

void RTCEncodedUnderlyingSourceWrapper::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Close();
  audio_from_encoder_underlying_source_ = nullptr;
  video_from_encoder_underlying_source_ = nullptr;
}

void RTCEncodedUnderlyingSourceWrapper::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_from_encoder_underlying_source_) {
    audio_from_encoder_underlying_source_->Close();
  }
  if (video_from_encoder_underlying_source_) {
    video_from_encoder_underlying_source_->Close();
  }
}

}  // namespace blink
