// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_SANDBOX_MAC_H_
#define REMOTING_HOST_MAC_SANDBOX_MAC_H_

#include <string_view>

namespace remoting::mac {

// Drops privileges from root to `nobody` (-2, if running as root) and claims
// inherited Mach bootstrap ports via `MachPortRendezvousClient` before Seatbelt
// lockdown. Must be called early in `HostMain()` for unprivileged worker
// processes (`kProcessTypeNetwork` and `kProcessTypePeerConnection`).
//
// Unlike Linux (`fork()` + `LaunchOptions::PreExecDelegate` in
// `LinuxWorkerProcessLauncherDelegate`), `base::LaunchProcess()` on macOS uses
// `posix_spawn()`, which has no `PreExecDelegate` (`!BUILDFLAG(IS_APPLE)`) and
// no public macOS `posix_spawn` API to switch to an arbitrary UID/GID before
// `exec`, so privilege drop happens child-side at the start of `HostMain()`.
void DropRootPrivilegesAndAcquireBootstrapPort();

// Applies the Seatbelt sandbox profile for `process_type`
// (`kProcessTypeNetwork` or `kProcessTypePeerConnection`). Must be called after
// `DropRootPrivilegesAndAcquireBootstrapPort()`, `InitializeICU()`, and
// `LoadResources()`, and before `mojo::core::Init()`. CHECKs on failure.
void EnterSeatbeltSandbox(std::string_view process_type);

}  // namespace remoting::mac

#endif  // REMOTING_HOST_MAC_SANDBOX_MAC_H_
