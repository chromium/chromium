// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_utils.h"

#include "build/build_config.h"

#if defined(OS_FUCHSIA)
#include <lib/zx/vmo.h>
#elif defined(OS_POSIX)
#include "base/files/scoped_file.h"
#elif defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/scoped_mach_port.h"
#endif

namespace mojo {
namespace core {

void ExtractPlatformHandlesFromSharedMemoryRegionHandle(
    base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle handle,
    PlatformHandle* extracted_handle,
    PlatformHandle* extracted_readonly_handle) {
#if defined(OS_WIN)
  *extracted_handle = PlatformHandle(base::win::ScopedHandle(handle.Take()));
#elif defined(OS_FUCHSIA)
  *extracted_handle = PlatformHandle(std::move(handle));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // This is a Mach port. Same code as above and below, but separated for
  // clarity.
  *extracted_handle = PlatformHandle(std::move(handle));
#elif defined(OS_ANDROID)
  // This is a file descriptor. Same code as above, but separated for clarity.
  *extracted_handle = PlatformHandle(std::move(handle));
#else
  *extracted_handle = PlatformHandle(std::move(handle.fd));
  *extracted_readonly_handle = PlatformHandle(std::move(handle.readonly_fd));
#endif
}

base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle
CreateSharedMemoryRegionHandleFromPlatformHandles(
    PlatformHandle handle,
    PlatformHandle readonly_handle) {
#if defined(OS_WIN)
  DCHECK(!readonly_handle.is_valid());
  return handle.TakeHandle();
#elif defined(OS_FUCHSIA)
  DCHECK(!readonly_handle.is_valid());
  return zx::vmo(handle.TakeHandle());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  DCHECK(!readonly_handle.is_valid());
  return handle.TakeMachSendRight();
#elif defined(OS_ANDROID)
  DCHECK(!readonly_handle.is_valid());
  return handle.TakeFD();
#else
  return base::subtle::ScopedFDPair(handle.TakeFD(), readonly_handle.TakeFD());
#endif
}

}  // namespace core
}  // namespace mojo
