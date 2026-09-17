// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SANDBOX_INIT_H_
#define CONTENT_UTILITY_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SANDBOX_INIT_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#endif

namespace on_device_model {

// This file and the corresponding .cc file require a review from Linux sandbox
// owners.

// Must be called in the service's process before the sandbox policy is engaged.
//
// On non-Linux platforms, this is called from UtilityMain() prior to sandbox
// initialization.
// On Linux/ChromeOS, this is called from PreSandboxHook() after starting the
// syscall broker process while the process is still single-threaded, avoiding
// fork() in a multi-threaded process.
[[nodiscard]] bool PreSandboxInit();

// Returns true if PreSandboxInit() has been called. Since PreSandboxInit() is
// called in different locations depending on the platform (e.g. from
// UtilityMain() on non-Linux platforms, and from PreSandboxHook() on
// Linux/ChromeOS), this is used to confirm that it has been called, no matter
// the platform, when entering the service's main function.
bool WasPreSandboxInitCalled();

// Must be called in the service's process after the run loop finished.
[[nodiscard]] bool Shutdown();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void AddSandboxLinuxOptions(sandbox::policy::SandboxLinux::Options& options);

[[nodiscard]] bool PreSandboxHook(
    sandbox::policy::SandboxLinux::Options options);
#endif

}  // namespace on_device_model

#endif  // CONTENT_UTILITY_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SANDBOX_INIT_H_
