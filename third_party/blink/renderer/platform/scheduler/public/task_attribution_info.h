// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_INFO_H_

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink::scheduler {

class TaskAttributionInfo final : public GarbageCollected<TaskAttributionInfo> {
 public:
  explicit TaskAttributionInfo(TaskAttributionId task_id) : task_id_(task_id) {}

  ~TaskAttributionInfo() = default;

  TaskAttributionId Id() const { return task_id_; }

  void Trace(Visitor* visitor) const {}

 private:
  const TaskAttributionId task_id_;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_INFO_H_
