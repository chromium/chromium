// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/task/sequence_manager/task_queue.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
class TimeDomain;
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
  ~BrowserUIThreadScheduler();

  // Setting the DefaultTaskRunner is up to the caller.
  static std::unique_ptr<BrowserUIThreadScheduler> CreateForTesting(
      base::sequence_manager::SequenceManager* sequence_manager,
      base::sequence_manager::TimeDomain* time_domain);

  using QueueType = BrowserTaskQueues::QueueType;

  scoped_refptr<Handle> GetHandle() const { return handle_; }

  // Must be called on the UI thread.
  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunnerForCurrentTask()
      const;

 private:
  friend class BrowserTaskExecutor;

  BrowserUIThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager,
      base::sequence_manager::TimeDomain* time_domain);

  void CommonSequenceManagerSetup(
      base::sequence_manager::SequenceManager* sequence_manager);

  // In production the BrowserUIThreadScheduler will own its SequenceManager,
  // but in tests it may not.
  std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;

  base::sequence_manager::SequenceManager* sequence_manager_ = nullptr;
  BrowserTaskQueues task_queues_;
  scoped_refptr<Handle> handle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserUIThreadScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
