// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "rtc_rtp_script_transform.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transform_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

// This method runs in the worker context, triggered by a callback.
Event* CreateRTCTransformEvent(
    CrossThreadWeakHandle<RTCRtpScriptTransform> transform,
    scoped_refptr<base::SequencedTaskRunner> transform_task_runner,
    ScriptState* script_state,
    CustomEventMessage data) {
  auto* event = MakeGarbageCollected<RTCTransformEvent>(
      script_state, std::move(data), transform_task_runner, transform);

  PostCrossThreadTask(
      *transform_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &RTCRtpScriptTransform::SetRtpTransformer,
          MakeUnwrappingCrossThreadWeakHandle(transform),
          MakeCrossThreadWeakHandle(event->transformer()),
          WrapRefCounted(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)
                             .get())));
  return event;
}

bool IsValidReceiverDirection(
    std::optional<V8RTCRtpTransceiverDirection> direction) {
  if (!direction.has_value()) {
    return false;
  }
  return direction.value().AsEnum() ==
             V8RTCRtpTransceiverDirection::Enum::kSendrecv ||
         direction.value().AsEnum() ==
             V8RTCRtpTransceiverDirection::Enum::kRecvonly;
}

}  // namespace

RTCRtpScriptTransform* RTCRtpScriptTransform::Create(
    ScriptState* script_state,
    DedicatedWorker* worker,
    ExceptionState& exception_state) {
  HeapVector<ScriptValue> transfer;
  return Create(script_state, worker, ScriptValue(), transfer, exception_state);
}

RTCRtpScriptTransform* RTCRtpScriptTransform::Create(
    ScriptState* script_state,
    DedicatedWorker* worker,
    const ScriptValue& message,
    ExceptionState& exception_state) {
  HeapVector<ScriptValue> transfer;
  return Create(script_state, worker, message, transfer, exception_state);
}

RTCRtpScriptTransform* RTCRtpScriptTransform::Create(
    ScriptState* script_state,
    DedicatedWorker* worker,
    const ScriptValue& message,
    HeapVector<ScriptValue>& transfer,
    ExceptionState& exception_state) {
  auto* transform = MakeGarbageCollected<RTCRtpScriptTransform>();
  worker->PostCustomEvent(
      TaskType::kInternalMediaRealTime, script_state,
      CrossThreadBindRepeating(
          &CreateRTCTransformEvent, MakeCrossThreadWeakHandle(transform),
          ExecutionContext::From(script_state)
              ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      CrossThreadFunction<Event*(ScriptState*)>(), message, transfer,
      exception_state);
  return transform;
}

void RTCRtpScriptTransform::CreateAudioUnderlyingSourceAndSink(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        encoded_audio_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (rtp_transformer_) {
    SetUpAudioRtpTransformer(std::move(disconnect_callback_source),
                             std::move(encoded_audio_transformer));
  } else {
    // Saving these fields so once the transformer is set,
    // SetUpAudioRtpTransformer can be called.
    encoded_audio_transformer_ = std::move(encoded_audio_transformer);
    disconnect_callback_source_ = std::move(disconnect_callback_source);
  }
}

void RTCRtpScriptTransform::CreateVideoUnderlyingSourceAndSink(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
        encoded_video_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (rtp_transformer_) {
    SetUpVideoRtpTransformer(std::move(disconnect_callback_source),
                             std::move(encoded_video_transformer));
  } else {
    // Saving these fields so once the transformer is set,
    // SetUpVideoRtpTransformer can be called.
    encoded_video_transformer_ = std::move(encoded_video_transformer);
    disconnect_callback_source_ = std::move(disconnect_callback_source);
  }
}

void RTCRtpScriptTransform::SetUpAudioRtpTransformer(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        encoded_audio_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(rtp_transformer_);
  PostCrossThreadTask(
      *rtp_transformer_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(
          &RTCRtpScriptTransformer::SetUpAudio,
          MakeUnwrappingCrossThreadWeakHandle(*rtp_transformer_),
          std::move(disconnect_callback_source),
          std::move(encoded_audio_transformer)));
}

void RTCRtpScriptTransform::SetUpVideoRtpTransformer(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
        encoded_video_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(rtp_transformer_);
  PostCrossThreadTask(
      *rtp_transformer_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(
          &RTCRtpScriptTransformer::SetUpVideo,
          MakeUnwrappingCrossThreadWeakHandle(*rtp_transformer_),
          std::move(disconnect_callback_source),
          std::move(encoded_video_transformer)));
}

void RTCRtpScriptTransform::SetRtpTransformer(
    CrossThreadWeakHandle<RTCRtpScriptTransformer> transformer,
    scoped_refptr<base::SingleThreadTaskRunner> transformer_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtp_transformer_.emplace(std::move(transformer));
  rtp_transformer_task_runner_ = std::move(transformer_task_runner);
  if (disconnect_callback_source_ && encoded_audio_transformer_) {
    SetUpAudioRtpTransformer(std::move(disconnect_callback_source_),
                             std::move(encoded_audio_transformer_));
    return;
  }
  if (disconnect_callback_source_ && encoded_video_transformer_) {
    SetUpVideoRtpTransformer(std::move(disconnect_callback_source_),
                             std::move(encoded_video_transformer_));
  }
}

void RTCRtpScriptTransform::AttachToReceiver(RTCRtpReceiver* receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_attached_);
  is_attached_ = true;
  receiver_ = receiver;
}

void RTCRtpScriptTransform::Detach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_attached_ = false;
  receiver_ = nullptr;
  encoded_video_transformer_ = nullptr;
  encoded_audio_transformer_ = nullptr;
  disconnect_callback_source_.Reset();
  if (rtp_transformer_) {
    PostCrossThreadTask(
        *rtp_transformer_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RTCRtpScriptTransformer::Clear,
            MakeUnwrappingCrossThreadWeakHandle(*rtp_transformer_)));
  }
}

RTCRtpScriptTransform::SendKeyFrameRequestResult
RTCRtpScriptTransform::HandleSendKeyFrameRequestResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!rtp_transformer_) {
    return SendKeyFrameRequestResult::kInvalidState;
  }
  if (!receiver_) {
    return SendKeyFrameRequestResult::kNoReceiver;
  }
  if (receiver_->kind() == RTCRtpReceiver::MediaKind::kAudio) {
    return SendKeyFrameRequestResult::kNoVideo;
  }
  if (!IsValidReceiverDirection(receiver_->TransceiverDirection()) ||
      !IsValidReceiverDirection(receiver_->TransceiverCurrentDirection())) {
    return SendKeyFrameRequestResult::kInvalidState;
  }
  if (receiver_->track()->readyState() == "ended") {
    return SendKeyFrameRequestResult::kTrackEnded;
  }
  MediaStreamVideoSource* video_source = MediaStreamVideoSource::GetVideoSource(
      receiver_->track()->Component()->Source());
  video_source->RequestKeyFrame();
  return SendKeyFrameRequestResult::kSuccess;
}

void RTCRtpScriptTransform::SendKeyFrameRequestToReceiver(
    CrossThreadFunction<void(const SendKeyFrameRequestResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendKeyFrameRequestResult result = HandleSendKeyFrameRequestResults();
  PostCrossThreadTask(*rtp_transformer_task_runner_, FROM_HERE,
                      WTF::CrossThreadBindOnce(std::move(callback), result));
}

void RTCRtpScriptTransform::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(receiver_);
}

}  // namespace blink
