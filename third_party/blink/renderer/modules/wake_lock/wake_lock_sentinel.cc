// Copyright 2019 The Chromium Authors
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
                                   V8WakeLockType::Enum type,
                                   WakeLockManager* manager)
    : ActiveScriptWrappable<WakeLockSentinel>({}),
      ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      manager_(manager),
      type_(type) {}

WakeLockSentinel::~WakeLockSentinel() = default;

ScriptPromise<IDLUndefined> WakeLockSentinel::release(
    ScriptState* script_state) {
  // https://w3c.github.io/screen-wake-lock/#the-release-method
  // 1. If this's [[Released]] is false, then run release a wake lock with lock
  //    set to this and type set to the value of this's type attribute.
  DoRelease();
  // 2. Return a promise resolved with undefined.
  return ToResolvedUndefinedPromise(script_state);
}

bool WakeLockSentinel::released() const {
  return released_;
}

V8WakeLockType WakeLockSentinel::type() const {
  // https://w3c.github.io/screen-wake-lock/#dom-wakelocksentinel-type
  // The type attribute corresponds to the WakeLockSentinel's wake lock type.
  return V8WakeLockType(type_);
}

ExecutionContext* WakeLockSentinel::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& WakeLockSentinel::InterfaceName() const {
  return event_target_names::kWakeLockSentinel;
}

void WakeLockSentinel::Trace(Visitor* visitor) const {
  visitor->Trace(manager_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool WakeLockSentinel::HasPendingActivity() const {
  // This WakeLockSentinel needs to remain alive as long as:
  // 1. DoRelease() has not not been called yet AND
  // 2. It has at least one event listener.
  return manager_ && HasEventListeners();
}

void WakeLockSentinel::ContextDestroyed() {
  // Release all event listeners so that HasPendingActivity() does not return
  // true forever once a listener has been added to the object.
  RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
}

void WakeLockSentinel::DoRelease() {
  // https://w3c.github.io/screen-wake-lock/#release-wake-lock-algorithm
  if (!manager_)
    return;

  manager_->UnregisterSentinel(this);
  manager_.Clear();

  // This function may be called on ExecutionContext destruction. Events should
  // not be dispatched in this case.
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  // 4. Set lock's [[Released]] to true.
  // 5. Fire an event named "release" at lock.
  DCHECK(!released_);
  released_ = true;
  DispatchEvent(*Event::Create(event_type_names::kRelease));
}

}  // namespace blink
