// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/semaphore_handle.h"

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#include "base/posix/eintr_wrapper.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/fuchsia_logging.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace gpu {

SemaphoreHandle::SemaphoreHandle() = default;
SemaphoreHandle::SemaphoreHandle(VkExternalSemaphoreHandleTypeFlagBits type,
                                 PlatformHandle handle)
    : type_(type), handle_(std::move(handle)) {}
SemaphoreHandle::SemaphoreHandle(SemaphoreHandle&&) = default;

SemaphoreHandle::~SemaphoreHandle() = default;

SemaphoreHandle& SemaphoreHandle::operator=(SemaphoreHandle&&) = default;

SemaphoreHandle::SemaphoreHandle(gfx::GpuFenceHandle fence_handle) {
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia's Vulkan driver allows zx::event to be obtained from a
  // VkSemaphore, which can then be used to submit present work, see
  // https://fuchsia.dev/reference/fidl/fuchsia.ui.scenic.
  Init(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA,
       fence_handle.Release());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  Init(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR,
       fence_handle.Release());
#elif BUILDFLAG(IS_POSIX)
  Init(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
       fence_handle.Release());
#elif BUILDFLAG(IS_WIN)
  Init(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
       fence_handle.Release());
#endif  // BUILDFLAG(IS_FUCHSIA)
}

void SemaphoreHandle::Init(VkExternalSemaphoreHandleTypeFlagBits type,
                           PlatformHandle handle) {
  type_ = type;
  handle_ = std::move(handle);
}

gfx::GpuFenceHandle SemaphoreHandle::ToGpuFenceHandle() && {
  gfx::GpuFenceHandle fence_handle;
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia's Vulkan driver allows zx::event to be obtained from a
  // VkSemaphore, which can then be used to submit present work, see
  // https://fuchsia.dev/reference/fidl/fuchsia.ui.scenic.
  fence_handle.Adopt(TakeHandle());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  if (type_ == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR) {
    fence_handle.Adopt(TakeHandle());
  } else {
    DLOG(ERROR) << "Unable to convert SemaphoreHandle to GpuFenceHandle";
  }
#elif BUILDFLAG(IS_POSIX)
  fence_handle.Adopt(TakeHandle());
#elif BUILDFLAG(IS_WIN)
  fence_handle.Adopt(TakeHandle());
#endif  // BUILDFLAG(IS_FUCHSIA)
  return fence_handle;
}

SemaphoreHandle SemaphoreHandle::Duplicate() const {
  if (!is_valid())
    return SemaphoreHandle();

#if BUILDFLAG(IS_POSIX)
  return SemaphoreHandle(type_,
                         base::ScopedFD(HANDLE_EINTR(dup(handle_.get()))));
#elif BUILDFLAG(IS_WIN)
  HANDLE handle_dup;
  if (!::DuplicateHandle(::GetCurrentProcess(), handle_.Get(),
                         ::GetCurrentProcess(), &handle_dup, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    return SemaphoreHandle();
  }
  return SemaphoreHandle(type_, base::win::ScopedHandle(handle_dup));
#elif BUILDFLAG(IS_FUCHSIA)
  zx::event event_dup;
  zx_status_t status = handle_.duplicate(ZX_RIGHT_SAME_RIGHTS, &event_dup);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
    return SemaphoreHandle();
  }
  return SemaphoreHandle(type_, std::move(event_dup));
#else
#error Unsupported OS
#endif
}

}  // namespace gpu
