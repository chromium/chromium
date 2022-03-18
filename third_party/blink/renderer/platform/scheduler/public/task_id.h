// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ID_H_

#include <cstdint>
#include "base/types/strong_alias.h"

namespace blink::scheduler {

using TaskIdType = uint32_t;

// TaskId represents the ID of a task, enabling comparison and incrementation
// operations on it, while abstracting the underlying value from callers.
class TaskId {
 public:
  explicit TaskId(TaskIdType value) : value_(value) {}
  TaskId(const TaskId&) = default;
  TaskId& operator=(const TaskId&) = default;
  scheduler::TaskIdType value() const { return value_; }

  bool operator==(const TaskId& id) const { return id.value_ == value_; }
  bool operator!=(const TaskId& id) const { return id.value_ != value_; }
  bool operator<(const TaskId& id) const { return value_ < id.value_; }
  TaskId NextTaskId() const { return TaskId(value_ + 1); }

 private:
  TaskIdType value_;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ID_H_
