/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webmidi/midi_port.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access.h"
#include "third_party/blink/renderer/modules/webmidi/midi_connection_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using midi::mojom::PortState;

namespace blink {

MIDIPort::MIDIPort(MIDIAccess* access,
                   const String& id,
                   const String& manufacturer,
                   const String& name,
                   TypeCode type,
                   const String& version,
                   PortState state)
    : ContextLifecycleObserver(access->GetExecutionContext()),
      id_(id),
      manufacturer_(manufacturer),
      name_(name),
      type_(type),
      version_(version),
      access_(access),
      connection_(kConnectionStateClosed) {
  DCHECK(access);
  DCHECK(type == kTypeInput || type == kTypeOutput);
  DCHECK(state == PortState::DISCONNECTED || state == PortState::CONNECTED);
  state_ = state;
}

String MIDIPort::connection() const {
  switch (connection_) {
    case kConnectionStateOpen:
      return "open";
    case kConnectionStateClosed:
      return "closed";
    case kConnectionStatePending:
      return "pending";
  }
  return g_empty_string;
}

String MIDIPort::state() const {
  switch (state_) {
    case PortState::DISCONNECTED:
      return "disconnected";
    case PortState::CONNECTED:
      return "connected";
    case PortState::OPENED:
      NOTREACHED();
      return "connected";
  }
  return g_empty_string;
}

String MIDIPort::type() const {
  switch (type_) {
    case kTypeInput:
      return "input";
    case kTypeOutput:
      return "output";
  }
  return g_empty_string;
}

ScriptPromise MIDIPort::open(ScriptState* script_state) {
  if (connection_ == kConnectionStateOpen)
    return Accept(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&MIDIPort::OpenAsynchronously, WrapPersistent(this),
                           WrapPersistent(resolver)));
  running_open_count_++;
  return resolver->Promise();
}

void MIDIPort::open() {
  if (connection_ == kConnectionStateOpen || running_open_count_)
    return;
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, WTF::Bind(&MIDIPort::OpenAsynchronously,
                                      WrapPersistent(this), nullptr));
  running_open_count_++;
}

ScriptPromise MIDIPort::close(ScriptState* script_state) {
  if (connection_ == kConnectionStateClosed)
    return Accept(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&MIDIPort::CloseAsynchronously, WrapPersistent(this),
                           WrapPersistent(resolver)));
  return resolver->Promise();
}

void MIDIPort::SetState(PortState state) {
  switch (state) {
    case PortState::DISCONNECTED:
      switch (connection_) {
        case kConnectionStateOpen:
        case kConnectionStatePending:
          SetStates(PortState::DISCONNECTED, kConnectionStatePending);
          break;
        case kConnectionStateClosed:
          // Will do nothing.
          SetStates(PortState::DISCONNECTED, kConnectionStateClosed);
          break;
      }
      break;
    case PortState::CONNECTED:
      switch (connection_) {
        case kConnectionStateOpen:
          NOTREACHED();
          break;
        case kConnectionStatePending:
          // We do not use |setStates| in order not to dispatch events twice.
          // |open| calls |setStates|.
          state_ = PortState::CONNECTED;
          open();
          break;
        case kConnectionStateClosed:
          SetStates(PortState::CONNECTED, kConnectionStateClosed);
          break;
      }
      break;
    case PortState::OPENED:
      NOTREACHED();
      break;
  }
}

ExecutionContext* MIDIPort::GetExecutionContext() const {
  return access_->GetExecutionContext();
}

bool MIDIPort::HasPendingActivity() const {
  // MIDIPort should survive if ConnectionState is "open" or can be "open" via
  // a MIDIConnectionEvent even if there are no references from JavaScript.
  return connection_ != kConnectionStateClosed;
}

void MIDIPort::ContextDestroyed(ExecutionContext*) {
  // Should be "closed" to assume there are no pending activities.
  connection_ = kConnectionStateClosed;
}

void MIDIPort::Trace(blink::Visitor* visitor) {
  visitor->Trace(access_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void MIDIPort::OpenAsynchronously(ScriptPromiseResolver* resolver) {
  // The frame should exist, but it may be already detached and the execution
  // context may be lost here.
  if (!GetExecutionContext())
    return;

  UseCounter::Count(*To<Document>(GetExecutionContext()),
                    WebFeature::kMIDIPortOpen);
  DCHECK_NE(0u, running_open_count_);
  running_open_count_--;

  DidOpen(state_ == PortState::CONNECTED);
  switch (state_) {
    case PortState::DISCONNECTED:
      SetStates(state_, kConnectionStatePending);
      break;
    case PortState::CONNECTED:
      // TODO(toyoshim): Add blink API to perform a real open and close
      // operation.
      SetStates(state_, kConnectionStateOpen);
      break;
    case PortState::OPENED:
      NOTREACHED();
      break;
  }
  if (resolver)
    resolver->Resolve(this);
}

void MIDIPort::CloseAsynchronously(ScriptPromiseResolver* resolver) {
  // The frame should exist, but it may be already detached and the execution
  // context may be lost here.
  if (!GetExecutionContext())
    return;

  DCHECK(resolver);
  // TODO(toyoshim): Do clear() operation on MIDIOutput.
  // TODO(toyoshim): Add blink API to perform a real close operation.
  SetStates(state_, kConnectionStateClosed);
  resolver->Resolve(this);
}

ScriptPromise MIDIPort::Accept(ScriptState* script_state) {
  return ScriptPromise::Cast(script_state,
                             ToV8(this, script_state->GetContext()->Global(),
                                  script_state->GetIsolate()));
}

void MIDIPort::SetStates(PortState state, ConnectionState connection) {
  DCHECK(state != PortState::DISCONNECTED ||
         connection != kConnectionStateOpen);
  if (state_ == state && connection_ == connection)
    return;
  state_ = state;
  connection_ = connection;
  DispatchEvent(*MIDIConnectionEvent::Create(this));
  access_->DispatchEvent(*MIDIConnectionEvent::Create(this));
}

}  // namespace blink
