// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/scoped_sequence.h"

#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace webnn {

ScopedSequence::ScopedSequence(
    gpu::Scheduler& scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::CommandBufferId command_buffer_id)
    : scheduler_(scheduler),
      sequence_id_(scheduler_->CreateSequence(
          gpu::SchedulingPriority::kNormal,
          std::move(task_runner),
          gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE,
          command_buffer_id)) {}

ScopedSequence::~ScopedSequence() {
  scheduler_->DestroySequence(sequence_id_);
}

}  // namespace webnn
