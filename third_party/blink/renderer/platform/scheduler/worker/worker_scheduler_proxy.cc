// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"

#include "base/functional/bind.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerSchedulerProxy::WorkerSchedulerProxy(FrameOrWorkerScheduler* scheduler) {
  DCHECK(scheduler);
  throttling_observer_handle_ = scheduler->AddLifecycleObserver(
      FrameOrWorkerScheduler::ObserverType::kWorkerScheduler,
      base::BindRepeating(&WorkerSchedulerProxy::OnLifecycleStateChanged,
                          base::Unretained(this)));
  if (FrameScheduler* frame_scheduler = scheduler->ToFrameScheduler()) {
    parent_frame_type_ = GetFrameOriginType(frame_scheduler);
    initial_frame_status_ = GetFrameStatus(frame_scheduler);
    ukm_source_id_ = frame_scheduler->GetUkmSourceId();
  }
}

WorkerSchedulerProxy::~WorkerSchedulerProxy() {
  DETACH_FROM_THREAD(parent_thread_checker_);
}

void WorkerSchedulerProxy::OnWorkerSchedulerCreated(
    base::WeakPtr<WorkerScheduler> worker_scheduler) {
  DCHECK(!IsMainThread())
      << "OnWorkerSchedulerCreated should be called from the worker thread";
  DCHECK(!worker_scheduler_) << "OnWorkerSchedulerCreated is called twice";
  DCHECK(worker_scheduler) << "WorkerScheduler is expected to exist";
  worker_scheduler_ = std::move(worker_scheduler);
  worker_thread_task_runner_ = worker_scheduler_->GetWorkerThreadScheduler()
                                   ->ControlTaskQueue()
                                   ->GetTaskRunnerWithDefaultTaskType();
  initialized_ = true;
}

void WorkerSchedulerProxy::OnLifecycleStateChanged(
    SchedulingLifecycleState lifecycle_state) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  if (lifecycle_state_ == lifecycle_state)
    return;
  lifecycle_state_ = lifecycle_state;

  if (!initialized_)
    return;

  worker_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WorkerScheduler::OnLifecycleStateChanged,
                                worker_scheduler_, lifecycle_state));
}

}  // namespace scheduler
}  // namespace blink
