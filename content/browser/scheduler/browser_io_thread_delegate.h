// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_IO_THREAD_DELEGATE_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_IO_THREAD_DELEGATE_H_

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;

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

  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override;
  void BindToCurrentThread() override;

  bool allow_blocking_for_testing() const {
    return allow_blocking_for_testing_;
  }

  // Call this before handing this over to a base::Thread to allow blocking in
  // tests.
  void SetAllowBlockingForTesting() { allow_blocking_for_testing_ = true; }

  scoped_refptr<Handle> GetHandle() { return task_queues_->GetHandle(); }

 private:
  // Creates a sequence funneled BrowserIOThreadDelegate for use in testing.
  explicit BrowserIOThreadDelegate(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Performs the actual initialization of all the members that require a
  // SequenceManager.
  void Init();

  bool allow_blocking_for_testing_ = false;
  // Owned SequenceManager, null if instance created via CreateForTesting.
  const std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;

  const raw_ptr<base::sequence_manager::SequenceManager> sequence_manager_;

  std::unique_ptr<BrowserTaskQueues> task_queues_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_IO_THREAD_DELEGATE_H_
