// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_SANDBOX_WIN_H_
#define SANDBOX_POLICY_WIN_SANDBOX_WIN_H_

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/sandbox_delegate.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_level.h"

namespace base {
class CommandLine;
class Value;
}  // namespace base

namespace sandbox {
class BrokerServices;
class TargetConfig;
class TargetPolicy;
class TargetServices;

namespace mojom {
enum class Sandbox;
}  // namespace mojom
}  // namespace sandbox

namespace sandbox {
namespace policy {

class SANDBOX_POLICY_EXPORT SandboxWin {
 public:
  // Create a sandboxed process `process` with the specified `cmd_line` of type
  // `process_type` (e.g. 'renderer' or 'utility'). `handles_to_inherit`
  // specifies a set of handles to inherit. `delegate` specifies the sandbox
  // delegate to use when resolving specific sandbox policy.
  //
  // Returns SBOX_ALL_OK if the process was successfully created.
  // Otherwise, returns one of sandbox::ResultCode for any other error.
  static ResultCode StartSandboxedProcess(
      const base::CommandLine& cmd_line,
      const std::string& process_type,
      const base::HandlesToInheritVector& handles_to_inherit,
      SandboxDelegate* delegate,
      base::Process* process);

  // Generates a sandbox policy into `policy` to match the one that would be
  // applied during `StartSandboxedProcess` for the identical set of arguments.
  //
  // Returns SBOX_ALL_OK if the policy was successfully generated.
  // Returns SBOX_ERROR_UNSANDBOXED_PROCESS if the process has no valid
  // sandbox policy because it should be run unsandboxed, otherwise returns one
  // of sandbox::ResultCode for any other error while constructing the policy.
  static ResultCode GeneratePolicyForSandboxedProcess(
      const base::CommandLine& cmd_line,
      const std::string& process_type,
      const base::HandlesToInheritVector& handles_to_inherit,
      SandboxDelegate* delegate,
      TargetPolicy* policy);

  // Wrapper around TargetPolicy::SetJobLevel that checks if the
  // sandbox should be let to run without a job object assigned.
  static ResultCode SetJobLevel(sandbox::mojom::Sandbox sandbox_type,
                                JobLevel job_level,
                                uint32_t ui_exceptions,
                                TargetConfig* config);

  // Closes handles that are opened at process creation and initialization.
  static ResultCode AddBaseHandleClosePolicy(TargetConfig* config);

  // Add AppContainer policy for |sid| on supported OS.
  static ResultCode AddAppContainerPolicy(TargetConfig* config,
                                          const wchar_t* sid);

  // Add the win32k lockdown policy on supported OS.
  static ResultCode AddWin32kLockdownPolicy(TargetConfig* config);

  // Add the AppContainer sandbox profile to the config. `sandbox_type`
  // determines what policy is enabled. `appcontainer_id` is used to create
  // a unique package SID, it can be anything the caller wants.
  static ResultCode AddAppContainerProfileToConfig(
      const base::CommandLine& command_line,
      sandbox::mojom::Sandbox sandbox_type,
      const std::string& appcontainer_id,
      TargetConfig* config);

  // Returns whether the AppContainer sandbox is enabled or not for a specific
  // sandbox type from |command_line| and |sandbox_type|.
  static bool IsAppContainerEnabledForSandbox(
      const base::CommandLine& command_line,
      sandbox::mojom::Sandbox sandbox_type);

  static bool InitBrokerServices(BrokerServices* broker_services);
  static bool InitTargetServices(TargetServices* target_services);

  // Report diagnostic information about policies applied to sandboxed
  // processes. This is a snapshot and may describe processes which
  // have subsequently finished. This can be invoked on any sequence and posts
  // to |response| to the origin sequence on completion. |response|
  // will be an empty value if an error is encountered.
  static ResultCode GetPolicyDiagnostics(
      base::OnceCallback<void(base::Value)> response);

  // Provides a friendly name for the sandbox for chrome://sandbox and tracing.
  static std::string GetSandboxTypeInEnglish(
      sandbox::mojom::Sandbox sandbox_type);

  // Helper for sandbox delegates to generate a SandboxTag
  static std::string GetSandboxTagForDelegate(
      base::StringPiece prefix,
      sandbox::mojom::Sandbox sandbox_type);
};

SANDBOX_POLICY_EXPORT
void BlocklistAddOneDllForTesting(const wchar_t* module_name,
                                  bool check_in_browser,
                                  TargetConfig* config);

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_WIN_SANDBOX_WIN_H_
