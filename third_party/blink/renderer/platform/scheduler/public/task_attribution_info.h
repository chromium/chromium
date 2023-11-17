// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_INFO_H_

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink::scheduler {

class TaskAttributionInfo final : public GarbageCollected<TaskAttributionInfo> {
  USING_PRE_FINALIZER(TaskAttributionInfo, Dispose);

 public:
  TaskAttributionInfo(TaskAttributionId task_id, TaskAttributionInfo* parent)
      : task_id_(task_id),
        parent_(parent),
        chain_length_(parent ? parent->chain_length_ + 1 : 1) {}

  ~TaskAttributionInfo() = default;

  PLATFORM_EXPORT void Dispose();
  TaskAttributionId Id() const { return task_id_; }
  TaskAttributionInfo* Parent() const { return parent_.Get(); }
  bool MaxChainLengthReached() const {
    return chain_length_ >= kMaxChainLength;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(parent_);
  }

 private:
  static constexpr uint8_t kMaxChainLength = 10;
  const TaskAttributionId task_id_;
  const Member<TaskAttributionInfo> parent_;
  const uint8_t chain_length_;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_TASK_ATTRIBUTION_INFO_H_
