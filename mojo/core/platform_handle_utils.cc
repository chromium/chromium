// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/process.h>
#include <lib/zx/vmo.h>
#elif BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_mach_port.h"
#endif

namespace mojo {
namespace core {

void ExtractPlatformHandlesFromSharedMemoryRegionHandle(
    base::subtle::ScopedPlatformSharedMemoryHandle handle,
    PlatformHandle* extracted_handle,
    PlatformHandle* extracted_readonly_handle) {
#if BUILDFLAG(IS_WIN)
  *extracted_handle = PlatformHandle(base::win::ScopedHandle(handle.Take()));
#elif BUILDFLAG(IS_FUCHSIA)
  *extracted_handle = PlatformHandle(std::move(handle));
#elif BUILDFLAG(IS_APPLE)
  // This is a Mach port. Same code as above and below, but separated for
  // clarity.
  *extracted_handle = PlatformHandle(std::move(handle));
#elif BUILDFLAG(IS_ANDROID)
  // This is a file descriptor. Same code as above, but separated for clarity.
  *extracted_handle = PlatformHandle(std::move(handle));
#else
  *extracted_handle = PlatformHandle(std::move(handle.fd));
  *extracted_readonly_handle = PlatformHandle(std::move(handle.readonly_fd));
#endif
}

base::subtle::ScopedPlatformSharedMemoryHandle
CreateSharedMemoryRegionHandleFromPlatformHandles(
    PlatformHandle handle,
    PlatformHandle readonly_handle) {
#if BUILDFLAG(IS_WIN)
  DCHECK(!readonly_handle.is_valid());
  return handle.TakeHandle();
#elif BUILDFLAG(IS_FUCHSIA)
  DCHECK(!readonly_handle.is_valid());
  return zx::vmo(handle.TakeHandle());
#elif BUILDFLAG(IS_APPLE)
  DCHECK(!readonly_handle.is_valid());
  return handle.TakeMachSendRight();
#elif BUILDFLAG(IS_ANDROID)
  DCHECK(!readonly_handle.is_valid());
  return handle.TakeFD();
#else
  return base::subtle::ScopedFDPair(handle.TakeFD(), readonly_handle.TakeFD());
#endif
}

MojoResult UnwrapAndClonePlatformProcessHandle(
    const MojoPlatformProcessHandle* process_handle,
    base::Process& process) {
  if (process_handle->struct_size < sizeof(*process_handle))
    return MOJO_RESULT_INVALID_ARGUMENT;

#if BUILDFLAG(IS_WIN)
  base::ProcessHandle in_handle = reinterpret_cast<base::ProcessHandle>(
      static_cast<uintptr_t>(process_handle->value));
#else
  base::ProcessHandle in_handle =
      static_cast<base::ProcessHandle>(process_handle->value);
#endif

  if (in_handle == base::kNullProcessHandle) {
    process = base::Process();
    return MOJO_RESULT_OK;
  }

#if BUILDFLAG(IS_WIN)
  base::ProcessHandle out_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), in_handle,
                         ::GetCurrentProcess(), &out_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  process = base::Process(out_handle);
#elif BUILDFLAG(IS_FUCHSIA)
  zx::process out;
  if (zx::unowned_process(in_handle)->duplicate(ZX_RIGHT_SAME_RIGHTS, &out) !=
      ZX_OK) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  process = base::Process(out.release());
#else
  process = base::Process(in_handle);
#endif
  return MOJO_RESULT_OK;
}

}  // namespace core
}  // namespace mojo
