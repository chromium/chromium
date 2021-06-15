// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_PRIORITY_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_PRIORITY_CHANGE_EVENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_task_priority.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class TaskPriorityChangeEventInit;

class MODULES_EXPORT TaskPriorityChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TaskPriorityChangeEvent* Create(const AtomicString& type,
                                         const TaskPriorityChangeEventInit*);

  TaskPriorityChangeEvent(const AtomicString& type,
                          const TaskPriorityChangeEventInit*);
  ~TaskPriorityChangeEvent() override;

  const AtomicString& InterfaceName() const override;

  V8TaskPriority previousPriority() const;

 private:
  const V8TaskPriority previous_priority_;

  DISALLOW_COPY_AND_ASSIGN(TaskPriorityChangeEvent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_PRIORITY_CHANGE_EVENT_H_
