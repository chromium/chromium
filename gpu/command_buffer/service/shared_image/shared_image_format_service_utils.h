// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FORMAT_SERVICE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FORMAT_SERVICE_UTILS_H_

#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "ui/gfx/buffer_types.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif

namespace skgpu::graphite {
class TextureInfo;
}

namespace gpu {

// GLFormatDesc is a struct containing the GL data type, data format, internal
// format used by the image, internal format used for storage and GL target.
struct GLFormatDesc {
  GLenum data_type = 0;
  GLenum data_format = 0;
  GLenum image_internal_format = 0;
  GLenum storage_internal_format = 0;
  GLenum target = 0;
};

// A set of utility functions to get the equivalent GPU API (GL, Vulkan, Dawn,
// Metal) type/format information for a given SharedImageFormat. These functions
// should ideally only be called from the GPU service and viz.

// Only use this function with single planar formats.
// Returns the bits per pixel for given `format`.
GPU_GLES2_EXPORT int BitsPerPixel(viz::SharedImageFormat format);

// BufferFormat is being transitioned out of SharedImage code (to use
// SharedImageFormat instead). Refrain from using this function or preferably
// use with single planar SharedImageFormats. Returns BufferFormat for given
// `format`.
GPU_GLES2_EXPORT gfx::BufferFormat ToBufferFormat(
    viz::SharedImageFormat format);

// Returns SkYUVAInfo::PlaneConfig equivalent of
// SharedImageFormat::PlaneConfig.
GPU_GLES2_EXPORT SkYUVAInfo::PlaneConfig ToSkYUVAPlaneConfig(
    viz::SharedImageFormat format);

// Returns SkYUVAInfo::Subsampling equivalent of
// SharedImageFormat::Subsampling.
GPU_GLES2_EXPORT SkYUVAInfo::Subsampling ToSkYUVASubsampling(
    viz::SharedImageFormat format);

// Following functions return the appropriate GL type/format for a
// SharedImageFormat.
// Return the GLFormatDesc when using external sampler for a given `format`.
GPU_GLES2_EXPORT GLFormatDesc
ToGLFormatDescExternalSampler(viz::SharedImageFormat format);
// Return the GLFormatDesc for a given `format`.
GPU_GLES2_EXPORT GLFormatDesc ToGLFormatDesc(viz::SharedImageFormat format,
                                             int plane_index,
                                             bool use_angle_rgbx_format);
// Returns GL data type for given `format`.
GPU_GLES2_EXPORT GLenum GLDataType(viz::SharedImageFormat format);
// Returns GL data format for given `format`.
GPU_GLES2_EXPORT GLenum GLDataFormat(viz::SharedImageFormat format,
                                     int plane_index = 0);
// Returns the GL format used internally for matching with the texture format
// for a given `format`.
GPU_GLES2_EXPORT GLenum GLInternalFormat(viz::SharedImageFormat format,
                                         int plane_index = 0);
// Returns texture storage format for given `format`.
GPU_GLES2_EXPORT GLenum TextureStorageFormat(viz::SharedImageFormat format,
                                             bool use_angle_rgbx_format,
                                             int plane_index = 0);

// Following functions return the appropriate Vulkan format for a
// SharedImageFormat.
#if BUILDFLAG(ENABLE_VULKAN)
// Returns true if given `format` is supported by Vulkan.
GPU_GLES2_EXPORT bool HasVkFormat(viz::SharedImageFormat format);
// Returns vulkan format for given `format`.
GPU_GLES2_EXPORT VkFormat ToVkFormat(viz::SharedImageFormat format,
                                     int plane_index = 0);
#endif

// Following functions return the appropriate WebGPU/Dawn format for a
// SharedImageFormat.
// TODO (hitawala): Add support for multiplanar formats.
// Returns wgpu::TextureFormat format for given `format`.
GPU_GLES2_EXPORT wgpu::TextureFormat ToDawnFormat(
    viz::SharedImageFormat format);
// Same as ToDawnFormat, except it casts from wgpu::TextureFormat to
// WGPUTextureFormat instead.
GPU_GLES2_EXPORT WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format);

// Following function return the appropriate Metal format for a
// SharedImageFormat.
#if BUILDFLAG(IS_APPLE)
// Returns MtlPixelFormat format for given `format`.
GPU_GLES2_EXPORT unsigned int ToMTLPixelFormat(viz::SharedImageFormat format,
                                               int plane_index = 0);
#endif

// Returns the graphite::TextureInfo for a given `format`.
GPU_GLES2_EXPORT skgpu::graphite::TextureInfo GetGraphiteTextureInfo(
    GrContextType gr_context_type,
    viz::SharedImageFormat format,
    int plane_index = 0,
    bool mipmapped = false,
    bool root_surface = false);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FORMAT_SERVICE_UTILS_H_
