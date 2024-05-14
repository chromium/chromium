// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PRIVILEGED_CPP_CONTEXT_LOST_REASON_TRAITS_H_
#define SERVICES_VIZ_PRIVILEGED_CPP_CONTEXT_LOST_REASON_TRAITS_H_

#include "base/notreached.h"
#include "gpu/command_buffer/common/constants.h"
#include "services/viz/privileged/mojom/gl/context_lost_reason.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::ContextLostReason,
                  gpu::error::ContextLostReason> {
  static viz::mojom::ContextLostReason ToMojom(
      gpu::error::ContextLostReason reason) {
    switch (reason) {
      case gpu::error::kGuilty:
        return viz::mojom::ContextLostReason::GUILTY;
      case gpu::error::kInnocent:
        return viz::mojom::ContextLostReason::INNOCENT;
      case gpu::error::kUnknown:
        return viz::mojom::ContextLostReason::UNKNOWN;
      case gpu::error::kOutOfMemory:
        return viz::mojom::ContextLostReason::OUT_OF_MEMORY;
      case gpu::error::kMakeCurrentFailed:
        return viz::mojom::ContextLostReason::MAKE_CURRENT_FAILED;
      case gpu::error::kGpuChannelLost:
        return viz::mojom::ContextLostReason::GPU_CHANNEL_LOST;
      case gpu::error::kInvalidGpuMessage:
        return viz::mojom::ContextLostReason::INVALID_GPU_MESSAGE;
    }
    NOTREACHED_IN_MIGRATION();
    return viz::mojom::ContextLostReason::UNKNOWN;
  }

  static bool FromMojom(viz::mojom::ContextLostReason reason,
                        gpu::error::ContextLostReason* out) {
    switch (reason) {
      case viz::mojom::ContextLostReason::GUILTY:
        *out = gpu::error::kGuilty;
        return true;
      case viz::mojom::ContextLostReason::INNOCENT:
        *out = gpu::error::kInnocent;
        return true;
      case viz::mojom::ContextLostReason::UNKNOWN:
        *out = gpu::error::kUnknown;
        return true;
      case viz::mojom::ContextLostReason::OUT_OF_MEMORY:
        *out = gpu::error::kOutOfMemory;
        return true;
      case viz::mojom::ContextLostReason::MAKE_CURRENT_FAILED:
        *out = gpu::error::kMakeCurrentFailed;
        return true;
      case viz::mojom::ContextLostReason::GPU_CHANNEL_LOST:
        *out = gpu::error::kGpuChannelLost;
        return true;
      case viz::mojom::ContextLostReason::INVALID_GPU_MESSAGE:
        *out = gpu::error::kInvalidGpuMessage;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PRIVILEGED_CPP_CONTEXT_LOST_REASON_TRAITS_H_
