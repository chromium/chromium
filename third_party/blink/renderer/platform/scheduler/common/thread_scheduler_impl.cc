// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

void ThreadSchedulerImpl::ExecuteAfterCurrentTask(
    base::OnceClosure on_completion_task) {
  GetOnTaskCompletionCallbacks().push_back(std::move(on_completion_task));
}

void ThreadSchedulerImpl::DispatchOnTaskCompletionCallbacks() {
  for (auto& closure : GetOnTaskCompletionCallbacks()) {
    std::move(closure).Run();
  }
  GetOnTaskCompletionCallbacks().clear();
}

}  // namespace scheduler
}  // namespace blink
