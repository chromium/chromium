// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_impl.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
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
#include "base/time/default_tick_clock.h"
#include "mojo/public/cpp/bindings/direct_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {

std::unique_ptr<NonMainThread> NonMainThread::CreateThread(
    const ThreadCreationParams& params) {
#if DCHECK_IS_ON()
  WTF::WillCreateThread();
#endif
  auto thread = std::make_unique<scheduler::NonMainThreadImpl>(params);
  thread->Init();
  return std::move(thread);
}

namespace scheduler {

NonMainThreadImpl::NonMainThreadImpl(const ThreadCreationParams& params)
    : thread_type_(params.thread_type),
      worker_scheduler_proxy_(params.frame_or_worker_scheduler
                                  ? std::make_unique<WorkerSchedulerProxy>(
                                        params.frame_or_worker_scheduler)
                                  : nullptr),
      supports_gc_(params.supports_gc) {
  base::SimpleThread::Options options;
  options.thread_type = params.base_thread_type;

  base::MessagePumpType message_pump_type = base::MessagePumpType::DEFAULT;
  if (params.thread_type == ThreadType::kCompositorThread &&
      base::FeatureList::IsEnabled(features::kDirectCompositorThreadIpc) &&
      mojo::IsDirectReceiverSupported()) {
    message_pump_type = base::MessagePumpType::IO;
  }
  thread_ = std::make_unique<SimpleThreadImpl>(
      params.name ? params.name : String(), options, params.realtime_period,
      supports_gc_, const_cast<scheduler::NonMainThreadImpl*>(this),
      message_pump_type);
  if (supports_gc_) {
    MemoryPressureListenerRegistry::Instance().RegisterThread(
        const_cast<scheduler::NonMainThreadImpl*>(this));
  }
}

NonMainThreadImpl::~NonMainThreadImpl() {
  if (supports_gc_) {
    MemoryPressureListenerRegistry::Instance().UnregisterThread(
        const_cast<scheduler::NonMainThreadImpl*>(this));
  }
  thread_->Quit();
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  thread_->Join();
}

void NonMainThreadImpl::Init() {
  thread_->CreateScheduler();
  thread_->StartAsync();
}

std::unique_ptr<NonMainThreadSchedulerBase>
NonMainThreadImpl::CreateNonMainThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager) {
  return std::make_unique<WorkerThreadScheduler>(thread_type_, sequence_manager,
                                                 worker_scheduler_proxy_.get());
}

blink::ThreadScheduler* NonMainThreadImpl::Scheduler() {
  return static_cast<WorkerThreadScheduler*>(
      thread_->GetNonMainThreadScheduler());
}

scoped_refptr<base::SingleThreadTaskRunner> NonMainThreadImpl::GetTaskRunner()
    const {
  return thread_->GetDefaultTaskRunner();
}

void NonMainThreadImpl::ShutdownOnThread() {
  thread_->ShutdownOnThread();
  Scheduler()->Shutdown();
}

NonMainThreadImpl::SimpleThreadImpl::SimpleThreadImpl(
    const WTF::String& name_prefix,
    const base::SimpleThread ::Options& options,
    base::TimeDelta realtime_period,
    bool supports_gc,
    NonMainThreadImpl* worker_thread,
    base::MessagePumpType message_pump_type)
    : SimpleThread(name_prefix.Utf8(), options),
#if BUILDFLAG(IS_APPLE)
      realtime_period_((options.thread_type == base::ThreadType::kRealtimeAudio)
                           ? realtime_period
                           : base::TimeDelta()),
#endif
      message_pump_type_(message_pump_type),
      thread_(worker_thread),
      supports_gc_(supports_gc) {
  // TODO(alexclarke): Do we need to unify virtual time for workers and the main
  // thread?
  sequence_manager_ = base::sequence_manager::CreateUnboundSequenceManager(
      base::sequence_manager::SequenceManager::Settings::Builder()
          .SetMessagePumpType(message_pump_type)
          .SetRandomisedSamplingEnabled(true)
          .SetPrioritySettings(CreatePrioritySettings())
          .Build());
  internal_task_queue_ = sequence_manager_->CreateTaskQueue(
      base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::QueueName::WORKER_THREAD_INTERNAL_TQ));
  internal_task_runner_ = internal_task_queue_->CreateTaskRunner(
      base::sequence_manager::kTaskTypeNone);
}

void NonMainThreadImpl::SimpleThreadImpl::CreateScheduler() {
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

NonMainThreadImpl::GCSupport::GCSupport(NonMainThreadImpl* thread) {
  ThreadState* thread_state = ThreadState::AttachCurrentThread();
  blink_gc_memory_dump_provider_ = std::make_unique<BlinkGCMemoryDumpProvider>(
      thread_state, base::SingleThreadTaskRunner::GetCurrentDefault(),
      BlinkGCMemoryDumpProvider::HeapType::kBlinkWorkerThread);
}

NonMainThreadImpl::GCSupport::~GCSupport() {
  // Ensure no posted tasks will run from this point on.
  blink_gc_memory_dump_provider_.reset();

  ThreadState::DetachCurrentThread();
}

void NonMainThreadImpl::SimpleThreadImpl::ShutdownOnThread() {
  gc_support_.reset();
}

void NonMainThreadImpl::SimpleThreadImpl::Run() {
  DCHECK(non_main_thread_scheduler_)
      << "CreateScheduler() should be called before starting the thread.";
  non_main_thread_scheduler_->AttachToCurrentThread();

  auto scoped_sequence_manager = std::move(sequence_manager_);
  auto scoped_internal_task_queue = std::move(internal_task_queue_);
  scoped_sequence_manager->BindToMessagePump(
      base::MessagePump::Create(message_pump_type_));

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

void NonMainThreadImpl::SimpleThreadImpl::Quit() {
  if (!internal_task_runner_->RunsTasksInCurrentSequence()) {
    internal_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NonMainThreadImpl::SimpleThreadImpl::Quit,
                                  base::Unretained(this)));
    return;
  }
  // We should only get here if we are called by the run loop.
  DCHECK(run_loop_);
  run_loop_->Quit();
}

}  // namespace scheduler
}  // namespace blink
