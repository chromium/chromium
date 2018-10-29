// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_access_initializer.h"

#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access.h"
#include "third_party/blink/renderer/modules/webmidi/midi_options.h"
#include "third_party/blink/renderer/modules/webmidi/midi_port.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

namespace blink {

using midi::mojom::PortState;
using midi::mojom::Result;
using mojom::blink::PermissionStatus;

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* script_state,
                                             const MIDIOptions& options)
    : ScriptPromiseResolver(script_state), options_(options) {}

void MIDIAccessInitializer::ContextDestroyed(ExecutionContext* context) {
  accessor_.reset();
  permission_service_.reset();

  ScriptPromiseResolver::ContextDestroyed(context);
}

ScriptPromise MIDIAccessInitializer::Start() {
  ScriptPromise promise = this->Promise();
  accessor_ = MIDIAccessor::Create(this);

  ConnectToPermissionService(GetExecutionContext(),
                             mojo::MakeRequest(&permission_service_));

  Document& doc = To<Document>(*GetExecutionContext());
  permission_service_->RequestPermission(
      CreateMidiPermissionDescriptor(options_.hasSysex() && options_.sysex()),
      LocalFrame::HasTransientUserActivation(doc.GetFrame()),
      WTF::Bind(&MIDIAccessInitializer::OnPermissionsUpdated,
                WrapPersistent(this)));

  return promise;
}

void MIDIAccessInitializer::DidAddInputPort(const String& id,
                                            const String& manufacturer,
                                            const String& name,
                                            const String& version,
                                            PortState state) {
  DCHECK(accessor_);
  port_descriptors_.push_back(PortDescriptor(
      id, manufacturer, name, MIDIPort::kTypeInput, version, state));
}

void MIDIAccessInitializer::DidAddOutputPort(const String& id,
                                             const String& manufacturer,
                                             const String& name,
                                             const String& version,
                                             PortState state) {
  DCHECK(accessor_);
  port_descriptors_.push_back(PortDescriptor(
      id, manufacturer, name, MIDIPort::kTypeOutput, version, state));
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
  DCHECK(accessor_);
  // We would also have AbortError and SecurityError according to the spec.
  // SecurityError is handled in onPermission(s)Updated().
  switch (result) {
    case Result::NOT_INITIALIZED:
      break;
    case Result::OK:
      return Resolve(MIDIAccess::Create(
          std::move(accessor_), options_.hasSysex() && options_.sysex(),
          port_descriptors_, GetExecutionContext()));
    case Result::NOT_SUPPORTED:
      return Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError));
    case Result::INITIALIZATION_ERROR:
      return Reject(
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "Platform dependent initialization failed."));
  }
  NOTREACHED();
  Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                              "Unknown internal error occurred."));
}

ExecutionContext* MIDIAccessInitializer::GetExecutionContext() const {
  return ExecutionContext::From(GetScriptState());
}

void MIDIAccessInitializer::OnPermissionsUpdated(PermissionStatus status) {
  permission_service_.reset();
  if (status == PermissionStatus::GRANTED)
    accessor_->StartSession();
  else
    Reject(DOMException::Create(DOMExceptionCode::kSecurityError));
}

void MIDIAccessInitializer::OnPermissionUpdated(PermissionStatus status) {
  permission_service_.reset();
  if (status == PermissionStatus::GRANTED)
    accessor_->StartSession();
  else
    Reject(DOMException::Create(DOMExceptionCode::kSecurityError));
}

}  // namespace blink
