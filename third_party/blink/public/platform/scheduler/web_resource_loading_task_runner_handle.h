// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_RESOURCE_LOADING_TASK_RUNNER_HANDLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_RESOURCE_LOADING_TASK_RUNNER_HANDLE_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/request_priority.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {
namespace scheduler {

// Provides an interface to the loading stack (i.e. WebURLLoader) to post
// resource loading tasks and to notify the appropriate entities when a
// resource's fetch priority changes.
//
// For tasks that are bound to a frame, a handle for a prioritizable task runner
// can be obtained from the frame scheduler's exposed interface
// CreateResourceLoadingTaskRunnerHandle.
//
// For the remaining tasks, the static function CreateUnprioritized (provided in
// this API) can be used to get a handle for an unprioritizeable task runner.
class BLINK_PLATFORM_EXPORT WebResourceLoadingTaskRunnerHandle {
 public:
  static std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
  CreateUnprioritized(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const = 0;

  virtual void DidChangeRequestPriority(net::RequestPriority priority) = 0;

  virtual ~WebResourceLoadingTaskRunnerHandle() = default;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_RESOURCE_LOADING_TASK_RUNNER_HANDLE_H_
