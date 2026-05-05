// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {
class SharedContextState;
class SharedImageBacking;
struct Mailbox;

// Factory that creates all SharedImageBackings that can be placed into a DComp
// layer tree.
class GPU_GLES2_EXPORT DCompImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  explicit DCompImageBackingFactory(
      scoped_refptr<SharedContextState> context_state);

  DCompImageBackingFactory(const DCompImageBackingFactory&) = delete;
  DCompImageBackingFactory& operator=(const DCompImageBackingFactory&) = delete;

  ~DCompImageBackingFactory() override;

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

  bool IsSupportedForAccessStream(SharedImageAccessStream stream,
                                  viz::SharedImageFormat format,
                                  const AccessParams* params) const override;

  SharedImageBackingType GetBackingType() override;

 private:
  scoped_refptr<SharedContextState> context_state_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_IMAGE_BACKING_FACTORY_H_
