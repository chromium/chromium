// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_BACKING_FACTORY_H_

#include <climits>
#include <cstdint>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
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
  // Mask for all valid usage flags.
  static constexpr SharedImageUsageSet kUsageAll =
      SharedImageUsageSet((LAST_SHARED_IMAGE_USAGE << 1) - 1);

  // `valid_usages` is an allowlist of usages that the backing created by
  // factory can support. Requests to create a new shared image that contain
  // any usages not in `valid_usages` will be rejected by the factory. However,
  // if all usages are in `valid_usages` that doesn't imply support as
  // IsSupported() may contain additional logic.
  explicit SharedImageBackingFactory(SharedImageUsageSet valid_usages);
  virtual ~SharedImageBackingFactory();

  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe) = 0;
  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) = 0;
  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle) = 0;

  // This new api is introduced for MappableSI work where client code sends
  // |buffer_usage| info while creating shared image. This info is used in some
  // backings to create native handle.
  // TODO(crbug.com/40276430) : Remove this api once the MappableSI is complete
  // and we have a mapping between shared image usage and BufferUsage.
  virtual std::unique_ptr<SharedImageBacking> CreateSharedImage(
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
      gfx::BufferUsage buffer_usage);

  // Returns true if the factory is supported
  bool CanCreateSharedImage(SharedImageUsageSet usage,
                            viz::SharedImageFormat format,
                            const gfx::Size& size,
                            bool thread_safe,
                            gfx::GpuMemoryBufferType gmb_type,
                            GrContextType gr_context_type,
                            base::span<const uint8_t> pixel_data);

  // Return BackingType of the implementation. This value isn't guaranteed to
  // be precise, use it for logging/tracing only.
  virtual SharedImageBackingType GetBackingType() = 0;

  base::WeakPtr<SharedImageBackingFactory> GetWeakPtr();

 protected:
  // Returns true if the factory is supported. This must return false if `usage`
  // contains any usages from `invalid_usages_`. This is a temporary state to
  // verify `invalid_usages_` is correct.
  virtual bool IsSupported(SharedImageUsageSet usage,
                           viz::SharedImageFormat format,
                           const gfx::Size& size,
                           bool thread_safe,
                           gfx::GpuMemoryBufferType gmb_type,
                           GrContextType gr_context_type,
                           base::span<const uint8_t> pixel_data) = 0;

  void InvalidateWeakPtrsForTesting();

 private:
  const SharedImageUsageSet valid_usages_;
  base::WeakPtrFactory<SharedImageBackingFactory> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_BACKING_FACTORY_H_
