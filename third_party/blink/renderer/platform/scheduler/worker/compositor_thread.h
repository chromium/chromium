// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_THREAD_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT CompositorThread : public WorkerThread {
 public:
  explicit CompositorThread(const ThreadCreationParams& params);
  ~CompositorThread() override;

 private:
  std::unique_ptr<NonMainThreadSchedulerImpl> CreateNonMainThreadScheduler()
      override;

  DISALLOW_COPY_AND_ASSIGN(CompositorThread);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_THREAD_H_
