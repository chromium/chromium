// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CONTEXT_CREATION_ATTRIBS_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CONTEXT_CREATION_ATTRIBS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/common/gpu_channel.mojom-shared.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

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
      case gpu::CONTEXT_TYPE_OPENGLES3:
        return gpu::mojom::ContextType::kOpenGLES3;
      case gpu::CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
        return gpu::mojom::ContextType::kOpenGLES31ForTesting;
      default:
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
      case gpu::mojom::ContextType::kOpenGLES3:
        *out = gpu::CONTEXT_TYPE_OPENGLES3;
        return true;
      case gpu::mojom::ContextType::kOpenGLES31ForTesting:
        *out = gpu::CONTEXT_TYPE_OPENGLES31_FOR_TESTING;
        return true;
      default:
        return false;
    }
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CONTEXT_CREATION_ATTRIBS_MOJOM_TRAITS_H_
