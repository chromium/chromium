// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from chrome/common/profiler/thread_profiler.h

// TODO(crbug.com/40778431): remove this once //chrome/common/profiler is moved
// to components/profiler.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_IOS_THREAD_PROFILER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_IOS_THREAD_PROFILER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "components/sampling_profiler/process_type.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

// IOSThreadProfiler performs startup and periodic profiling of Chrome
// threads.
class IOSThreadProfiler {
 public:
  ~IOSThreadProfiler();
  IOSThreadProfiler(const IOSThreadProfiler&) = delete;
  IOSThreadProfiler& operator=(const IOSThreadProfiler&) = delete;

  // Creates a profiler for a main thread and immediately starts it. This
  // function should only be used when profiling the main thread of a
  // process. The returned profiler must be destroyed prior to thread exit to
  // stop the profiling.
  //
  // SetMainThreadTaskRunner() should be called after the message loop has been
  // started on the thread. It is the caller's responsibility to ensure that
  // the instance returned by this function is still alive when the static API
  // SetMainThreadTaskRunner() is used. The latter is static to support Chrome's
  // set up where the IOSThreadProfiler is created in chrome/app which cannot be
  // easily accessed from chrome_browser_main.cc which sets the task runner.
  static std::unique_ptr<IOSThreadProfiler> CreateAndStartOnMainThread();

  // Sets the task runner when profiling on the main thread. This occurs in a
  // separate call from CreateAndStartOnMainThread so that startup profiling can
  // occur prior to message loop start. The task runner is associated with the
  // instance returned by CreateAndStartOnMainThread(), which must be alive when
  // this is called.
  static void SetMainThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Get the stack sampling params to use.
  static base::StackSamplingProfiler::SamplingParams GetSamplingParams();

  // Creates a profiler for a child thread and immediately starts it. This
  // should be called from a task posted on the child thread immediately after
  // thread start. The thread will be profiled until exit.
  static void StartOnChildThread(sampling_profiler::ProfilerThreadType thread);

  // Sets the callback to use for reporting browser process profiles. This
  // indirection is required to avoid a dependency on unnecessary metrics code
  // in child processes.
  static void SetBrowserProcessReceiverCallback(
      const base::RepeatingCallback<void(base::TimeTicks,
                                         metrics::SampledProfile)>& callback);

  // This function must be called within child processes to supply the Service
  // Manager's connector, to bind the interface through which a profile is sent
  // back to the browser process.
  //
  // Note that the metrics::CallStackProfileCollector interface also must be
  // exposed to the child process, and metrics::mojom::CallStackProfileCollector
  // declared in chrome_content_browser_manifest_overlay.json, for the binding
  // to succeed.
  static void SetCollectorForChildProcess(
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector);

 private:
  class WorkIdRecorder;

  // Creates the profiler. The task runner will be supplied for child threads
  // but not for main threads.
  IOSThreadProfiler(
      sampling_profiler::ProfilerThreadType thread,
      scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner =
          scoped_refptr<base::SingleThreadTaskRunner>());

  // Posts a task on `owning_thread_task_runner` to start the next periodic
  // sampling collection on the completion of the previous collection.
  static void OnPeriodicCollectionCompleted(
      scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner,
      base::WeakPtr<IOSThreadProfiler> thread_profiler);

  // Sets the task runner when profiling on the main thread. This occurs in a
  // separate call from CreateAndStartOnMainThread so that startup profiling can
  // occur prior to message loop start.
  void SetMainThreadTaskRunnerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Posts a delayed task to start the next periodic sampling collection.
  void ScheduleNextPeriodicCollection();

  // Creates a new periodic profiler and initiates a collection with it.
  void StartPeriodicSamplingCollection();

  const sampling_profiler::ProfilerProcessType process_;
  const sampling_profiler::ProfilerThreadType thread_;

  scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

  std::unique_ptr<WorkIdRecorder> work_id_recorder_;

  std::unique_ptr<base::StackSamplingProfiler> startup_profiler_;

  std::unique_ptr<base::StackSamplingProfiler> periodic_profiler_;
  std::unique_ptr<base::PeriodicSamplingScheduler> periodic_sampling_scheduler_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<IOSThreadProfiler> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_IOS_THREAD_PROFILER_H_
