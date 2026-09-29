// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SYSTEMD_UTILS_H_
#define REMOTING_HOST_LINUX_SYSTEMD_UTILS_H_

#include <sys/types.h>

namespace remoting {

using SdPidGetSessionFunction = int (*)(pid_t pid, char** session);
using SdSessionIsRemoteFunction = int (*)(const char* session);
using SdSessionGetServiceFunction = int (*)(const char* session,
                                            char** service);

// Returns true if the calling process is running in a headless systemd session,
// i.e. a session with `Remote=yes` (e.g. created by GDM's remote display API),
// or a session opened with the "chrome-remote-desktop" PAM service (i.e.
// created by CRD). Returns false if the session is local or if an error occurs.
bool IsRunningInHeadlessSystemdSession();

// Overload that allows injecting mock functions for `sd_pid_get_session`,
// `sd_session_is_remote`, and `sd_session_get_service` for testing.
bool IsRunningInHeadlessSystemdSession(
    SdPidGetSessionFunction pid_get_session_func,
    SdSessionIsRemoteFunction session_is_remote_func,
    SdSessionGetServiceFunction session_get_service_func);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SYSTEMD_UTILS_H_
