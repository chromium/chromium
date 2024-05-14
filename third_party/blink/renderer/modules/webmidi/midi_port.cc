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
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_port_device_state.h"
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
                   MIDIPortType type,
                   const String& version,
                   PortState state)
    : ActiveScriptWrappable<MIDIPort>({}),
      ExecutionContextLifecycleObserver(access->GetExecutionContext()),
      id_(id),
      manufacturer_(manufacturer),
      name_(name),
      type_(type),
      version_(version),
      access_(access),
      connection_(MIDIPortConnectionState::kClosed) {
  DCHECK(access);
  DCHECK(type == MIDIPortType::kInput || type == MIDIPortType::kOutput);
  DCHECK(state == PortState::DISCONNECTED || state == PortState::CONNECTED);
  state_ = state;
}

V8MIDIPortConnectionState MIDIPort::connection() const {
  return V8MIDIPortConnectionState(connection_);
}

V8MIDIPortDeviceState MIDIPort::state() const {
  switch (state_) {
    case PortState::DISCONNECTED:
      return V8MIDIPortDeviceState(V8MIDIPortDeviceState::Enum::kDisconnected);
    case PortState::CONNECTED:
      return V8MIDIPortDeviceState(V8MIDIPortDeviceState::Enum::kConnected);
    case PortState::OPENED:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return V8MIDIPortDeviceState(V8MIDIPortDeviceState::Enum::kConnected);
}

V8MIDIPortType MIDIPort::type() const {
  return V8MIDIPortType(type_);
}

ScriptPromise<MIDIPort> MIDIPort::open(ScriptState* script_state) {
  if (connection_ == MIDIPortConnectionState::kOpen)
    return Accept(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<MIDIPort>>(script_state);
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&MIDIPort::OpenAsynchronously,
                               WrapPersistent(this), WrapPersistent(resolver)));
  running_open_count_++;
  return resolver->Promise();
}

void MIDIPort::open() {
  if (connection_ == MIDIPortConnectionState::kOpen || running_open_count_)
    return;
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, WTF::BindOnce(&MIDIPort::OpenAsynchronously,
                                          WrapPersistent(this), nullptr));
  running_open_count_++;
}

ScriptPromise<MIDIPort> MIDIPort::close(ScriptState* script_state) {
  if (connection_ == MIDIPortConnectionState::kClosed)
    return Accept(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<MIDIPort>>(script_state);
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&MIDIPort::CloseAsynchronously,
                               WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

void MIDIPort::SetState(PortState state) {
  switch (state) {
    case PortState::DISCONNECTED:
      switch (connection_) {
        case MIDIPortConnectionState::kOpen:
        case MIDIPortConnectionState::kPending:
          SetStates(PortState::DISCONNECTED, MIDIPortConnectionState::kPending);
          break;
        case MIDIPortConnectionState::kClosed:
          // Will do nothing.
          SetStates(PortState::DISCONNECTED, MIDIPortConnectionState::kClosed);
          break;
      }
      break;
    case PortState::CONNECTED:
      switch (connection_) {
        case MIDIPortConnectionState::kOpen:
          NOTREACHED_IN_MIGRATION();
          break;
        case MIDIPortConnectionState::kPending:
          // We do not use |setStates| in order not to dispatch events twice.
          // |open| calls |setStates|.
          state_ = PortState::CONNECTED;
          open();
          break;
        case MIDIPortConnectionState::kClosed:
          SetStates(PortState::CONNECTED, MIDIPortConnectionState::kClosed);
          break;
      }
      break;
    case PortState::OPENED:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

ExecutionContext* MIDIPort::GetExecutionContext() const {
  return access_->GetExecutionContext();
}

bool MIDIPort::HasPendingActivity() const {
  // MIDIPort should survive if ConnectionState is "open" or can be "open" via
  // a MIDIConnectionEvent even if there are no references from JavaScript.
  return connection_ != MIDIPortConnectionState::kClosed;
}

void MIDIPort::ContextDestroyed() {
  // Should be "closed" to assume there are no pending activities.
  connection_ = MIDIPortConnectionState::kClosed;
}

void MIDIPort::Trace(Visitor* visitor) const {
  visitor->Trace(access_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void MIDIPort::OpenAsynchronously(ScriptPromiseResolver<MIDIPort>* resolver) {
  // The frame should exist, but it may be already detached and the execution
  // context may be lost here.
  if (!GetExecutionContext())
    return;

  UseCounter::Count(GetExecutionContext(), WebFeature::kMIDIPortOpen);
  DCHECK_NE(0u, running_open_count_);
  running_open_count_--;

  DidOpen(state_ == PortState::CONNECTED);
  switch (state_) {
    case PortState::DISCONNECTED:
      SetStates(state_, MIDIPortConnectionState::kPending);
      break;
    case PortState::CONNECTED:
      // TODO(toyoshim): Add blink API to perform a real open and close
      // operation.
      SetStates(state_, MIDIPortConnectionState::kOpen);
      break;
    case PortState::OPENED:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  if (resolver)
    resolver->Resolve(this);
}

void MIDIPort::CloseAsynchronously(ScriptPromiseResolver<MIDIPort>* resolver) {
  // The frame should exist, but it may be already detached and the execution
  // context may be lost here.
  if (!GetExecutionContext())
    return;

  DCHECK(resolver);
  // TODO(toyoshim): Do clear() operation on MIDIOutput.
  // TODO(toyoshim): Add blink API to perform a real close operation.
  SetStates(state_, MIDIPortConnectionState::kClosed);
  resolver->Resolve(this);
}

ScriptPromise<MIDIPort> MIDIPort::Accept(ScriptState* script_state) {
  return ToResolvedPromise<MIDIPort>(script_state, this);
}

void MIDIPort::SetStates(PortState state, MIDIPortConnectionState connection) {
  DCHECK(state != PortState::DISCONNECTED ||
         connection != MIDIPortConnectionState::kOpen);
  if (state_ == state && connection_ == connection)
    return;
  state_ = state;
  connection_ = connection;
  DispatchEvent(*MIDIConnectionEvent::Create(this));
  access_->DispatchEvent(*MIDIConnectionEvent::Create(this));
}

}  // namespace blink
