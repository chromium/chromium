// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_TASK_PRIORITY_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_TASK_PRIORITY_H_

#include <memory>

#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "gpu/command_buffer/service/gpu_command_buffer_service_export.h"

namespace gpu {

enum class GpuTaskPriority : base::sequence_manager::TaskQueue::QueuePriority {
  kControl = 0,
  kHighContinuation = 1,
  kHigh = 2,
  kNormalContinuation = 3,
  kNormal = 4,
  kDefault = kNormal,
  kLowContinuation = 5,
  kLow = 6,
  kMaxValue = kLow,
};

GPU_COMMAND_BUFFER_SERVICE_EXPORT
base::sequence_manager::SequenceManager::PrioritySettings
CreateGpuPrioritySettings();

// Returns SequenceManager settings with the GPU priority levels (see
// CreateGpuPrioritySettings()) for a thread running `message_pump_type`.
GPU_COMMAND_BUFFER_SERVICE_EXPORT
std::unique_ptr<base::sequence_manager::SequenceManagerSettings>
CreateGpuSequenceManagerSettings(
    base::MessagePumpType message_pump_type = base::MessagePumpType::DEFAULT);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_TASK_PRIORITY_H_
