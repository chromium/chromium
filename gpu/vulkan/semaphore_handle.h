// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_SEMAPHORE_HANDLE_H_
#define GPU_VULKAN_SEMAPHORE_HANDLE_H_

#include <vulkan/vulkan_core.h>
#include <utility>

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/gfx/gpu_fence_handle.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/event.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace gpu {

// Thin wrapper around platform-specific handles for VkSemaphores.
// Note that handle transference depends on a handle type.
// SYNC_FD handles that use copy transference, while reference transference is
// used other handles types.
class COMPONENT_EXPORT(VULKAN) SemaphoreHandle {
 public:
#if BUILDFLAG(IS_POSIX)
  using PlatformHandle = base::ScopedFD;
#elif BUILDFLAG(IS_WIN)
  using PlatformHandle = base::win::ScopedHandle;
#elif BUILDFLAG(IS_FUCHSIA)
  using PlatformHandle = zx::event;
#endif

  SemaphoreHandle();
  SemaphoreHandle(VkExternalSemaphoreHandleTypeFlagBits type,
                  PlatformHandle handle);
  explicit SemaphoreHandle(gfx::GpuFenceHandle fence);
  SemaphoreHandle(SemaphoreHandle&&);

  SemaphoreHandle(const SemaphoreHandle&) = delete;
  SemaphoreHandle& operator=(const SemaphoreHandle&) = delete;

  ~SemaphoreHandle();

  SemaphoreHandle& operator=(SemaphoreHandle&&);

  VkExternalSemaphoreHandleTypeFlagBits vk_handle_type() { return type_; }

  bool is_valid() const {
#if BUILDFLAG(IS_WIN)
    return handle_.IsValid();
#else
    return handle_.is_valid();
#endif
  }

  // Returns underlying platform-specific handle for the semaphore. is_valid()
  // becomes false after this function returns.
  PlatformHandle TakeHandle() { return std::move(handle_); }

  // Moves platform specific instances to gfx::GpuFenceHandle.
  gfx::GpuFenceHandle ToGpuFenceHandle() &&;

  SemaphoreHandle Duplicate() const;

 private:
  void Init(VkExternalSemaphoreHandleTypeFlagBits type, PlatformHandle handle);

  VkExternalSemaphoreHandleTypeFlagBits type_;
  PlatformHandle handle_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_SEMAPHORE_HANDLE_H_
