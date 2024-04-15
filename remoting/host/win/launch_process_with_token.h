// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
#define REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_

#include <windows.h>

#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/process/launch.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_handle.h"

namespace remoting {

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32_t session_id,
                        base::win::ScopedHandle* token_out);

// Launches |binary| in the security context of the user represented by
// |user_token|. The session ID specified by the token is respected as well.
// If |handles_to_inherit| is non-empty, these handles will be inherited by the
// new process. The other parameters are passed directly to
// CreateProcessAsUser().
bool LaunchProcessWithToken(
    const base::FilePath& binary,
    const base::CommandLine::StringType& command_line,
    HANDLE user_token,
    SECURITY_ATTRIBUTES* process_attributes,
    SECURITY_ATTRIBUTES* thread_attributes,
    const base::HandlesToInheritVector& handles_to_inherit,
    DWORD creation_flags,
    const wchar_t* desktop_name,
    base::win::ScopedHandle* process_out,
    base::win::ScopedHandle* thread_out);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
