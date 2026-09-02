// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SYSTEMD_UTILS_H_
#define REMOTING_HOST_LINUX_SYSTEMD_UTILS_H_

#include <sys/types.h>

namespace remoting {

using SdPidGetSessionFunction = int (*)(pid_t pid, char** session);
using SdSessionIsRemoteFunction = int (*)(const char* session);

// Returns true if the calling process is running in a headless systemd session.
// Returns false if the session is local or if an error occurs.
bool IsRunningInHeadlessSystemdSession();

// Overload that allows injecting mock functions for `sd_pid_get_session` and
// `sd_session_is_remote` for testing.
bool IsRunningInHeadlessSystemdSession(
    SdPidGetSessionFunction pid_get_session_func,
    SdSessionIsRemoteFunction session_is_remote_func);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SYSTEMD_UTILS_H_
