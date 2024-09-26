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

SetSinkIdResolver::SetSinkIdResolver(
    ScriptState* script_state,
    AudioContext& audio_context,
    const V8UnionAudioSinkOptionsOrString& sink_id)
    : audio_context_(audio_context),
      resolver_(MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state)) {
  DCHECK(IsMainThread());

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

void SetSinkIdResolver::Trace(Visitor* visitor) const {
  visitor->Trace(audio_context_);
  visitor->Trace(resolver_);
}

void SetSinkIdResolver::Start() {
  TRACE_EVENT1("webaudio", "SetSinkIdResolver::Start", "sink_id",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
  DCHECK(IsMainThread());

  auto* execution_context = resolver_->GetExecutionContext();
  if (!execution_context || !audio_context_ ||
      audio_context_->IsContextCleared()) {
    // No point in rejecting promise, as it will bail out upon detached
    // context anyway.
    return;
  }

  // Refer to
  // https://webaudio.github.io/web-audio-api/#validating-sink-identifier for
  // sink_id/sink_descriptor validation steps.
  if (sink_descriptor_ == audio_context_->GetSinkDescriptor()) {
    OnSetSinkIdComplete(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  } else if (!audio_context_->IsValidSinkDescriptor(sink_descriptor_)) {
    OnSetSinkIdComplete(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
  } else {
    auto* audio_destination = audio_context_->destination();
    // A sanity check to make sure we have valid audio_destination node from
    // `audio_context_`.
    if (!audio_destination) {
      OnSetSinkIdComplete(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    } else {
      audio_context_->NotifySetSinkIdBegins();
      auto set_sink_id_completion_callback = WTF::BindOnce(
          &SetSinkIdResolver::OnSetSinkIdComplete, WrapPersistent(this));
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

ScriptPromiseResolver<IDLUndefined>* SetSinkIdResolver::Resolver() {
  return resolver_;
}

void SetSinkIdResolver::OnSetSinkIdComplete(media::OutputDeviceStatus status) {
  TRACE_EVENT1("webaudio", "SetSinkIdResolver::OnSetSinkIdComplete", "sink_id",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
  DCHECK(IsMainThread());

  auto* excecution_context = resolver_->GetExecutionContext();
  if (!excecution_context || excecution_context->IsContextDestroyed()) {
    return;
  }

  ScriptState* script_state = resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);
  switch (status) {
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK:
      if (audio_context_ && !audio_context_->IsContextCleared()) {
        // Update AudioContext's sink ID and fire the 'onsinkchange' event
        audio_context_->NotifySetSinkIdIsDone(sink_descriptor_);
      }
      resolver_->Resolve();
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      resolver_->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotFoundError,
          "AudioContext.setSinkId(): failed: the device " +
              String(sink_descriptor_.SinkId()) + " is not found."));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      resolver_->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
          "AudioContext.setSinkId() failed: access to the device " +
              String(sink_descriptor_.SinkId()) + " is not permitted."));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      resolver_->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kTimeoutError,
          "AudioContext.setSinkId() failed: the request for device " +
              String(sink_descriptor_.SinkId()) + " is timed out."));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  auto& resolvers = audio_context_->GetSetSinkIdResolver();
  resolvers.pop_front();
  if (!resolvers.empty() && (audio_context_->PendingDeviceListUpdates() == 0)) {
    // Prevent potential stack overflow under heavy load by scheduling the next
    // resolver start asynchronously instead of invoking it directly.
    auto next_start_task = WTF::BindOnce(
        &SetSinkIdResolver::Start, WrapWeakPersistent(resolvers.front().Get()));
    audio_context_->GetExecutionContext()
        ->GetTaskRunner(TaskType::kInternalMediaRealTime)
        ->PostTask(FROM_HERE, std::move(next_start_task));
  }
}

}  // namespace blink
