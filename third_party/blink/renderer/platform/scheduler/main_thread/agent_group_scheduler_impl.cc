// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

namespace blink {
namespace scheduler {

// static
std::unique_ptr<WebAgentGroupScheduler>
WebAgentGroupScheduler::CreateForTesting() {
  return CreateDummyAgentGroupScheduler();
}

MainThreadTaskQueue::QueueCreationParams DefaultTaskQueueCreationParams(
    AgentGroupSchedulerImpl* agent_group_scheduler_impl) {
  return MainThreadTaskQueue::QueueCreationParams(
             MainThreadTaskQueue::QueueType::kDefault)
      .SetShouldMonitorQuiescence(true)
      .SetAgentGroupScheduler(agent_group_scheduler_impl);
}

AgentGroupSchedulerImpl::AgentGroupSchedulerImpl(
    MainThreadSchedulerImpl& main_thread_scheduler)
    : default_task_queue_(main_thread_scheduler.NewTaskQueue(
          DefaultTaskQueueCreationParams(this))),
      default_task_runner_(default_task_queue_->CreateTaskRunner(
          TaskType::kMainThreadTaskQueueDefault)),
      main_thread_scheduler_(main_thread_scheduler) {
  DCHECK(!default_task_queue_->GetFrameScheduler());
  DCHECK_EQ(default_task_queue_->GetAgentGroupScheduler(), this);
}

AgentGroupSchedulerImpl::~AgentGroupSchedulerImpl() {
  default_task_queue_->ShutdownTaskQueue();
  main_thread_scheduler_.RemoveAgentGroupScheduler(this);
}

std::unique_ptr<PageScheduler> AgentGroupSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  auto page_scheduler = std::make_unique<PageSchedulerImpl>(delegate, *this);
  main_thread_scheduler_.AddPageScheduler(page_scheduler.get());
  return page_scheduler;
}

AgentGroupScheduler& AgentGroupSchedulerImpl::AsAgentGroupScheduler() {
  return *this;
}

}  // namespace scheduler
}  // namespace blink
