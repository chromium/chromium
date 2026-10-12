// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PLATFORM_HANDLE_UTILS_H_
#define MOJO_CORE_PLATFORM_HANDLE_UTILS_H_

#include "base/process/process.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace core {

// Takes a MojoPlatformProcessHandle, which does not own the handle value
// contained within, duplicates the value, and stores the strongly-owned result
// in |process|.
MojoResult UnwrapAndClonePlatformProcessHandle(
    const MojoPlatformProcessHandle* process_handle,
    base::Process& process);

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PLATFORM_HANDLE_UTILS_H_
