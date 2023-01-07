// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SANDBOX_H_
#define SANDBOX_POLICY_SANDBOX_H_

#include "build/build_config.h"
#include "sandbox/policy/export.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#endif

namespace sandbox {
namespace mojom {
enum class Sandbox;
}  // namespace mojom
struct SandboxInterfaceInfo;
}  // namespace sandbox

namespace sandbox {
namespace policy {
// Interface to the service manager sandboxes across the various platforms.
//
// Ideally, this API would abstract away the platform differences, but there
// are some major OS differences that shape this interface, including:
// * Whether the sandboxing is performed by the launcher (Windows, Fuchsia
//   someday) or by the launchee (Linux, Mac).
// * The means of specifying the additional resources that are permitted.
// * The need to "warmup" other resources before engaing the sandbox.

class SANDBOX_POLICY_EXPORT Sandbox {
 public:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static bool Initialize(sandbox::mojom::Sandbox sandbox_type,
                         SandboxLinux::PreSandboxHook hook,
                         const SandboxLinux::Options& options);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  static bool Initialize(sandbox::mojom::Sandbox sandbox_type,
                         SandboxInterfaceInfo* sandbox_info);
#endif  // BUILDFLAG(IS_WIN)

  // Returns true if the current process is running with a sandbox, and false
  // if the process is not sandboxed. This should be used to assert that code is
  // not running at high-privilege (e.g. in the browser process):
  //
  //    DCHECK(Sandbox::IsProcessSandboxed());
  //
  // The definition of what constitutes a sandbox, and the relative strength of
  // the restrictions placed on the process, and a per-platform implementation
  // detail.
  //
  // Except if the process is the browser, if the process is running with the
  // --no-sandbox flag, this unconditionally returns true.
  static bool IsProcessSandboxed();
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_SANDBOX_H_
