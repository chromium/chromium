// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SANDBOX_TYPE_H_
#define SANDBOX_POLICY_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "sandbox/policy/export.h"

namespace sandbox {
namespace mojom {
enum class Sandbox;
}  // namespace mojom

namespace policy {

SANDBOX_POLICY_EXPORT bool IsUnsandboxedSandboxType(
    sandbox::mojom::Sandbox sandbox_type);

SANDBOX_POLICY_EXPORT void SetCommandLineFlagsForSandboxType(
    base::CommandLine* command_line,
    sandbox::mojom::Sandbox sandbox_type);

SANDBOX_POLICY_EXPORT sandbox::mojom::Sandbox SandboxTypeFromCommandLine(
    const base::CommandLine& command_line);

SANDBOX_POLICY_EXPORT std::string StringFromUtilitySandboxType(
    sandbox::mojom::Sandbox sandbox_type);

SANDBOX_POLICY_EXPORT sandbox::mojom::Sandbox UtilitySandboxTypeFromString(
    const std::string& sandbox_string);

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_SANDBOX_TYPE_H_
