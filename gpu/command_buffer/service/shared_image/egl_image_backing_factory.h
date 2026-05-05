// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EGL_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EGL_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
class GpuDriverBugWorkarounds;
struct GpuPreferences;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces EGL backed
// SharedImages.
class GPU_GLES2_EXPORT EGLImageBackingFactory
    : public GLCommonImageBackingFactory {
 public:
  EGLImageBackingFactory(const GpuPreferences& gpu_preferences,
                         const GpuDriverBugWorkarounds& workarounds,
                         const gles2::FeatureInfo* feature_info);
  ~EGLImageBackingFactory() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      const SharedImageInfo& si_info,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) override;
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;

 private:
  std::unique_ptr<SharedImageBacking> MakeEglImageBacking(
      const Mailbox& mailbox,
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EGL_IMAGE_BACKING_FACTORY_H_
