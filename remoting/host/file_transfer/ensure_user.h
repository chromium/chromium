// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_ENSURE_USER_H_
#define REMOTING_HOST_FILE_TRANSFER_ENSURE_USER_H_

#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

// Ensures that the given thread is running as a normal user. On macOS and
// Windows, this ensures a user is logged in. On Windows (where the host runs as
// SYSTEM), it will additionally call ImpersonateLoggedOnUser to drop privileges
// on the current thread. As such, it should only be called on a dedicated
// thread. If success is returned, the thread is running as a normal user. If
// user is on the log-in screen, an error of type NOT_LOGGED_IN will be
// returned. If something else goes wrong, the error type will be
// UNEXPECTED_ERROR.
protocol::FileTransferResult<absl::monostate> EnsureUserContext();

// Makes `EnsureUserContext` always return success, for use during unittests.
void DisableUserContextCheckForTesting();

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_ENSURE_USER_H_
