// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SCHEDULING_PRIORITY_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_SCHEDULING_PRIORITY_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_channel.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct GPU_EXPORT
    EnumTraits<gpu::mojom::SchedulingPriority, gpu::SchedulingPriority> {
  static gpu::mojom::SchedulingPriority ToMojom(
      gpu::SchedulingPriority priority) {
    switch (priority) {
      case gpu::SchedulingPriority::kHigh:
        return gpu::mojom::SchedulingPriority::kHigh;
      case gpu::SchedulingPriority::kNormal:
        return gpu::mojom::SchedulingPriority::kNormal;
      case gpu::SchedulingPriority::kLow:
        return gpu::mojom::SchedulingPriority::kNormal;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  static bool FromMojom(gpu::mojom::SchedulingPriority priority,
                        gpu::SchedulingPriority* out_priority) {
    switch (priority) {
      case gpu::mojom::SchedulingPriority::kHigh:
        *out_priority = gpu::SchedulingPriority::kHigh;
        return true;
      case gpu::mojom::SchedulingPriority::kNormal:
        *out_priority = gpu::SchedulingPriority::kNormal;
        return true;
      case gpu::mojom::SchedulingPriority::kLow:
        *out_priority = gpu::SchedulingPriority::kLow;
        return true;
      default:
        return false;
    }
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_SCHEDULING_PRIORITY_MOJOM_TRAITS_H_
