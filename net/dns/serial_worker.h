// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_SERIAL_WORKER_H_
#define NET_DNS_SERIAL_WORKER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_export.h"

namespace net {

// SerialWorker executes a job on ThreadPool serially -- **once at a time**.
// On |WorkNow|, a call to |DoWork| is scheduled on ThreadPool. Once it
// completes, |OnWorkFinished| is called on the origin thread. If |WorkNow| is
// called (1 or more times) while |DoWork| is already under way, |DoWork| will
// be called once: after current |DoWork| completes, before a call to
// |OnWorkFinished|.
//
// If |OnWorkFinished| returns a failure and |max_number_of_retries|
// is non-zero, retries will be scheduled according to the |backoff_policy|.
// A default backoff policy is used if one is not provided.
//
// This behavior is designed for updating a result after some trigger, for
// example reading a file once FilePathWatcher indicates it changed.
//
// Derived classes should store results of work done in |DoWork| in dedicated
// fields and read them in |OnWorkFinished| which is executed on the origin
// thread. This avoids the need to template this class.
//
// This implementation avoids locking by using the |state_| member to ensure
// that |DoWork| and |OnWorkFinished| cannot execute in parallel.
class NET_EXPORT_PRIVATE SerialWorker
    : public base::RefCountedDeleteOnSequence<SerialWorker> {
 public:
  explicit SerialWorker(
      int max_number_of_retries = 0,
      const net::BackoffEntry::Policy* backoff_policy = nullptr);

  // Unless already scheduled, post |DoWork| to ThreadPool.
  // Made virtual to allow mocking.
  virtual void WorkNow();

  // Stop scheduling jobs.
  void Cancel();

  bool IsCancelled() const { return state_ == CANCELLED; }

  // Allows tests to inspect the current backoff/retry state.
  const BackoffEntry& GetBackoffEntryForTesting() const;
  const base::OneShotTimer& GetRetryTimerForTesting() const;

 protected:
  friend class base::DeleteHelper<SerialWorker>;
  friend class base::RefCountedDeleteOnSequence<SerialWorker>;
  // protected to allow sub-classing, but prevent deleting
  virtual ~SerialWorker();

  // Executed on ThreadPool, at most once at a time.
  virtual void DoWork() = 0;

  // Executed on origin thread after |DoRead| completes.
  // Must return true on success.
  virtual bool OnWorkFinished() = 0;

  // Used to verify that the constructor, WorkNow(), Cancel() and
  // OnWorkJobFinished() are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  enum State {
    CANCELLED = -1,
    IDLE = 0,
    WORKING,  // |DoWorkJob| posted to ThreadPool, until |OnWorkJobFinished|
    PENDING,  // |WorkNow| while WORKING, must re-do work
  };

  void WorkNowInternal();

  // Called on the the origin thread after |DoWork| completes.
  void OnWorkJobFinished();

  State state_;

  // Max retries and backoff entry to control timing.
  const int max_number_of_retries_;
  BackoffEntry backoff_entry_;
  base::OneShotTimer retry_timer_;

  base::WeakPtrFactory<SerialWorker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SerialWorker);
};

}  // namespace net

#endif  // NET_DNS_SERIAL_WORKER_H_
