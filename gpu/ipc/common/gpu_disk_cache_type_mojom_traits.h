// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_DISK_CACHE_TYPE_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_DISK_CACHE_TYPE_MOJOM_TRAITS_H_

#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "gpu/ipc/common/gpu_disk_cache_type.mojom.h"

namespace mojo {

template <>
struct GPU_EXPORT
    EnumTraits<gpu::mojom::GpuDiskCacheType, gpu::GpuDiskCacheType> {
  static gpu::mojom::GpuDiskCacheType ToMojom(gpu::GpuDiskCacheType support);
  static bool FromMojom(gpu::mojom::GpuDiskCacheType input,
                        gpu::GpuDiskCacheType* out);
};

namespace internal {

template <typename MojomDataViewType, typename HandleType>
struct GpuDiskCacheHandleMojomTraitsHelper {
  static bool IsNull(const HandleType& handle) { return handle.is_null(); }

  static void SetToNull(HandleType* handle) { *handle = HandleType(); }

  static bool Read(MojomDataViewType& input, HandleType* output) {
    *output = HandleType(input.value());
    return true;
  }

  static int32_t value(const HandleType& input) { return *input; }
};

}  // namespace internal

template <>
struct StructTraits<gpu::mojom::GpuDiskCacheGlShaderHandleDataView,
                    gpu::GpuDiskCacheGlShaderHandle>
    : public internal::GpuDiskCacheHandleMojomTraitsHelper<
          gpu::mojom::GpuDiskCacheGlShaderHandleDataView,
          gpu::GpuDiskCacheGlShaderHandle> {};

template <>
struct StructTraits<gpu::mojom::GpuDiskCacheDawnWebGPUHandleDataView,
                    gpu::GpuDiskCacheDawnWebGPUHandle>
    : public internal::GpuDiskCacheHandleMojomTraitsHelper<
          gpu::mojom::GpuDiskCacheDawnWebGPUHandleDataView,
          gpu::GpuDiskCacheDawnWebGPUHandle> {};

template <>
struct StructTraits<gpu::mojom::GpuDiskCacheDawnGraphiteHandleDataView,
                    gpu::GpuDiskCacheDawnGraphiteHandle>
    : public internal::GpuDiskCacheHandleMojomTraitsHelper<
          gpu::mojom::GpuDiskCacheDawnGraphiteHandleDataView,
          gpu::GpuDiskCacheDawnGraphiteHandle> {};

template <>
struct GPU_EXPORT UnionTraits<gpu::mojom::GpuDiskCacheHandleDataView,
                              gpu::GpuDiskCacheHandle> {
  static bool IsNull(const gpu::GpuDiskCacheHandle& handle);
  static void SetToNull(gpu::GpuDiskCacheHandle* handle);

  static bool Read(gpu::mojom::GpuDiskCacheHandleDataView input,
                   gpu::GpuDiskCacheHandle* output);
  static gpu::mojom::GpuDiskCacheHandleDataView::Tag GetTag(
      const gpu::GpuDiskCacheHandle& handle);
  static gpu::GpuDiskCacheGlShaderHandle gl_shader_handle(
      const gpu::GpuDiskCacheHandle& handle);
  static gpu::GpuDiskCacheDawnWebGPUHandle dawn_webgpu_handle(
      const gpu::GpuDiskCacheHandle& handle);
  static gpu::GpuDiskCacheDawnGraphiteHandle dawn_graphite_handle(
      const gpu::GpuDiskCacheHandle& handle);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_DISK_CACHE_TYPE_MOJOM_TRAITS_H_
