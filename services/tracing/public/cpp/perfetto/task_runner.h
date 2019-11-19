// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TASK_RUNNER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TASK_RUNNER_H_

#include <list>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/base/task_runner.h"

#if defined(OS_ANDROID)
#include <map>
// Needed for base::FileDescriptorWatcher::Controller and for implementing
// AddFileDescriptorWatch & RemoveFileDescriptorWatch on Android.
#include "base/files/file_descriptor_watcher_posix.h"
#endif  // defined(OS_ANDROID)

namespace tracing {

// This wraps a base::TaskRunner implementation to be able
// to provide it to Perfetto.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoTaskRunner
    : public perfetto::base::TaskRunner {
 public:
  explicit PerfettoTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~PerfettoTaskRunner() override;

  // perfetto::base::TaskRunner implementation. Only called by
  // the Perfetto implementation itself.
  void PostTask(std::function<void()> task) override;
  void PostDelayedTask(std::function<void()> task, uint32_t delay_ms) override;
  // This in Chrome would more correctly be called "RunsTasksInCurrentSequence".
  // Perfetto calls this to determine wheather CommitData requests should be
  // flushed synchronously. RunsTasksInCurrentSequence is sufficient for that
  // use case.
  bool RunsTasksOnCurrentThread() const override;

  void SetTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);
  scoped_refptr<base::SequencedTaskRunner> GetOrCreateTaskRunner();
  bool HasTaskRunner() const { return !!task_runner_; }

  // These are only used on Android when talking to the system Perfetto service.
  void AddFileDescriptorWatch(int fd, std::function<void()>) override;
  void RemoveFileDescriptorWatch(int fd) override;


  // Tests will shut down all task runners in between runs, so we need
  // to re-create any static instances on each SetUp();
  void ResetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void OnDeferredTasksDrainTimer();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
#if defined(OS_ANDROID)
  std::map<int, std::unique_ptr<base::FileDescriptorWatcher::Controller>>
      fd_controllers_;
#endif  // defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(PerfettoTaskRunner);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TASK_RUNNER_H_
