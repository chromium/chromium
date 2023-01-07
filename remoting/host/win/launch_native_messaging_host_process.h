// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_LAUNCH_NATIVE_MESSAGING_HOST_PROCESS_H_
#define REMOTING_HOST_WIN_LAUNCH_NATIVE_MESSAGING_HOST_PROCESS_H_

#include <cstdint>

#include "base/win/scoped_handle.h"

namespace base {
class FilePath;
}  // namespace base

namespace remoting {

enum ProcessLaunchResult {
  PROCESS_LAUNCH_RESULT_SUCCESS,
  PROCESS_LAUNCH_RESULT_CANCELLED,
  PROCESS_LAUNCH_RESULT_FAILED,
};

// Launches the executable at |binary_path| using the parameters passed in.
// If the process is launched successfully, |read_handle| and |write_handle| are
// valid for I/O and the function returns PROCESS_LAUNCH_RESULT_SUCCESS.
ProcessLaunchResult LaunchNativeMessagingHostProcess(
    const base::FilePath& binary_path,
    intptr_t parent_window_handle,
    bool elevate_process,
    base::win::ScopedHandle* read_handle,
    base::win::ScopedHandle* write_handle);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_LAUNCH_NATIVE_MESSAGING_HOST_PROCESS_H_