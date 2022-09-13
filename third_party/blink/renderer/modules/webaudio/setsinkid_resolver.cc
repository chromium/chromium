// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/setsinkid_resolver.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

namespace blink {

SetSinkIdResolver* SetSinkIdResolver::Create(ScriptState* script_state,
                                             AudioContext& audio_context,
                                             const String& sink_id) {
  DCHECK(IsMainThread());

  SetSinkIdResolver* resolver = MakeGarbageCollected<SetSinkIdResolver>(
      script_state, audio_context, sink_id);
  resolver->KeepAliveWhilePending();
  return resolver;
}

SetSinkIdResolver::SetSinkIdResolver(ScriptState* script_state,
                                     AudioContext& audio_context,
                                     const String& sink_id)
    : ScriptPromiseResolver(script_state),
      audio_context_(audio_context),
      sink_id_(sink_id) {}

void SetSinkIdResolver::Start() {
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

  auto set_sink_id_completion_callback = WTF::Bind(
      &SetSinkIdResolver::OnSetSinkIdComplete, WrapWeakPersistent(this));

  audio_context_->destination()->SetSinkId(
      sink_id_, std::move(set_sink_id_completion_callback));
}

void SetSinkIdResolver::OnSetSinkIdComplete(media::OutputDeviceStatus status) {
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
          "AudioContext.setSinkId(): failed: the device " + sink_id_ +
              " is not found."));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
          "AudioContext.setSinkId() failed: access to the device " + sink_id_ +
              " is not permitted."));
      break;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kTimeoutError,
          "AudioContext.setSinkId() failed: the request for device " +
              sink_id_ + " is timed out."));
      break;
    default:
      NOTREACHED();
  }

  auto& resolvers = audio_context_->GetSetSinkIdResolver();

  resolvers.pop_front();

  if (!resolvers.IsEmpty()) {
    resolvers.front()->Start();
  }
}

void SetSinkIdResolver::NotifySetSinkIdIsDone() {
  DCHECK(IsMainThread());

  if (!audio_context_ || audio_context_->IsContextCleared()) {
    return;
  }

  audio_context_->NotifySetSinkIdIsDone(sink_id_);
}

void SetSinkIdResolver::Trace(Visitor* visitor) const {
  visitor->Trace(audio_context_);
  ScriptPromiseResolver::Trace(visitor);
}

}  // namespace blink
