// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/dedicated_task_runner_for_resource.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

namespace {

// A registry that maps file paths to dedicated single-thread task runners.
// This is used to ensure that all operations on a given path (e.g., SQLite
// databases) are sequenced, even across different context instances (like
// during profile recreation), while still running on a dedicated thread for
// performance.
class PathTaskRunnerMap {
 public:
  static PathTaskRunnerMap& GetInstance();

  PathTaskRunnerMap() = default;
  PathTaskRunnerMap(const PathTaskRunnerMap&) = delete;
  PathTaskRunnerMap& operator=(const PathTaskRunnerMap&) = delete;

  // Acquires a dedicated task runner for the given path. If a task runner
  // already exists for this path, it is reused and its reference count is
  // incremented. Otherwise, a new dedicated thread is spawned.
  scoped_refptr<base::SingleThreadTaskRunner> Acquire(
      const base::TaskTraits& traits,
      const base::FilePath& path);

  // Releases the runner at `path`. The dedicated thread for `path` is allowed
  // to exit when the final reference is released.
  void Release(const base::FilePath& path);

 private:
  struct Entry {
    Entry(const base::FilePath& path, const base::TaskTraits& traits) {
      task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
          traits, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    }
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    int ref_count = 0;
  };

  using MapType = absl::flat_hash_map<base::FilePath, std::unique_ptr<Entry>>;

  base::Lock lock_;
  MapType map_ GUARDED_BY(lock_);
};

// static
PathTaskRunnerMap& PathTaskRunnerMap::GetInstance() {
  static base::NoDestructor<PathTaskRunnerMap> instance;
  return *instance;
}

scoped_refptr<base::SingleThreadTaskRunner> PathTaskRunnerMap::Acquire(
    const base::TaskTraits& traits,
    const base::FilePath& path) {
  base::AutoLock lock(lock_);
  auto [iter, inserted] = map_.try_emplace(path, nullptr);
  auto& entry = iter->second;
  if (inserted) {
    entry = std::make_unique<Entry>(path, traits);
  }
  ++entry->ref_count;
  return entry->task_runner;
}

void PathTaskRunnerMap::Release(const base::FilePath& path) {
  base::AutoLock lock(lock_);
  auto it = map_.find(path);
  CHECK(it != map_.end());
  if (--it->second->ref_count == 0) {
    map_.erase(it);
  }
}

}  // namespace

// static
DedicatedTaskRunnerForResource DedicatedTaskRunnerForResource::Acquire(
    const base::TaskTraits& traits,
    const base::FilePath& path) {
  return {PathTaskRunnerMap::GetInstance().Acquire(traits, path), path};
}

DedicatedTaskRunnerForResource::DedicatedTaskRunnerForResource() = default;

DedicatedTaskRunnerForResource::DedicatedTaskRunnerForResource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::FilePath path)
    : task_runner_(std::move(task_runner)), path_(std::move(path)) {}

DedicatedTaskRunnerForResource::DedicatedTaskRunnerForResource(
    DedicatedTaskRunnerForResource&& other) noexcept
    : task_runner_(std::move(other.task_runner_)),
      path_(std::move(other.path_)) {}

DedicatedTaskRunnerForResource& DedicatedTaskRunnerForResource::operator=(
    DedicatedTaskRunnerForResource&& other) noexcept {
  if (this != &other) {
    Reset();
    task_runner_ = std::move(other.task_runner_);
    path_ = std::move(other.path_);
  }
  return *this;
}

DedicatedTaskRunnerForResource::~DedicatedTaskRunnerForResource() {
  Reset();
}

void DedicatedTaskRunnerForResource::Reset() {
  if (task_runner_) {
    PathTaskRunnerMap::GetInstance().Release(path_);
    task_runner_.reset();
  }
  path_.clear();
}

}  // namespace content
