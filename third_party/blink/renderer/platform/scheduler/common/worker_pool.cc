// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace blink {

namespace worker_pool {

void PostTask(const base::Location& location, CrossThreadOnceClosure closure) {
  PostTask(location, {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
           std::move(closure));
}

void PostTask(const base::Location& location,
              const base::TaskTraits& traits,
              CrossThreadOnceClosure closure) {
  base::ThreadPool::PostTask(location, traits,
                             ConvertToBaseOnceCallback(std::move(closure)));
}

scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunner(
    const base::TaskTraits& traits) {
  return base::ThreadPool::CreateSequencedTaskRunner(traits);
}

}  // namespace worker_pool

}  // namespace blink
