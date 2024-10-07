// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from chrome/common/profiler/thread_profiler.cc

#include "ios/chrome/browser/web/model/ios_thread_profiler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/work_id_provider.h"
#include "base/process/process.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"

#if BUILDFLAG(USE_BLINK)
#include "base/process/port_provider_mac.h"
#endif

using CallStackProfileBuilder = metrics::CallStackProfileBuilder;
using CallStackProfileParams = sampling_profiler::CallStackProfileParams;
using StackSamplingProfiler = base::StackSamplingProfiler;

namespace {

// Pointer to the main thread instance, if any. Stored as a global because it's
// created very early in chrome/app - and is thus otherwise inaccessible from
// chrome_dll, by the time we need to register the main thread task runner.
IOSThreadProfiler* g_main_thread_instance = nullptr;

// Run continuous profiling 2% of the time.
constexpr double kFractionOfExecutionTimeToSample = 0.02;

bool IsCurrentProcessBackgrounded() {
#if BUILDFLAG(USE_BLINK)
  base::SelfPortProvider self_provider;
  return base::Process::Current().GetPriority(&self_provider) ==
         base::Process::Priority::kBestEffort;
#else
  return base::Process::Current().GetPriority() ==
         base::Process::Priority::kBestEffort;
#endif
}

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  return base::StackSamplingProfiler::UnwindersFactory();
}

const base::RepeatingClosure GetApplyPerSampleMetadataCallback(
    sampling_profiler::ProfilerProcessType process) {
  if (process != sampling_profiler::ProfilerProcessType::kRenderer) {
    return base::RepeatingClosure();
  }
  static const base::SampleMetadata process_backgrounded(
      "ProcessBackgrounded", base::SampleMetadataScope::kProcess);
  return base::BindRepeating(
      [](base::SampleMetadata process_backgrounded) {
        process_backgrounded.Set(IsCurrentProcessBackgrounded());
      },
      process_backgrounded);
}

}  // namespace

// Records the current unique id for the work item being executed in the target
// thread's message loop.
class IOSThreadProfiler::WorkIdRecorder : public metrics::WorkIdRecorder {
 public:
  explicit WorkIdRecorder(base::WorkIdProvider* work_id_provider)
      : work_id_provider_(work_id_provider) {}

  // Invoked on the profiler thread while the target thread is suspended.
  unsigned int RecordWorkId() const override {
    return work_id_provider_->GetWorkId();
  }

  WorkIdRecorder(const WorkIdRecorder&) = delete;
  WorkIdRecorder& operator=(const WorkIdRecorder&) = delete;

 private:
  const raw_ptr<base::WorkIdProvider> work_id_provider_;
};

IOSThreadProfiler::~IOSThreadProfiler() {
  if (g_main_thread_instance == this)
    g_main_thread_instance = nullptr;
}

// static
std::unique_ptr<IOSThreadProfiler>
IOSThreadProfiler::CreateAndStartOnMainThread() {
  DCHECK(!g_main_thread_instance);
  auto instance = base::WrapUnique(
      new IOSThreadProfiler(sampling_profiler::ProfilerThreadType::kMain));
  if (!g_main_thread_instance)
    g_main_thread_instance = instance.get();
  return instance;
}

// static
void IOSThreadProfiler::SetMainThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(g_main_thread_instance);
  g_main_thread_instance->SetMainThreadTaskRunnerImpl(task_runner);
}

// static
void IOSThreadProfiler::StartOnChildThread(
    sampling_profiler::ProfilerThreadType thread) {
  // The profiler object is stored in a SequenceLocalStorageSlot on child
  // threads to give it the same lifetime as the threads.
  static base::SequenceLocalStorageSlot<std::unique_ptr<IOSThreadProfiler>>
      child_thread_profiler_sequence_local_storage;
  child_thread_profiler_sequence_local_storage.emplace(new IOSThreadProfiler(
      thread, base::SingleThreadTaskRunner::GetCurrentDefault()));
}

// static
void IOSThreadProfiler::SetBrowserProcessReceiverCallback(
    const base::RepeatingCallback<void(base::TimeTicks,
                                       metrics::SampledProfile)>& callback) {
  CallStackProfileBuilder::SetBrowserProcessReceiverCallback(callback);
}

// static
void IOSThreadProfiler::SetCollectorForChildProcess(
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector) {
  CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
      std::move(collector));
}

