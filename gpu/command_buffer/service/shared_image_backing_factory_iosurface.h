// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_IOSURFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_IOSURFACE_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct Mailbox;
class SharedImageBacking;

// Implementation of SharedImageBackingFactory that produce IOSurface backed
// SharedImages. This is meant to be used on macOS only.
class GPU_GLES2_EXPORT SharedImageBackingFactoryIOSurface
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryIOSurface(const GpuDriverBugWorkarounds& workarounds,
                                     const GpuFeatureInfo& gpu_feature_info,
                                     bool use_gl);
  ~SharedImageBackingFactoryIOSurface() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override;
  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override;

 private:
  void CollectGLFormatInfo(const GpuDriverBugWorkarounds& workarounds,
                           const GpuFeatureInfo& gpu_feature_info);
  bool format_supported_by_gl_[viz::RESOURCE_FORMAT_MAX + 1];
  bool use_gl_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingFactoryIOSurface);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_IOSURFACE_H_
