// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/worker_thread_registry.h"

#include <atomic>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/renderer/worker_thread.h"

namespace content {

namespace {

using WorkerThreadObservers =
    base::ObserverList<WorkerThread::Observer>::Unchecked;

struct WorkerThreadData {
  WorkerThreadData() {
    // Worker thread ID starts with 1 (0 is reserved for the main thread).
    static std::atomic_int seq{1};
    thread_id = seq++;
  }

  int thread_id = 0;
  WorkerThreadObservers observers;
};

constinit thread_local WorkerThreadData* worker_data = nullptr;

// A task-runner that refuses to run any tasks.
class DoNothingTaskRunner : public base::SequencedTaskRunner {
 public:
  DoNothingTaskRunner() {}

 private:
  ~DoNothingTaskRunner() override {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return false;
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return false;
  }

  bool RunsTasksInCurrentSequence() const override { return false; }
};

}  // namespace

// WorkerThread implementation:

int WorkerThread::GetCurrentId() {
  return worker_data ? worker_data->thread_id : 0;
}

void WorkerThread::PostTask(int id, base::OnceClosure task) {
  WorkerThreadRegistry::Instance()->PostTask(id, std::move(task));
}

void WorkerThread::AddObserver(Observer* observer) {
  DCHECK(worker_data);
  worker_data->observers.AddObserver(observer);
}

void WorkerThread::RemoveObserver(Observer* observer) {
  DCHECK(worker_data);
  worker_data->observers.RemoveObserver(observer);
}

// WorkerThreadRegistry implementation:

WorkerThreadRegistry::WorkerThreadRegistry()
    : task_runner_for_dead_worker_(new DoNothingTaskRunner()) {}

int WorkerThreadRegistry::PostTaskToAllThreads(
    const base::RepeatingClosure& closure) {
  base::AutoLock locker(task_runner_map_lock_);
  for (const auto& it : task_runner_map_)
    it.second->PostTask(FROM_HERE, closure);
  return static_cast<int>(task_runner_map_.size());
}

WorkerThreadRegistry* WorkerThreadRegistry::Instance() {
  static base::NoDestructor<WorkerThreadRegistry> worker_thread_registry;
  return worker_thread_registry.get();
}

WorkerThreadRegistry::~WorkerThreadRegistry() = default;

void WorkerThreadRegistry::DidStartCurrentWorkerThread() {
  DCHECK(!worker_data);
  DCHECK(!base::PlatformThread::CurrentRef().is_null());
  worker_data = new WorkerThreadData();
  base::AutoLock locker_(task_runner_map_lock_);
  task_runner_map_[worker_data->thread_id] =
      base::SingleThreadTaskRunner::GetCurrentDefault().get();
  CHECK(task_runner_map_[worker_data->thread_id]);
}

void WorkerThreadRegistry::WillStopCurrentWorkerThread() {
  DCHECK(worker_data);
  for (auto& observer : worker_data->observers)
    observer.WillStopCurrentWorkerThread();
  {
    base::AutoLock locker(task_runner_map_lock_);
    task_runner_map_.erase(worker_data->thread_id);
  }
  delete worker_data;
  worker_data = nullptr;
}

base::SequencedTaskRunner* WorkerThreadRegistry::GetTaskRunnerFor(
    int worker_id) {
  base::AutoLock locker(task_runner_map_lock_);
  return base::Contains(task_runner_map_, worker_id)
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
