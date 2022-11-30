// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_COMMAND_BUFFER_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_COMMAND_BUFFER_MOJOM_TRAITS_H_

#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_channel.mojom-shared.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"

namespace mojo {

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::CommandBufferStateDataView,
                               gpu::CommandBuffer::State> {
 public:
  static int32_t get_offset(const gpu::CommandBuffer::State& state) {
    return state.get_offset;
  }

  static int32_t token(const gpu::CommandBuffer::State& state) {
    return state.token;
  }

  static uint64_t release_count(const gpu::CommandBuffer::State& state) {
    return state.release_count;
  }

  static gpu::error::Error error(const gpu::CommandBuffer::State& state) {
    return state.error;
  }

  static gpu::error::ContextLostReason context_lost_reason(
      const gpu::CommandBuffer::State& state) {
    return state.context_lost_reason;
  }

  static uint32_t generation(const gpu::CommandBuffer::State& state) {
    return state.generation;
  }

  static uint32_t set_get_buffer_count(const gpu::CommandBuffer::State& state) {
    return state.set_get_buffer_count;
  }

  static bool Read(gpu::mojom::CommandBufferStateDataView data,
                   gpu::CommandBuffer::State* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_COMMAND_BUFFER_MOJOM_TRAITS_H_
