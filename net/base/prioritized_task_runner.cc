// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/prioritized_task_runner.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"

namespace net {

PrioritizedTaskRunner::Job::Job(const base::Location& from_here,
                                base::OnceClosure task,
                                base::OnceClosure reply,
                                uint32_t priority,
                                uint32_t task_count)
    : from_here(from_here),
      task(std::move(task)),
      reply(std::move(reply)),
      priority(priority),
      task_count(task_count) {}

PrioritizedTaskRunner::Job::Job() = default;

PrioritizedTaskRunner::Job::~Job() = default;
PrioritizedTaskRunner::Job::Job(Job&& other) = default;
PrioritizedTaskRunner::Job& PrioritizedTaskRunner::Job::operator=(Job&& other) =
    default;

PrioritizedTaskRunner::PrioritizedTaskRunner(
    const base::TaskTraits& task_traits)
    : task_traits_(task_traits) {}

void PrioritizedTaskRunner::PostTaskAndReply(const base::Location& from_here,
                                             base::OnceClosure task,
                                             base::OnceClosure reply,
                                             uint32_t priority) {
  Job job(from_here, std::move(task), std::move(reply), priority,
          task_count_++);
  task_jobs_.Push(std::move(job));

  scoped_refptr<base::TaskRunner> task_runner;
  if (task_runner_for_testing_) {
    task_runner = task_runner_for_testing_;
  } else {
    task_runner = base::ThreadPool::CreateSequencedTaskRunner(task_traits_);
  }

  task_runner->PostTaskAndReply(
      from_here,
      base::BindOnce(&PrioritizedTaskRunner::RunTaskAndPostReply, this),
      base::BindOnce(&PrioritizedTaskRunner::RunReply, this));
}

PrioritizedTaskRunner::~PrioritizedTaskRunner() = default;

void PrioritizedTaskRunner::RunTaskAndPostReply() {
  // Find the next job to run.
  Job job = task_jobs_.Pop();

  std::move(job.task).Run();

  // Add the job to the reply priority queue.
  reply_jobs_.Push(std::move(job));
}

void PrioritizedTaskRunner::RunReply() {
  // Find the next job to run.
  Job job = reply_jobs_.Pop();

  // Run the job.
  std::move(job.reply).Run();
}

struct PrioritizedTaskRunner::JobComparer {
  bool operator()(const Job& left, const Job& right) {
    if (left.priority == right.priority) {
      return left.task_count > right.task_count;
    }
    return left.priority > right.priority;
  }
};

PrioritizedTaskRunner::JobPriorityQueue::JobPriorityQueue() = default;
PrioritizedTaskRunner::JobPriorityQueue::~JobPriorityQueue() = default;

void PrioritizedTaskRunner::JobPriorityQueue::Push(Job job) {
  base::AutoLock auto_lock(lock_);
  heap_.push_back(std::move(job));
  std::push_heap(heap_.begin(), heap_.end(), JobComparer());
}

PrioritizedTaskRunner::Job PrioritizedTaskRunner::JobPriorityQueue::Pop() {
  base::AutoLock auto_lock(lock_);
  CHECK(!heap_.empty());
  std::pop_heap(heap_.begin(), heap_.end(), JobComparer());
  Job job = std::move(heap_.back());
  heap_.pop_back();
  return job;
}

}  // namespace net
