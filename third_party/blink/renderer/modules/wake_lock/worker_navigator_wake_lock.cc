// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/worker_navigator_wake_lock.h"

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

namespace blink {

WorkerNavigatorWakeLock::WorkerNavigatorWakeLock(WorkerNavigator& navigator)
    : Supplement<WorkerNavigator>(navigator) {}

// static
const char WorkerNavigatorWakeLock::kSupplementName[] =
    "WorkerNavigatorWakeLock";

// static
WorkerNavigatorWakeLock& WorkerNavigatorWakeLock::From(
    WorkerNavigator& navigator) {
  WorkerNavigatorWakeLock* supplement =
      Supplement<WorkerNavigator>::From<WorkerNavigatorWakeLock>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<WorkerNavigatorWakeLock>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
WakeLock* WorkerNavigatorWakeLock::wakeLock(ScriptState* script_state,
                                            WorkerNavigator& navigator) {
  return WorkerNavigatorWakeLock::From(navigator).GetWakeLock(script_state);
}

WakeLock* WorkerNavigatorWakeLock::GetWakeLock(ScriptState* script_state) {
  if (!wake_lock_) {
    auto* execution_context = ExecutionContext::From(script_state);
    DCHECK(execution_context);

    // TODO(https://crbug.com/839117): Remove this check once the Exposed
    // attribute is fixed to only expose this property in dedicated workers.
    if (execution_context->IsDedicatedWorkerGlobalScope()) {
      wake_lock_ = MakeGarbageCollected<WakeLock>(
          *To<DedicatedWorkerGlobalScope>(execution_context));
    }
  }
  return wake_lock_;
}

void WorkerNavigatorWakeLock::Trace(blink::Visitor* visitor) {
  visitor->Trace(wake_lock_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

}  // namespace blink
