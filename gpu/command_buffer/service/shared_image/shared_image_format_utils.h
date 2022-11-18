// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FORMAT_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FORMAT_UTILS_H_

#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "ui/gfx/buffer_types.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif

namespace gpu {

// A set of utility functions to get the equivalent GPU API (GL, Vulkan, Dawn,
// Metal) type/format information for a given SharedImageFormat. These functions
// should ideally only be called from the GPU service and viz.
// WARNING: For all following functions, the `format` must be single planar.
// TODO(hitawala): Add multiplanar format support.

// Returns the bits per pixel for given `format`.
GPU_GLES2_EXPORT int BitsPerPixel(viz::SharedImageFormat format);

// Returns BufferFormat for given `format`.
GPU_GLES2_EXPORT gfx::BufferFormat ToBufferFormat(
    viz::SharedImageFormat format);

// Following functions return the appropriate GL type/format for a
// SharedImageFormat.
// Returns true if given `format` is supported by GL.
GPU_GLES2_EXPORT bool GLSupportsFormat(viz::SharedImageFormat format);
// Returns GL data type for given `format`.
GPU_GLES2_EXPORT unsigned int GLDataType(viz::SharedImageFormat format);
// Returns GL data format for given `format`.
GPU_GLES2_EXPORT unsigned int GLDataFormat(viz::SharedImageFormat format);
// Returns the GL format used internally for matching with the texture format
// for a given `format`.
GPU_GLES2_EXPORT unsigned int GLInternalFormat(viz::SharedImageFormat format);
// Returns texture storage format for given `format`.
GPU_GLES2_EXPORT unsigned int TextureStorageFormat(
    viz::SharedImageFormat format,
    bool use_angle_rgbx_format);

// Following functions return the appropriate Vulkan format for a
// SharedImageFormat.
#if BUILDFLAG(ENABLE_VULKAN)
// Returns true if given `format` is supported by Vulkan.
GPU_GLES2_EXPORT bool HasVkFormat(viz::SharedImageFormat format);
// Returns vulkan format for given `format`.
GPU_GLES2_EXPORT VkFormat ToVkFormat(viz::SharedImageFormat format);
#endif

// Following functions return the appropriate WebGPU/Dawn format for a
// SharedImageFormat.
// Returns wgpu::TextureFormat format for given `format`.
GPU_GLES2_EXPORT wgpu::TextureFormat ToDawnFormat(
    viz::SharedImageFormat format);
// Returns WGPUTextureFormat format for given `format`.
GPU_GLES2_EXPORT WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format);

// Following function return the appropriate Metal format for a
// SharedImageFormat.
#if BUILDFLAG(IS_APPLE)
// Returns MtlPixelFormat format for given `format`.
GPU_GLES2_EXPORT unsigned int ToMTLPixelFormat(viz::SharedImageFormat format);
#endif

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FORMAT_UTILS_H_