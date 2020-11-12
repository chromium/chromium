// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/scoped_process_handle.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_FUCHSIA)
#include <zircon/syscalls.h>
#endif

namespace mojo {
namespace core {

namespace {

base::ProcessHandle GetCurrentProcessHandle() {
#if defined(OS_NACL_NONSFI)
  // Doesn't really matter, it's not going to be used for anything interesting
  // under NaCl.
  return 1;
#else
  return base::GetCurrentProcessHandle();
#endif
}

}  // namespace

ScopedProcessHandle::ScopedProcessHandle() = default;

ScopedProcessHandle::ScopedProcessHandle(base::ProcessHandle handle)
#if defined(OS_FUCHSIA)
    : process_(handle)
#else
    : handle_(handle)
#endif
{
  DCHECK_NE(handle, GetCurrentProcessHandle());
}

ScopedProcessHandle::ScopedProcessHandle(ScopedProcessHandle&&) = default;

ScopedProcessHandle::~ScopedProcessHandle() = default;

// static
ScopedProcessHandle ScopedProcessHandle::CloneFrom(base::ProcessHandle handle) {
  DCHECK_NE(handle, GetCurrentProcessHandle());
  if (handle == base::kNullProcessHandle)
    return ScopedProcessHandle();

#if defined(OS_WIN)
  BOOL ok = ::DuplicateHandle(GetCurrentProcessHandle(), handle,
                              GetCurrentProcessHandle(), &handle, 0, FALSE,
                              DUPLICATE_SAME_ACCESS);
  DCHECK(ok);
  return ScopedProcessHandle(handle);
#elif defined(OS_FUCHSIA)
  base::ProcessHandle new_handle;
  zx_status_t status =
      zx_handle_duplicate(handle, ZX_RIGHT_SAME_RIGHTS, &new_handle);
  if (status != ZX_OK)
    return ScopedProcessHandle();
  return ScopedProcessHandle(new_handle);
#elif defined(OS_POSIX)
  return ScopedProcessHandle(handle);
#else
#error "Unsupported platform."
  return ScopedProcessHandle();
#endif
}

ScopedProcessHandle& ScopedProcessHandle::operator=(ScopedProcessHandle&&) =
    default;

bool ScopedProcessHandle::is_valid() const {
#if defined(OS_WIN)
  return handle_.IsValid();
#elif defined(OS_FUCHSIA)
  return process_.is_valid();
#else
  return handle_ != base::kNullProcessHandle;
#endif
}

base::ProcessHandle ScopedProcessHandle::get() const {
#if defined(OS_WIN)
  return handle_.Get();
#elif defined(OS_FUCHSIA)
  return process_.get();
#else
  return handle_;
#endif
}

base::ProcessHandle ScopedProcessHandle::release() {
#if defined(OS_WIN)
  return handle_.Take();
#elif defined(OS_FUCHSIA)
  return process_.release();
#else
  return handle_;
#endif
}

ScopedProcessHandle ScopedProcessHandle::Clone() const {
  if (is_valid())
    return CloneFrom(get());
  return ScopedProcessHandle();
}

}  // namespace core
}  // namespace mojo
