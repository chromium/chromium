// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_INIT_WIN_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_INIT_WIN_H_

#include "base/process/launch.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "sandbox/win/src/sandbox_types.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {

class SandboxedProcessLauncherDelegate;

// Launch a sandboxed process. |delegate| may be NULL. If |delegate| is non-NULL
// then it just has to outlive this method call. |handles_to_inherit| is a list
// of handles for the child process to inherit. The caller retains ownership of
// the handles.
//
// Note that calling this function does not always create a sandboxed process,
// as the process might be unsandboxed depending on the behavior of the
// delegate, the command line of the caller, and the command line of the target.
CONTENT_EXPORT sandbox::ResultCode StartSandboxedProcess(
    SandboxedProcessLauncherDelegate* delegate,
    const base::CommandLine& target_command_line,
    const base::HandlesToInheritVector& handles_to_inherit,
    sandbox::StartSandboxedProcessCallback result_callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_INIT_WIN_H_
