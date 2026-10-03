// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <errno.h>
#include <sys/stat.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/process_metrics.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // With MappableSI, renderer processes hold FDs to GMBs in addition to the GPU
  // process, which can exhaust the default 2048 FD limit on ChromeOS when many
  // video decoders are active. So increasing the limit to 4096.
  // See https://crbug.com/404365358 and https://crbug.com/563801582.
  base::IncreaseFdLimitTo(4096);

  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox.md
  //
  // Anything else is started in InitializeSandbox().
  sandbox::policy::SandboxLinux::Options options;
  sandbox::policy::Sandbox::Initialize(
      sandbox::policy::SandboxTypeFromCommandLine(
          *base::CommandLine::ForCurrentProcess()),
      sandbox::policy::SandboxLinux::PreSandboxHook(), options);

  // about:sandbox uses a value returned from SandboxLinux::GetStatus() before
  // any renderer has been started.
  // Here, we test that the status of SeccompBpf in the renderer is consistent
  // with what SandboxLinux::GetStatus() said we would do.
  auto* linux_sandbox = sandbox::policy::SandboxLinux::GetInstance();
  if (linux_sandbox->GetStatus() & sandbox::policy::SandboxLinux::kSeccompBPF) {
    CHECK(linux_sandbox->seccomp_bpf_started());
  }

  // Under the setuid sandbox, we should not be able to open any file via the
  // filesystem.
  if (linux_sandbox->GetStatus() & sandbox::policy::SandboxLinux::kSUID) {
    CHECK(!base::PathExists(base::FilePath("/proc/cpuinfo")));
  }

#if defined(__x86_64__)
  // Limit this test to architectures where seccomp BPF is active in renderers.
  if (linux_sandbox->seccomp_bpf_started()) {
    errno = 0;
    // This should normally return EBADF since the first argument is bogus,
    // but we know that under the seccomp-bpf sandbox, this should return EPERM.
    CHECK_EQ(fchmod(-1, 07777), -1);
    CHECK_EQ(errno, EPERM);
  }
#endif  // __x86_64__

  return true;
}

}  // namespace content
