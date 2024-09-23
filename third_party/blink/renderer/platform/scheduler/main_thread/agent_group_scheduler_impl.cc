// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_group_scheduler_impl.h"

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/policy_updater.h"
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

AgentGroupSchedulerImpl::~AgentGroupSchedulerImpl() {
  CHECK(page_schedulers_.empty());
}

void AgentGroupSchedulerImpl::Dispose() {
  default_task_queue_->DetachTaskQueue();
  compositor_task_queue_->DetachTaskQueue();
}

std::unique_ptr<PageScheduler> AgentGroupSchedulerImpl::CreatePageScheduler(
    PageScheduler::Delegate* delegate) {
  CHECK(!is_updating_policy_);
  auto page_scheduler = std::make_unique<PageSchedulerImpl>(delegate, *this);
  main_thread_scheduler_->AddPageScheduler(page_scheduler.get());
  page_schedulers_.insert(page_scheduler.get());
  return page_scheduler;
}

scoped_refptr<base::SingleThreadTaskRunner>
AgentGroupSchedulerImpl::DefaultTaskRunner() {
  return default_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
AgentGroupSchedulerImpl::CompositorTaskRunner() {
  return compositor_task_runner_;
}

scoped_refptr<MainThreadTaskQueue>
AgentGroupSchedulerImpl::CompositorTaskQueue() {
  return compositor_task_queue_;
}

WebThreadScheduler& AgentGroupSchedulerImpl::GetMainThreadScheduler() {
  return *main_thread_scheduler_;
}

v8::Isolate* AgentGroupSchedulerImpl::Isolate() {
  // TODO(dtapuska): crbug.com/1051790 implement an Isolate per scheduler.
  v8::Isolate* isolate = main_thread_scheduler_->isolate();
  DCHECK(isolate);
  return isolate;
}

void AgentGroupSchedulerImpl::AddAgent(Agent* agent) {
  DCHECK(!base::Contains(agents_, agent));
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

void AgentGroupSchedulerImpl::AddPageSchedulerForTesting(
    PageSchedulerImpl* page_scheduler) {
  CHECK(!is_updating_policy_);
  CHECK(!base::Contains(page_schedulers_, page_scheduler));
  page_schedulers_.insert(page_scheduler);
}

void AgentGroupSchedulerImpl::RemovePageScheduler(
    PageSchedulerImpl* page_scheduler) {
  CHECK(!is_updating_policy_);
  auto it = page_schedulers_.find(page_scheduler);
  CHECK(it != page_schedulers_.end());
  page_schedulers_.erase(it);
}

void AgentGroupSchedulerImpl::IncrementVisibleFramesForAgent(
    const base::UnguessableToken& agent_cluster_id,
    PolicyUpdater& policy_updater) {
  // `agent_cluster_id` can be empty in tests.
  if (agent_cluster_id.is_empty()) {
    return;
  }
  auto [it, was_inserted] =
      num_visible_frames_per_agent_.emplace(agent_cluster_id, 0);
  CHECK_EQ(was_inserted, it->second == 0);
  if (it->second == 0) {
    policy_updater.UpdateAgentGroupPolicy(this);
  }
  it->second++;
}

void AgentGroupSchedulerImpl::DecrementVisibleFramesForAgent(
    const base::UnguessableToken& agent_cluster_id,
    PolicyUpdater& policy_updater) {
  // `agent_cluster_id` can be empty in tests.
  if (agent_cluster_id.is_empty()) {
    return;
  }
  auto it = num_visible_frames_per_agent_.find(agent_cluster_id);
  CHECK(it != num_visible_frames_per_agent_.end());
  if (it->second == 1) {
    policy_updater.UpdateAgentGroupPolicy(this);
    num_visible_frames_per_agent_.erase(it);
  } else {
    it->second--;
  }
}

bool AgentGroupSchedulerImpl::IsAgentVisible(
    const base::UnguessableToken& agent_cluster_id) const {
  auto it = num_visible_frames_per_agent_.find(agent_cluster_id);
  if (it == num_visible_frames_per_agent_.end()) {
    return false;
  }
  CHECK_GT(it->second, 0);
  return true;
}

void AgentGroupSchedulerImpl::UpdatePolicy() {
  CHECK(!is_updating_policy_);
  base::AutoReset auto_reset(&is_updating_policy_, true);

  for (auto* page_scheduler : page_schedulers_) {
    page_scheduler->UpdatePolicy();
  }
}

void AgentGroupSchedulerImpl::OnUrgentMessageReceived() {
  // TODO(crbug.com/40114705): This forwards to `main_thread_scheduler_`, which
  // will prioritize all default task queues until the urgent messages are
  // handled. It might be better to only prioritize `default_task_queue_`, which
  // depends on MBIMode being non-legacy and MbiOverrideTaskRunnerHandle being
  // enabled (because of crbug.com/40182014).
  main_thread_scheduler_->OnUrgentMessageReceived();
}

void AgentGroupSchedulerImpl::OnUrgentMessageProcessed() {
  main_thread_scheduler_->OnUrgentMessageProcessed();
}

}  // namespace scheduler
}  // namespace blink
