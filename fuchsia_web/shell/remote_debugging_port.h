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

// Returns the debugging port parsed from the |command_line|; if no flag is
// provided or the value is invalid, returns nullopt.
std::optional<uint16_t> GetRemoteDebuggingPort(
    const base::CommandLine& command_line);

#endif  // FUCHSIA_WEB_SHELL_REMOTE_DEBUGGING_PORT_H_
