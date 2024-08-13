// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

namespace blink {

TaskAttributionInfoImpl::TaskAttributionInfoImpl(
    scheduler::TaskAttributionId id,
    SoftNavigationContext* soft_navigation_context)
    : id_(id), soft_navigation_context_(soft_navigation_context) {}

void TaskAttributionInfoImpl::Trace(Visitor* visitor) const {
  visitor->Trace(soft_navigation_context_);
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

SoftNavigationContext* TaskAttributionInfoImpl::GetSoftNavigationContext() {
  return soft_navigation_context_.Get();
}

scheduler::TaskAttributionId TaskAttributionInfoImpl::Id() const {
  return id_;
}

}  // namespace blink
