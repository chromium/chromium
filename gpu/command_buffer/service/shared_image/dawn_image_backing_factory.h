// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"

namespace gpu {

// Implementation of SharedImageBackingFactory that produces webgpu texture
// backed SharedImages.
class GPU_GLES2_EXPORT DawnImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  DawnImageBackingFactory();
  ~DawnImageBackingFactory() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      bool is_thread_safe) override;
  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;
  bool IsSupportedForAccessStream(SharedImageAccessStream stream,
                                  viz::SharedImageFormat format,
                                  const AccessParams* params) const override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_BACKING_FACTORY_H_
