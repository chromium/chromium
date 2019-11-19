// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_SEMAPHORE_HANDLE_H_
#define GPU_VULKAN_SEMAPHORE_HANDLE_H_

#include <vulkan/vulkan.h>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"

#if defined(OS_POSIX)
#include "base/files/scoped_file.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/event.h>
#endif

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace gpu {

// Thin wrapper around platform-specific handles for VkSemaphores.
// Note that handle transference depends on a handle type.
// SYNC_FD handles that use copy transference, while reference transference is
// used other handles types.
class VULKAN_EXPORT SemaphoreHandle {
 public:
#if defined(OS_POSIX)
  using PlatformHandle = base::ScopedFD;
#elif defined(OS_WIN)
  using PlatformHandle = base::win::ScopedHandle;
#elif defined(OS_FUCHSIA)
  using PlatformHandle = zx::event;
#endif

  SemaphoreHandle();
  SemaphoreHandle(VkExternalSemaphoreHandleTypeFlagBits type,
                  PlatformHandle handle);
  SemaphoreHandle(SemaphoreHandle&&);

  ~SemaphoreHandle();

  SemaphoreHandle& operator=(SemaphoreHandle&&);

  VkExternalSemaphoreHandleTypeFlagBits vk_handle_type() { return type_; }

  bool is_valid() const {
#if defined(OS_WIN)
    return handle_.IsValid();
#else
    return handle_.is_valid();
#endif
  }

  // Returns underlying platform-specific handle for the semaphore. is_valid()
  // becomes false after this function returns.
  PlatformHandle TakeHandle() { return std::move(handle_); }

  SemaphoreHandle Duplicate() const;

 private:
  VkExternalSemaphoreHandleTypeFlagBits type_;
  PlatformHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(SemaphoreHandle);
};

}  // namespace gpu

#endif  // GPU_VULKAN_SEMAPHORE_HANDLE_H_