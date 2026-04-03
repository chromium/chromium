// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SYSTEMD_USER_ENV_SETTER_H_
#define REMOTING_HOST_LINUX_SYSTEMD_USER_ENV_SETTER_H_

#include "base/types/expected.h"
#include "remoting/base/loggable.h"

namespace remoting {

// Fetches the systemd environment variables for the current user and sets
// them for the current process.
// This function will poll the systemd environment until either DISPLAY or
// WAYLAND_DISPLAY is set, or until a timeout occurs.
// This is a synchronous call that is thread-unsafe and will block the current
// thread. It should only be called once at the start of the process.
base::expected<void, Loggable> SetSystemdUserEnvironment();

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SYSTEMD_USER_ENV_SETTER_H_
