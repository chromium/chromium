// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"

#include "base/logging.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WakeLockManager::WakeLockManager(ExecutionContext* execution_context,
                                 WakeLockType type)
    : wake_lock_type_(type), execution_context_(execution_context) {
  DCHECK_NE(execution_context, nullptr);
}

void WakeLockManager::AcquireWakeLock(ScriptPromiseResolver* resolver) {
  // https://w3c.github.io/wake-lock/#acquire-wake-lock-algorithm
  // 1. If the wake lock for type type is not applicable, return false.
  // 2. Set active to true if the platform wake lock has an active wake lock for
  // type.
  // 3. Otherwise, ask the underlying operation system to acquire the wake lock
  // of type type and set active to true if the operation succeeded, or else
  // false.
  // 4. If active is true:
  // 4.1. Let document be the responsible document of the current settings
  //      object.
  // 4.2. Let record be the platform wake lock's state record associated with
  //      document and type.
  // 4.3. Add lock to record.[[ActiveLocks]].
  // 5. Return active.
  if (!wake_lock_) {
    mojo::Remote<mojom::blink::WakeLockService> wake_lock_service;
    execution_context_->GetBrowserInterfaceBroker().GetInterface(
        wake_lock_service.BindNewPipeAndPassReceiver());

    wake_lock_service->GetWakeLock(ToMojomWakeLockType(wake_lock_type_),
                                   device::mojom::blink::WakeLockReason::kOther,
                                   "Blink Wake Lock",
                                   wake_lock_.BindNewPipeAndPassReceiver());
    wake_lock_.set_disconnect_handler(WTF::Bind(
        &WakeLockManager::OnWakeLockConnectionError, WrapWeakPersistent(this)));
    wake_lock_->RequestWakeLock();
  }
  // https://w3c.github.io/wake-lock/#the-request-method
  // 5.2. Let lock be a new WakeLockSentinel object with its type attribute set
  // to type.
  // 5.4. Resolve promise with lock.
  auto* sentinel = MakeGarbageCollected<WakeLockSentinel>(
      resolver->GetScriptState(), wake_lock_type_, this);
  wake_lock_sentinels_.insert(sentinel);
  resolver->Resolve(sentinel);
}

void WakeLockManager::UnregisterSentinel(WakeLockSentinel* sentinel) {
  // https://w3c.github.io/wake-lock/#release-wake-lock-algorithm
  // 1. Let document be the responsible document of the current settings object.
  // 2. Let record be the platform wake lock's state record associated with
  // document and type.
  // 3. If record.[[ActiveLocks]] does not contain lock, abort these steps.
  auto iterator = wake_lock_sentinels_.find(sentinel);
  DCHECK(iterator != wake_lock_sentinels_.end());

  // 4. Remove lock from record.[[ActiveLocks]].
  wake_lock_sentinels_.erase(iterator);

  // 5. If the internal slot [[ActiveLocks]] of all the platform wake lock's
  // state records are all empty, then run the following steps in parallel:
  // 5.1. Ask the underlying operation system to release the wake lock of type
  //      type and let success be true if the operation succeeded, or else
  //      false.
  if (wake_lock_sentinels_.IsEmpty() && wake_lock_.is_bound()) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();

    // 5.2. If success is true and type is "screen" run the following:
    // 5.2.1. Reset the platform-specific inactivity timer after which the
    //        screen is actually turned off.
  }
}

void WakeLockManager::ClearWakeLocks() {
  while (!wake_lock_sentinels_.IsEmpty())
    (*wake_lock_sentinels_.begin())->DoRelease();
}

void WakeLockManager::OnWakeLockConnectionError() {
  wake_lock_.reset();
  ClearWakeLocks();
}

void WakeLockManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(wake_lock_sentinels_);
}

}  // namespace blink
