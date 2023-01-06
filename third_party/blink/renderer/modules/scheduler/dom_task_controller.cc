// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_controller.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_task_controller_init.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

namespace blink {

// static
DOMTaskController* DOMTaskController::Create(ExecutionContext* context,
                                             TaskControllerInit* init) {
  return MakeGarbageCollected<DOMTaskController>(context, init->priority());
}

DOMTaskController::DOMTaskController(ExecutionContext* context,
                                     const AtomicString& priority)
    : AbortController(MakeGarbageCollected<DOMTaskSignal>(
          context,
          priority,
          AbortSignal::SignalType::kController)) {
  DCHECK(!context->IsContextDestroyed());
}

void DOMTaskController::setPriority(const AtomicString& priority,
                                    ExceptionState& exception_state) {
  static_cast<DOMTaskSignal*>(signal())->SignalPriorityChange(priority,
                                                              exception_state);
}

}  // namespace blink
