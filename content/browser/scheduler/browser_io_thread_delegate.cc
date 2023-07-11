// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_io_thread_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/public/browser/browser_thread.h"

namespace content {

using ::base::sequence_manager::CreateUnboundSequenceManager;
using ::base::sequence_manager::SequenceManager;
using ::base::sequence_manager::TaskQueue;

BrowserIOThreadDelegate::BrowserIOThreadDelegate()
    : owned_sequence_manager_(CreateUnboundSequenceManager(
          SequenceManager::Settings::Builder()
              .SetMessagePumpType(base::MessagePumpType::IO)
              .SetPrioritySettings(
                  internal::CreateBrowserTaskPrioritySettings())
              .Build())),
      sequence_manager_(owned_sequence_manager_.get()) {
  Init();
}

BrowserIOThreadDelegate::BrowserIOThreadDelegate(
    SequenceManager* sequence_manager)
    : sequence_manager_(sequence_manager) {
  Init();
}

void BrowserIOThreadDelegate::Init() {
  task_queues_ =
      std::make_unique<BrowserTaskQueues>(BrowserThread::IO, sequence_manager_);
  default_task_runner_ = task_queues_->GetHandle()->GetDefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserIOThreadDelegate::GetDefaultTaskRunner() {
  return default_task_runner_;
}

BrowserIOThreadDelegate::~BrowserIOThreadDelegate() = default;

void BrowserIOThreadDelegate::BindToCurrentThread() {
  DCHECK(sequence_manager_);
  sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(base::MessagePumpType::IO));
  sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
  sequence_manager_->EnableCrashKeys("io_scheduler_async_stack");
}

}  // namespace content
