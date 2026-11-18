// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_task_priority.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace gpu {
namespace {

perfetto::protos::pbzero::SequenceManagerTask::Priority GpuTaskPriorityToProto(
    base::sequence_manager::TaskQueue::QueuePriority priority) {
  switch (static_cast<GpuTaskPriority>(priority)) {
    case GpuTaskPriority::kControl:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          CONTROL_PRIORITY;
    case GpuTaskPriority::kHighContinuation:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          HIGH_PRIORITY_CONTINUATION;
    case GpuTaskPriority::kHigh:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          HIGH_PRIORITY;
    case GpuTaskPriority::kNormalContinuation:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          NORMAL_PRIORITY_CONTINUATION;
    case GpuTaskPriority::kNormal:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          NORMAL_PRIORITY;
    case GpuTaskPriority::kLowContinuation:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          LOW_PRIORITY_CONTINUATION;
    case GpuTaskPriority::kLow:
      return perfetto::protos::pbzero::SequenceManagerTask::Priority::
          LOW_PRIORITY;
  }
  NOTREACHED();
}

}  // namespace

base::sequence_manager::SequenceManager::PrioritySettings
CreateGpuPrioritySettings() {
  using PrioritySettings =
      base::sequence_manager::SequenceManager::PrioritySettings;
  using QueuePriority = base::sequence_manager::TaskQueue::QueuePriority;
  PrioritySettings settings(
      static_cast<QueuePriority>(
          std::to_underlying(GpuTaskPriority::kMaxValue) + 1),
      std::to_underlying(GpuTaskPriority::kDefault));
  settings.SetProtoPriorityConverter(&GpuTaskPriorityToProto);
  return settings;
}

std::unique_ptr<base::sequence_manager::SequenceManagerSettings>
CreateGpuSequenceManagerSettings(base::MessagePumpType message_pump_type) {
  return std::make_unique<base::sequence_manager::SequenceManagerSettings>(
      base::sequence_manager::SequenceManager::Settings::Builder()
          .SetMessagePumpType(message_pump_type)
          .SetPrioritySettings(CreateGpuPrioritySettings())
          .Build());
}

}  // namespace gpu
