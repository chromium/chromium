// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/prioritized_dispatcher.h"

#include <ostream>

#include "base/check_op.h"

namespace net {

PrioritizedDispatcher::Limits::Limits(Priority num_priorities,
                                      size_t total_jobs)
    : total_jobs(total_jobs), reserved_slots(num_priorities) {}

PrioritizedDispatcher::Limits::Limits(const Limits& other) = default;

PrioritizedDispatcher::Limits::~Limits() = default;

PrioritizedDispatcher::PrioritizedDispatcher(const Limits& limits)
    : queue_(limits.reserved_slots.size()),
      max_running_jobs_(limits.reserved_slots.size()) {
  SetLimits(limits);
}

PrioritizedDispatcher::~PrioritizedDispatcher() = default;

PrioritizedDispatcher::Handle PrioritizedDispatcher::Add(
    Job* job, Priority priority) {
  DCHECK(job);
  DCHECK_LT(priority, num_priorities());
  if (TryAcquireSlot(priority)) {
    job->Start();
    return Handle();
  }
  return queue_.Insert(job, priority);
}

PrioritizedDispatcher::Handle PrioritizedDispatcher::AddAtHead(
    Job* job, Priority priority) {
  DCHECK(job);
  DCHECK_LT(priority, num_priorities());
  if (TryAcquireSlot(priority)) {
    job->Start();
    return Handle();
  }
  return queue_.InsertAtFront(job, priority);
}

void PrioritizedDispatcher::Cancel(const Handle& handle) {
  queue_.Erase(handle);
}

PrioritizedDispatcher::Job* PrioritizedDispatcher::EvictOldestLowest() {
  Handle handle = queue_.FirstMin();
  if (handle.is_null())
    return nullptr;
  Job* job = handle.value();
  Cancel(handle);
  return job;
}

PrioritizedDispatcher::Handle PrioritizedDispatcher::ChangePriority(
    const Handle& handle, Priority priority) {
  DCHECK(!handle.is_null());
  DCHECK_LT(priority, num_priorities());
  DCHECK_GE(num_running_jobs_, max_running_jobs_[handle.priority()]) <<
      "Job should not be in queue when limits permit it to start.";

  if (handle.priority() == priority)
    return handle;

  if (MaybeDispatchJob(handle, priority))
    return Handle();
  Job* job = handle.value();
  queue_.Erase(handle);
  return queue_.Insert(job, priority);
}

void PrioritizedDispatcher::OnJobFinished() {
  DCHECK_GT(num_running_jobs_, 0u);
  --num_running_jobs_;
  const size_t soft_cap = max_running_jobs_.back();
  if (expandability_ == SocketPoolExpandability::kCapped &&
      additional_capacity_.capacity() > 0 && soft_cap > 0 &&
      num_running_jobs_ <= soft_cap + additional_capacity_.capacity()) {
    expandability_ = additional_capacity_.NextExpandabilityAfterRelease(
        expandability_, num_running_jobs_, soft_cap);
  }
  // Becoming `kUncapped` may allow more than one queued job to start.
  while (MaybeDispatchNextJob()) {
  }
}

PrioritizedDispatcher::Limits PrioritizedDispatcher::GetLimits() const {
  size_t num_priorities = max_running_jobs_.size();
  Limits limits(num_priorities, max_running_jobs_.back());
  limits.additional_capacity = additional_capacity_;

  // Calculate the number of jobs reserved for each priority and higher.  Leave
  // the number of jobs reserved for the lowest priority or higher as 0.
  for (size_t i = 1; i < num_priorities; ++i) {
    limits.reserved_slots[i] = max_running_jobs_[i] - max_running_jobs_[i - 1];
  }

  return limits;
}

void PrioritizedDispatcher::SetLimits(const Limits& limits) {
  DCHECK_EQ(queue_.num_priorities(), limits.reserved_slots.size());
  size_t total = 0;
  for (size_t i = 0; i < limits.reserved_slots.size(); ++i) {
    total += limits.reserved_slots[i];
    max_running_jobs_[i] = total;
  }
  // Unreserved slots are available for all priorities.
  DCHECK_LE(total, limits.total_jobs) << "sum(reserved_slots) <= total_jobs";
  size_t spare = limits.total_jobs - total;
  for (size_t i = limits.reserved_slots.size(); i > 0; --i) {
    max_running_jobs_[i - 1] += spare;
  }
  additional_capacity_ = limits.additional_capacity;
  expandability_ = SocketPoolExpandability::kUncapped;
  // Additional capacity isn't supported with reserved slots.
  CHECK(additional_capacity_.capacity() == 0 || total == 0);

  // Start pending jobs, if limits permit.
  while (true) {
    if (!MaybeDispatchNextJob())
      break;
  }
}

void PrioritizedDispatcher::SetLimitsToZero() {
  SetLimits(Limits(queue_.num_priorities(), 0));
}

bool PrioritizedDispatcher::TryAcquireSlot(Priority priority) {
  DCHECK_LT(priority, num_priorities());
  if (num_running_jobs_ >= max_running_jobs_[priority]) {
    // Above the soft cap, mirror `ClientSocketPool` by updating
    // `expandability_` before each attempted allocation.
    const size_t soft_cap = max_running_jobs_.back();
    if (expandability_ == SocketPoolExpandability::kCapped ||
        additional_capacity_.capacity() == 0 || soft_cap == 0 ||
        num_running_jobs_ > soft_cap + additional_capacity_.capacity()) {
      return false;
    }
    expandability_ = additional_capacity_.NextExpandabilityBeforeAllocation(
        expandability_, num_running_jobs_, soft_cap);
    if (expandability_ == SocketPoolExpandability::kCapped) {
      return false;
    }
  }
  ++num_running_jobs_;
  return true;
}

bool PrioritizedDispatcher::MaybeDispatchJob(const Handle& handle,
                                             Priority job_priority) {
  if (!TryAcquireSlot(job_priority)) {
    return false;
  }
  Job* job = handle.value();
  queue_.Erase(handle);
  job->Start();
  return true;
}

bool PrioritizedDispatcher::MaybeDispatchNextJob() {
  Handle handle = queue_.FirstMax();
  if (handle.is_null()) {
    DCHECK_EQ(0u, queue_.size());
    return false;
  }
  return MaybeDispatchJob(handle, handle.priority());
}

}  // namespace net
