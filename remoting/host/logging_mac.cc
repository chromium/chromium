// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/logging.h"

#include <asl.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"

namespace remoting {

namespace {

const char kChromotingLoggingFacility[] = "org.chromium.chromoting";

// Define a scoper for objects allocated by asl_new.
struct ScopedAslMsgTraits {
  static aslmsg InvalidValue() {
    return nullptr;
  }
  static void Free(aslmsg msg) {
    asl_free(msg);
  }
};
typedef base::ScopedGeneric<aslmsg, ScopedAslMsgTraits> ScopedAslMsg;

// Logging message handler that writes to syslog.
// The log can be obtained by running the following in a terminal:
// syslog -k Facility org.chromium.chromoting
bool LogMessageToAsl(
    logging::LogSeverity severity,
    const char* file,
    int line,
    size_t message_start,
    const std::string& message) {
  int level;
  switch(severity) {
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

}  // namespace

void InitHostLogging() {
  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  // Write logs to syslog as well.
  logging::SetLogMessageHandler(LogMessageToAsl);
}

}  // namespace remoting
