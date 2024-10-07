// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init_win.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_process_information.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_types.h"

namespace content {

sandbox::ResultCode StartSandboxedProcess(
    SandboxedProcessLauncherDelegate* delegate,
    const base::CommandLine& target_command_line,
    const base::HandlesToInheritVector& handles_to_inherit,
    sandbox::StartSandboxedProcessCallback result_callback) {
  std::string type_str =
      target_command_line.GetSwitchValueASCII(switches::kProcessType);
  TRACE_EVENT1("startup", "StartProcessWithAccess", "type", type_str);

  // Updates the command line arguments with debug-related flags. If debug
  // flags have been used with this process, they will be filtered and added
  // to full_command_line as needed.
  const base::CommandLine& current_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine full_command_line = target_command_line;
  if (current_command_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    std::string value = current_command_line.GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    full_command_line.AppendSwitchASCII(switches::kWaitForDebuggerChildren,
                                        value);
    if (value.empty() || value == type_str)
      full_command_line.AppendSwitch(switches::kWaitForDebugger);
  }

  return sandbox::policy::SandboxWin::StartSandboxedProcess(
      full_command_line, handles_to_inherit, delegate,
      std::move(result_callback));
}

}  // namespace content
