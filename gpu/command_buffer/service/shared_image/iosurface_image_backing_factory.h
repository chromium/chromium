// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/mac/io_surface.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {

class SharedImageBacking;
struct Mailbox;

// IOSurfaceImageBackingFactory is used to create SharedImage backings
// backed by IOSurface instances.
class GPU_GLES2_EXPORT IOSurfaceImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  IOSurfaceImageBackingFactory(GrContextType gr_context_type,
                               int32_t max_texture_size,
                               const gles2::FeatureInfo* feature_info,
                               gl::ProgressReporter* progress_reporter,
                               uint32_t texture_target);
  ~IOSurfaceImageBackingFactory() override;

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
      bool is_thread_safe,
      gfx::BufferUsage buffer_usage) override;
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;

 private:
  std::unique_ptr<SharedImageBacking> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      base::span<const uint8_t> pixel_data);
  std::unique_ptr<SharedImageBacking> CreateSharedImageGMBs(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);

  const GrContextType gr_context_type_;
  const int32_t max_texture_size_;
  const bool angle_texture_usage_;

  const GpuMemoryBufferFormatSet gpu_memory_buffer_formats_;
  base::flat_set<viz::SharedImageFormat> supported_formats_;

  // Used to notify the watchdog before a buffer allocation in case it takes
  // long.
  const raw_ptr<gl::ProgressReporter> progress_reporter_;

  uint32_t texture_target_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_FACTORY_H_
