// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ozone.h"

#include <dawn/dawn_proc_table.h>
#include <dawn_native/DawnNative.h>

#include "base/logging.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_ozone.h"
#include "ui/gl/buildflags.h"

namespace gpu {

SharedImageBackingFactoryOzone::SharedImageBackingFactoryOzone(
    SharedContextState* shared_context_state)
    : shared_context_state_(shared_context_state) {
#if BUILDFLAG(USE_DAWN)
  dawn_procs_ = base::MakeRefCounted<base::RefCountedData<DawnProcTable>>(
      dawn_native::GetProcs());
#endif  // BUILDFLAG(USE_DAWN)
}

SharedImageBackingFactoryOzone::~SharedImageBackingFactoryOzone() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryOzone::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return SharedImageBackingOzone::Create(
      dawn_procs_, shared_context_state_, mailbox, format, size, color_space,
      surface_origin, alpha_type, usage, surface_handle);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryOzone::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryOzone::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool SharedImageBackingFactoryOzone::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox) {
  bool using_dawn = usage & SHARED_IMAGE_USAGE_WEBGPU;
  bool vulkan_usage = gr_context_type == GrContextType::kVulkan &&
                      (usage & SHARED_IMAGE_USAGE_DISPLAY);
  bool using_interop_factory =
      vulkan_usage || using_dawn || (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE);

  if (using_interop_factory) {
    // TODO(crbug.com/969114): Not all shared image factory implementations
    // support concurrent read/write usage.
    if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
      LOG(ERROR) << "Unable to create SharedImage backing: Interoperability is "
                    "not supported for concurrent read/write usage";
      return false;
    }

    *allow_legacy_mailbox = false;
    return true;
  }

  return false;
}

}  // namespace gpu
