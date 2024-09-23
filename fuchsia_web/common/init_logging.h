// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_INIT_LOGGING_H_
#define FUCHSIA_WEB_COMMON_INIT_LOGGING_H_

#include <string_view>

namespace base {
class CommandLine;
}

// Configures logging for the current process based on the supplied
// |command_line|. Returns false if a logging output stream could not
// be created.
bool InitLoggingFromCommandLine(const base::CommandLine& command_line);

// Same as InitLoggingFromCommandLine but defaults to "stderr" if the logging
// target is not specified.
bool InitLoggingFromCommandLineDefaultingToStderrForTest(
    base::CommandLine* command_line);

// Emits an INFO log indicating that |component_name| is starting along with the
// version. Call during the startup of a Fuchsia Component (e.g., in main())
// after InitLoggingFromCommandLine() succeeds.
void LogComponentStartWithVersion(std::string_view component_name);

#endif  // FUCHSIA_WEB_COMMON_INIT_LOGGING_H_
