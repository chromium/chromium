// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CONTEXT_TYPE_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CONTEXT_TYPE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/common/context_type.mojom-shared.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT EnumTraits<gpu::mojom::ContextType,
                                        gpu::ContextType> {
  static gpu::mojom::ContextType ToMojom(gpu::ContextType type) {
    switch (type) {
      case gpu::CONTEXT_TYPE_WEBGL1:
        return gpu::mojom::ContextType::kWebGL1;
      case gpu::CONTEXT_TYPE_WEBGL2:
        return gpu::mojom::ContextType::kWebGL2;
      case gpu::CONTEXT_TYPE_OPENGLES2:
        return gpu::mojom::ContextType::kOpenGLES2;
      // OPENGLES3  should not be serialized as there are no production usages
      // and it is planned to be removed.
      case gpu::CONTEXT_TYPE_OPENGLES3:
        NOTREACHED();
    }
  }

  static bool FromMojom(gpu::mojom::ContextType type, gpu::ContextType* out) {
    switch (type) {
      case gpu::mojom::ContextType::kWebGL1:
        *out = gpu::CONTEXT_TYPE_WEBGL1;
        return true;
      case gpu::mojom::ContextType::kWebGL2:
        *out = gpu::CONTEXT_TYPE_WEBGL2;
        return true;
      case gpu::mojom::ContextType::kOpenGLES2:
        *out = gpu::CONTEXT_TYPE_OPENGLES2;
        return true;
      // OPENGLES3 should not be serialized as there are no production usages
      // and it is planned to be removed.
      case gpu::mojom::ContextType::kOpenGLES3:
        return false;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CONTEXT_TYPE_MOJOM_TRAITS_H_
