// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/setsinkid_resolver.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiosinkinfo_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

namespace blink {

SetSinkIdResolver* SetSinkIdResolver::Create(
    ScriptState* script_state,
    AudioContext& audio_context,
    const V8UnionAudioSinkOptionsOrString& sink_id) {
  DCHECK(IsMainThread());

  SetSinkIdResolver* resolver = MakeGarbageCollected<SetSinkIdResolver>(
      script_state, audio_context, sink_id);
  resolver->KeepAliveWhilePending();
  return resolver;
}

SetSinkIdResolver::SetSinkIdResolver(
    ScriptState* script_state,
    AudioContext& audio_context,
    const V8UnionAudioSinkOptionsOrString& sink_id)
    : ScriptPromiseResolver(script_state), audio_context_(audio_context) {
  // Currently the only available AudioSinkOptions is a type of a silent sink,
  // which can be specified by an empty descriptor constructor.
  auto& frame_token = To<LocalDOMWindow>(audio_context_->GetExecutionContext())
                          ->GetLocalFrameToken();
  if (sink_id.GetContentType() ==
      V8UnionAudioSinkOptionsOrString::ContentType::kAudioSinkOptions) {
    sink_descriptor_ = WebAudioSinkDescriptor(frame_token);
  } else {
    sink_descriptor_ =
        WebAudioSinkDescriptor(sink_id.GetAsString(), frame_token);
  }

  TRACE_EVENT1("webaudio", "SetSinkIdResolver::SetSinkIdResolver",
               "sink_id (after setting sink_descriptor_)",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
}

void SetSinkIdResolver::Start() {
  TRACE_EVENT1("webaudio", "SetSinkIdResolver::Start", "sink_id",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
  DCHECK(IsMainThread());

  ExecutionContext* context = GetExecutionContext();
  if (!context || !audio_context_ || audio_context_->IsContextCleared()) {
    // A detached BaseAudioContext should not be playing audio. The
    // `Reject()` call below will not trigger any JS callbacks because
    // the associated execution context is already detached.
    ScriptState* script_state = GetScriptState();
    ScriptState::Scope scope(script_state);
    Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "Cannot invoke AudioContext.setSinkId() on a detached document."));
    return;
  }

  auto set_sink_id_completion_callback = WTF::BindOnce(
      &SetSinkIdResolver::OnSetSinkIdComplete, WrapWeakPersistent(this));

  // Refer to
  // https://webaudio.github.io/web-audio-api/#validating-sink-identifier for
  // sink_id/sink_descriptor validation steps.
  if (sink_descriptor_ == audio_context_->GetSinkDescriptor()) {
    std::move(set_sink_id_completion_callback)
        .Run(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  } else if (!audio_context_->IsValidSinkDescriptor(sink_descriptor_)) {
    std::move(set_sink_id_completion_callback)
        .Run(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
  } else {
    auto* audio_destination = audio_context_->destination();

    // A sanity check to make sure we have valid audio_destination node from
    // `audio_context_`.
    if (!audio_destination) {
      std::move(set_sink_id_completion_callback)
          .Run(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    } else {
      auto set_sink_descriptor_callback = WTF::BindOnce(
          &RealtimeAudioDestinationNode::SetSinkDescriptor,
          WrapWeakPersistent(
              static_cast<RealtimeAudioDestinationNode*>(audio_destination)),
          sink_descriptor_, std::move(set_sink_id_completion_callback));

      audio_context_->GetExecutionContext()
          ->GetTaskRunner(TaskType::kInternalMediaRealTime)
          ->PostTask(FROM_HERE, std::move(set_sink_descriptor_callback));
    }
  }
}

void SetSinkIdResolver::OnSetSinkIdComplete(media::OutputDeviceStatus status) {
  TRACE_EVENT1("webaudio", "SetSinkIdResolver::OnSetSinkIdComplete", "sink_id",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
  DCHECK(IsMainThread());

  auto* excecution_context = GetExecutionContext();
  if (!excecution_context || excecution_context->IsContextDestroyed()) {
    return;
  }

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  switch (status) {
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK:
      // Update AudioContext's sink ID and fire the 'onsinkchange' event
      NotifySetSinkIdIsDone();
      Resolve();
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotFoundError,
          "AudioContext.setSinkId(): failed: the device " +
              String(sink_descriptor_.SinkId()) + " is not found."));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
          "AudioContext.setSinkId() failed: access to the device " +
              String(sink_descriptor_.SinkId()) + " is not permitted."));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kTimeoutError,
          "AudioContext.setSinkId() failed: the request for device " +
              String(sink_descriptor_.SinkId()) + " is timed out."));
      break;
    default:
      NOTREACHED();
  }

  auto& resolvers = audio_context_->GetSetSinkIdResolver();

  resolvers.pop_front();

  if (!resolvers.empty()) {
    resolvers.front()->Start();
  }
}

void SetSinkIdResolver::NotifySetSinkIdIsDone() {
  DCHECK(IsMainThread());

  if (!audio_context_ || audio_context_->IsContextCleared()) {
    return;
  }

  audio_context_->NotifySetSinkIdIsDone(sink_descriptor_);
}

void SetSinkIdResolver::Trace(Visitor* visitor) const {
  visitor->Trace(audio_context_);
  ScriptPromiseResolver::Trace(visitor);
}

}  // namespace blink
