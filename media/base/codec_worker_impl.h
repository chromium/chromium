// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CODEC_WORKER_IMPL_H_
#define MEDIA_BASE_CODEC_WORKER_IMPL_H_

#include <cstring>

#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"

// This file contains guts of thread wrappers for libvpx and libaom to help
// avoid code duplication.
namespace media {

// Template class for returning codec worker implementations
template <class WorkerInterface,
          class WorkerImpl,
          class Worker,
          class WorkerStatus,
          WorkerStatus StatusNotOk,
          WorkerStatus StatusOk,
          WorkerStatus StatusWork>
class CodecWorkerImpl {
 public:
  // Returns the codec worker interface.
  static constexpr WorkerInterface GetCodecWorkerInterface() {
    return WorkerInterface{.init = Init,
                           .reset = Reset,
                           .sync = Sync,
                           .launch = Launch,
                           .execute = Execute,
                           .end = End};
  }

 private:
  CodecWorkerImpl()
      : thread_("CodecWorker"),
        event_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {
    thread_.Start();
  }

  static CodecWorkerImpl* GetImpl(Worker* const worker) {
    return reinterpret_cast<CodecWorkerImpl*>(worker->impl_);
  }

  static void Init(Worker* const worker) {
    memset(worker, 0, sizeof(*worker));
    worker->status_ = StatusNotOk;
    worker->impl_ = nullptr;
  }

  static void Execute(Worker* const worker) {
    if (worker->hook) {
      worker->had_error |= !worker->hook(worker->data1, worker->data2);
    }
  }

  void ChangeStateImpl(Worker* const worker, WorkerStatus new_status) {
    // Await work complete if necessary before setting the new state.
    bool deplete_work = false;
    {
      base::AutoLock lock(mutex_);
      deplete_work = worker->status_ == StatusWork;
      if (!deplete_work) {
        worker->status_ = new_status;
      }
    }
    if (deplete_work) {
      thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                         // Unretained is safe because End waits until
                         // work is complete (see `deplete_work` above).
                         base::Unretained(&event_)));
      // Sequences calling into libvpx & libaom with a threading configuration
      // need to be annotated with base::MayBlock. This is because the threading
      // interface requires Launch/Sync/End to deplete scheduled work before
      // completion.
      base::ScopedAllowBaseSyncPrimitives allow_wait;
      event_.Wait();
      base::AutoLock lock(mutex_);
      worker->status_ = new_status;
    }
    // Schedule new work.
    if (new_status == StatusWork) {
      thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&CodecWorkerImpl::ExecuteOnTaskRunner,
                         // Unretained is safe because End waits until
                         // work is complete (see `deplete_work` above).
                         base::Unretained(this), base::Unretained(worker)));
    }
  }

  void ExecuteOnTaskRunner(Worker* worker) {
    base::AutoLock lock(mutex_);
    CHECK_EQ(worker->status_, StatusWork);
    Execute(worker);
    worker->status_ = StatusOk;
  }

  static void ChangeState(Worker* const worker, WorkerStatus new_status) {
    // No-op when attempting to change state on a thread that didn't come up
    // with VpxWorkerReset.
    if (!worker->impl_) {
      return;
    }
    CodecWorkerImpl* impl = GetImpl(worker);
    impl->ChangeStateImpl(worker, new_status);
  }

  static int Sync(Worker* const worker) {
    ChangeState(worker, StatusOk);
    return !worker->had_error;
  }

  static int Reset(Worker* const worker) {
    if (worker->status_ < StatusOk) {
      worker->had_error = false;
      worker->impl_ = reinterpret_cast<WorkerImpl*>(new CodecWorkerImpl());
      worker->status_ = StatusOk;
      return 1;
    }
    int ok = Sync(worker);
    worker->had_error = false;
    CHECK(!ok || worker->status_ == StatusOk);
    return ok;
  }

  static void Launch(Worker* const worker) { ChangeState(worker, StatusWork); }

  static void End(Worker* const worker) {
    if (worker->impl_) {
      ChangeState(worker, StatusNotOk);
      CodecWorkerImpl* impl = GetImpl(worker);
      base::ScopedAllowBaseSyncPrimitives allow_wait;
      delete impl;
      worker->impl_ = nullptr;
    }
  }

  // Protects `Worker::status_`. The other attributes in `Worker` are expected
  // to be updated in isolation.
  base::Lock mutex_;
  // TODO(crbug.com/41486982): consider using sequenced task runner instead of
  // dedicated threads.
  base::Thread thread_;
  // Avoids creating a WaitableEvent on stack when work needs to be depleted.
  // Sequences calling into libvpx & libaom with a threading configuration need
  // to be annotated with base::MayBlock. This is because the threading
  // interface requires Launch/Sync/End to deplete scheduled work before
  // completion.
  base::WaitableEvent event_;
};

}  // namespace media

#endif  // MEDIA_BASE_CODEC_WORKER_IMPL_H_
