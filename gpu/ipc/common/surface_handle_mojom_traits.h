// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SURFACE_HANDLE_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_SURFACE_HANDLE_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/common/surface_handle.mojom-shared.h"

namespace mojo {

template <>
struct GPU_EXPORT
    StructTraits<gpu::mojom::SurfaceHandleDataView, gpu::SurfaceHandle> {
  static uint64_t surface_handle(const gpu::SurfaceHandle& handle) {
#if BUILDFLAG(IS_WIN)
    return reinterpret_cast<uint64_t>(handle);
#else
    return static_cast<uint64_t>(handle);
#endif
  }

  static bool Read(gpu::mojom::SurfaceHandleDataView data,
                   gpu::SurfaceHandle* out) {
    uint64_t handle = data.surface_handle();
#if BUILDFLAG(IS_WIN)
    *out = reinterpret_cast<gpu::SurfaceHandle>(handle);
#else
    *out = static_cast<gpu::SurfaceHandle>(handle);
#endif
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_SURFACE_HANDLE_MOJOM_TRAITS_H_
