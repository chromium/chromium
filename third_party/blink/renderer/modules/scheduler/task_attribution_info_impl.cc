// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_attribution_info_impl.h"

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"

namespace blink {

TaskAttributionInfoImpl::TaskAttributionInfoImpl(
    scheduler::TaskAttributionId id)
    : id_(id) {}

void TaskAttributionInfoImpl::Trace(Visitor* visitor) const {
  ScriptWrappableTaskState::Trace(visitor);
}

AbortSignal* TaskAttributionInfoImpl::AbortSource() {
  return nullptr;
}

DOMTaskSignal* TaskAttributionInfoImpl::PrioritySource() {
  return nullptr;
}

scheduler::TaskAttributionInfo*
TaskAttributionInfoImpl::GetTaskAttributionInfo() {
  return this;
}

scheduler::TaskAttributionId TaskAttributionInfoImpl::Id() const {
  return id_;
}

}  // namespace blink
