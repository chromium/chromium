// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_io_thread_delegate.h"

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/task_executor.h"
#include "base/task/task_observer.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/browser_thread.h"

namespace content {

using ::base::sequence_manager::CreateUnboundSequenceManager;
using ::base::sequence_manager::SequenceManager;
using ::base::sequence_manager::TaskQueue;

class BrowserIOThreadDelegate::TLSMultiplexer : public base::TaskObserver {
 public:
  TLSMultiplexer() = default;
  ~TLSMultiplexer() override = default;

  void SetIOTaskExecutor(base::TaskExecutor* io_task_executor) {
    io_task_executor_ = io_task_executor;
  }

  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {
    base::TaskExecutor* previous_executor =
        base::GetTaskExecutorForCurrentThread();
    if (previous_executor) {
      previous_executors_.push_back(previous_executor);
      base::SetTaskExecutorForCurrentThread(nullptr);
    }
    base::SetTaskExecutorForCurrentThread(io_task_executor_);
  }

  void DidProcessTask(const base::PendingTask& pending_task) override {
    base::SetTaskExecutorForCurrentThread(nullptr);
    if (!previous_executors_.empty()) {
      base::SetTaskExecutorForCurrentThread(previous_executors_.back());
      previous_executors_.pop_back();
    }
  }

  base::TaskExecutor* io_task_executor_ = nullptr;
  std::vector<base::TaskExecutor*> previous_executors_;
};

BrowserIOThreadDelegate::BrowserIOThreadDelegate()
    : owned_sequence_manager_(CreateUnboundSequenceManager(
          SequenceManager::Settings::Builder()
              .SetMessagePumpType(base::MessagePumpType::IO)
              .Build())),
      sequence_manager_(owned_sequence_manager_.get()) {
  Init();
}

BrowserIOThreadDelegate::BrowserIOThreadDelegate(
    SequenceManager* sequence_manager)
    : sequence_manager_(sequence_manager),
      tls_multiplexer_(std::make_unique<TLSMultiplexer>()) {
  sequence_manager_->AddTaskObserver(tls_multiplexer_.get());
  Init();
}

void BrowserIOThreadDelegate::Init() {
  task_queues_ = std::make_unique<BrowserTaskQueues>(
      BrowserThread::IO, sequence_manager_,
      sequence_manager_->GetRealTimeDomain());
  default_task_runner_ = task_queues_->GetHandle()->GetDefaultTaskRunner();
}

void BrowserIOThreadDelegate::SetTaskExecutor(
    base::TaskExecutor* task_executor) {
  if (tls_multiplexer_) {
    tls_multiplexer_->SetIOTaskExecutor(task_executor);
  } else {
    task_executor_ = task_executor;
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserIOThreadDelegate::GetDefaultTaskRunner() {
  return default_task_runner_;
}

BrowserIOThreadDelegate::~BrowserIOThreadDelegate() {
  if (task_executor_) {
    base::SetTaskExecutorForCurrentThread(nullptr);
  }
  if (tls_multiplexer_) {
    sequence_manager_->RemoveTaskObserver(tls_multiplexer_.get());
  }
}

void BrowserIOThreadDelegate::BindToCurrentThread(
    base::TimerSlack timer_slack) {
  DCHECK(sequence_manager_);
  sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(base::MessagePumpType::IO));
  sequence_manager_->SetTimerSlack(timer_slack);
  sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
  sequence_manager_->EnableCrashKeys("io_scheduler_async_stack");

  if (task_executor_) {
    base::SetTaskExecutorForCurrentThread(task_executor_);
  }
}

}  // namespace content
