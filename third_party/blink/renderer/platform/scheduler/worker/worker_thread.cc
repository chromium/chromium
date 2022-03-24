// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread.h"

#include <memory>
#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

WorkerThread::WorkerThread(const ThreadCreationParams& params)
    : thread_type_(params.thread_type),
      worker_scheduler_proxy_(params.frame_or_worker_scheduler
                                  ? std::make_unique<WorkerSchedulerProxy>(
                                        params.frame_or_worker_scheduler)
                                  : nullptr),
      supports_gc_(params.supports_gc) {
  base::SimpleThread::Options options;
  options.priority = params.thread_priority;
  thread_ = std::make_unique<SimpleThreadImpl>(
      params.name ? params.name : String(), options, supports_gc_,
      const_cast<scheduler::WorkerThread*>(this));
  if (supports_gc_) {
    MemoryPressureListenerRegistry::Instance().RegisterThread(
        const_cast<scheduler::WorkerThread*>(this));
  }
}

WorkerThread::~WorkerThread() {
  if (supports_gc_) {
    MemoryPressureListenerRegistry::Instance().UnregisterThread(
        const_cast<scheduler::WorkerThread*>(this));
  }
  thread_->Quit();
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  thread_->Join();
}

void WorkerThread::Init() {
  thread_->CreateScheduler();
  thread_->StartAsync();
}

std::unique_ptr<NonMainThreadSchedulerImpl>
WorkerThread::CreateNonMainThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager) {
  return NonMainThreadSchedulerImpl::Create(thread_type_, sequence_manager,
                                            worker_scheduler_proxy_.get());
}

blink::ThreadScheduler* WorkerThread::Scheduler() {
  return thread_->GetNonMainThreadScheduler();
}

scoped_refptr<base::SingleThreadTaskRunner> WorkerThread::GetTaskRunner()
    const {
  return thread_->GetDefaultTaskRunner();
}

void WorkerThread::ShutdownOnThread() {
  thread_->ShutdownOnThread();
  Scheduler()->Shutdown();
}

WorkerThread::SimpleThreadImpl::SimpleThreadImpl(
    const WTF::String& name_prefix,
    const base::SimpleThread ::Options& options,
    bool supports_gc,
    WorkerThread* worker_thread)
    : SimpleThread(name_prefix.Utf8(), options),
      thread_(worker_thread),
      supports_gc_(supports_gc) {
  // TODO(alexclarke): Do we need to unify virtual time for workers and the main
  // thread?
  sequence_manager_ = base::sequence_manager::CreateUnboundSequenceManager(
      base::sequence_manager::SequenceManager::Settings::Builder()
          .SetMessagePumpType(base::MessagePumpType::DEFAULT)
          .SetRandomisedSamplingEnabled(true)
          .Build());
  internal_task_queue_ = sequence_manager_->CreateTaskQueue(
      base::sequence_manager::TaskQueue::Spec("worker_thread_internal_tq"));
  internal_task_runner_ = internal_task_queue_->CreateTaskRunner(
      base::sequence_manager::kTaskTypeNone);
}

void WorkerThread::SimpleThreadImpl::CreateScheduler() {
  DCHECK(!non_main_thread_scheduler_);
  DCHECK(!default_task_runner_);
  DCHECK(sequence_manager_);

  non_main_thread_scheduler_ =
      thread_->CreateNonMainThreadScheduler(sequence_manager_.get());
  non_main_thread_scheduler_->Init();
  default_task_runner_ =
      non_main_thread_scheduler_->DefaultTaskQueue()->CreateTaskRunner(
          TaskType::kWorkerThreadTaskQueueDefault);
}

WorkerThread::GCSupport::GCSupport(WorkerThread* thread) {
  ThreadState* thread_state = ThreadState::AttachCurrentThread();
  gc_task_runner_ = std::make_unique<GCTaskRunner>(thread);
  blink_gc_memory_dump_provider_ = std::make_unique<BlinkGCMemoryDumpProvider>(
      thread_state, base::ThreadTaskRunnerHandle::Get(),
      BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread);
}

WorkerThread::GCSupport::~GCSupport() {
  // Ensure no posted tasks will run from this point on.
  gc_task_runner_.reset();
  blink_gc_memory_dump_provider_.reset();

  ThreadState::DetachCurrentThread();
}

void WorkerThread::SimpleThreadImpl::ShutdownOnThread() {
  gc_support_.reset();
}

void WorkerThread::SimpleThreadImpl::Run() {
  DCHECK(non_main_thread_scheduler_)
      << "CreateScheduler() should be called before starting the thread.";
  non_main_thread_scheduler_->AttachToCurrentThread();

  auto scoped_sequence_manager = std::move(sequence_manager_);
  auto scoped_internal_task_queue = std::move(internal_task_queue_);
  scoped_sequence_manager->BindToMessagePump(
      base::MessagePump::Create(base::MessagePumpType::DEFAULT));

  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  Thread::UpdateThreadTLS(thread_);

  if (supports_gc_)
    gc_support_ = std::make_unique<GCSupport>(thread_);
  run_loop_->Run();
  gc_support_.reset();

  non_main_thread_scheduler_.reset();
  run_loop_ = nullptr;
}

void WorkerThread::SimpleThreadImpl::Quit() {
  if (!internal_task_runner_->RunsTasksInCurrentSequence()) {
    internal_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WorkerThread::SimpleThreadImpl::Quit,
                                  base::Unretained(this)));
    return;
  }
  // We should only get here if we are called by the run loop.
  DCHECK(run_loop_);
  run_loop_->Quit();
}

}  // namespace scheduler
}  // namespace blink
