// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/logging_internal.h"

#include <os/log.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"

namespace remoting {

namespace {

const char kChromotingLoggingFacility[] = "org.chromium.chromoting";

bool LogMessageToOsLog(logging::LogSeverity severity,
                       const char* file,
                       int line,
                       size_t message_start,
                       const std::string& message) {
  const class OSLog {
   public:
    explicit OSLog()
        : os_log_(
              os_log_create(kChromotingLoggingFacility, "chromium_logging")) {}
    OSLog(const OSLog&) = delete;
    OSLog& operator=(const OSLog&) = delete;
    ~OSLog() { os_release(os_log_); }
    os_log_t get() const { return os_log_; }

   private:
    os_log_t os_log_;
  } log;

  const os_log_type_t os_log_type = [](logging::LogSeverity severity) {
    switch (severity) {
      case logging::LOGGING_INFO:
        return OS_LOG_TYPE_INFO;
      case logging::LOGGING_WARNING:
        return OS_LOG_TYPE_DEFAULT;
      case logging::LOGGING_ERROR:
        return OS_LOG_TYPE_ERROR;
      case logging::LOGGING_FATAL:
        return OS_LOG_TYPE_FAULT;
      case logging::LOGGING_VERBOSE:
        return OS_LOG_TYPE_DEBUG;
      default:
        return OS_LOG_TYPE_DEFAULT;
    }
  }(severity);

  os_log_with_type(log.get(), os_log_type, "%{public}s",
                   message.c_str() + message_start);

  // Don't prevent message from being logged by traditional means.
  return false;
}

}  // namespace

void InitHostLogging() {
  InitHostLoggingCommon();

  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Write logs to syslog as well.
  logging::SetLogMessageHandler(LogMessageToOsLog);
}

}  // namespace remoting
