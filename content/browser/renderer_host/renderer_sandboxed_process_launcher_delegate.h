// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDERER_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDERER_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace content {
// NOTE: changes to this class need to be reviewed by the security team.
class CONTENT_EXPORT RendererSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  RendererSandboxedProcessLauncherDelegate() = default;

  ~RendererSandboxedProcessLauncherDelegate() override = default;

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  ZygoteHandle GetZygote() override;
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if BUILDFLAG(IS_MAC)
  bool EnableCpuSecurityMitigations() override;
#endif  // BUILDFLAG(IS_MAC)

  sandbox::mojom::Sandbox GetSandboxType() override;
};

#if BUILDFLAG(IS_WIN)
// NOTE: changes to this class need to be reviewed by the security team.
class CONTENT_EXPORT RendererSandboxedProcessLauncherDelegateWin
    : public RendererSandboxedProcessLauncherDelegate {
 public:
  RendererSandboxedProcessLauncherDelegateWin(base::CommandLine* cmd_line,
                                              bool is_jit_disabled);

  std::string GetSandboxTag() override;

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override;
  void PostSpawnTarget(base::ProcessHandle process) override;

  bool CetCompatible() override;

 private:
  const bool renderer_code_integrity_enabled_;
  const bool renderer_app_container_disabled_;
  bool dynamic_code_can_be_disabled_ = false;
};
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDERER_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
