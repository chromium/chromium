// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PRIORITIZED_DISPATCHER_H_
#define NET_BASE_PRIORITIZED_DISPATCHER_H_

#include <stddef.h>

#include <vector>

#include "net/base/net_export.h"
#include "net/base/priority_queue.h"

namespace net {

// A priority-based dispatcher of jobs. Dispatch order is by priority (highest
// first) and then FIFO. The dispatcher enforces limits on the number of running
// jobs. It never revokes a job once started. The job must call OnJobFinished
// once it finishes in order to dispatch further jobs.
//
// This class is NOT thread-safe which is enforced by the underlying
// non-thread-safe PriorityQueue. All operations are O(p) time for p priority
// levels. It is safe to execute any method, including destructor, from within
// Job::Start.
//
class NET_EXPORT_PRIVATE PrioritizedDispatcher {
 public:
  class Job;
  typedef PriorityQueue<Job*>::Priority Priority;

  // Describes the limits for the number of jobs started by the dispatcher.
  // For example, |total_jobs| = 30 and |reserved_slots| = { 0, 5, 10, 5 } allow
  // for at most 30 running jobs in total. Jobs at priority 0 can't use slots
  // reserved for higher priorities, so they are limited to 10.
  // If there are already 24 jobs running, then only 6 more jobs can start. No
  // jobs at priority 1 or below can start. After one more job starts, no jobs
  // at priority 2 or below can start, since the remaining 5 slots are reserved
  // for priority 3 or above.
  struct NET_EXPORT_PRIVATE Limits {
    Limits(Priority num_priorities, size_t total_jobs);
    Limits(const Limits& other);
    ~Limits();

    // Total allowed running jobs.
    size_t total_jobs;
    // Number of slots reserved for each priority and higher.
    // Sum of |reserved_slots| must be no greater than |total_jobs|.
    std::vector<size_t> reserved_slots;
  };

  // An interface to the job dispatched by PrioritizedDispatcher. The dispatcher
  // does not own the Job but expects it to live as long as the Job is queued.
  // Use Cancel to remove Job from queue before it is dispatched. The Job can be
  // deleted after it is dispatched or canceled, or the dispatcher is destroyed.
  class Job {
   public:
    // Note: PrioritizedDispatcher will never delete a Job.
    virtual ~Job() = default;
    // Called when the dispatcher starts the job. Once the job finishes, it must
    // call OnJobFinished.
    virtual void Start() = 0;
  };

  // A handle to the enqueued job. The handle becomes invalid when the job is
  // canceled, updated, or started.
  typedef PriorityQueue<Job*>::Pointer Handle;

  // Creates a dispatcher enforcing |limits| on number of running jobs.
  explicit PrioritizedDispatcher(const Limits& limits);

  PrioritizedDispatcher(const PrioritizedDispatcher&) = delete;
  PrioritizedDispatcher& operator=(const PrioritizedDispatcher&) = delete;
  ~PrioritizedDispatcher();

  size_t num_running_jobs() const { return num_running_jobs_; }
  size_t num_queued_jobs() const { return queue_.size(); }
  size_t num_priorities() const { return max_running_jobs_.size(); }

  // Adds |job| with |priority| to the dispatcher. If limits permit, |job| is
  // started immediately. Returns handle to the job or null-handle if the job is
  // started. The dispatcher does not own |job|, but |job| must live as long as
  // it is queued in the dispatcher.
  Handle Add(Job* job, Priority priority);

  // Just like Add, except that it adds Job at the font of queue of jobs with
  // priorities of |priority|.
  Handle AddAtHead(Job* job, Priority priority);

  // Removes the job with |handle| from the queue. Invalidates |handle|.
  // Note: a Handle is valid iff the job is in the queue, i.e. has not Started.
  void Cancel(const Handle& handle);

  // Cancels and returns the oldest-lowest-priority Job invalidating any
  // handles to it. Returns NULL if the queue is empty.
  Job* EvictOldestLowest();

  // Moves the queued job with |handle| to the end of all values with priority
  // |priority| and returns the updated handle, or null-handle if it starts the
  // job. Invalidates |handle|. No-op if priority did not change.
  Handle ChangePriority(const Handle& handle, Priority priority);

  // Notifies the dispatcher that a running job has finished. Could start a job.
  void OnJobFinished();

  // Retrieves the Limits that |this| is currently using.  This may not exactly
  // match the Limits this was created with.  In particular, the number of slots
  // reserved for the lowest priority will always be 0, even if it was non-zero
  // in the Limits passed to the constructor or to SetLimits.
  Limits GetLimits() const;

  // Updates |max_running_jobs_| to match |limits|.  Starts jobs if new limit
  // allows.  Does not stop jobs if the new limits are lower than the old ones.
  void SetLimits(const Limits& limits);

  // Set the limits to zero for all priorities, allowing no new jobs to start.
  void SetLimitsToZero();

 private:
  // Attempts to dispatch the job with |handle| at priority |priority| (might be
  // different than |handle.priority()|. Returns true if successful. If so
  // the |handle| becomes invalid.
  bool MaybeDispatchJob(const Handle& handle, Priority priority);

  // Attempts to dispatch the next highest priority job in the queue. Returns
  // true if successful, and all handles to that job become invalid.
  bool MaybeDispatchNextJob();

  // Queue for jobs that need to wait for a spare slot.
  PriorityQueue<Job*> queue_;
  // Maximum total number of running jobs allowed after a job at a particular
  // priority is started. If a greater or equal number of jobs are running, then
  // another job cannot be started.
  std::vector<size_t> max_running_jobs_;
  // Total number of running jobs.
  size_t num_running_jobs_ = 0;
};

}  // namespace net

#endif  // NET_BASE_PRIORITIZED_DISPATCHER_H_
