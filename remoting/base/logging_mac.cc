// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/logging_internal.h"

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"

#include <AvailabilityMacros.h>
#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#define USE_ASL
#endif

#if defined(USE_ASL)
#include <asl.h>
#else
#include <os/log.h>
#endif

namespace remoting {

namespace {

const char kChromotingLoggingFacility[] = "org.chromium.chromoting";

#if defined(USE_ASL)

// Define a scoper for objects allocated by asl_new.
struct ScopedAslMsgTraits {
  static aslmsg InvalidValue() { return nullptr; }
  static void Free(aslmsg msg) { asl_free(msg); }
};
typedef base::ScopedGeneric<aslmsg, ScopedAslMsgTraits> ScopedAslMsg;

// Logging message handler that writes to syslog.
// The log can be obtained by running the following in a terminal:
// syslog -k Facility org.chromium.chromoting
bool LogMessageToAsl(logging::LogSeverity severity,
                     const char* file,
                     int line,
                     size_t message_start,
                     const std::string& message) {
  int level;
  switch (severity) {
    case logging::LOG_INFO:
      level = ASL_LEVEL_NOTICE;
      break;
    case logging::LOG_WARNING:
      level = ASL_LEVEL_WARNING;
      break;
    case logging::LOG_ERROR:
    case logging::LOG_FATAL:
      level = ASL_LEVEL_ERR;
      break;
    default:
      // 'notice' is the lowest priority that the asl libraries will log by
      // default.
      level = ASL_LEVEL_NOTICE;
      break;
  }

  ScopedAslMsg asl_message(asl_new(ASL_TYPE_MSG));
  if (!asl_message.is_valid())
    return false;

  if (asl_set(asl_message.get(), ASL_KEY_FACILITY,
              kChromotingLoggingFacility) != 0)
    return false;

  if (asl_set(asl_message.get(), ASL_KEY_LEVEL,
              base::NumberToString(level).c_str()) != 0)
    return false;

  // Restrict read access to the message to root and the current user.
  if (asl_set(asl_message.get(), ASL_KEY_READ_UID,
              base::NumberToString(geteuid()).c_str()) != 0)
    return false;

  if (asl_set(asl_message.get(), ASL_KEY_MSG,
              message.c_str() + message_start) != 0)
    return false;

  asl_send(nullptr, asl_message.get());

  // Don't prevent message from being logged by traditional means.
  return false;
}

#else

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

#endif  // defined(USE_ASL)

}  // namespace

void InitHostLogging() {
  InitHostLoggingCommon();

  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Write logs to syslog as well.
#if defined(USE_ASL)
  logging::SetLogMessageHandler(LogMessageToAsl);
#else
  logging::SetLogMessageHandler(LogMessageToOsLog);
#endif  // defined(USE_ASL)
}

}  // namespace remoting
