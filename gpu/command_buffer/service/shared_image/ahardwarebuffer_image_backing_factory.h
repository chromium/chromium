// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_AHARDWAREBUFFER_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_AHARDWAREBUFFER_IMAGE_BACKING_FACTORY_H_

#include "base/containers/flat_map.h"
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
      const GpuPreferences& gpu_preferences);

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

 private:
  struct FormatInfo {
    FormatInfo();
    ~FormatInfo();

    unsigned int ahb_format = 0;

    // Whether this format can be used to create a GL texture from the AHB.
    bool gl_supported = false;

    // GL internal_format/format/type triplet.
    GLuint internal_format = 0;
    GLenum gl_format = 0;
    GLenum gl_type = 0;
  };

  // Constructs and returns a FormatInfo corresponding to `format`, which must
  // be a supported format.
  static FormatInfo FormatInfoForSupportedFormat(
      viz::SharedImageFormat format,
      const gles2::Validators* validators,
      const GLFormatCaps& gl_format_caps);

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

  const FormatInfo& GetFormatInfo(viz::SharedImageFormat format) const {
    auto iter = format_infos_.find(format);
    CHECK(iter != format_infos_.end());
    return iter->second;
  }

  base::flat_map<viz::SharedImageFormat, FormatInfo> format_infos_;

  // Used to limit the max size of AHardwareBuffer.
  int32_t max_gl_texture_size_ = 0;

  const bool use_passthrough_;
  const GLFormatCaps gl_format_caps_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_AHARDWAREBUFFER_IMAGE_BACKING_FACTORY_H_
