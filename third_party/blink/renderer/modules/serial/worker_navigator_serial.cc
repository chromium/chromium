// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/worker_navigator_serial.h"

#include "third_party/blink/renderer/modules/serial/serial.h"

namespace blink {

const char WorkerNavigatorSerial::kSupplementName[] = "WorkerNavigatorSerial";

WorkerNavigatorSerial& WorkerNavigatorSerial::From(WorkerNavigator& navigator) {
  WorkerNavigatorSerial* supplement =
      Supplement<WorkerNavigator>::From<WorkerNavigatorSerial>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<WorkerNavigatorSerial>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

Serial* WorkerNavigatorSerial::serial(ScriptState* script_state,
                                      WorkerNavigator& navigator) {
  return WorkerNavigatorSerial::From(navigator).serial(script_state);
}

Serial* WorkerNavigatorSerial::serial(ScriptState* script_state) {
  if (!serial_) {
    auto* execution_context = ExecutionContext::From(script_state);
    DCHECK(execution_context);

    // TODO(https://crbug.com/839117): Remove this check once the Exposed
    // attribute is fixed to only expose this property in dedicated workers.
    if (execution_context->IsDedicatedWorkerGlobalScope())
      serial_ = MakeGarbageCollected<Serial>(*execution_context);
  }
  return serial_;
}

void WorkerNavigatorSerial::Trace(blink::Visitor* visitor) {
  visitor->Trace(serial_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

WorkerNavigatorSerial::WorkerNavigatorSerial(WorkerNavigator& navigator)
    : Supplement<WorkerNavigator>(navigator) {}

}  // namespace blink
