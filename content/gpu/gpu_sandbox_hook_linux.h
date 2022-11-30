// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_SANDBOX_HOOK_LINUX_H_
#define CONTENT_GPU_GPU_SANDBOX_HOOK_LINUX_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace content {

bool GpuProcessPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace content

#endif  // CONTENT_GPU_GPU_SANDBOX_HOOK_LINUX_H_
