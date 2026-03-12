// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_AHARDWAREBUFFER_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_AHARDWAREBUFFER_IMAGE_BACKING_FACTORY_H_

#include "base/containers/flat_set.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace viz {
class VulkanContextProvider;
}

namespace gpu {

namespace gles2 {
class FeatureInfo;
}  // namespace gles2

class SharedImageBacking;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces AHardwareBuffer
// backed SharedImages. This is meant to be used on Android only.
class GPU_GLES2_EXPORT AHardwareBufferImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  explicit AHardwareBufferImageBackingFactory(
      const gles2::FeatureInfo* feature_info,
      const GpuPreferences& gpu_preferences,
      const scoped_refptr<viz::VulkanContextProvider>& vulkan_context_provider);

  AHardwareBufferImageBackingFactory(
      const AHardwareBufferImageBackingFactory&) = delete;
  AHardwareBufferImageBackingFactory& operator=(
      const AHardwareBufferImageBackingFactory&) = delete;

  ~AHardwareBufferImageBackingFactory() override;

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
      bool is_thread_safe,
      gfx::GpuMemoryBufferHandle handle) override;
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;
  bool IsFormatSupported(viz::SharedImageFormat format);
  bool IsSupportedForMappableBuffer(SharedImageUsageSet usage,
                                    viz::SharedImageFormat format,
                                    gfx::GpuMemoryBufferType gmb_type);
  static bool CopyNativeBufferToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory);

 private:
  bool ValidateUsage(SharedImageUsageSet usage,
                     const gfx::Size& size,
                     viz::SharedImageFormat format) const;

  bool CanImportGpuMemoryBuffer(gfx::GpuMemoryBufferType memory_buffer_type);

  std::unique_ptr<SharedImageBacking> MakeBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data);

  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider_;

  base::flat_set<viz::SharedImageFormat> supported_gl_formats_;

  // Used to limit the max size of AHardwareBuffer.
  int32_t max_gl_texture_size_ = 0;

  const bool use_passthrough_;
  const GLFormatCaps gl_format_caps_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_AHARDWAREBUFFER_IMAGE_BACKING_FACTORY_H_
