// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "components/viz/common/resources/resource_format.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
struct Mailbox;

class GPU_GLES2_EXPORT SharedImageBackingFactory {
 public:
  virtual ~SharedImageBackingFactory() = default;
  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) = 0;
  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) = 0;
  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
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
      uint32_t usage) = 0;

  // Only implemented in the D3D backing factory.
  virtual std::vector<std::unique_ptr<SharedImageBacking>>
  CreateSharedImageVideoPlanes(base::span<const Mailbox> mailboxes,
                               gfx::GpuMemoryBufferHandle handle,
                               gfx::BufferFormat format,
                               const gfx::Size& size,
                               uint32_t usage);

  // Returns true if the factory is supported
  virtual bool IsSupported(uint32_t usage,
                           viz::ResourceFormat format,
                           bool thread_safe,
                           gfx::GpuMemoryBufferType gmb_type,
                           GrContextType gr_context_type,
                           bool* allow_legacy_mailbox,
                           bool is_pixel_used) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_H_
