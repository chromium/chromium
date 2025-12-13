// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_SCOPED_GPU_SEQUENCE_H_
#define SERVICES_WEBNN_SCOPED_GPU_SEQUENCE_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
class Scheduler;
class SchedulerTaskRunner;
}  // namespace gpu

namespace webnn {

// Ensures gpu::Sequence is destroyed when the WebNNContext is lost or
// destroyed. The sequence must be destroyed even if context creation fails,
// since gpu::Scheduler will DCHECK if any sequences remain alive at
// destruction.
class COMPONENT_EXPORT(WEBNN_SERVICE) ScopedGpuSequence {
 public:
  ScopedGpuSequence(gpu::Scheduler& scheduler,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    gpu::CommandBufferId command_buffer_id,
                    gpu::CommandBufferNamespace namespace_id);

  ~ScopedGpuSequence();

  // Move and copy not allowed.
  ScopedGpuSequence(ScopedGpuSequence&&) = delete;
  ScopedGpuSequence& operator=(ScopedGpuSequence&&) = delete;

  ScopedGpuSequence(const ScopedGpuSequence&) = delete;
  ScopedGpuSequence& operator=(const ScopedGpuSequence&) = delete;

  // Exposes a SequencedTaskRunner which can be used to schedule tasks in
  // this sequence. Does not support nested loops or delayed tasks.
  const scoped_refptr<gpu::SchedulerTaskRunner>& scheduler_task_runner() const {
    return scheduler_task_runner_;
  }

  // Waits for the given SyncToken to release before executing scheduled tasks.
  void WaitSyncToken(const gpu::SyncToken& fence);

  // Generates a verified SyncToken that will be released once scheduled tasks
  // complete execution.
  gpu::SyncToken GenVerifiedSyncToken();

 private:
  // Marks the completion of the last schedule task in this sequence.
  // Used to generate a SyncToken which can be waited on by another sequence.
  uint64_t last_sync_token_release_id_ = 0;

  const raw_ref<gpu::Scheduler> scheduler_;
  const gpu::CommandBufferId command_buffer_id_;
  const gpu::CommandBufferNamespace namespace_id_;
  const gpu::SequenceId sequence_id_;
  const scoped_refptr<gpu::SchedulerTaskRunner> scheduler_task_runner_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_SCOPED_GPU_SEQUENCE_H_
