// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/core/timing/resource_timing_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

namespace blink {

TaskAttributionInfoImpl::TaskAttributionInfoImpl(
    SoftNavigationContext* soft_navigation_context,
    ResourceTimingContext* resource_timing_context,
    uint32_t async_data_for_test)
    : id_(scheduler::TaskAttributionId::NextId()),
      async_data_for_test_(async_data_for_test),
      soft_navigation_context_(soft_navigation_context),
      resource_timing_context_(resource_timing_context) {}

void TaskAttributionInfoImpl::Trace(Visitor* visitor) const {
  TaskAttributionTaskState::Trace(visitor);
  visitor->Trace(soft_navigation_context_);
  visitor->Trace(resource_timing_context_);
}

SchedulerTaskContext* TaskAttributionInfoImpl::GetSchedulerTaskContext() {
  return nullptr;
}

bool TaskAttributionInfoImpl::IsTaskAttributionInfoImpl() const {
  return true;
}

TaskAttributionTaskState* TaskAttributionInfoImpl::ForkAndSetVariable(
    ResourceTimingContext* resource_timing_context) {
  return MakeGarbageCollected<TaskAttributionInfoImpl>(
      GetSoftNavigationContext(), resource_timing_context, AsyncDataForTest());
}

TaskAttributionTaskState* TaskAttributionInfoImpl::ForkAndSetVariable(
    SoftNavigationContext* soft_navigation_context) {
  return MakeGarbageCollected<TaskAttributionInfoImpl>(
      soft_navigation_context, GetResourceTimingContext(), AsyncDataForTest());
}

scheduler::TaskAttributionInfo*
TaskAttributionInfoImpl::GetTaskAttributionInfo() {
  return this;
}

SoftNavigationContext* TaskAttributionInfoImpl::GetSoftNavigationContext() {
  return soft_navigation_context_.Get();
}

ResourceTimingContext* TaskAttributionInfoImpl::GetResourceTimingContext() {
  return resource_timing_context_.Get();
}

scheduler::TaskAttributionId TaskAttributionInfoImpl::Id() const {
  return id_;
}

uint32_t TaskAttributionInfoImpl::AsyncDataForTest() const {
  return async_data_for_test_;
}
}  // namespace blink
