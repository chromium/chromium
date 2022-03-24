// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_status.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/permissions/permissions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
PermissionStatus* PermissionStatus::Take(
    Permissions& associated_permissions_object,
    ScriptPromiseResolver* resolver,
    MojoPermissionStatus status,
    MojoPermissionDescriptor descriptor) {
  return PermissionStatus::CreateAndListen(associated_permissions_object,
                                           resolver->GetExecutionContext(),
                                           status, std::move(descriptor));
}

PermissionStatus* PermissionStatus::CreateAndListen(
    Permissions& associated_permissions_object,
    ExecutionContext* execution_context,
    MojoPermissionStatus status,
    MojoPermissionDescriptor descriptor) {
  PermissionStatus* permission_status = MakeGarbageCollected<PermissionStatus>(
      associated_permissions_object, execution_context, status,
      std::move(descriptor));
  permission_status->UpdateStateIfNeeded();
  permission_status->StartListening();
  return permission_status;
}

PermissionStatus::PermissionStatus(Permissions& associated_permissions_object,
                                   ExecutionContext* execution_context,
                                   MojoPermissionStatus status,
                                   MojoPermissionDescriptor descriptor)
    : ExecutionContextLifecycleStateObserver(execution_context),
      status_(status),
      descriptor_(std::move(descriptor)),
      receiver_(this, execution_context) {
  associated_permissions_object.PermissionStatusObjectCreated();
}

PermissionStatus::~PermissionStatus() = default;

const AtomicString& PermissionStatus::InterfaceName() const {
  return event_target_names::kPermissionStatus;
}

ExecutionContext* PermissionStatus::GetExecutionContext() const {
  return ExecutionContextLifecycleStateObserver::GetExecutionContext();
}

bool PermissionStatus::HasPendingActivity() const {
  return receiver_.is_bound();
}

void PermissionStatus::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning)
    StartListening();
  else
    StopListening();
}

String PermissionStatus::state() const {
  return PermissionStatusToString(status_);
}

String PermissionStatus::name() const {
  return PermissionNameToString(descriptor_->name);
}

void PermissionStatus::StartListening() {
  DCHECK(!receiver_.is_bound());
  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kPermission);
  receiver_.Bind(observer.InitWithNewPipeAndPassReceiver(), task_runner);

  mojo::Remote<mojom::blink::PermissionService> service;
  ConnectToPermissionService(GetExecutionContext(),
                             service.BindNewPipeAndPassReceiver(task_runner));
  service->AddPermissionObserver(descriptor_->Clone(), status_,
                                 std::move(observer));
}

void PermissionStatus::StopListening() {
  receiver_.reset();
}

void PermissionStatus::OnPermissionStatusChange(MojoPermissionStatus status) {
  if (status_ == status)
    return;

  status_ = status;
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void PermissionStatus::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
