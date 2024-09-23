// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_priority_change_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_task_priority_change_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
TaskPriorityChangeEvent* TaskPriorityChangeEvent::Create(
    const AtomicString& type,
    const TaskPriorityChangeEventInit* initializer) {
  return MakeGarbageCollected<TaskPriorityChangeEvent>(type, initializer);
}

TaskPriorityChangeEvent::TaskPriorityChangeEvent(
    const AtomicString& type,
    const TaskPriorityChangeEventInit* initializer)
    : Event(type, initializer),
      previous_priority_(initializer->previousPriority()) {}

TaskPriorityChangeEvent::~TaskPriorityChangeEvent() = default;

const AtomicString& TaskPriorityChangeEvent::InterfaceName() const {
  return event_interface_names::kTaskPriorityChangeEvent;
}

V8TaskPriority TaskPriorityChangeEvent::previousPriority() const {
  return previous_priority_;
}

}  // namespace blink
