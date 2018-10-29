// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/scoped_process_handle.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
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
    : handle_(handle) {
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

  // TODO(https://crbug.com/887576): Revert this to a DCHECK once we have sorted
  // out the cause of handle verifier failures.
  PCHECK(ok);
#endif
  return ScopedProcessHandle(handle);
}

ScopedProcessHandle& ScopedProcessHandle::operator=(ScopedProcessHandle&&) =
    default;

bool ScopedProcessHandle::is_valid() const {
#if defined(OS_WIN)
  return handle_.IsValid();
#else
  return handle_ != base::kNullProcessHandle;
#endif
}

base::ProcessHandle ScopedProcessHandle::get() const {
#if defined(OS_WIN)
  return handle_.Get();
#else
  return handle_;
#endif
}

base::ProcessHandle ScopedProcessHandle::release() {
#if defined(OS_WIN)
  return handle_.Take();
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
