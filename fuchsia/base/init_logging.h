// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_INIT_LOGGING_H_
#define FUCHSIA_BASE_INIT_LOGGING_H_

#include "base/strings/string_piece_forward.h"

namespace base {
class CommandLine;
}

namespace cr_fuchsia {

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
void LogComponentStartWithVersion(base::StringPiece component_name);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_INIT_LOGGING_H_