// static
base::StackSamplingProfiler::SamplingParams
IOSThreadProfiler::GetSamplingParams() {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::Milliseconds(0);
  const base::TimeDelta duration = base::Seconds(30);
  params.sampling_interval = base::Milliseconds(100);
  params.samples_per_profile = duration / params.sampling_interval;

  return params;
}

// IOSThreadProfiler implementation synopsis:
//
// On creation, the profiler creates and starts the startup
// StackSamplingProfiler, and configures the PeriodicSamplingScheduler such that
// it starts scheduling from the time the startup profiling will be complete.
// When a message loop is available (either in the constructor, or via
// SetMainThreadTaskRunner) a task is posted to start the first periodic
// collection at the initial scheduled collection time.
//
// When the periodic collection task executes, it creates and starts a new
// periodic profiler and configures it to call OnPeriodicCollectionCompleted as
// its completion callback. OnPeriodicCollectionCompleted is called on the
// profiler thread and schedules a task on the original thread to schedule
// another periodic collection. When the task runs, it posts a new task to start
// another periodic collection at the next scheduled collection time.
//
// The process in previous paragraph continues until the IOSThreadProfiler is
// destroyed prior to thread exit.
IOSThreadProfiler::IOSThreadProfiler(
    sampling_profiler::ProfilerThreadType thread,
    scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner)
    : process_(sampling_profiler::ProfilerProcessType::kBrowser),
      thread_(thread),
      owning_thread_task_runner_(owning_thread_task_runner),
      work_id_recorder_(std::make_unique<WorkIdRecorder>(
          base::WorkIdProvider::GetForCurrentThread())) {
  const base::StackSamplingProfiler::SamplingParams sampling_params =
      IOSThreadProfiler::GetSamplingParams();

  startup_profiler_ = std::make_unique<StackSamplingProfiler>(
      base::GetSamplingProfilerCurrentThreadToken(), sampling_params,
      std::make_unique<CallStackProfileBuilder>(
          CallStackProfileParams(
              process_, thread,
              CallStackProfileParams::Trigger::kProcessStartup),
          work_id_recorder_.get()),
      CreateCoreUnwindersFactory(),
      GetApplyPerSampleMetadataCallback(process_));

  startup_profiler_->Start();

  // Estimated time at which the startup profiling will be completed. It's OK if
  // this doesn't exactly coincide with the end of the startup profiling, since
  // there's no harm in having a brief overlap of startup and periodic
  // profiling.
  base::TimeTicks startup_profiling_completion_time =
      base::TimeTicks::Now() +
      sampling_params.samples_per_profile * sampling_params.sampling_interval;

  periodic_sampling_scheduler_ =
      std::make_unique<base::PeriodicSamplingScheduler>(
          sampling_params.samples_per_profile *
              sampling_params.sampling_interval,
          kFractionOfExecutionTimeToSample, startup_profiling_completion_time);

  if (owning_thread_task_runner_)
    ScheduleNextPeriodicCollection();
}

// static
void IOSThreadProfiler::OnPeriodicCollectionCompleted(
    scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner,
    base::WeakPtr<IOSThreadProfiler> thread_profiler) {
  owning_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&IOSThreadProfiler::ScheduleNextPeriodicCollection,
                     thread_profiler));
}

void IOSThreadProfiler::SetMainThreadTaskRunnerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This should only be called if the task runner wasn't provided in the
  // constructor.
  DCHECK(!owning_thread_task_runner_);
  owning_thread_task_runner_ = task_runner;
  ScheduleNextPeriodicCollection();
}

void IOSThreadProfiler::ScheduleNextPeriodicCollection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  owning_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IOSThreadProfiler::StartPeriodicSamplingCollection,
                     weak_factory_.GetWeakPtr()),
      periodic_sampling_scheduler_->GetTimeToNextCollection());
}

void IOSThreadProfiler::StartPeriodicSamplingCollection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // NB: Destroys the previous profiler as side effect.
  periodic_profiler_ = std::make_unique<StackSamplingProfiler>(
      base::GetSamplingProfilerCurrentThreadToken(),
      IOSThreadProfiler::GetSamplingParams(),
      std::make_unique<CallStackProfileBuilder>(
          CallStackProfileParams(
              process_, thread_,
              CallStackProfileParams::Trigger::kPeriodicCollection),
          work_id_recorder_.get(),
          base::BindOnce(&IOSThreadProfiler::OnPeriodicCollectionCompleted,
                         owning_thread_task_runner_,
                         weak_factory_.GetWeakPtr())),
      CreateCoreUnwindersFactory(),
      GetApplyPerSampleMetadataCallback(process_));

  periodic_profiler_->Start();
}
