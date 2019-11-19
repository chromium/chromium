// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

WakeLockSentinel::WakeLockSentinel(ScriptState* script_state,
                                   WakeLockType type,
                                   WakeLockManager* manager)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      manager_(manager),
      type_(type) {}

WakeLockSentinel::~WakeLockSentinel() = default;

ScriptPromise WakeLockSentinel::release(ScriptState* script_state) {
  // https://w3c.github.io/wake-lock/#the-release-method
  // 1. Let promise be a new promise.
  // 2. Run the following steps in parallel:
  // 2.1. Run release wake lock with lock set to this object and type set to the
  //      value of this object's type attribute.
  // 2.2. Resolve promise.
  // 3. Return promise.
  DoRelease();
  return ScriptPromise::CastUndefined(script_state);
}

String WakeLockSentinel::type() const {
  // https://w3c.github.io/wake-lock/#dom-wakelocksentinel-type
  // The type attribute corresponds to the WakeLockSentinel's wake lock type.
  switch (type_) {
    case WakeLockType::kScreen:
      return "screen";
    case WakeLockType::kSystem:
      return "system";
  }
}

ExecutionContext* WakeLockSentinel::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& WakeLockSentinel::InterfaceName() const {
  return event_target_names::kWakeLockSentinel;
}

void WakeLockSentinel::Trace(blink::Visitor* visitor) {
  visitor->Trace(manager_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

bool WakeLockSentinel::HasPendingActivity() const {
  // This WakeLockSentinel needs to remain alive as long as:
  // 1. DoRelease() has not not been called yet AND
  // 2. It has at least one event listener.
  return manager_ && HasEventListeners();
}

void WakeLockSentinel::ContextDestroyed(ExecutionContext*) {
  // Release all event listeners so that HasPendingActivity() does not return
  // true forever once a listener has been added to the object.
  RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
}

void WakeLockSentinel::DoRelease() {
  // https://w3c.github.io/wake-lock/#release-wake-lock-algorithm
  // 1. Let document be the responsible document of the current settings object.
  // 2. Let record be the platform wake lock's state record associated with
  // document and type.
  // 3. If record.[[ActiveLocks]] does not contain lock, abort these steps.
  if (!manager_)
    return;

  manager_->UnregisterSentinel(this);
  manager_.Clear();

  // This function may be called on ExecutionContext destruction. Events should
  // not be dispatched in this case.
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  // 6. Queue a task to fire an event named "release" at lock.
  DispatchEvent(*Event::Create(event_type_names::kRelease));
}

}  // namespace blink
