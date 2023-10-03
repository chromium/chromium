// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/blink_scheduler_single_thread_task_runner.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

namespace blink::scheduler {

namespace {

void DeleteOrReleaseSoonImpl(
    const base::Location& from_here,
    void (*deleter)(const void*),
    const void* object,
    scoped_refptr<base::SingleThreadTaskRunner> preferred_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> fallback_task_runner);

class DeleteHelper {
 public:
  DeleteHelper(
      const base::Location& from_here,
      void (*deleter)(const void*),
      const void* object,
      scoped_refptr<base::SingleThreadTaskRunner> preferred_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> fallback_task_runner)
      : from_here_(from_here),
        deleter_(deleter),
        object_(object),
        preferred_task_runner_(std::move(preferred_task_runner)),
        fallback_task_runner_(std::move(fallback_task_runner)) {}

  void Delete() {
    deleter_(object_);
    object_ = nullptr;
  }

  ~DeleteHelper() {
    if (!object_) {
      return;
    }

    // The deleter task is being destroyed without running, which happens if the
    // task queue is shut down after queuing the task queued or if posting it
    // failed. It's safe to run the deleter in the former case, but since these
    // cases can't be differentiated without synchronization or API changes, use
    // the `fallback_task_runner_` if present and delete synchronously if not.
    if (fallback_task_runner_) {
      DeleteOrReleaseSoonImpl(from_here_, deleter_, object_,
                              fallback_task_runner_, nullptr);
    } else if (preferred_task_runner_->BelongsToCurrentThread()) {
      // Note: `deleter_` will run synchronously in [Delete|Release]Soon() if
      // the deleter task failed to post to the original preferred and fallback
      // task runners. This happens when the APIs are called during thread
      // shutdown, and should only occur if invoking those APIs in object
      // destructors (on task destruction), where it should be safe to
      // synchronously delete.
      Delete();
    } else {
      // The deleter task couldn't be posted to the intended thread, so the only
      // safe thing to do is leak the object.
      // TODO(crbug.com/1376851): Add a CHECK, DumpWithoutCrashing, or trace
      // event to determine if leaks still occur.
    }
  }

 private:
  base::Location from_here_;
  void (*deleter_)(const void*) = nullptr;
  raw_ptr<const void, DanglingUntriaged> object_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> preferred_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> fallback_task_runner_;
};

void DeleteOrReleaseSoonImpl(
    const base::Location& from_here,
    void (*deleter)(const void*),
    const void* object,
    scoped_refptr<base::SingleThreadTaskRunner> preferred_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> fallback_task_runner) {
  auto delete_helper = std::make_unique<DeleteHelper>(
      from_here, deleter, object, preferred_task_runner, fallback_task_runner);
  preferred_task_runner->PostNonNestableTask(
      from_here,
      base::BindOnce(&DeleteHelper::Delete, std::move(delete_helper)));
}

}  // namespace

BlinkSchedulerSingleThreadTaskRunner::BlinkSchedulerSingleThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> wrapped_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner)
    : outer_(std::move(wrapped_task_runner)),
      thread_task_runner_(std::move(thread_task_runner)) {
  DCHECK(outer_);
}

BlinkSchedulerSingleThreadTaskRunner::~BlinkSchedulerSingleThreadTaskRunner() =
    default;

bool BlinkSchedulerSingleThreadTaskRunner::DeleteOrReleaseSoonInternal(
    const base::Location& from_here,
    void (*deleter)(const void*),
    const void* object) {
  DCHECK(deleter);
  // `object` might be null, in which case there's nothing to do.
  if (!object) {
    return true;
  }

  DeleteOrReleaseSoonImpl(from_here, deleter, object, outer_,
                          thread_task_runner_);
  return true;
}

}  // namespace blink::scheduler
