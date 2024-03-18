// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_DISK_CACHE_TYPE_H_
#define GPU_IPC_COMMON_GPU_DISK_CACHE_TYPE_H_

#include <array>
#include <limits>
#include <ostream>

#include "base/files/file_path.h"
#include "base/types/id_type.h"
#include "gpu/gpu_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace gpu {

//
// GPU disk cache types and utilities to get the appropriate subdirs.
//

// The GPU process can uses multiple different caches for different purposes. In
// order to differentiate between the different caches (which generally map to
// different physical disk paths), and add type safety when using them, these
// types explicitly define what a cache for the GPU process will be used for.
enum class GpuDiskCacheType {
  kGlShaders,
  kDawnWebGPU,
  kDawnGraphite,
};

// Stream operator implemented for GpuDiskCacheType for debugging.
GPU_EXPORT std::ostream& operator<<(std::ostream& s,
                                    const GpuDiskCacheType& type);

static constexpr std::array<GpuDiskCacheType, 3> kGpuDiskCacheTypes = {
    GpuDiskCacheType::kGlShaders,
    GpuDiskCacheType::kDawnWebGPU,
    GpuDiskCacheType::kDawnGraphite,
};
GPU_EXPORT base::FilePath::StringType GetGpuDiskCacheSubdir(
    GpuDiskCacheType type);

//
// Typed GPU disk cache handles (1:1 with GpuDiskCacheType)
//   (Correlates physical paths with cross-process in-memory cache mirrors.)
//   Note that the handles currently do not need to be unguessable because they
//   are only used between the browser and the GPU process.
//

// Shader handles allow negative values for representing reserved handles.
using GpuDiskCacheGlShaderHandle =
    base::IdType<class GpuDiskCacheGlShader,
                 int32_t,
                 std::numeric_limits<int32_t>::min(),
                 1>;

// Dawn WebGPU cache handles (for the most part, these should be 1:1 per
// profile).
using GpuDiskCacheDawnWebGPUHandle =
    base::IdType<class GpuDiskCacheDawnWebGPU,
                 int32_t,
                 std::numeric_limits<int32_t>::min(),
                 1>;

// Dawn Graphite cache handles (for the most part, these should be 1:1 per
// profile).
using GpuDiskCacheDawnGraphiteHandle =
    base::IdType<class GpuDiskCacheDawnGraphite,
                 int32_t,
                 std::numeric_limits<int32_t>::min(),
                 1>;
//
// Variant handle that encompasses all possible handles, and utilities.
//
using GpuDiskCacheHandle = absl::variant<absl::monostate,
                                         GpuDiskCacheGlShaderHandle,
                                         GpuDiskCacheDawnWebGPUHandle,
                                         GpuDiskCacheDawnGraphiteHandle>;
GPU_EXPORT GpuDiskCacheType GetHandleType(const GpuDiskCacheHandle& handle);
GPU_EXPORT int32_t GetHandleValue(const GpuDiskCacheHandle& handle);

// Stream operator implemented for GpuDiskCacheHandle for debugging.
GPU_EXPORT std::ostream& operator<<(std::ostream& s,
                                    const GpuDiskCacheHandle& handle);

//
// Reserved cache handles that are specifically used for static caches.
//

// The handle used by the display compositor running in the GPU process.
constexpr GpuDiskCacheGlShaderHandle kDisplayCompositorGpuDiskCacheHandle(-1);

// The handle used for storing shaders created by skia in the GPU process.
constexpr GpuDiskCacheGlShaderHandle kGrShaderGpuDiskCacheHandle(-2);

// The handle used by GraphiteDawn running in the GPU process. It is used by
// RasterDecoder and SkiaRenderer.
constexpr GpuDiskCacheDawnGraphiteHandle kGraphiteDawnGpuDiskCacheHandle(-3);

GPU_EXPORT bool IsReservedGpuDiskCacheHandle(const GpuDiskCacheHandle& handle);

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_DISK_CACHE_TYPE_H_
