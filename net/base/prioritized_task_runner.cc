// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/prioritized_task_runner.h"

#include <algorithm>

#include "base/bind.h"
#include "base/task/task_runner.h"
#include "base/task/task_runner_util.h"

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

PrioritizedTaskRunner::Job::Job() {}

PrioritizedTaskRunner::Job::~Job() = default;
PrioritizedTaskRunner::Job::Job(Job&& other) = default;
PrioritizedTaskRunner::Job& PrioritizedTaskRunner::Job::operator=(Job&& other) =
    default;

PrioritizedTaskRunner::PrioritizedTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

void PrioritizedTaskRunner::PostTaskAndReply(const base::Location& from_here,
                                             base::OnceClosure task,
                                             base::OnceClosure reply,
                                             uint32_t priority) {
  Job job(from_here, std::move(task), std::move(reply), priority,
          task_count_++);
  {
    base::AutoLock lock(task_job_heap_lock_);
    task_job_heap_.push_back(std::move(job));
    std::push_heap(task_job_heap_.begin(), task_job_heap_.end(), JobComparer());
  }

  task_runner_->PostTaskAndReply(
      from_here,
      base::BindOnce(&PrioritizedTaskRunner::RunTaskAndPostReply, this),
      base::BindOnce(&PrioritizedTaskRunner::RunReply, this));
}

PrioritizedTaskRunner::~PrioritizedTaskRunner() = default;

void PrioritizedTaskRunner::RunTaskAndPostReply() {
  // Find the next job to run.
  Job job;
  {
    base::AutoLock lock(task_job_heap_lock_);
    std::pop_heap(task_job_heap_.begin(), task_job_heap_.end(), JobComparer());
    job = std::move(task_job_heap_.back());
    task_job_heap_.pop_back();
  }

  std::move(job.task).Run();

  // Add the job to the reply priority queue.
  base::AutoLock reply_lock(reply_job_heap_lock_);
  reply_job_heap_.push_back(std::move(job));
  std::push_heap(reply_job_heap_.begin(), reply_job_heap_.end(), JobComparer());
}

void PrioritizedTaskRunner::RunReply() {
  // Find the next job to run.
  Job job;
  {
    base::AutoLock lock(reply_job_heap_lock_);
    std::pop_heap(reply_job_heap_.begin(), reply_job_heap_.end(),
                  JobComparer());
    job = std::move(reply_job_heap_.back());
    reply_job_heap_.pop_back();
  }

  // Run the job.
  std::move(job.reply).Run();
}

}  // namespace net
