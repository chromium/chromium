// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker_thread_registry.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/worker_thread.h"

namespace content {

namespace {

using WorkerThreadObservers =
    base::ObserverList<WorkerThread::Observer>::Unchecked;
using ThreadLocalWorkerThreadObservers =
    base::ThreadLocalPointer<WorkerThreadObservers>;

// Stores a WorkerThreadObservers instance per thread.
base::LazyInstance<ThreadLocalWorkerThreadObservers>::DestructorAtExit
    g_observers_tls = LAZY_INSTANCE_INITIALIZER;

// A task-runner that refuses to run any tasks.
class DoNothingTaskRunner : public base::TaskRunner {
 public:
  DoNothingTaskRunner() {}

 private:
  ~DoNothingTaskRunner() override {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return false;
  }

  bool RunsTasksInCurrentSequence() const override { return false; }
};

}  // namespace

// WorkerThread implementation:

int WorkerThread::GetCurrentId() {
  if (!g_observers_tls.Pointer()->Get())
    return 0;
  return base::PlatformThread::CurrentId();
}

void WorkerThread::PostTask(int id, base::OnceClosure task) {
  WorkerThreadRegistry::Instance()->PostTask(id, std::move(task));
}

void WorkerThread::AddObserver(Observer* observer) {
  DCHECK(GetCurrentId() > 0);
  WorkerThreadObservers* observers = g_observers_tls.Pointer()->Get();
  DCHECK(observers);
  observers->AddObserver(observer);
}

void WorkerThread::RemoveObserver(Observer* observer) {
  DCHECK(GetCurrentId() > 0);
  WorkerThreadObservers* observers = g_observers_tls.Pointer()->Get();
  DCHECK(observers);
  observers->RemoveObserver(observer);
}

// WorkerThreadRegistry implementation:

WorkerThreadRegistry::WorkerThreadRegistry()
    : task_runner_for_dead_worker_(new DoNothingTaskRunner()) {}

int WorkerThreadRegistry::PostTaskToAllThreads(base::Closure closure) {
  base::AutoLock locker(task_runner_map_lock_);
  for (const auto& it : task_runner_map_)
    it.second->PostTask(FROM_HERE, closure);
  return static_cast<int>(task_runner_map_.size());
}

WorkerThreadRegistry* WorkerThreadRegistry::Instance() {
  static base::LazyInstance<WorkerThreadRegistry>::Leaky worker_task_runner =
      LAZY_INSTANCE_INITIALIZER;
  return worker_task_runner.Pointer();
}

WorkerThreadRegistry::~WorkerThreadRegistry() {}

void WorkerThreadRegistry::DidStartCurrentWorkerThread() {
  DCHECK(!g_observers_tls.Pointer()->Get());
  DCHECK(!base::PlatformThread::CurrentRef().is_null());
  g_observers_tls.Pointer()->Set(new WorkerThreadObservers());
  int id = base::PlatformThread::CurrentId();
  base::AutoLock locker_(task_runner_map_lock_);
  task_runner_map_[id] = base::ThreadTaskRunnerHandle::Get().get();
  CHECK(task_runner_map_[id]);
}

void WorkerThreadRegistry::WillStopCurrentWorkerThread() {
  WorkerThreadObservers* observers = g_observers_tls.Pointer()->Get();
  DCHECK(observers);
  for (auto& observer : *observers)
    observer.WillStopCurrentWorkerThread();

  {
    base::AutoLock locker(task_runner_map_lock_);
    task_runner_map_.erase(WorkerThread::GetCurrentId());
  }
  delete observers;
  g_observers_tls.Pointer()->Set(nullptr);
}

base::TaskRunner* WorkerThreadRegistry::GetTaskRunnerFor(int worker_id) {
  base::AutoLock locker(task_runner_map_lock_);
  return base::ContainsKey(task_runner_map_, worker_id)
             ? task_runner_map_[worker_id]
             : task_runner_for_dead_worker_.get();
}

bool WorkerThreadRegistry::PostTask(int id, base::OnceClosure closure) {
  DCHECK(id > 0);
  base::AutoLock locker(task_runner_map_lock_);
  auto found = task_runner_map_.find(id);
  if (found == task_runner_map_.end())
    return false;
  return found->second->PostTask(FROM_HERE, std::move(closure));
}

}  // namespace content
