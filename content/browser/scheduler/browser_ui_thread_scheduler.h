// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}  // namespace sequence_manager
}  // namespace base

namespace content {
class BrowserTaskExecutor;

// The BrowserUIThreadScheduler vends TaskQueues and manipulates them to
// implement scheduling policy. This class is never deleted in production.
class CONTENT_EXPORT BrowserUIThreadScheduler {
 public:
  using Handle = BrowserTaskQueues::Handle;

  BrowserUIThreadScheduler();

  BrowserUIThreadScheduler(const BrowserUIThreadScheduler&) = delete;
  BrowserUIThreadScheduler& operator=(const BrowserUIThreadScheduler&) = delete;

  ~BrowserUIThreadScheduler();

  static BrowserUIThreadScheduler* Get();

  // Setting the DefaultTaskRunner is up to the caller.
  static std::unique_ptr<BrowserUIThreadScheduler> CreateForTesting(
      base::sequence_manager::SequenceManager* sequence_manager);

  using QueueType = BrowserTaskQueues::QueueType;

  scoped_refptr<Handle> GetHandle() const { return handle_; }

 private:
  friend class BrowserTaskExecutor;

  using QueueEnabledVoter =
      base::sequence_manager::TaskQueue::QueueEnabledVoter;

  explicit BrowserUIThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);

  void CommonSequenceManagerSetup(
      base::sequence_manager::SequenceManager* sequence_manager);

  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::LazyNow* lazy_now);

  // In production the BrowserUIThreadScheduler will own its SequenceManager,
  // but in tests it may not.
  std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;

  BrowserTaskQueues task_queues_;

  scoped_refptr<Handle> handle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
