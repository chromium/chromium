// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CONSTANTS_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CONSTANTS_MOJOM_TRAITS_H_

#include "gpu/command_buffer/common/constants.h"
#include "gpu/ipc/common/constants.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<gpu::mojom::Error, gpu::error::Error> {
  static gpu::mojom::Error ToMojom(gpu::error::Error error);
  static bool FromMojom(gpu::mojom::Error input, gpu::error::Error* out);
};

template <>
struct EnumTraits<gpu::mojom::ContextLostReason,
                  gpu::error::ContextLostReason> {
  static gpu::mojom::ContextLostReason ToMojom(
      gpu::error::ContextLostReason reason);
  static bool FromMojom(gpu::mojom::ContextLostReason input,
                        gpu::error::ContextLostReason* out);
};

template <>
struct EnumTraits<gpu::mojom::CommandBufferNamespace,
                  gpu::CommandBufferNamespace> {
  static gpu::mojom::CommandBufferNamespace ToMojom(
      gpu::CommandBufferNamespace namespace_id);
  static bool FromMojom(gpu::mojom::CommandBufferNamespace input,
                        gpu::CommandBufferNamespace* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CONSTANTS_MOJOM_TRAITS_H_
