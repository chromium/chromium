// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_UTILITY_SANDBOX_DELEGATE_H_
#define CONTENT_BROWSER_UTILITY_SANDBOX_DELEGATE_H_

#include "base/command_line.h"
#include "base/environment.h"
#include "build/build_config.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "sandbox/policy/sandbox_type.h"

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/common/zygote/zygote_handle_impl_linux.h"
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#endif  // OS_WIN

namespace content {
class UtilitySandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  UtilitySandboxedProcessLauncherDelegate(
      sandbox::policy::SandboxType sandbox_type,
      const base::EnvironmentMap& env,
      const base::CommandLine& cmd_line);
  ~UtilitySandboxedProcessLauncherDelegate() override;

  sandbox::policy::SandboxType GetSandboxType() override;

#if defined(OS_WIN)
  bool GetAppContainerId(std::string* appcontainer_id) override;
  bool DisableDefaultPolicy() override;
  bool ShouldLaunchElevated() override;
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override;
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  ZygoteHandle GetZygote() override;
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
  base::EnvironmentMap GetEnvironment() override;
#endif  // OS_POSIX

#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  // If true, the child process will be launched as x86_64 code under Rosetta
  // translation.
  void set_launch_x86_64(bool launch_x86_64) { launch_x86_64_ = launch_x86_64; }
  bool LaunchX86_64() override;
#endif  // OS_MAC && ARCH_CPU_ARM64

 private:
#if defined(OS_POSIX)
  base::EnvironmentMap env_;
#endif  // OS_POSIX
  sandbox::policy::SandboxType sandbox_type_;
  base::CommandLine cmd_line_;
#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  bool launch_x86_64_ = false;
#endif  // OS_MAC && ARCH_CPU_ARM64
};
}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_SANDBOX_DELEGATE_H_
