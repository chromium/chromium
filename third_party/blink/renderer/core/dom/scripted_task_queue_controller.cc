// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_task_queue_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scripted_task_queue.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

const char ScriptedTaskQueueController::kSupplementName[] =
    "ScriptedTaskQueueController";

ScriptedTaskQueueController* ScriptedTaskQueueController::From(
    Document& document) {
  ScriptedTaskQueueController* task_queue_controller =
      Supplement<Document>::From<ScriptedTaskQueueController>(document);
  if (!task_queue_controller) {
    task_queue_controller = new ScriptedTaskQueueController(&document);
    Supplement<Document>::ProvideTo(document, task_queue_controller);
  }
  return task_queue_controller;
}

ScriptedTaskQueueController::ScriptedTaskQueueController(
    ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

void ScriptedTaskQueueController::Trace(blink::Visitor* visitor) {
  visitor->Trace(task_queues_);
  Supplement<Document>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

ScriptedTaskQueue* ScriptedTaskQueueController::defaultQueue(
    const String& queue_name) {
  auto iter = task_queues_.find(queue_name);
  if (iter != task_queues_.end()) {
    return iter->value.Get();
  }

  TaskType task_type = TaskType::kExperimentalWebSchedulingBestEffort;
  if (queue_name == "user-interaction")
    task_type = TaskType::kExperimentalWebSchedulingUserInteraction;
  else if (queue_name != "best-effort")
    NOTREACHED();

  auto* task_queue =
      ScriptedTaskQueue::Create(GetExecutionContext(), task_type);
  task_queues_.insert(queue_name, task_queue);

  return task_queue;
}

}  // namespace blink
