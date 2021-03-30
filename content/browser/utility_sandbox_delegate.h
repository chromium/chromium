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
  bool ShouldUnsandboxedRunInJob() override;
  bool CetCompatible() override;
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  ZygoteHandle GetZygote() override;
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
  base::EnvironmentMap GetEnvironment() override;
#endif  // OS_POSIX

 private:
#if defined(OS_POSIX)
  base::EnvironmentMap env_;
#endif  // OS_POSIX
  sandbox::policy::SandboxType sandbox_type_;
  base::CommandLine cmd_line_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_UTILITY_SANDBOX_DELEGATE_H_
