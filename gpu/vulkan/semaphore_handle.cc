// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/semaphore_handle.h"

#if defined(OS_POSIX)
#include <unistd.h>
#include "base/posix/eintr_wrapper.h"
#endif

#if defined(OS_FUCHSIA)
#include "base/fuchsia/fuchsia_logging.h"
#endif

#if defined(OS_WIN)
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

SemaphoreHandle SemaphoreHandle::Duplicate() const {
  if (!is_valid())
    return SemaphoreHandle();

#if defined(OS_POSIX)
  return SemaphoreHandle(type_,
                         base::ScopedFD(HANDLE_EINTR(dup(handle_.get()))));
#elif defined(OS_WIN)
  HANDLE handle_dup;
  if (!::DuplicateHandle(::GetCurrentProcess(), handle_.Get(),
                         ::GetCurrentProcess(), &handle_dup, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    return SemaphoreHandle();
  }
  return SemaphoreHandle(type_, base::win::ScopedHandle(handle_dup));
#elif defined(OS_FUCHSIA)
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
