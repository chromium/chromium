// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
void HandleSendKeyFrameRequestResult(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const RTCRtpScriptTransform::SendKeyFrameRequestResult result) {
  CHECK(!resolver->GetExecutionContext() ||
        resolver->GetExecutionContext()->IsContextThread());
  String message;
  switch (result) {
    case RTCRtpScriptTransform::SendKeyFrameRequestResult::kNoReceiver:
      message = "Not attached to a receiver.";
      break;
    case RTCRtpScriptTransform::SendKeyFrameRequestResult::kNoVideo:
      message = "The kind of the receiver is not video.";
      break;
    case RTCRtpScriptTransform::SendKeyFrameRequestResult::kInvalidState:
      message = "Invalid state.";
      break;
    case RTCRtpScriptTransform::SendKeyFrameRequestResult::kTrackEnded:
      message = "The receiver track is ended.";
      break;
    case RTCRtpScriptTransform::SendKeyFrameRequestResult::kSuccess:
      resolver->Resolve();
      return;
  }
  resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                   message);
}
}  // namespace

RTCRtpScriptTransformer::RTCRtpScriptTransformer(
    ScriptState* script_state,
    CustomEventMessage options,
    scoped_refptr<base::SequencedTaskRunner> transform_task_runner,
    CrossThreadWeakHandle<RTCRtpScriptTransform> transform)
    : rtp_transformer_task_runner_(
          ExecutionContext::From(script_state)
              ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      rtp_transform_task_runner_(transform_task_runner),
      data_as_serialized_script_value_(
          SerializedScriptValue::Unpack(std::move(options.message))),
      serialized_data_memory_accounter_(V8ExternalMemoryAccounter()),
      ports_(MessagePort::EntanglePorts(*ExecutionContext::From(script_state),
                                        std::move(options.ports))),
      transform_(std::move(transform)),
      rtc_encoded_underlying_source_(
          MakeGarbageCollected<RTCEncodedUnderlyingSourceWrapper>(
              script_state)),
      rtc_encoded_underlying_sink_(
          MakeGarbageCollected<RTCEncodedUnderlyingSinkWrapper>(script_state)) {
  // scope is needed because this call may not come directly from JavaScript,
  // and ReadableStream::CreateWithCountQueueingStrategy requires entering the
  // ScriptState.
  ScriptState::Scope scope(script_state);
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, rtc_encoded_underlying_source_,
      /*high_water_mark=*/0);
  // The high water mark for the stream is set to 1 so that the stream seems
  // ready to write, but without queuing frames.
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, rtc_encoded_underlying_sink_,
      /*high_water_mark=*/1);
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

RTCRtpScriptTransformer::~RTCRtpScriptTransformer() {
  serialized_data_memory_accounter_.Clear(v8::Isolate::GetCurrent());
}

size_t RTCRtpScriptTransformer::SizeOfExternalMemoryInBytes() {
  if (!data_as_serialized_script_value_) {
    return 0;
  }
  size_t result = 0;
  for (auto const& array_buffer :
       data_as_serialized_script_value_->ArrayBuffers()) {
    result += array_buffer->ByteLength();
  }
  return result;
}

void RTCRtpScriptTransformer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(data_as_serialized_script_value_);
  visitor->Trace(ports_);
  visitor->Trace(readable_);
  visitor->Trace(writable_);
  visitor->Trace(rtc_encoded_underlying_source_);
  visitor->Trace(rtc_encoded_underlying_sink_);
}

//  Relies on [CachedAttribute] to ensure it isn't run more than once.
ScriptValue RTCRtpScriptTransformer::options(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MessagePortArray message_ports = ports_ ? *ports_ : MessagePortArray();
  SerializedScriptValue::DeserializeOptions options;
  options.message_ports = &message_ports;
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> value;
  if (data_as_serialized_script_value_) {
    // The data is put on the V8 GC heap here, and therefore the V8 GC does
    // the accounting from here on. We unregister the registered memory to
    // avoid double accounting.
    serialized_data_memory_accounter_.Clear(isolate);
    value = data_as_serialized_script_value_->Deserialize(isolate, options);
  } else {
    value = v8::Null(isolate);
  }
  return ScriptValue(isolate, value);
}

ScriptPromise<IDLUndefined> RTCRtpScriptTransformer::sendKeyFrameRequest(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  PostCrossThreadTask(
      *rtp_transform_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &RTCRtpScriptTransform::SendKeyFrameRequestToReceiver,
          MakeUnwrappingCrossThreadWeakHandle(*transform_),
          CrossThreadBindRepeating(&HandleSendKeyFrameRequestResult,
                                   MakeUnwrappingCrossThreadHandle(resolver))));

  return promise;
}

bool RTCRtpScriptTransformer::IsOptionsDirty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void RTCRtpScriptTransformer::SetUpAudio(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        encoded_audio_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UnguessableToken owner_id = base::UnguessableToken::Create();
  rtc_encoded_underlying_source_->CreateAudioUnderlyingSource(
      std::move(disconnect_callback_source), owner_id);
  encoded_audio_transformer->SetTransformerCallback(
      rtc_encoded_underlying_source_->GetAudioTransformer());
  encoded_audio_transformer->SetSourceTaskRunner(rtp_transformer_task_runner_);
  rtc_encoded_underlying_sink_->CreateAudioUnderlyingSink(
      std::move(encoded_audio_transformer), owner_id);
}

void RTCRtpScriptTransformer::SetUpVideo(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
        encoded_video_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UnguessableToken owner_id = base::UnguessableToken::Create();
  rtc_encoded_underlying_source_->CreateVideoUnderlyingSource(
      std::move(disconnect_callback_source), owner_id);
  encoded_video_transformer->SetTransformerCallback(
      rtc_encoded_underlying_source_->GetVideoTransformer());
  encoded_video_transformer->SetSourceTaskRunner(rtp_transformer_task_runner_);
  rtc_encoded_underlying_sink_->CreateVideoUnderlyingSink(
      std::move(encoded_video_transformer), owner_id);
}

void RTCRtpScriptTransformer::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtc_encoded_underlying_source_->Clear();
  rtc_encoded_underlying_sink_->Clear();
}

}  // namespace blink
