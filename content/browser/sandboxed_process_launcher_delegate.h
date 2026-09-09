// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define CONTENT_BROWSER_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include <optional>
#include <string>

#include "base/environment.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "sandbox/policy/sandbox_delegate.h"

#if BUILDFLAG(USE_ZYGOTE)
#include "content/public/common/zygote/zygote_handle.h"  // nogncheck
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_MAC)
#include "base/mac/process_requirement.h"
#endif  // BUILDFLAG(IS_MAC)

namespace content {

// Allows a caller of BrowserChildProcessHostImpl or ChildProcessLauncher to
// control the sandbox policy, i.e. to loosen it if needed.
// The methods below will be called on the PROCESS_LAUNCHER thread.
class CONTENT_EXPORT SandboxedProcessLauncherDelegate
    : public sandbox::policy::SandboxDelegate {
 public:
  ~SandboxedProcessLauncherDelegate() override = default;

#if BUILDFLAG(IS_WIN)
  // sandbox::policy::SandboxDelegate:
  std::string GetSandboxTag() override;
  bool DisableDefaultPolicy() override;
  bool GetAppContainerId(std::string* appcontainer_id) override;
  bool InitializeConfig(sandbox::TargetConfig* config) override;
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override;
  void PostSpawnTarget(base::ProcessHandle process) override;
  bool ShouldUnsandboxedRunInJob() override;
  bool CetCompatible() override;
  bool RestrictCoreSharing() override;
  std::optional<std::wstring> GetSecurityAttributeName() override;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
  // Override to return true if the process should be launched as an elevated
  // process (which implies no sandbox).
  virtual bool ShouldLaunchElevated();

  // Whether or not to use the MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS flag
  // on the outgoing invitation used to create the mojo connection to this
  // process.
  virtual bool ShouldUseUntrustedMojoInvitation();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
  // Returns the zygote used to launch the process.
  virtual ZygoteCommunication* GetZygote();
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_POSIX)
  // Override this if the process needs a non-empty environment map.
  virtual base::EnvironmentMap GetEnvironment();
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_MAC)
  // Whether or not to disclaim TCC responsibility for the process, defaults to
  // false. See base::LaunchOptions::disclaim_responsibility.
  virtual bool DisclaimResponsibility();

  // Whether or not to enable CPU security mitigations against side-channel
  // attacks. See base::LaunchOptions::enable_cpu_security_mitigations.
  virtual bool EnableCpuSecurityMitigations();

  // A `ProcessRequirement` that the launched process will be validated against
  // before it can retrieve any Mach ports and bootstrap Mojo IPC.
  virtual std::optional<base::mac::ProcessRequirement> GetProcessRequirement();

  // Whether to create process-type isolated subdirectories of th Darwin user,
  // cache, and temporary directories. These directories are shared between
  // different child processes of the same type (e.g., the GPU process) but are
  // not isolated to single processes (i.e., different instances of the GPU
  // process will have access to the same subdirectories).
  //
  // When true, the browser creates a unique per-child-process-type subdirectory
  // under each macOS user directory if needed and sets the
  // DIRHELPER_USER_DIR_SUFFIX environment variable to
  // `org.chromium.Chromium.gpu-process` and the TMPDIR environment variable to
  // the Temp value below. For example, the directories for the GPU process
  // would be:
  //   - User:  `/var/folders/.../0/org.chromium.Chromium.helper.gpu`
  //   - Cache: `/var/folders/.../C/org.chromium.Chromium.helper.gpu`
  //   - Temp:  `/var/folders/.../T/org.chromium.Chromium.helper.gpu`,
  //
  // This causes macOS system APIs (e.g. `NSTemporaryDirectory()`, `confstr()`)
  // and `base::GetTempDir()` to resolve directly to the isolated paths,
  // ensuring that the child process will use these subdirectories. The Seatbelt
  // profile enforces isolation by denying read/write access to parent
  // directories while allowing access within the subdirectories.
  //
  // Note: `base::GetTempDir()`, which uses the Temp dir by default, can be
  // overridden by MAC_CHROMIUM_TMPDIR. The override is limited to
  // `base::GetTempDir()` and does not affect the paths vended by the system
  // (i.e., `NSTemporaryDirectory()` and `confstr()` will still return the Temp
  // path).
  //
  // Defaults to false.
  virtual bool NeedsIsolatedDarwinUserDirs();
#endif  // BUILDFLAG(IS_MAC)
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
