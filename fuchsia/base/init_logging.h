// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_INIT_LOGGING_H_
#define FUCHSIA_BASE_INIT_LOGGING_H_

namespace base {
class CommandLine;
}

namespace cr_fuchsia {

// Configures logging for the current process based on the supplied
// |command_line|. Returns false if a logging output stream could not
// be created.
bool InitLoggingFromCommandLine(const base::CommandLine& command_line);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_INIT_LOGGING_H_
