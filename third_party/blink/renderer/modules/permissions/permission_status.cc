// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_status.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/permissions/permission_status_listener.h"

namespace blink {

// static
PermissionStatus* PermissionStatus::Take(PermissionStatusListener* listener,
                                         ScriptPromiseResolverBase* resolver) {
  ExecutionContext* execution_context = resolver->GetExecutionContext();
  PermissionStatus* permission_status =
      MakeGarbageCollected<PermissionStatus>(listener, execution_context);
  permission_status->UpdateStateIfNeeded();
  permission_status->StartListening();
  return permission_status;
}

PermissionStatus::PermissionStatus(PermissionStatusListener* listener,
                                   ExecutionContext* execution_context)
    : ActiveScriptWrappable<PermissionStatus>({}),
      ExecutionContextLifecycleStateObserver(execution_context),
      listener_(listener) {}

PermissionStatus::~PermissionStatus() = default;

const AtomicString& PermissionStatus::InterfaceName() const {
  return event_target_names::kPermissionStatus;
}

ExecutionContext* PermissionStatus::GetExecutionContext() const {
  return ExecutionContextLifecycleStateObserver::GetExecutionContext();
}

void PermissionStatus::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);

  if (!listener_)
    return;

  if (event_type == event_type_names::kChange) {
    listener_->AddedEventListener(event_type);
  }
}

void PermissionStatus::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!listener_)
    return;

  // Permission `change` event listener can be set via two independent JS-API.
  // We should remove an internal listener only if none of the two JS-based
  // event listeners exist. Without checking it, the internal listener will be
  // removed while there could be an alive JS listener.
  if (!HasJSBasedEventListeners(event_type_names::kChange)) {
    listener_->RemovedEventListener(event_type);
  }
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
  // https://www.w3.org/TR/permissions/#onchange-attribute
  // 1. If this's relevant global object is a Window object, then:
  // - Let document be status's relevant global object's associated Document.
  // - If document is null or document is not fully active, terminate this
  // algorithm.
  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    auto* document = window->document();
    if (!document || !document->IsActive()) {
      // Note: if the event is dropped out while in BFCache, one single change
      // event might be dispatched later when the page is restored from BFCache.
      return;
    }
  }
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void PermissionStatus::Trace(Visitor* visitor) const {
  visitor->Trace(listener_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  PermissionStatusListener::Observer::Trace(visitor);
}

}  // namespace blink
