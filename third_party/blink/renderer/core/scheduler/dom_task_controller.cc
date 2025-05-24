// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_task_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_task_controller_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task_priority.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"

namespace blink {

// static
DOMTaskController* DOMTaskController::Create(ExecutionContext* context,
                                             TaskControllerInit* init) {
  return MakeGarbageCollected<DOMTaskController>(context, init->priority());
}

DOMTaskController::DOMTaskController(ExecutionContext* context,
                                     const V8TaskPriority& priority)
    : AbortController(MakeGarbageCollected<DOMTaskSignal>(
          context,
          priority.AsEnum(),
          AbortSignal::SignalType::kController)) {
  DCHECK(!context->IsContextDestroyed());
}

void DOMTaskController::setPriority(const V8TaskPriority& priority,
                                    ExceptionState& exception_state) {
  static_cast<DOMTaskSignal*>(signal())->SignalPriorityChange(priority.AsEnum(),
                                                              exception_state);
}

}  // namespace blink
