// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANANGER_SANDBOX_WIN_SANDBOX_WIN_H_
#define SERVICES_SERVICE_MANANGER_SANDBOX_WIN_SANDBOX_WIN_H_

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_level.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/sandbox_delegate.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace base {
class CommandLine;
class Value;
}  // namespace base

namespace sandbox {
class BrokerServices;
class TargetPolicy;
class TargetServices;
}  // namespace sandbox

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT SandboxWin {
 public:
  static sandbox::ResultCode StartSandboxedProcess(
      base::CommandLine* cmd_line,
      const std::string& process_type,
      const base::HandlesToInheritVector& handles_to_inherit,
      SandboxDelegate* delegate,
      base::Process* process);

  // Wrapper around sandbox::TargetPolicy::SetJobLevel that checks if the
  // sandbox should be let to run without a job object assigned.
  static sandbox::ResultCode SetJobLevel(const base::CommandLine& cmd_line,
                                         sandbox::JobLevel job_level,
                                         uint32_t ui_exceptions,
                                         sandbox::TargetPolicy* policy);

  // Closes handles that are opened at process creation and initialization.
  static sandbox::ResultCode AddBaseHandleClosePolicy(
      sandbox::TargetPolicy* policy);

  // Add AppContainer policy for |sid| on supported OS.
  static sandbox::ResultCode AddAppContainerPolicy(
      sandbox::TargetPolicy* policy,
      const wchar_t* sid);

  // Add the win32k lockdown policy on supported OS.
  static sandbox::ResultCode AddWin32kLockdownPolicy(
      sandbox::TargetPolicy* policy,
      bool enable_opm);

  // Add the AppContainer sandbox profile to the policy. |sandbox_type|
  // determines what policy is enabled. |appcontainer_id| is used to create
  // a unique package SID, it can be anything the caller wants.
  static sandbox::ResultCode AddAppContainerProfileToPolicy(
      const base::CommandLine& command_line,
      service_manager::SandboxType sandbox_type,
      const std::string& appcontainer_id,
      sandbox::TargetPolicy* policy);

  // Returns whether the AppContainer sandbox is enabled or not for a specific
  // sandbox type from |command_line| and |sandbox_type|.
  static bool IsAppContainerEnabledForSandbox(
      const base::CommandLine& command_line,
      service_manager::SandboxType sandbox_type);

  static bool InitBrokerServices(sandbox::BrokerServices* broker_services);
  static bool InitTargetServices(sandbox::TargetServices* target_services);

  // Report diagnostic information about policies applied to sandboxed
  // processes. This is a snapshot and may describe processes which
  // have subsequently finished. This can be invoked on any sequence and posts
  // to |response| to the origin sequence on completion. |response|
  // will be an empty value if an error is encountered.
  static sandbox::ResultCode GetPolicyDiagnostics(
      base::OnceCallback<void(base::Value)> response);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANANGER_SANDBOX_WIN_SANDBOX_WIN_H_
