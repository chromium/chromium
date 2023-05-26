// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/thread_delegate.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace network {
namespace {

using ProtoPriority = perfetto::protos::pbzero::SequenceManagerTask::Priority;

ABSL_CONST_INIT thread_local ThreadDelegate* thread_local_delegate = nullptr;

ProtoPriority TaskPriorityToProto(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  DCHECK_LT(static_cast<size_t>(priority),
            static_cast<size_t>(TaskPriority::kNumPriorities));
  switch (static_cast<TaskPriority>(priority)) {
    case TaskPriority::kHighPriority:
      return ProtoPriority::HIGH_PRIORITY;
    case TaskPriority::kNormalPriority:
      return ProtoPriority::NORMAL_PRIORITY;
    case TaskPriority::kNumPriorities:
      return ProtoPriority::UNKNOWN;
  }
}

base::sequence_manager::SequenceManager::PrioritySettings
GetPrioritySettings() {
  base::sequence_manager::SequenceManager::PrioritySettings settings(
      TaskPriority::kNumPriorities, TaskPriority::kNormalPriority);
  settings.SetProtoPriorityConverter(&TaskPriorityToProto);
  return settings;
}

}  // namespace

// static
scoped_refptr<base::SequencedTaskRunner>
ThreadDelegate::GetHighPriorityTaskRunner() {
  if (thread_local_delegate) {
    return thread_local_delegate->high_priority_task_queue_->task_runner();
  }
  return base::SequencedTaskRunner::GetCurrentDefault();
}

ThreadDelegate::ThreadDelegate(base::MessagePumpType message_pump_type)
    : sequence_manager_(base::sequence_manager::CreateUnboundSequenceManager(
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetMessagePumpType(message_pump_type)
              .SetPrioritySettings(GetPrioritySettings())
              .Build())),
      default_task_queue_(sequence_manager_->CreateTaskQueue(
          base::sequence_manager::TaskQueue::Spec(
              base::sequence_manager::QueueName::DEFAULT_TQ))),
      high_priority_task_queue_(sequence_manager_->CreateTaskQueue(
          base::sequence_manager::TaskQueue::Spec(
              base::sequence_manager::QueueName::OTHER_TQ))),
      message_pump_type_(message_pump_type) {
  default_task_queue_->SetQueuePriority(TaskPriority::kNormalPriority);
  high_priority_task_queue_->SetQueuePriority(TaskPriority::kHighPriority);
  sequence_manager_->SetDefaultTaskRunner(default_task_queue_->task_runner());
}

ThreadDelegate::~ThreadDelegate() {
  thread_local_delegate = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
ThreadDelegate::GetDefaultTaskRunner() {
  return default_task_queue_->task_runner();
}

void ThreadDelegate::BindToCurrentThread(base::TimerSlack timer_slack) {
  thread_local_delegate = this;
  sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(message_pump_type_));
  sequence_manager_->SetTimerSlack(timer_slack);
}

}  // namespace network
