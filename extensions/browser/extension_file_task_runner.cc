// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_file_task_runner.h"

#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace extensions {

namespace {

// Note: All tasks posted to a single task runner have the same priority. This
// is unfortunate, since some file-related tasks are high priority (like serving
// a file from the extension protocols or loading an extension in response to a
// user action), and others are low priority (like garbage collection). Split
// the difference and use USER_VISIBLE, which is the default priority and what a
// task posted to a named thread (like the FILE thread) would receive.
base::LazyThreadPoolSequencedTaskRunner g_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                         base::TaskPriority::USER_VISIBLE));

}  // namespace

scoped_refptr<base::SequencedTaskRunner> GetExtensionFileTaskRunner() {
  return g_task_runner.Get();
}

scoped_refptr<base::SequencedTaskRunner> GetOneShotFileTaskRunner(
    base::TaskPriority priority) {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       priority});
}

}  // namespace extensions
