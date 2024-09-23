// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_FACTORY_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedContextState;

// Implementation of SharedImageBackingFactory that produces NativePixmap
// backed SharedImages.
class GPU_GLES2_EXPORT OzoneImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  explicit OzoneImageBackingFactory(
      scoped_refptr<SharedContextState> shared_context_state,
      const GpuDriverBugWorkarounds& workarounds);

  ~OzoneImageBackingFactory() override;

  // SharedImageBackingFactory implementation
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
  bool CanImportNativePixmapToVulkan();
  bool CanVulkanSynchronizeGpuFence();
  bool CanImportNativePixmapToWebGPU();
  bool CanWebGPUSynchronizeGpuFence();

  const scoped_refptr<SharedContextState> shared_context_state_;
  const GpuDriverBugWorkarounds workarounds_;

  // This method optionally takes BufferUsage as a parameter.
  // TODO(crbug.com/40276844) : BufferUsage will be eventually merged into
  // SharedImageUsage at which point BufferUsage should be removed.
  std::unique_ptr<OzoneImageBacking> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_FACTORY_H_
