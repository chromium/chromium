// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

namespace blink {
namespace scheduler {

// static
std::unique_ptr<WebAgentGroupScheduler>
WebAgentGroupScheduler::CreateForTesting() {
  return std::make_unique<WebAgentGroupScheduler>(
      CreateDummyAgentGroupScheduler());
}

MainThreadTaskQueue::QueueCreationParams DefaultTaskQueueCreationParams(
    AgentGroupSchedulerImpl* agent_group_scheduler_impl) {
  return MainThreadTaskQueue::QueueCreationParams(
             MainThreadTaskQueue::QueueType::kDefault)
      .SetShouldMonitorQuiescence(true)
      .SetAgentGroupScheduler(agent_group_scheduler_impl);
}

MainThreadTaskQueue::QueueCreationParams CompositorTaskRunnerCreationParams(
    AgentGroupSchedulerImpl* agent_group_scheduler_impl) {
  return MainThreadTaskQueue::QueueCreationParams(
             MainThreadTaskQueue::QueueType::kCompositor)
      .SetShouldMonitorQuiescence(true)
      .SetPrioritisationType(
          MainThreadTaskQueue::QueueTraits::PrioritisationType::kCompositor)
      .SetAgentGroupScheduler(agent_group_scheduler_impl);
}

AgentGroupSchedulerImpl::AgentGroupSchedulerImpl(
    MainThreadSchedulerImpl& main_thread_scheduler)
    : default_task_queue_(main_thread_scheduler.NewTaskQueue(
          DefaultTaskQueueCreationParams(this))),
      default_task_runner_(default_task_queue_->CreateTaskRunner(
          TaskType::kMainThreadTaskQueueDefault)),
      compositor_task_queue_(main_thread_scheduler.NewTaskQueue(
          CompositorTaskRunnerCreationParams(this))),
      compositor_task_runner_(compositor_task_queue_->CreateTaskRunner(
          TaskType::kMainThreadTaskQueueCompositor)),
      main_thread_scheduler_(main_thread_scheduler) {
  DCHECK(!default_task_queue_->GetFrameScheduler());
  DCHECK_EQ(default_task_queue_->GetAgentGroupScheduler(), this);
}

void AgentGroupSchedulerImpl::Dispose() {
  default_task_queue_->DetachFromMainThreadScheduler();
  compositor_task_queue_->DetachFromMainThreadScheduler();
}

std::unique_ptr<PageScheduler> AgentGroupSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  auto page_scheduler = std::make_unique<PageSchedulerImpl>(delegate, *this);
  main_thread_scheduler_.AddPageScheduler(page_scheduler.get());
  return page_scheduler;
}

scoped_refptr<base::SingleThreadTaskRunner>
AgentGroupSchedulerImpl::DefaultTaskRunner() {
  return default_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
AgentGroupSchedulerImpl::CompositorTaskRunner() {
  if (main_thread_scheduler_.scheduling_settings()
          .mbi_compositor_task_runner_per_agent_scheduling_group) {
    return compositor_task_runner_;
  }
  // We temporarily redirect the per-AGS compositor task runner to the main
  // thread's compositor task runner.
  return main_thread_scheduler_.CompositorTaskRunner();
}

scoped_refptr<MainThreadTaskQueue>
AgentGroupSchedulerImpl::CompositorTaskQueue() {
  return compositor_task_queue_;
}

WebThreadScheduler& AgentGroupSchedulerImpl::GetMainThreadScheduler() {
  return main_thread_scheduler_;
}

void AgentGroupSchedulerImpl::BindInterfaceBroker(
    mojo::PendingRemote<mojom::BrowserInterfaceBroker> remote_broker) {
  DCHECK(!broker_.is_bound());
  broker_.Bind(std::move(remote_broker), default_task_runner_);
}

BrowserInterfaceBrokerProxy&
AgentGroupSchedulerImpl::GetBrowserInterfaceBroker() {
  DCHECK(broker_.is_bound());
  return broker_;
}

v8::Isolate* AgentGroupSchedulerImpl::Isolate() {
  // TODO(dtapuska): crbug.com/1051790 implement an Isolate per scheduler.
  v8::Isolate* isolate = main_thread_scheduler_.isolate();
  DCHECK(isolate);
  return isolate;
}

void AgentGroupSchedulerImpl::AddAgent(Agent* agent) {
  DCHECK(agents_.find(agent) == agents_.end());
  agents_.insert(agent);
}

void AgentGroupSchedulerImpl::PerformMicrotaskCheckpoint() {
  // This code is performance sensitive so we do not wish to allocate
  // memory, use an inline vector of 10.
  HeapVector<Member<Agent>, 10> agents;
  for (Agent* agent : agents_) {
    agents.push_back(agent);
  }
  for (Agent* agent : agents) {
    DCHECK(agents_.Contains(agent));
    agent->PerformMicrotaskCheckpoint();
  }
}

void AgentGroupSchedulerImpl::Trace(Visitor* visitor) const {
  AgentGroupScheduler::Trace(visitor);
  visitor->Trace(agents_);
}

}  // namespace scheduler
}  // namespace blink
