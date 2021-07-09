// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_EGL_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_EGL_H_

#include <memory>

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
class SharedImageBatchAccessManager;
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct GpuPreferences;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces EGL backed
// SharedImages.
class GPU_GLES2_EXPORT SharedImageBackingFactoryEGL
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryEGL(
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& workarounds,
      const GpuFeatureInfo& gpu_feature_info,
      SharedImageBatchAccessManager* batch_access_manager);
  ~SharedImageBackingFactoryEGL() override;

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

 private:
  std::unique_ptr<SharedImageBacking> MakeEglImageBacking(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage);

  struct FormatInfo {
    // Whether this format is supported.
    bool enabled = false;

    // Whether the texture is a compressed type.
    bool is_compressed = false;

    GLenum gl_format = 0;
    GLenum gl_type = 0;
  };

  // Whether we're using the passthrough command decoder and should generate
  // passthrough textures.
  const bool use_passthrough_;

  FormatInfo format_info_[viz::RESOURCE_FORMAT_MAX + 1];
  int32_t max_texture_size_ = 0;
  GpuDriverBugWorkarounds workarounds_;

  SharedImageBatchAccessManager* batch_access_manager_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_EGL_H_
