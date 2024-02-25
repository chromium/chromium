// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_SHELL_REMOTE_DEBUGGING_PORT_H_
#define FUCHSIA_WEB_SHELL_REMOTE_DEBUGGING_PORT_H_

#include <optional>

extern const char kRemoteDebuggingPortSwitch[];

namespace base {

class CommandLine;

}  // namespace base

// Return default value of 0 if |command_line| does not have remote debugging
// port switch. If |command_line| contains the appropriate switch, returns the
// remote debugging port specified in the |command_line| or nullopt on parsing
// failure.
std::optional<uint16_t> GetRemoteDebuggingPort(
    const base::CommandLine& command_line);

#endif  // FUCHSIA_WEB_SHELL_REMOTE_DEBUGGING_PORT_H_
