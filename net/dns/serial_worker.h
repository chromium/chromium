// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_SERIAL_WORKER_H_
#define NET_DNS_SERIAL_WORKER_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "net/base/net_export.h"

namespace net {

// `SerialWorker` executes a job on `ThreadPool` serially -- **one at a time**.
// On `WorkNow()`, a `WorkItem` is created using `CreateWorkItem()` and sent to
// the `ThreadPool`. There, a call to `DoWork()` is made. On completion of work,
// `OnWorkFinished()` is called on the origin thread (if the `SerialWorker` is
// still alive), passing back the `WorkItem` to allow retrieving any results or
// passed objects. If `WorkNow()` is called (1 or more times) while a `WorkItem`
// is already under way, after completion of the work and before any call is
// made to `OnWorkFinished()` the same `WorkItem` will be passed back to the
// `ThreadPool`, and `DoWork()` will be called once more.
//
// This behavior is designed for updating a result after some trigger, for
// example reading a file once FilePathWatcher indicates it changed.
//
// Derived classes should store results of work in the `WorkItem` and retrieve
// results from it when passed back to `OnWorkFinished()`. The `SerialWorker` is
// guaranteed to only run one `WorkItem` at a time, always passing it back to
// `OnWorkFinished()` before calling `CreateWorkItem()` again. Therefore, a
// derived class may safely pass objects between `WorkItem`s, or even reuse the
// same `WorkItem`, to allow storing helper objects directly in the `WorkItem`.
// However, it is not guaranteed that the `SerialWorker` will remain alive while
// the `WorkItem` runs. Therefore, the `WorkItem` should never access any memory
// owned by the `SerialWorker` or derived class.
class NET_EXPORT_PRIVATE SerialWorker {
 public:
  // A work item that will be passed to and run on the `ThreadPool` (potentially
  // multiple times if the `SerialWorker` needs to run again immediately) and
  // then passed back to the origin thread on completion. Expected usage is to
  // store any parameters, results, and helper objects in the `WorkItem` and
  // read results from it when passed back to the origin thread.
  //
  // `SerialWorker` calls `FollowupWork()` *on the origin thread* after calling
  // `DoWork()` on the `ThreadPool` to asynchronously handle any work that must
  // be part of the serialization but that cannot run on a worker thread.
  class NET_EXPORT_PRIVATE WorkItem {
   public:
    virtual ~WorkItem() = default;
    virtual void DoWork() = 0;
    virtual void FollowupWork(base::OnceClosure closure);
  };

  SerialWorker();

  SerialWorker(const SerialWorker&) = delete;
  SerialWorker& operator=(const SerialWorker&) = delete;

  // Unless already scheduled, post |DoWork| to ThreadPool.
  // Made virtual to allow mocking.
  virtual void WorkNow();

  // Stop scheduling jobs.
  void Cancel();

  bool IsCancelled() const { return state_ == State::kCancelled; }

 protected:
  // protected to allow sub-classing, but prevent deleting
  virtual ~SerialWorker();

  // Create a new WorkItem to be passed to and run on the ThreadPool.
  virtual std::unique_ptr<WorkItem> CreateWorkItem() = 0;

  // Executed on origin thread after `WorkItem` completes.
  virtual void OnWorkFinished(std::unique_ptr<WorkItem> work_item) = 0;

  base::WeakPtr<SerialWorker> AsWeakPtr();

  // Used to verify that the constructor, WorkNow(), Cancel() and
  // OnWorkJobFinished() are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  enum class State {
    kCancelled = -1,
    kIdle = 0,
    kWorking,  // |DoWorkJob| posted to ThreadPool, until |OnWorkJobFinished|
    kPending,  // |WorkNow| while WORKING, must re-do work
  };

  // Called on the origin thread after `WorkItem::DoWork()` completes.
  void OnDoWorkFinished(std::unique_ptr<WorkItem> work_item);

  // Called on the origin thread after `WorkItem::FollowupWork()` completes.
  void OnFollowupWorkFinished(std::unique_ptr<WorkItem> work_item);

  void RerunWork(std::unique_ptr<WorkItem> work_item);

  State state_;

  base::WeakPtrFactory<SerialWorker> weak_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_SERIAL_WORKER_H_
