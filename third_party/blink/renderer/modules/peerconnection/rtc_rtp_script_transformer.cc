// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"

#include "third_party/blink/renderer/core/messaging/message_port.h"

namespace blink {

RTCRtpScriptTransformer::RTCRtpScriptTransformer(ScriptState* script_state,
                                                 CustomEventMessage options)
    : task_runner_(ExecutionContext::From(script_state)
                       ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      serialized_data_(MakeGarbageCollected<SerializedDataForEvent>(
          std::move(options.message))),
      ports_(MessagePort::EntanglePorts(*ExecutionContext::From(script_state),
                                        std::move(options.ports))),
      rtc_encoded_underlying_source_(
          MakeGarbageCollected<RTCEncodedUnderlyingSourceWrapper>(
              script_state)) {
  // Scope is needed because this call may not come directly from JavaScript.
  ScriptState::Scope scope(script_state);
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, rtc_encoded_underlying_source_,
      /*high_water_mark=*/0);
}

void RTCRtpScriptTransformer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(serialized_data_);
  visitor->Trace(ports_);
  visitor->Trace(readable_);
  visitor->Trace(rtc_encoded_underlying_source_);
}

//  Relies on [CachedAttribute] to ensure it isn't run more than once.
ScriptValue RTCRtpScriptTransformer::options(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MessagePortArray message_ports = ports_ ? *ports_ : MessagePortArray();
  SerializedScriptValue::DeserializeOptions options;
  options.message_ports = &message_ports;
  return serialized_data_->Deserialize(script_state, options);
}

bool RTCRtpScriptTransformer::IsOptionsDirty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return serialized_data_->IsDataDirty();
}

void RTCRtpScriptTransformer::CreateAudioUnderlyingSource(
    WTF::CrossThreadOnceClosure disconnect_callback_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtc_encoded_underlying_source_->CreateAudioUnderlyingSource(
      std::move(disconnect_callback_source));
}

void RTCRtpScriptTransformer::CreateVideoUnderlyingSource(
    WTF::CrossThreadOnceClosure disconnect_callback_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtc_encoded_underlying_source_->CreateVideoUnderlyingSource(
      std::move(disconnect_callback_source));
}

void RTCRtpScriptTransformer::SetVideoTransformerCallback(
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
        encoded_video_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  encoded_video_transformer_ = std::move(encoded_video_transformer);
  encoded_video_transformer_->SetTransformerCallback(
      rtc_encoded_underlying_source_->GetVideoTransformer());
  encoded_video_transformer_->SetSourceTaskRunner(task_runner_);
}

void RTCRtpScriptTransformer::SetAudioTransformerCallback(
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        encoded_audio_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  encoded_audio_transformer_ = std::move(encoded_audio_transformer);
  encoded_audio_transformer_->SetTransformerCallback(
      rtc_encoded_underlying_source_->GetAudioTransformer());
  encoded_audio_transformer_->SetSourceTaskRunner(task_runner_);
}

}  // namespace blink
