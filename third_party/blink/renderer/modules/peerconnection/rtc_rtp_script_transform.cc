// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "rtc_rtp_script_transform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
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

namespace blink {

namespace {

// This method runs in the worker context, triggered by a callback.
Event* CreateRTCTransformEvent(
    CrossThreadWeakHandle<RTCRtpScriptTransform> transform,
    scoped_refptr<base::SequencedTaskRunner> transform_task_runner,
    ScriptState* script_state,
    CustomEventMessage data) {
  auto* event =
      MakeGarbageCollected<RTCTransformEvent>(script_state, std::move(data));

  PostCrossThreadTask(
      *transform_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &RTCRtpScriptTransform::SetTransformer,
          MakeUnwrappingCrossThreadWeakHandle(transform),
          MakeCrossThreadWeakHandle(event->transformer()),
          WrapRefCounted(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)
                             .get())));
  return event;
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

void RTCRtpScriptTransform::CreateUnderlyingSource(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    String kind) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (transformer_) {
    CreateUnderlyingSourceInternal(kind, std::move(disconnect_callback_source));
  } else {
    // Saving these fields so once the transformer is set,
    // CreateUnderlyingSourceInternal can be called.
    kind_ = kind;
    disconnect_callback_source_ = std::move(disconnect_callback_source);
  }
}

void RTCRtpScriptTransform::CreateUnderlyingSourceAndSetAudioTransformer(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
        encoded_audio_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock locker(transformer_lock_);
  CreateUnderlyingSource(std::move(disconnect_callback_source), "audio");
  if (transformer_) {
    PostCrossThreadTask(
        *transformer_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RTCRtpScriptTransformer::SetAudioTransformerCallback,
            MakeUnwrappingCrossThreadWeakHandle(*transformer_),
            std::move(encoded_audio_transformer)));
  } else {
    // Saving the audio transformer so once the transformer is set,
    // SetAudioTransformerCallback can be called.
    encoded_audio_transformer_ = std::move(encoded_audio_transformer);
  }
}

void RTCRtpScriptTransform::CreateUnderlyingSourceAndSetVideoTransformer(
    WTF::CrossThreadOnceClosure disconnect_callback_source,
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
        encoded_video_transformer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock locker(transformer_lock_);
  CreateUnderlyingSource(std::move(disconnect_callback_source), "video");
  if (transformer_) {
    PostCrossThreadTask(
        *transformer_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RTCRtpScriptTransformer::SetVideoTransformerCallback,
            MakeUnwrappingCrossThreadWeakHandle(*transformer_),
            std::move(encoded_video_transformer)));
  } else {
    // Saving the video transformer so once the transformer is set,
    // SetVideoTransformerCallback can be called.
    encoded_video_transformer_ = std::move(encoded_video_transformer);
  }
}

void RTCRtpScriptTransform::CreateUnderlyingSourceInternal(
    const String& kind,
    WTF::CrossThreadOnceClosure disconnect_callback_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(transformer_);
  if (kind == "audio") {
    PostCrossThreadTask(
        *transformer_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RTCRtpScriptTransformer::CreateAudioUnderlyingSource,
            MakeUnwrappingCrossThreadWeakHandle(*transformer_),
            std::move(disconnect_callback_source)));
    return;
  }
  CHECK_EQ(kind, "video");
  PostCrossThreadTask(*transformer_task_runner_, FROM_HERE,
                      WTF::CrossThreadBindOnce(
                          &RTCRtpScriptTransformer::CreateVideoUnderlyingSource,
                          MakeUnwrappingCrossThreadWeakHandle(*transformer_),
                          std::move(disconnect_callback_source)));
}

void RTCRtpScriptTransform::SetTransformer(
    CrossThreadWeakHandle<RTCRtpScriptTransformer> transformer,
    scoped_refptr<base::SingleThreadTaskRunner> transformer_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock locker(transformer_lock_);
  transformer_.emplace(std::move(transformer));
  transformer_task_runner_ = transformer_task_runner;
  if (disconnect_callback_source_) {
    CreateUnderlyingSourceInternal(kind_,
                                   std::move(disconnect_callback_source_));
  }
  if (encoded_audio_transformer_) {
    PostCrossThreadTask(
        *transformer_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RTCRtpScriptTransformer::SetAudioTransformerCallback,
            MakeUnwrappingCrossThreadWeakHandle(*transformer_),
            std::move(encoded_audio_transformer_)));
  } else if (encoded_video_transformer_) {
    PostCrossThreadTask(
        *transformer_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RTCRtpScriptTransformer::SetVideoTransformerCallback,
            MakeUnwrappingCrossThreadWeakHandle(*transformer_),
            std::move(encoded_video_transformer_)));
  }
}

}  // namespace blink
