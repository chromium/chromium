// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_IO_THREAD_DELEGATE_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_IO_THREAD_DELEGATE_H_

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;
class TaskExecutor;

namespace sequence_manager {
class SequenceManager;
}  // namespace sequence_manager

}  // namespace base

namespace content {

// Delegate for the IO thread.
class CONTENT_EXPORT BrowserIOThreadDelegate : public base::Thread::Delegate {
 public:
  using Handle = BrowserTaskQueues::Handle;

  // Creates a BrowserIOThreadDelegate for use with a real IO thread.
  BrowserIOThreadDelegate();
  ~BrowserIOThreadDelegate() override;

  static std::unique_ptr<BrowserIOThreadDelegate> CreateForTesting(
      base::sequence_manager::SequenceManager* sequence_manager) {
    DCHECK(sequence_manager);
    return base::WrapUnique(new BrowserIOThreadDelegate(sequence_manager));
  }

  // If called this must be done prior to calling BindToCurrentThread.
  void SetTaskExecutor(base::TaskExecutor* task_executor);

  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override;
  void BindToCurrentThread(base::TimerSlack timer_slack) override;

  bool allow_blocking_for_testing() const {
    return allow_blocking_for_testing_;
  }

  // Call this before handing this over to a base::Thread to allow blocking in
  // tests.
  void SetAllowBlockingForTesting() { allow_blocking_for_testing_ = true; }

  scoped_refptr<Handle> GetHandle() { return task_queues_->GetHandle(); }

  // Must be called on the IO thread.
  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunnerForCurrentTask()
      const;

 private:
  class TLSMultiplexer;

  // Creates a sequence funneled BrowserIOThreadDelegate for use in testing.
  // Installs TLSMultiplexer which allows ensures the right results for
  // base::CurrentThread when running an "IO Thread" task.
  explicit BrowserIOThreadDelegate(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Performs the actual initialization of all the members that require a
  // SequenceManager.
  void Init();

  bool allow_blocking_for_testing_ = false;
  // Owned SequenceManager, null if instance created via CreateForTesting.
  const std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;

  base::sequence_manager::SequenceManager* const sequence_manager_;

  std::unique_ptr<BrowserTaskQueues> task_queues_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  // In unit tests the IO "thread" can be sequence funneled onto the main thread
  // so we need to multiplex the TLS binding to ensure base::CurrentThread
  // behaves as expected.
  std::unique_ptr<TLSMultiplexer> tls_multiplexer_;

  base::TaskExecutor* task_executor_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_IO_THREAD_DELEGATE_H_
