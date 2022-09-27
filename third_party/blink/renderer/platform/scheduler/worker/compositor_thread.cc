// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

CompositorThread::CompositorThread(const ThreadCreationParams& params)
    : NonMainThreadImpl(params) {}

CompositorThread::~CompositorThread() = default;

std::unique_ptr<NonMainThreadSchedulerBase>
CompositorThread::CreateNonMainThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager) {
  return std::make_unique<CompositorThreadSchedulerImpl>(sequence_manager);
}

}  // namespace scheduler
}  // namespace blink
