// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"

#include "base/check_op.h"
#include "base/not_fatal_until.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WakeLockManager::WakeLockManager(ExecutionContext* execution_context,
                                 V8WakeLockType::Enum type)
    : wake_lock_(execution_context),
      wake_lock_type_(type),
      execution_context_(execution_context) {
  DCHECK_NE(execution_context, nullptr);
}

void WakeLockManager::AcquireWakeLock(
    ScriptPromiseResolver<WakeLockSentinel>* resolver) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method
  if (!wake_lock_.is_bound()) {
    // 8.3.2. If document.[[ActiveLocks]]["screen"] is empty, then invoke the
    //        following steps in parallel:
    // 8.3.2.1. Invoke acquire a wake lock with "screen".
    mojo::Remote<mojom::blink::WakeLockService> wake_lock_service;
    execution_context_->GetBrowserInterfaceBroker().GetInterface(
        wake_lock_service.BindNewPipeAndPassReceiver());

    wake_lock_service->GetWakeLock(
        ToMojomWakeLockType(wake_lock_type_),
        device::mojom::blink::WakeLockReason::kOther, "Blink Wake Lock",
        wake_lock_.BindNewPipeAndPassReceiver(
            execution_context_->GetTaskRunner(TaskType::kWakeLock)));
    wake_lock_.set_disconnect_handler(WTF::BindOnce(
        &WakeLockManager::OnWakeLockConnectionError, WrapWeakPersistent(this)));
    wake_lock_->RequestWakeLock();
  }
  // 8.3.3. Let lock be a new WakeLockSentinel object with its type attribute
  //        set to type.
  // 8.3.4. Append lock to document.[[ActiveLocks]]["screen"].
  // 8.3.5. Resolve promise with lock.
  auto* sentinel = MakeGarbageCollected<WakeLockSentinel>(
      resolver->GetScriptState(), wake_lock_type_, this);
  wake_lock_sentinels_.insert(sentinel);
  resolver->Resolve(sentinel);
}

void WakeLockManager::UnregisterSentinel(WakeLockSentinel* sentinel) {
  // https://w3c.github.io/screen-wake-lock/#release-wake-lock-algorithm
  // 1. If document.[[ActiveLocks]][type] does not contain lock, abort these
  //    steps.
  auto iterator = wake_lock_sentinels_.find(sentinel);
  CHECK(iterator != wake_lock_sentinels_.end(), base::NotFatalUntil::M130);

  // 2. Remove lock from document.[[ActiveLocks]][type].
  wake_lock_sentinels_.erase(iterator);

  // 3. If document.[[ActiveLocks]][type] is empty, then run the following steps
  //    in parallel:
  // 3.1. Ask the underlying operating system to release the wake lock of type
  //      type and let success be true if the operation succeeded, or else
  //      false.
  if (wake_lock_sentinels_.empty() && wake_lock_.is_bound()) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }
}

void WakeLockManager::ClearWakeLocks() {
  while (!wake_lock_sentinels_.empty())
    (*wake_lock_sentinels_.begin())->DoRelease();
}

void WakeLockManager::OnWakeLockConnectionError() {
  wake_lock_.reset();
  ClearWakeLocks();
}

void WakeLockManager::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(wake_lock_sentinels_);
  visitor->Trace(wake_lock_);
}

}  // namespace blink
