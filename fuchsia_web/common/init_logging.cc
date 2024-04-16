// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/init_logging.h"

#include <string_view>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/version_info/version_info.h"

// These values must match content/public/common/content_switches.cc so that
// the values will be passed to child processes in projects that Chromium's
// Content layer.
constexpr char kEnableLogging[] = "enable-logging";
constexpr char kLogFile[] = "log-file";

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

bool InitLoggingFromCommandLineDefaultingToStderrForTest(
    base::CommandLine* command_line) {
  // Set logging to stderr if not specified.
  if (!command_line->HasSwitch(kEnableLogging)) {
    command_line->AppendSwitchNative(kEnableLogging, "stderr");
  }

  return InitLoggingFromCommandLine(*command_line);
}

void LogComponentStartWithVersion(std::string_view component_name) {
  std::string version_string = base::StringPrintf(
      "Starting %.*s %s", base::saturated_cast<int>(component_name.length()),
      component_name.data(), version_info::GetVersionNumber().data());
#if !defined(OFFICIAL_BUILD)
  version_string +=
      base::StrCat({" (built at ", version_info::GetLastChange(), ")"});
#endif  // !defined(OFFICIAL_BUILD)

  LOG(INFO) << version_string;
}
