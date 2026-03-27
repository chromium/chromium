// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CONTEXT_RESULT_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CONTEXT_RESULT_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/ipc/common/context_result.mojom-shared.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT EnumTraits<gpu::mojom::ContextResult,
                                        gpu::ContextResult> {
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
    NOTREACHED();
  }

  static gpu::ContextResult FromMojom(gpu::mojom::ContextResult input) {
    switch (input) {
      case gpu::mojom::ContextResult::Success:
        return gpu::ContextResult::kSuccess;
      case gpu::mojom::ContextResult::TransientFailure:
        return gpu::ContextResult::kTransientFailure;
      case gpu::mojom::ContextResult::FatalFailure:
        return gpu::ContextResult::kFatalFailure;
      case gpu::mojom::ContextResult::SurfaceFailure:
        return gpu::ContextResult::kSurfaceFailure;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CONTEXT_RESULT_MOJOM_TRAITS_H_
