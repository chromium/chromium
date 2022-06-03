// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PROFILING_UTILS_H_
#define CONTENT_PUBLIC_COMMON_PROFILING_UTILS_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/content_export.h"

namespace content {

// Open the file that should be used by a child process to save its profiling
// data.
CONTENT_EXPORT base::File OpenProfilingFile();

// Serves WaitableEvent that should be used by the child processes to signal
// that they have finished dumping the profiling data.
class CONTENT_EXPORT WaitForProcessesToDumpProfilingInfo {
 public:
  WaitForProcessesToDumpProfilingInfo();
  ~WaitForProcessesToDumpProfilingInfo();
  WaitForProcessesToDumpProfilingInfo(
      const WaitForProcessesToDumpProfilingInfo& other) = delete;
  WaitForProcessesToDumpProfilingInfo& operator=(
      const WaitForProcessesToDumpProfilingInfo&) = delete;

  // Wait for all the events served by |GetNewWaitableEvent| to signal.
  void WaitForAll();

  // Return a new waitable event. Calling |WaitForAll| will wait for this event
  // to be signaled.
  // The returned WaitableEvent is owned by this
  // WaitForProcessesToDumpProfilingInfo instance.
  base::WaitableEvent* GetNewWaitableEvent();

 private:
  // Implementation of WaitForAll that will run on the thread pool. This will
  // run |quit_closure| once it's done waiting.
  void WaitForAllOnThreadPool(base::OnceClosure quit_closure);

  std::vector<std::unique_ptr<base::WaitableEvent>> events_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PROFILING_UTILS_H_
