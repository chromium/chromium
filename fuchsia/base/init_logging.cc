// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/init_logging.h"

#include "base/command_line.h"

namespace cr_fuchsia {
namespace {

// These are intended to match those in content_switches.cc.
constexpr char kEnableLogging[] = "enable-logging";
constexpr char kLogFile[] = "log-file";

}  // namespace

bool InitLoggingFromCommandLine(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  if (command_line.GetSwitchValueASCII(kEnableLogging) == "stderr") {
    settings.logging_dest = logging::LOG_TO_STDERR;
  } else {
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  }
  if (command_line.HasSwitch(kLogFile)) {
    settings.logging_dest |= logging::LOG_TO_FILE;
    settings.log_file_path = command_line.GetSwitchValueASCII(kLogFile).c_str();
    settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  }
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
  return logging::InitLogging(settings);
}

}  // namespace cr_fuchsia
