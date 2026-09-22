// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_UTIL_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_UTIL_H_

#include "base/files/file_path.h"
#include "remoting/base/branding.h"

namespace remoting::installer {

// Returns true if the running host service currently has an active desktop
// session (i.e. `event_name_for_testing` is openable and signaled).
bool IsHostSessionActive(
    const wchar_t* event_name_for_testing = kHostSessionActiveEventName);

// Signals `event_name_for_testing` to notify a running host daemon that a
// software update has been staged and the host should restart once idle.
// Returns true if the event was successfully opened and signaled.
bool SignalHostUpdatePending(
    const wchar_t* event_name_for_testing = kHostUpdatePendingEventName);

// Installs or updates the Windows service configuration for Chrome Remote
// Desktop, including 1-minute failure recovery actions.
bool InstallOrUpdateService(const base::FilePath& host_binary_path,
                            const base::FilePath& host_config_path);

// Starts the Chrome Remote Desktop Windows service if it is not already
// running.
bool StartHostService();

// Stops the Chrome Remote Desktop Windows service and waits up to 30 seconds
// for it to reach SERVICE_STOPPED.
bool StopHostService();

// Stops and deletes the Chrome Remote Desktop Windows service.
bool UninstallHostService();

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_SERVICE_UTIL_H_
