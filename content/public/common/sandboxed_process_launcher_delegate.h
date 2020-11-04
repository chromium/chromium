// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "sandbox/policy/sandbox_delegate.h"
#include "sandbox/policy/sandbox_type.h"

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/public/common/zygote/zygote_handle.h"  // nogncheck
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

namespace content {

// Allows a caller of StartSandboxedProcess or
// BrowserChildProcessHost/ChildProcessLauncher to control the sandbox policy,
// i.e. to loosen it if needed.
// The methods below will be called on the PROCESS_LAUNCHER thread.
class CONTENT_EXPORT SandboxedProcessLauncherDelegate
    : public sandbox::policy::SandboxDelegate {
 public:
  ~SandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  // SandboxDelegate:
  bool DisableDefaultPolicy() override;
  bool GetAppContainerId(std::string* appcontainer_id) override;
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override;
  void PostSpawnTarget(base::ProcessHandle process) override;

  // Override to return true if the process should be launched as an elevated
  // process (which implies no sandbox).
  virtual bool ShouldLaunchElevated();
#endif  // defined(OS_WIN)

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  // Returns the zygote used to launch the process.
  virtual ZygoteHandle GetZygote();
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
  // Override this if the process needs a non-empty environment map.
  virtual base::EnvironmentMap GetEnvironment();
#endif  // defined(OS_POSIX)

#if defined(OS_MAC)
  // Whether or not to disclaim TCC responsibility for the process, defaults to
  // false. See base::LaunchOptions::disclaim_responsibility.
  virtual bool DisclaimResponsibility();

#if defined(ARCH_CPU_ARM64)
  virtual bool LaunchX86_64();
#endif  // ARCH_CPU_ARM64
#endif  // OS_MAC
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
