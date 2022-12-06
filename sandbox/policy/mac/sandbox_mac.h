// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_MAC_SANDBOX_MAC_H_
#define SANDBOX_POLICY_MAC_SANDBOX_MAC_H_

#include <string>

#include "base/files/file_path.h"
#include "sandbox/policy/export.h"

namespace base {
class FilePath;
}

namespace sandbox {
namespace mojom {
enum class Sandbox;
}  // namespace mojom
}  // namespace sandbox

namespace sandbox {
namespace policy {

// Convert provided path into a "canonical" path matching what the Sandbox
// expects i.e. one without symlinks.
// This path is not necessarily unique e.g. in the face of hardlinks.
SANDBOX_POLICY_EXPORT base::FilePath GetCanonicalPath(
    const base::FilePath& path);

// Returns the sandbox profile string for a given sandbox type.
// It CHECKs that the sandbox profile is a valid type, so it always returns a
// valid result, or crashes.
SANDBOX_POLICY_EXPORT std::string GetSandboxProfile(
    sandbox::mojom::Sandbox sandbox_type);

// Returns true if the compiled policy for the sandbox `sandbox_type` can be
// cached and reused across multiple processes. Some sandbox policies bind
// parameters that prevent the policy from being reused.
SANDBOX_POLICY_EXPORT bool CanCacheSandboxPolicy(
    sandbox::mojom::Sandbox sandbox_type);

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_MAC_SANDBOX_MAC_H_
