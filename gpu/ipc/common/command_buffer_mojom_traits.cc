// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/command_buffer_mojom_traits.h"

#include "gpu/ipc/common/gpu_channel.mojom.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::CommandBufferStateDataView,
                  gpu::CommandBuffer::State>::
    Read(gpu::mojom::CommandBufferStateDataView data,
         gpu::CommandBuffer::State* out) {
  if (!data.ReadError(&out->error) ||
      !data.ReadContextLostReason(&out->context_lost_reason)) {
    return false;
  }

  out->get_offset = data.get_offset();
  out->token = data.token();
  out->generation = data.generation();
  out->set_get_buffer_count = data.set_get_buffer_count();
  return true;
}

}  // namespace mojo
