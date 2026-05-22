// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CODE_CACHE_DEDICATED_TASK_RUNNER_FOR_RESOURCE_H_
#define CONTENT_BROWSER_CODE_CACHE_DEDICATED_TASK_RUNNER_FOR_RESOURCE_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"

namespace base {
class TaskTraits;
}  // namespace base

namespace content {

// A helper that keeps a TaskRunner associated with a FilePath alive for its
// lifetime. Unlike ThreadPool::CreateSequencedTaskRunnerForResource, this gives
// the caller a dedicated thread. It is the caller's responsibility to keep an
// instance alive as long as any tasks using the resource may run.
class CONTENT_EXPORT DedicatedTaskRunnerForResource final {
 public:
  // Returns the task runner for `path`. The same `traits` must be provided to
  // all calls with the same `path`.
  static DedicatedTaskRunnerForResource Acquire(const base::TaskTraits& traits,
                                                const base::FilePath& path);

  DedicatedTaskRunnerForResource();
  DedicatedTaskRunnerForResource(const DedicatedTaskRunnerForResource&) =
      delete;
  DedicatedTaskRunnerForResource& operator=(
      const DedicatedTaskRunnerForResource&) = delete;
  DedicatedTaskRunnerForResource(
      DedicatedTaskRunnerForResource&& other) noexcept;
  DedicatedTaskRunnerForResource& operator=(
      DedicatedTaskRunnerForResource&& other) noexcept;
  ~DedicatedTaskRunnerForResource();

  bool is_null() const { return !task_runner_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return task_runner_;
  }

 private:
  DedicatedTaskRunnerForResource(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::FilePath path);

  void Reset();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::FilePath path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CODE_CACHE_DEDICATED_TASK_RUNNER_FOR_RESOURCE_H_
