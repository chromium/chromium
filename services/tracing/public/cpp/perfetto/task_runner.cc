// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/task_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/task/common/checked_lock_impl.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_local_storage.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

namespace tracing {

PerfettoTaskRunner::PerfettoTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

PerfettoTaskRunner::~PerfettoTaskRunner() {
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
#if defined(OS_ANDROID)
  fd_controllers_.clear();
#endif  // defined(OS_ANDROID)
}

void PerfettoTaskRunner::PostTask(std::function<void()> task) {
  base::ScopedDeferTaskPosting::PostOrDefer(
      GetOrCreateTaskRunner(), FROM_HERE,
      base::BindOnce(
          [](std::function<void()> task) {
            // We block any trace events that happens while any
            // Perfetto task is running, or we'll get deadlocks in
            // situations where the StartupTraceWriterRegistry tries
            // to bind a writer which in turn causes a PostTask where
            // a trace event can be emitted, which then deadlocks as
            // it needs a new chunk from the same StartupTraceWriter
            // which we're trying to bind and are keeping the lock
            // to.
            // TODO(oysteine): Try to see if we can be more selective
            // about this.
            AutoThreadLocalBoolean thread_is_in_trace_event(
                TraceEventDataSource::GetThreadIsInTraceEventTLS());
            task();
          },
          task));
}

void PerfettoTaskRunner::PostDelayedTask(std::function<void()> task,
                                         uint32_t delay_ms) {
  if (delay_ms == 0) {
    PostTask(std::move(task));
    return;
  }

  // There's currently nothing which uses PostDelayedTask on the ProducerClient
  // side, where PostTask sometimes requires blocking. If this DCHECK ever
  // triggers, support for deferring delayed tasks need to be added.
  DCHECK(!base::ScopedDeferTaskPosting::IsPresent());
  GetOrCreateTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](std::function<void()> task) { task(); }, task),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

bool PerfettoTaskRunner::RunsTasksOnCurrentThread() const {
  DCHECK(task_runner_);
  return task_runner_->RunsTasksInCurrentSequence();
}

void PerfettoTaskRunner::AddFileDescriptorWatch(
    int fd,
    std::function<void()> callback) {
#if !defined(OS_ANDROID)
  NOTREACHED();
#else
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!base::Contains(fd_controllers_, fd));
  fd_controllers_[fd] = base::FileDescriptorWatcher::WatchReadable(
      fd,
      base::BindRepeating([](std::function<void()> callback) { callback(); },
                          std::move(callback)));
#endif  // !defined(OS_ANDROID)
}

void PerfettoTaskRunner::RemoveFileDescriptorWatch(int fd) {
#if !defined(OS_ANDROID)
  NOTREACHED();
#else
  DCHECK(GetOrCreateTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(base::Contains(fd_controllers_, fd));
  fd_controllers_.erase(fd);
#endif  // !defined(OS_ANDROID)
}

void PerfettoTaskRunner::ResetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

void PerfettoTaskRunner::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = std::move(task_runner);
}

scoped_refptr<base::SequencedTaskRunner>
PerfettoTaskRunner::GetOrCreateTaskRunner() {
  if (!task_runner_) {
    DCHECK(base::ThreadPoolInstance::Get());
    task_runner_ =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::USER_BLOCKING});
  }

  return task_runner_;
}

}  // namespace tracing
