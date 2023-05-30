// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_THREAD_DELEGATE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_THREAD_DELEGATE_H_

#include "base/component_export.h"
#include "base/message_loop/message_pump.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread.h"

namespace base {
namespace sequence_manager {
class TaskQueue;
class SequenceManager;
}  // namespace sequence_manager
}  // namespace base

namespace network {

enum class TaskPriority : base::sequence_manager::TaskQueue::QueuePriority {
  kHighPriority = 0,
  kNormalPriority = 1,

  kNumPriorities = 2,
};

// A thread delegate which allows running high priority tasks.
class COMPONENT_EXPORT(NETWORK_CPP) ThreadDelegate
    : public base::Thread::Delegate {
 public:
  explicit ThreadDelegate(base::MessagePumpType message_pump_type);
  ~ThreadDelegate() override;

  // Gets the high priority task runner for this thread, or the default task
  // runner if a high priority task runner doesn't exist.
  static scoped_refptr<base::SequencedTaskRunner> GetHighPriorityTaskRunner();

  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override;
  void BindToCurrentThread(base::TimerSlack timer_slack) override;

 private:
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue_;
  scoped_refptr<base::sequence_manager::TaskQueue> high_priority_task_queue_;
  base::MessagePumpType message_pump_type_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_THREAD_DELEGATE_H_
