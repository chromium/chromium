// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_PROCESS_UTIL_H_
#define REMOTING_HOST_BASE_PROCESS_UTIL_H_

#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace remoting {

// Gets the image path of |pid|. Note that on Linux, the process image's
// original path will still be returned even if the binary has been deleted from
// the storage.
base::FilePath GetProcessImagePath(base::ProcessId pid);

// Same as above but using an existing process handle.
base::FilePath GetProcessImagePath(const base::Process& process);

#if BUILDFLAG(IS_WIN)
// Returns the process ID of the named-pipe server connected to the specified
// input and output pipe handles. Returns base::kNullProcessId if either handle
// is invalid, not a named pipe, or connected to different named-pipe servers.
base::ProcessId GetLauncherProcessIdFromPipes(HANDLE stdin_handle,
                                              HANDLE stdout_handle);

// Returns the process ID of the named-pipe server connected to standard input
// and standard output. Returns base::kNullProcessId if stdin or stdout is not
// a named pipe, or if they are connected to different named-pipe servers.
base::ProcessId GetLauncherProcessIdFromStdioPipes();
#endif

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_PROCESS_UTIL_H_
