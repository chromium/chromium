// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_access_initializer.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
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

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* script_state,
                                             const MIDIOptions* options)
    : resolver_(MakeGarbageCollected<ScriptPromiseResolver<MIDIAccess>>(
          script_state)),
      options_(options),
      permission_service_(ExecutionContext::From(script_state)) {}

ScriptPromise<MIDIAccess> MIDIAccessInitializer::Start(LocalDOMWindow* window) {
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      window->GetTaskRunner(TaskType::kMiscPlatformAPI);

  ConnectToPermissionService(
      window,
      permission_service_.BindNewPipeAndPassReceiver(std::move(task_runner)));

  permission_service_->RequestPermission(
      CreateMidiPermissionDescriptor(
          base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)
              ? true
              : options_->hasSysex() && options_->sysex()),
      LocalFrame::HasTransientUserActivation(window->GetFrame()),
      WTF::BindOnce(&MIDIAccessInitializer::OnPermissionsUpdated,
                    WrapPersistent(this)));

  return resolver_->Promise();
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
  NOTREACHED_IN_MIGRATION();
}

void MIDIAccessInitializer::DidSetOutputPortState(unsigned port_index,
                                                  PortState state) {
  // See comments on didSetInputPortState().
  NOTREACHED_IN_MIGRATION();
}

void MIDIAccessInitializer::DidStartSession(Result result) {
  DCHECK(dispatcher_);
  // We would also have AbortError and SecurityError according to the spec.
  // SecurityError is handled in onPermission(s)Updated().
  switch (result) {
    case Result::NOT_INITIALIZED:
      NOTREACHED_IN_MIGRATION();
      return;
    case Result::OK:
      resolver_->Resolve(MakeGarbageCollected<MIDIAccess>(
          dispatcher_, options_->hasSysex() && options_->sysex(),
          port_descriptors_, resolver_->GetExecutionContext()));
      return;
    case Result::NOT_SUPPORTED:
      resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError));
      return;
    case Result::INITIALIZATION_ERROR:
      resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Platform dependent initialization failed."));
      return;
  }
}

void MIDIAccessInitializer::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(dispatcher_);
  visitor->Trace(options_);
  visitor->Trace(permission_service_);
}

void MIDIAccessInitializer::StartSession() {
  DCHECK(!dispatcher_);

  dispatcher_ =
      MakeGarbageCollected<MIDIDispatcher>(resolver_->GetExecutionContext());
  dispatcher_->SetClient(this);
}

void MIDIAccessInitializer::OnPermissionsUpdated(
    mojom::blink::PermissionStatus status) {
  permission_service_.reset();
  if (status == mojom::blink::PermissionStatus::GRANTED) {
    StartSession();
  } else {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError));
  }
}

void MIDIAccessInitializer::OnPermissionUpdated(
    mojom::blink::PermissionStatus status) {
  permission_service_.reset();
  if (status == mojom::blink::PermissionStatus::GRANTED) {
    StartSession();
  } else {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError));
  }
}

}  // namespace blink
