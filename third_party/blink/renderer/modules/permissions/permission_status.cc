// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_status.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/permissions/permission_status_listener.h"

namespace blink {

// static
PermissionStatus* PermissionStatus::Take(PermissionStatusListener* listener,
                                         ScriptPromiseResolver* resolver) {
  ExecutionContext* execution_context = resolver->GetExecutionContext();
  PermissionStatus* permission_status =
      MakeGarbageCollected<PermissionStatus>(listener, execution_context);
  permission_status->UpdateStateIfNeeded();
  permission_status->StartListening();
  return permission_status;
}

PermissionStatus::PermissionStatus(PermissionStatusListener* listener,
                                   ExecutionContext* execution_context)
    : ExecutionContextLifecycleStateObserver(execution_context),
      listener_(listener) {}

PermissionStatus::~PermissionStatus() = default;

const AtomicString& PermissionStatus::InterfaceName() const {
  return event_target_names::kPermissionStatus;
}

ExecutionContext* PermissionStatus::GetExecutionContext() const {
  return ExecutionContextLifecycleStateObserver::GetExecutionContext();
}

bool PermissionStatus::HasPendingActivity() const {
  if (!listener_)
    return false;
  return listener_->HasPendingActivity();
}

void PermissionStatus::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning)
    StartListening();
  else
    StopListening();
}

String PermissionStatus::state() const {
  if (!listener_)
    return String();
  return listener_->state();
}

String PermissionStatus::name() const {
  if (!listener_)
    return String();
  return listener_->name();
}

void PermissionStatus::StartListening() {
  if (!listener_)
    return;
  listener_->AddObserver(this);
}

void PermissionStatus::StopListening() {
  if (!listener_)
    return;
  listener_->RemoveObserver(this);
}

void PermissionStatus::OnPermissionStatusChange(MojoPermissionStatus status) {
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void PermissionStatus::Trace(Visitor* visitor) const {
  visitor->Trace(listener_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  PermissionStatusListener::Observer::Trace(visitor);
}

}  // namespace blink
