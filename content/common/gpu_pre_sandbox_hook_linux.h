// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_PRE_SANDBOX_HOOK_LINUX_H_
#define CONTENT_COMMON_GPU_PRE_SANDBOX_HOOK_LINUX_H_

#include <vector>

#include "sandbox/policy/linux/sandbox_linux.h"

namespace sandbox::syscall_broker {
class BrokerFilePermission;
}  // namespace sandbox::syscall_broker

namespace content {

// A pre-sandbox hook to use on Linux-based systems in sandboxed processes that
// require general GPU usage.
bool GpuPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

// These functions can be used by other pre-sandbox hooks that need GPU access
// in addition to their other permissions.

// Returns the set of commands (open, stat, unlink, rename, etc...) that are
// needed for a process to use the GPU.
sandbox::syscall_broker::BrokerCommandSet CommandSetForGPU(
    const sandbox::policy::SandboxLinux::Options& options);

// Returns a list of file permissions that are needed for a process to use
// the GPU. These will include the libraries that make up the graphics driver.
std::vector<sandbox::syscall_broker::BrokerFilePermission>
FilePermissionsForGpu(
    const sandbox::policy::SandboxSeccompBPF::Options& options);

// Loads the libraries needed for a process to use the GPU.
bool LoadLibrariesForGpu(
    const sandbox::policy::SandboxSeccompBPF::Options& options);

}  // namespace content

#endif  // CONTENT_COMMON_GPU_PRE_SANDBOX_HOOK_LINUX_H_
