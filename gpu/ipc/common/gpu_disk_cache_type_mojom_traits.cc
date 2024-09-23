// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_disk_cache_type_mojom_traits.h"

#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

// static
gpu::mojom::GpuDiskCacheType
EnumTraits<gpu::mojom::GpuDiskCacheType, gpu::GpuDiskCacheType>::ToMojom(
    gpu::GpuDiskCacheType gpu_disk_cache_type) {
  switch (gpu_disk_cache_type) {
    case gpu::GpuDiskCacheType::kGlShaders:
      return gpu::mojom::GpuDiskCacheType::kGlShaders;
    case gpu::GpuDiskCacheType::kDawnWebGPU:
      return gpu::mojom::GpuDiskCacheType::kDawnWebGPU;
    case gpu::GpuDiskCacheType::kDawnGraphite:
      return gpu::mojom::GpuDiskCacheType::kDawnGraphite;
  }

  NOTREACHED_IN_MIGRATION()
      << "Invalid gpu::GpuDiskCacheType: " << gpu_disk_cache_type;
  return gpu::mojom::GpuDiskCacheType::kGlShaders;
}

// static
bool EnumTraits<gpu::mojom::GpuDiskCacheType, gpu::GpuDiskCacheType>::FromMojom(
    gpu::mojom::GpuDiskCacheType input,
    gpu::GpuDiskCacheType* out) {
  switch (input) {
    case gpu::mojom::GpuDiskCacheType::kGlShaders:
      *out = gpu::GpuDiskCacheType::kGlShaders;
      return true;
    case gpu::mojom::GpuDiskCacheType::kDawnWebGPU:
      *out = gpu::GpuDiskCacheType::kDawnWebGPU;
      return true;
    case gpu::mojom::GpuDiskCacheType::kDawnGraphite:
      *out = gpu::GpuDiskCacheType::kDawnGraphite;
      return true;
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid gpu::mojom::GpuDiskCacheType: " << input;
  return false;
}

// static
bool UnionTraits<gpu::mojom::GpuDiskCacheHandleDataView,
                 gpu::GpuDiskCacheHandle>::IsNull(const gpu::GpuDiskCacheHandle&
                                                      handle) {
  return absl::holds_alternative<absl::monostate>(handle);
}

void UnionTraits<gpu::mojom::GpuDiskCacheHandleDataView,
                 gpu::GpuDiskCacheHandle>::SetToNull(gpu::GpuDiskCacheHandle*
                                                         handle) {
  *handle = gpu::GpuDiskCacheHandle();
}

// static
bool UnionTraits<
    gpu::mojom::GpuDiskCacheHandleDataView,
    gpu::GpuDiskCacheHandle>::Read(gpu::mojom::GpuDiskCacheHandleDataView input,
                                   gpu::GpuDiskCacheHandle* output) {
  using Tag = gpu::mojom::GpuDiskCacheHandleDataView::Tag;
  switch (input.tag()) {
    case Tag::kGlShaderHandle: {
      gpu::GpuDiskCacheGlShaderHandle handle;
      bool ret = input.ReadGlShaderHandle(&handle);
      *output = handle;
      return ret;
    }
    case Tag::kDawnWebgpuHandle: {
      gpu::GpuDiskCacheDawnWebGPUHandle handle;
      bool ret = input.ReadDawnWebgpuHandle(&handle);
      *output = handle;
      return ret;
    }
    case Tag::kDawnGraphiteHandle: {
      gpu::GpuDiskCacheDawnGraphiteHandle handle;
      bool ret = input.ReadDawnGraphiteHandle(&handle);
      *output = handle;
      return ret;
    }
  }
  return false;
}

// static
gpu::mojom::GpuDiskCacheHandleDataView::Tag UnionTraits<
    gpu::mojom::GpuDiskCacheHandleDataView,
    gpu::GpuDiskCacheHandle>::GetTag(const gpu::GpuDiskCacheHandle& handle) {
  using Tag = gpu::mojom::GpuDiskCacheHandleDataView::Tag;
  if (absl::holds_alternative<gpu::GpuDiskCacheGlShaderHandle>(handle))
    return Tag::kGlShaderHandle;
  if (absl::holds_alternative<gpu::GpuDiskCacheDawnWebGPUHandle>(handle)) {
    return Tag::kDawnWebgpuHandle;
  }
  DCHECK(absl::holds_alternative<gpu::GpuDiskCacheDawnGraphiteHandle>(handle));
  return Tag::kDawnGraphiteHandle;
}

// static
gpu::GpuDiskCacheGlShaderHandle
UnionTraits<gpu::mojom::GpuDiskCacheHandleDataView, gpu::GpuDiskCacheHandle>::
    gl_shader_handle(const gpu::GpuDiskCacheHandle& handle) {
  return absl::get<gpu::GpuDiskCacheGlShaderHandle>(handle);
}

// static
gpu::GpuDiskCacheDawnWebGPUHandle
UnionTraits<gpu::mojom::GpuDiskCacheHandleDataView, gpu::GpuDiskCacheHandle>::
    dawn_webgpu_handle(const gpu::GpuDiskCacheHandle& handle) {
  return absl::get<gpu::GpuDiskCacheDawnWebGPUHandle>(handle);
}

// static
gpu::GpuDiskCacheDawnGraphiteHandle
UnionTraits<gpu::mojom::GpuDiskCacheHandleDataView, gpu::GpuDiskCacheHandle>::
    dawn_graphite_handle(const gpu::GpuDiskCacheHandle& handle) {
  return absl::get<gpu::GpuDiskCacheDawnGraphiteHandle>(handle);
}

}  // namespace mojo
