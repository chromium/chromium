// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_AHARDWAREBUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_AHARDWAREBUFFER_H_

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {

namespace gles2 {
class FeatureInfo;
}  // namespace gles2

class SharedImageBacking;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces AHardwareBuffer
// backed SharedImages. This is meant to be used on Android only.
class GPU_GLES2_EXPORT SharedImageBackingFactoryAHB
    : public SharedImageBackingFactory {
 public:
  explicit SharedImageBackingFactoryAHB(const gles2::FeatureInfo* feature_info);

  SharedImageBackingFactoryAHB(const SharedImageBackingFactoryAHB&) = delete;
  SharedImageBackingFactoryAHB& operator=(const SharedImageBackingFactoryAHB&) =
      delete;

  ~SharedImageBackingFactoryAHB() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  bool IsSupported(uint32_t usage,
                   viz::ResourceFormat format,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   bool* allow_legacy_mailbox,
                   bool is_pixel_used) override;
  bool IsFormatSupported(viz::ResourceFormat format);

 private:
  bool ValidateUsage(uint32_t usage,
                     const gfx::Size& size,
                     viz::ResourceFormat format) const;

  bool CanImportGpuMemoryBuffer(gfx::GpuMemoryBufferType memory_buffer_type);

  std::unique_ptr<SharedImageBacking> MakeBacking(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data);

  struct FormatInfo {
    FormatInfo();
    ~FormatInfo();

    // Whether this format is supported by AHardwareBuffer.
    bool ahb_supported = false;
    unsigned int ahb_format = 0;

    // Whether this format can be used to create a GL texture from the AHB.
    bool gl_supported = false;

    // GL internal_format/format/type triplet.
    GLuint internal_format = 0;
    GLenum gl_format = 0;
    GLenum gl_type = 0;
  };

  FormatInfo format_info_[viz::RESOURCE_FORMAT_MAX + 1];

  // Used to limit the max size of AHardwareBuffer.
  int32_t max_gl_texture_size_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_AHARDWAREBUFFER_H_
