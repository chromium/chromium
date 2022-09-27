// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_access_initializer.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access.h"
#include "third_party/blink/renderer/modules/webmidi/midi_port.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

using midi::mojom::PortState;
using midi::mojom::Result;
using mojom::blink::PermissionStatus;

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* script_state,
                                             const MIDIOptions* options)
    : ScriptPromiseResolver(script_state),
      options_(options),
      permission_service_(ExecutionContext::From(script_state)) {}

void MIDIAccessInitializer::ContextDestroyed() {
  ScriptPromiseResolver::ContextDestroyed();
}

ScriptPromise MIDIAccessInitializer::Start() {
  ScriptPromise promise = Promise();

  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);

  ConnectToPermissionService(
      GetExecutionContext(),
      permission_service_.BindNewPipeAndPassReceiver(std::move(task_runner)));

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  permission_service_->RequestPermission(
      CreateMidiPermissionDescriptor(options_->hasSysex() && options_->sysex()),
      LocalFrame::HasTransientUserActivation(window->GetFrame()),
      WTF::BindOnce(&MIDIAccessInitializer::OnPermissionsUpdated,
                    WrapPersistent(this)));

  return promise;
}

void MIDIAccessInitializer::DidAddInputPort(const String& id,
                                            const String& manufacturer,
                                            const String& name,
                                            const String& version,
                                            PortState state) {
  DCHECK(dispatcher_);
  port_descriptors_.push_back(PortDescriptor(
      id, manufacturer, name, MIDIPortType::kInput, version, state));
}

void MIDIAccessInitializer::DidAddOutputPort(const String& id,
                                             const String& manufacturer,
                                             const String& name,
                                             const String& version,
                                             PortState state) {
  DCHECK(dispatcher_);
  port_descriptors_.push_back(PortDescriptor(
      id, manufacturer, name, MIDIPortType::kOutput, version, state));
}

void MIDIAccessInitializer::DidSetInputPortState(unsigned port_index,
                                                 PortState state) {
  // didSetInputPortState() is not allowed to call before didStartSession()
  // is called. Once didStartSession() is called, MIDIAccessorClient methods
  // are delegated to MIDIAccess. See constructor of MIDIAccess.
  NOTREACHED();
}

void MIDIAccessInitializer::DidSetOutputPortState(unsigned port_index,
                                                  PortState state) {
  // See comments on didSetInputPortState().
  NOTREACHED();
}

void MIDIAccessInitializer::DidStartSession(Result result) {
  DCHECK(dispatcher_);
  // We would also have AbortError and SecurityError according to the spec.
  // SecurityError is handled in onPermission(s)Updated().
  switch (result) {
    case Result::NOT_INITIALIZED:
      break;
    case Result::OK:
      return Resolve(MakeGarbageCollected<MIDIAccess>(
          dispatcher_, options_->hasSysex() && options_->sysex(),
          port_descriptors_, GetExecutionContext()));
    case Result::NOT_SUPPORTED:
      return Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError));
    case Result::INITIALIZATION_ERROR:
      return Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Platform dependent initialization failed."));
  }
  NOTREACHED();
  Reject(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                         "Unknown internal error occurred."));
}

void MIDIAccessInitializer::Trace(Visitor* visitor) const {
  visitor->Trace(dispatcher_);
  visitor->Trace(options_);
  visitor->Trace(permission_service_);
  ScriptPromiseResolver::Trace(visitor);
}

ExecutionContext* MIDIAccessInitializer::GetExecutionContext() const {
  return ExecutionContext::From(GetScriptState());
}

void MIDIAccessInitializer::StartSession() {
  DCHECK(!dispatcher_);

  dispatcher_ = MakeGarbageCollected<MIDIDispatcher>(GetExecutionContext());
  dispatcher_->SetClient(this);
}

void MIDIAccessInitializer::OnPermissionsUpdated(PermissionStatus status) {
  permission_service_.reset();
  if (status == PermissionStatus::GRANTED) {
    StartSession();
  } else {
    Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
  }
}

void MIDIAccessInitializer::OnPermissionUpdated(PermissionStatus status) {
  permission_service_.reset();
  if (status == PermissionStatus::GRANTED) {
    StartSession();
  } else {
    Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError));
  }
}

}  // namespace blink
