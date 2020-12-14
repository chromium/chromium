// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContextOptions.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned int GLuint;

class GrBackendTexture;

namespace gfx {
class Size;
}  // namespace gfx

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {

#if BUILDFLAG(ENABLE_VULKAN)
class VulkanImage;
#endif

namespace gles2 {
class FeatureInfo;
}  // namespace gles2

class SharedContextState;

// Returns default GrContextOptions.
GPU_GLES2_EXPORT GrContextOptions
GetDefaultGrContextOptions(GrContextType type);

// Returns internal gl format of texture for Skia
GPU_GLES2_EXPORT GLuint
GetGrGLBackendTextureFormat(const gles2::FeatureInfo* feature_info,
                            viz::ResourceFormat resource_format);

// Creates a GrBackendTexture from a service ID. Skia does not take ownership.
// Returns true on success.
GPU_GLES2_EXPORT bool GetGrBackendTexture(
    const gles2::FeatureInfo* feature_info,
    GLenum target,
    const gfx::Size& size,
    GLuint service_id,
    viz::ResourceFormat resource_format,
    GrBackendTexture* gr_texture);

// Adds a task to be executed when the flush in |flush_info| is complete.
GPU_GLES2_EXPORT void AddCleanupTaskForSkiaFlush(base::OnceClosure task,
                                                 GrFlushInfo* flush_info);

// Helper which associates cleanup callbacks with a Skia GrFlushInfo's callback.
// Is a no-op if |context_provider| is null.
GPU_GLES2_EXPORT void AddVulkanCleanupTaskForSkiaFlush(
    viz::VulkanContextProvider* context_provider,
    GrFlushInfo* flush_info);

GPU_GLES2_EXPORT void DeleteGrBackendTexture(
    SharedContextState* context_state,
    GrBackendTexture* backend_textures);

GPU_GLES2_EXPORT void DeleteSkImage(SharedContextState* context_state,
                                    sk_sp<SkImage> sk_image);

GPU_GLES2_EXPORT void DeleteSkSurface(SharedContextState* context_state,
                                      sk_sp<SkSurface> sk_surface);

#if BUILDFLAG(ENABLE_VULKAN)
GPU_GLES2_EXPORT GrVkImageInfo CreateGrVkImageInfo(VulkanImage* image);

GPU_GLES2_EXPORT GrVkYcbcrConversionInfo CreateGrVkYcbcrConversionInfo(
    VkPhysicalDevice physical_device,
    VkImageTiling tiling,
    const base::Optional<VulkanYCbCrInfo>& ycbcr_info);
#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_
