// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PRIORITIZED_TASK_RUNNER_H_
#define NET_BASE_PRIORITIZED_TASK_RUNNER_H_

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task_and_reply_with_result_internal.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_export.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace net {

namespace internal {
template <typename ReturnType>
void ReturnAsParamAdapter(base::OnceCallback<ReturnType()> func,
                          ReturnType* result) {
  *result = std::move(func).Run();
}

// Adapts a T* result to a callblack that expects a T.
template <typename TaskReturnType, typename ReplyArgType>
void ReplyAdapter(base::OnceCallback<void(ReplyArgType)> callback,
                  TaskReturnType* result) {
  std::move(callback).Run(std::move(*result));
}
}  // namespace internal

// PrioritizedTaskRunner allows for prioritization of posted tasks and their
// replies. It provides up to 2^32 priority levels. All tasks posted via the
// PrioritizedTaskRunner will run in priority order. All replies from
// PostTaskAndReply will also run in priority order. Be careful, as it is
// possible to starve a task.
class NET_EXPORT_PRIVATE PrioritizedTaskRunner
    : public base::RefCountedThreadSafe<PrioritizedTaskRunner> {
 public:
  enum class ReplyRunnerType { kStandard, kPrioritized };
  explicit PrioritizedTaskRunner(const base::TaskTraits& task_traits);
  PrioritizedTaskRunner(const PrioritizedTaskRunner&) = delete;
  PrioritizedTaskRunner& operator=(const PrioritizedTaskRunner&) = delete;

  // Similar to TaskRunner::PostTaskAndReply, except that the task runs at
  // |priority|. Priority 0 is the highest priority and will run before other
  // priority values. Multiple tasks with the same |priority| value are run in
  // order of posting. The replies are also run in prioritized order on the
  // calling taskrunner.
  void PostTaskAndReply(const base::Location& from_here,
                        base::OnceClosure task,
                        base::OnceClosure reply,
                        uint32_t priority);

  // Similar to TaskRunner::PostTaskAndReplyWithResult, except that the task
  // runs at |priority|. See PostTaskAndReply for a description of |priority|.
  template <typename TaskReturnType, typename ReplyArgType>
  void PostTaskAndReplyWithResult(const base::Location& from_here,
                                  base::OnceCallback<TaskReturnType()> task,
                                  base::OnceCallback<void(ReplyArgType)> reply,
                                  uint32_t priority) {
    TaskReturnType* result = new TaskReturnType();
    return PostTaskAndReply(
        from_here,
        BindOnce(&internal::ReturnAsParamAdapter<TaskReturnType>,
                 std::move(task), result),
        BindOnce(&internal::ReplyAdapter<TaskReturnType, ReplyArgType>,
                 std::move(reply), base::Owned(result)),
        priority);
  }

  void SetTaskRunnerForTesting(scoped_refptr<base::TaskRunner> task_runner) {
    task_runner_for_testing_ = std::move(task_runner);
  }

 private:
  friend class base::RefCountedThreadSafe<PrioritizedTaskRunner>;

  struct Job {
    Job(const base::Location& from_here,
        base::OnceClosure task,
        base::OnceClosure reply,
        uint32_t priority,
        uint32_t task_count);
    Job();
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    ~Job();

    Job(Job&& other);
    Job& operator=(Job&& other);

    base::Location from_here;
    base::OnceClosure task;
    base::OnceClosure reply;
    uint32_t priority = 0;
    uint32_t task_count = 0;
  };

  struct JobComparer {
    bool operator()(const Job& left, const Job& right) {
      if (left.priority == right.priority)
        return left.task_count > right.task_count;
      return left.priority > right.priority;
    }
  };

  void RunTaskAndPostReply();
  void RunReply();

  ~PrioritizedTaskRunner();

  // TODO(jkarlin): Replace the heaps with std::priority_queue once it
  // supports move-only types.
  // Accessed on both task_runner_ and the reply task runner.
  std::vector<Job> task_job_heap_;
  base::Lock task_job_heap_lock_;
  std::vector<Job> reply_job_heap_;
  base::Lock reply_job_heap_lock_;

  const base::TaskTraits task_traits_;
  scoped_refptr<base::TaskRunner> task_runner_for_testing_;

  // Used to preserve order of jobs of equal priority. This can overflow and
  // cause periodic priority inversion. This should be infrequent enough to be
  // of negligible impact.
  uint32_t task_count_ = 0;
};

}  // namespace net

#endif  // NET_BASE_PRIORITIZED_TASK_RUNNER_H_
