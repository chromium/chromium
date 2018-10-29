// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread.h"

#include <memory>
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerThread::WorkerThread(const ThreadCreationParams& params)
    : thread_(new base::Thread(params.name ? params.name : std::string())),
      thread_type_(params.thread_type),
      worker_scheduler_proxy_(params.frame_or_worker_scheduler
                                  ? std::make_unique<WorkerSchedulerProxy>(
                                        params.frame_or_worker_scheduler)
                                  : nullptr) {
  bool started = thread_->StartWithOptions(params.thread_options);
  CHECK(started);
  thread_task_runner_ = thread_->task_runner();
}

void WorkerThread::Init() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WorkerThread::InitOnThread,
                                base::Unretained(this), &completion));
  completion.Wait();
}

WorkerThread::~WorkerThread() {
  // We want to avoid blocking main thread when the thread was already
  // shut down, but calling ShutdownOnThread twice does not cause any problems.
  if (!was_shutdown_on_thread_.IsSet()) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WorkerThread::ShutdownOnThread,
                                  base::Unretained(this), &completion));
    completion.Wait();
  }
  thread_->Stop();
}

void WorkerThread::InitOnThread(base::WaitableEvent* completion) {
  // TODO(alexclarke): Do we need to unify virtual time for workers and the
  // main thread?
  non_main_thread_scheduler_ = CreateNonMainThreadScheduler();
  non_main_thread_scheduler_->Init();
  task_queue_ = non_main_thread_scheduler_->DefaultTaskQueue();
  task_runner_ =
      task_queue_->CreateTaskRunner(TaskType::kWorkerThreadTaskQueueDefault);
  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
  completion->Signal();
}

void WorkerThread::ShutdownOnThread(base::WaitableEvent* completion) {
  was_shutdown_on_thread_.Set();

  task_queue_ = nullptr;
  task_runner_ = nullptr;
  non_main_thread_scheduler_ = nullptr;

  if (completion)
    completion->Signal();
}

std::unique_ptr<NonMainThreadSchedulerImpl>
WorkerThread::CreateNonMainThreadScheduler() {
  return NonMainThreadSchedulerImpl::Create(thread_type_,
                                            worker_scheduler_proxy_.get());
}

void WorkerThread::WillDestroyCurrentMessageLoop() {
  ShutdownOnThread(nullptr);
}

blink::PlatformThreadId WorkerThread::ThreadId() const {
  return thread_->GetThreadId();
}

blink::ThreadScheduler* WorkerThread::Scheduler() {
  return non_main_thread_scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerThread::GetTaskRunner()
    const {
  return task_runner_;
}

}  // namespace scheduler
}  // namespace blink
