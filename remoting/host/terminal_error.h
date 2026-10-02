// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TERMINAL_ERROR_H_
#define REMOTING_HOST_TERMINAL_ERROR_H_

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "base/location.h"

namespace remoting {

// Describes a failure in a terminal session operation, with enough detail to
// be reported to the client.
struct TerminalError {
  // Broad category of the failure. More specific details are provided by the
  // location, message and system error code.
  enum class Reason {
    // An unexpected error occurred.
    kInternalError,
    // The operation could not be performed now, but may succeed if retried.
    kBusy,
    // A pseudo-terminal could not be allocated.
    kPtyError,
    // tmux is not installed.
    kTmuxMissing,
    // The terminal process could not be launched.
    kLaunchFailed,
  };

  TerminalError(const base::Location& location,
                Reason reason,
                std::string message,
                std::optional<int32_t> system_error_code = std::nullopt);
  TerminalError(const TerminalError&);
  TerminalError(TerminalError&&);
  TerminalError& operator=(const TerminalError&);
  TerminalError& operator=(TerminalError&&);
  ~TerminalError();

  // Creates an error for a failed system call. `operation` is a short
  // description of what failed (e.g. "posix_openpt"), and `system_error_code`
  // is interpreted as a logging::SystemErrorCode (errno on POSIX,
  // GetLastError() on Windows). The description of the error code is appended
  // to the message.
  static TerminalError FromSystemError(const base::Location& location,
                                       Reason reason,
                                       std::string_view operation,
                                       int32_t system_error_code);

  // Where the error occurred.
  base::Location location;

  // The category of the error.
  Reason reason;

  // A human-readable description of the error.
  std::string message;

  // The platform error code, if the error was caused by a failed system call.
  std::optional<int32_t> system_error_code;
};

std::ostream& operator<<(std::ostream& stream, const TerminalError& error);

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_ERROR_H_
