// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CONTEXT_RESULT_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CONTEXT_RESULT_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/context_result.mojom-shared.h"

namespace mojo {

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::ContextResult, gpu::ContextResult> {
  static gpu::mojom::ContextResult ToMojom(gpu::ContextResult context_result) {
    switch (context_result) {
      case gpu::ContextResult::kSuccess:
        return gpu::mojom::ContextResult::Success;
      case gpu::ContextResult::kTransientFailure:
        return gpu::mojom::ContextResult::TransientFailure;
      case gpu::ContextResult::kFatalFailure:
        return gpu::mojom::ContextResult::FatalFailure;
      case gpu::ContextResult::kSurfaceFailure:
        return gpu::mojom::ContextResult::SurfaceFailure;
    }
    NOTREACHED_IN_MIGRATION();
    return gpu::mojom::ContextResult::FatalFailure;
  }

  static bool FromMojom(gpu::mojom::ContextResult input,
                        gpu::ContextResult* out) {
    switch (input) {
      case gpu::mojom::ContextResult::Success:
        *out = gpu::ContextResult::kSuccess;
        return true;
      case gpu::mojom::ContextResult::TransientFailure:
        *out = gpu::ContextResult::kTransientFailure;
        return true;
      case gpu::mojom::ContextResult::FatalFailure:
        *out = gpu::ContextResult::kFatalFailure;
        return true;
      case gpu::mojom::ContextResult::SurfaceFailure:
        *out = gpu::ContextResult::kSurfaceFailure;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CONTEXT_RESULT_MOJOM_TRAITS_H_
