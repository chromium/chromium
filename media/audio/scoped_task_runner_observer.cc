// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/scoped_task_runner_observer.h"

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"

namespace media {

ScopedTaskRunnerObserver::ScopedTaskRunnerObserver(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  ObserveLoopDestruction(true, NULL);
}

ScopedTaskRunnerObserver::~ScopedTaskRunnerObserver() {
  ObserveLoopDestruction(false, NULL);
}

void ScopedTaskRunnerObserver::ObserveLoopDestruction(
    bool enable,
    base::WaitableEvent* done) {
  // Note: |done| may be NULL.
  if (task_runner_->BelongsToCurrentThread()) {
    base::CurrentThread loop = base::CurrentThread::Get();
    if (enable) {
      loop->AddDestructionObserver(this);
    } else {
      loop->RemoveDestructionObserver(this);
    }
  } else {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    if (task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&ScopedTaskRunnerObserver::ObserveLoopDestruction,
                           base::Unretained(this), enable, &event))) {
      event.Wait();
    } else {
      // The message loop's thread has already terminated, so no need to wait.
    }
  }

  if (done)
    done->Signal();
}

}  // namespace media.
