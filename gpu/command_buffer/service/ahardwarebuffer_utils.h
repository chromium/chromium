// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/scoped_egl_image.h"

extern "C" typedef struct AHardwareBuffer AHardwareBuffer;

typedef unsigned int GLenum;

namespace base {
namespace android {
class ScopedHardwareBufferHandle;
}  // namespace android
}  // namespace base

namespace gfx {
class ColorSpace;
class Rect;
class Size;
}  // namespace gfx

namespace gpu {
class SharedContextState;
class VulkanImage;

namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

// TODO(vikassoni): In future we will need to expose the set of formats and
// constraints (e.g. max size) to the clients somehow that are available for
// certain combinations of SharedImageUsage flags (e.g. when Vulkan is on,
// SHARED_IMAGE_USAGE_GLES2 + SHARED_IMAGE_USAGE_DISPLAY_READ implies AHB, so
// those restrictions apply, but that's decided on the service side). For now
// getting supported format is a static mechanism like this. We probably need
// something like gpu::Capabilities.texture_target_exception_list.

// Returns whether the format is supported by AHardwareBuffer.
bool GPU_GLES2_EXPORT
AHardwareBufferSupportedFormat(viz::ResourceFormat format);

// Returns the corresponding AHardwareBuffer format.
unsigned int GPU_GLES2_EXPORT AHardwareBufferFormat(viz::ResourceFormat format);

// Generates a gles2 texture from AHB. This method must be called with a current
// GLContext which will be used to create the Texture. This method adds a
// lightweight ref on the Texture which the caller is responsible for releasing.
gles2::Texture* GenGLTexture(AHardwareBuffer* buffer,
                             GLenum target,
                             const gfx::ColorSpace& color_space,
                             const gfx::Size& size,
                             const size_t estimated_size,
                             const gfx::Rect& cleared_rect);

// Generates a passthrough texture from AHB. This method must be called with a
// current GLContext which will be used to create the Texture.
scoped_refptr<gles2::TexturePassthrough> GenGLTexturePassthrough(
    AHardwareBuffer* buffer,
    GLenum target,
    const gfx::ColorSpace& color_space,
    const gfx::Size& size,
    const size_t estimated_size,
    const gfx::Rect& cleared_rect);

// Create a vulkan image from the AHB handle.
std::unique_ptr<VulkanImage> CreateVkImageFromAhbHandle(
    base::android::ScopedHardwareBufferHandle ahb_handle,
    SharedContextState* context_state,
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    uint32_t queue_family_index);

// Creates an EGLImage from |buffer|, setting EGL_IMAGE_PRESERVED_KHR to false.
GPU_GLES2_EXPORT ui::ScopedEGLImage CreateEGLImageFromAHardwareBuffer(
    AHardwareBuffer* buffer);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
