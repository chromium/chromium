// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_error.h"

#include <ostream>
#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"

namespace remoting {

TerminalError::TerminalError(const base::Location& location,
                             Reason reason,
                             std::string message,
                             std::optional<int32_t> system_error_code)
    : location(location),
      reason(reason),
      message(std::move(message)),
      system_error_code(system_error_code) {}

TerminalError::TerminalError(const TerminalError&) = default;
TerminalError::TerminalError(TerminalError&&) = default;
TerminalError& TerminalError::operator=(const TerminalError&) = default;
TerminalError& TerminalError::operator=(TerminalError&&) = default;
TerminalError::~TerminalError() = default;

// static
TerminalError TerminalError::FromSystemError(const base::Location& location,
                                             Reason reason,
                                             std::string_view operation,
                                             int32_t system_error_code) {
  return TerminalError(
      location, reason,
      base::StrCat(
          {operation, " failed: ",
           logging::SystemErrorCodeToString(
               static_cast<logging::SystemErrorCode>(system_error_code))}),
      system_error_code);
}

std::ostream& operator<<(std::ostream& stream, const TerminalError& error) {
  stream << error.message;
  // A default-constructed base::Location has no file name.
  if (const char* file_name = error.location.file_name()) {
    stream << " [" << file_name << ":" << error.location.line_number() << "]";
  }
  return stream;
}

}  // namespace remoting
