// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_SANDBOX_HOOK_LINUX_H_
#define CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_SANDBOX_HOOK_LINUX_H_

#include <optional>
#include <string>
#include <string_view>

#include "sandbox/policy/linux/sandbox_linux.h"

namespace vr {

// Which OpenXR runtime a manifest names. The hook grants file and socket
// access per runtime, so this selects the extra access a given runtime needs
// beyond the common set. Add a value as a new runtime gains a hook rule.
enum class XrRuntimeId {
  // Any runtime with no runtime-specific rules (e.g. Monado): the common
  // grants are enough.
  kOther,
  // SteamVR, identified by "VALVE_runtime_is_steamvr" in its manifest; its IPC
  // needs extra /dev/shm and log-directory access.
  kSteamVr,
};

// The fields of an OpenXR runtime manifest's "runtime" object that the hook
// acts on.
struct XrRuntimeManifest {
  // "library_path" as written: absolute, or relative to the manifest's
  // directory.
  std::string library_path;
  // The runtime the manifest identifies, selecting its hook rules.
  XrRuntimeId runtime_id = XrRuntimeId::kOther;
};

// Parses |json|, the contents of an OpenXR runtime manifest. Returns nullopt
// if it is not a manifest naming a runtime library. Exposed for testing.
std::optional<XrRuntimeManifest> ParseXrRuntimeManifest(std::string_view json);

// Pre-sandbox hook for the XR Device Service (kXrCompositing) on Linux: maps
// the OpenXR loader and active runtime in before the sandbox seals, and
// brokers the manifest paths, runtime library, Vulkan ICD and DRM nodes.
bool XrPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace vr

#endif  // CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_SANDBOX_HOOK_LINUX_H_
