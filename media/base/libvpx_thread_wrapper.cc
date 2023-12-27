// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/libvpx_thread_wrapper.h"

#include <cstring>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/libvpx/source/libvpx/vpx_util/vpx_thread.h"

namespace media {

// Friend and derived class of ScopedAllowBaseSyncPrimitives which allows
// VpxChangeState() and VpxWorkerEnd() to wait on work completion.
// VpxChangeState/VpxWorkerEnd() can't itself be a friend of
// ScopedAllowBaseSyncPrimitives because it is in the anonymous namespace.
class [[maybe_unused, nodiscard]] VpxChangeStateScopedAllowBaseSyncPrimitives
    : public base::ScopedAllowBaseSyncPrimitives {};

namespace {

struct Impl {
  Impl() : thread("LibvpxWorker") { thread.Start(); }

  // Protects `VPxWorker::status_`. The other attributes in `VPxWorker` are
  // expected to be updated in isolation.
  base::Lock mutex;
  // TODO(crbug.com/1514426): consider using sequenced task runner instead of
  // dedicated threads.
  base::Thread thread;
};

Impl* GetImpl(VPxWorker* const worker) {
  return reinterpret_cast<Impl*>(worker->impl_);
}

void VpxWorkerInit(VPxWorker* const worker) {
  memset(worker, 0, sizeof(*worker));
  worker->status_ = NOT_OK;
  worker->impl_ = nullptr;
}

void VpxWorkerExecute(VPxWorker* const worker) {
  if (worker->hook) {
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
  }
}

void ChangeState(VPxWorker* const worker, VPxWorkerStatus new_status) {
  // No-op when attempting to change state on a thread that didn't come up
  // with VpxWorkerReset.
  if (!worker->impl_) {
    return;
  }
  Impl* impl = GetImpl(worker);
  // Await work complete if necessary before setting the new state.
  bool deplete_work = false;
  {
    base::AutoLock lock(impl->mutex);
    deplete_work = worker->status_ == WORK;
    if (!deplete_work) {
      worker->status_ = new_status;
    }
  }
  if (deplete_work) {
    base::WaitableEvent event;
    impl->thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                       base::Unretained(&event)));
    VpxChangeStateScopedAllowBaseSyncPrimitives allow_wait;
    event.Wait();
    base::AutoLock lock(impl->mutex);
    worker->status_ = new_status;
  }
  // Schedule new work.
  if (new_status == WORK) {
    impl->thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](VPxWorker* worker) {
                         Impl* impl = GetImpl(worker);
                         base::AutoLock lock(impl->mutex);
                         CHECK_EQ(worker->status_, WORK);
                         VpxWorkerExecute(worker);
                         worker->status_ = OK;
                       },
                       // Unretained is safe because VpxWorkerEnd waits until
                       // work is complete (see `deplete_work` above).
                       base::Unretained(worker)));
  }
}

int VpxWorkerSync(VPxWorker* const worker) {
  ChangeState(worker, OK);
  return !worker->had_error;
}

int VpxWorkerReset(VPxWorker* const worker) {
  if (worker->status_ < OK) {
    worker->had_error = false;
    worker->impl_ = reinterpret_cast<VPxWorkerImpl*>(new Impl());
    worker->status_ = OK;
    return 1;
  }
  int ok = VpxWorkerSync(worker);
  worker->had_error = false;
  CHECK(!ok || worker->status_ == OK);
  return ok;
}

void VpxWorkerLaunch(VPxWorker* const worker) {
  ChangeState(worker, WORK);
}

void VpxWorkerEnd(VPxWorker* const worker) {
  if (worker->impl_) {
    ChangeState(worker, NOT_OK);
    Impl* impl = GetImpl(worker);
    VpxChangeStateScopedAllowBaseSyncPrimitives allow_wait;
    delete impl;
    worker->impl_ = nullptr;
  }
}

}  // namespace

void InitLibVpxThreadWrapper() {
  const VPxWorkerInterface interface = {
      .init = VpxWorkerInit,
      .reset = VpxWorkerReset,
      .sync = VpxWorkerSync,
      .launch = VpxWorkerLaunch,
      .execute = VpxWorkerExecute,
      .end = VpxWorkerEnd,
  };

  CHECK(vpx_set_worker_interface(&interface));
}

}  // namespace media
