// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_SCOPED_SEQUENCE_H_
#define SERVICES_WEBNN_SCOPED_SEQUENCE_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
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
class COMPONENT_EXPORT(WEBNN_SERVICE) ScopedSequence {
 public:
  ScopedSequence(gpu::Scheduler& scheduler,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 gpu::CommandBufferId command_buffer_id);
  ~ScopedSequence();

  // Move and copy not allowed.
  ScopedSequence(ScopedSequence&&) = delete;
  ScopedSequence& operator=(ScopedSequence&&) = delete;

  ScopedSequence(const ScopedSequence&) = delete;
  ScopedSequence& operator=(const ScopedSequence&) = delete;

  gpu::SequenceId sequence_id() const { return sequence_id_; }
  gpu::Scheduler& scheduler() const { return scheduler_.get(); }

  // Exposes a SequencedTaskRunner which can be used to schedule tasks in
  // this sequence. Does not support nested loops or delayed tasks.
  const scoped_refptr<gpu::SchedulerTaskRunner>& scheduler_task_runner() const {
    return scheduler_task_runner_;
  }

 private:
  const raw_ref<gpu::Scheduler> scheduler_;
  const gpu::SequenceId sequence_id_;
  const scoped_refptr<gpu::SchedulerTaskRunner> scheduler_task_runner_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_SCOPED_SEQUENCE_H_
