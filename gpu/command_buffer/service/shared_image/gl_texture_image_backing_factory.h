// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {
class SharedImageBacking;
class GpuDriverBugWorkarounds;
struct GpuPreferences;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces GL-texture backed
// SharedImages.
class GPU_GLES2_EXPORT GLTextureImageBackingFactory
    : public GLCommonImageBackingFactory {
 public:
  // The `supports_cpu_upload` parameter controls if this factory accepts
  // `SHARED_IMAGE_USAGE_CPU_UPLOAD`.
  GLTextureImageBackingFactory(const GpuPreferences& gpu_preferences,
                               const GpuDriverBugWorkarounds& workarounds,
                               const gles2::FeatureInfo* feature_info,
                               gl::ProgressReporter* progress_reporter,
                               bool supports_cpu_upload = true);
  ~GLTextureImageBackingFactory() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle) override;
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;

  void EnableSupportForAllMetalUsagesForTesting(bool enable);
  void ForceSetUsingANGLEMetalForTesting(bool value);

 private:
  std::unique_ptr<SharedImageBacking> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      base::span<const uint8_t> pixel_data);

  const bool supports_cpu_upload_;

  // Many shared image usages are disabled on Metal so that they fall back to an
  // IOSurface backing. IOSurface backings are much better suited for cross-API
  // or cross-GPU usages.
  bool support_all_metal_usages_;
  bool emulate_using_angle_metal_for_testing_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_FACTORY_H_
