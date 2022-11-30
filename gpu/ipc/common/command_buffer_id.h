// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_COMMAND_BUFFER_ID_H_
#define GPU_IPC_COMMON_COMMAND_BUFFER_ID_H_

#include "gpu/command_buffer/common/command_buffer_id.h"

namespace gpu {

enum class GpuChannelReservedRoutes : int32_t {
  kSharedImageInterface = 0,
  kImageDecodeAccelerator = 1,
  kMaxValue = kImageDecodeAccelerator,
};

inline CommandBufferId CommandBufferIdFromChannelAndRoute(int channel_id,
                                                          int32_t route_id) {
  return CommandBufferId::FromUnsafeValue(
      (static_cast<uint64_t>(channel_id) << 32) | route_id);
}

inline int ChannelIdFromCommandBufferId(
    gpu::CommandBufferId command_buffer_id) {
  return static_cast<int>(command_buffer_id.GetUnsafeValue() >> 32);
}

inline int32_t RouteIdFromCommandBufferId(
    gpu::CommandBufferId command_buffer_id) {
  return 0xffffffff & command_buffer_id.GetUnsafeValue();
}

}  // namespace gpu

#endif  // GPU_IPC_COMMON_COMMAND_BUFFER_ID_H_
