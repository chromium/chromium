// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_logging.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <limits>
#include <string>

#include "build/build_config.h"

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

#if defined(ARCH_CPU_X86_64)
#define ABORT()                                                                \
  {                                                                            \
    asm volatile(                                                              \
        "int3; ud2; push %0;" ::"i"(static_cast<unsigned char>(__COUNTER__))); \
    __builtin_unreachable();                                                   \
  }
#elif defined(ARCH_CPU_ARM64)
#define ABORT()                                                             \
  {                                                                         \
    asm volatile("udf %0;" ::"i"(static_cast<unsigned char>(__COUNTER__))); \
    __builtin_unreachable();                                                \
  }
#endif

extern "C" {
void abort_report_np(const char*, ...);
}

namespace sandbox::logging {

namespace {

enum class Level { FATAL, ERR, WARN, INFO };

#if defined(USE_ASL)

void SendAslLog(Level level, const char* message) {
  class ASLClient {
   public:
    explicit ASLClient()
        : client_(asl_open(nullptr,
                           "com.apple.console",
                           ASL_OPT_STDERR | ASL_OPT_NO_DELAY)) {}
    ~ASLClient() { asl_close(client_); }

    aslclient get() const { return client_; }

    ASLClient(const ASLClient&) = delete;
    ASLClient& operator=(const ASLClient&) = delete;

   private:
    aslclient client_;
  } asl_client;

  class ASLMessage {
   public:
    ASLMessage() : message_(asl_new(ASL_TYPE_MSG)) {}
    ~ASLMessage() { asl_free(message_); }

    aslmsg get() const { return message_; }

    ASLMessage(const ASLMessage&) = delete;
    ASLMessage& operator=(const ASLMessage&) = delete;

   private:
    aslmsg message_;
  } asl_message;

  // By default, messages are only readable by the admin group. Explicitly
  // make them readable by the user generating the messages.
  char euid_string[12];
  snprintf(euid_string, sizeof(euid_string) / sizeof(euid_string[0]), "%d",
           geteuid());
  asl_set(asl_message.get(), ASL_KEY_READ_UID, euid_string);

  std::string asl_level_string;
  switch (level) {
    case Level::FATAL:
      asl_level_string = ASL_STRING_CRIT;
      break;
    case Level::ERR:
      asl_level_string = ASL_STRING_ERR;
      break;
    case Level::WARN:
      asl_level_string = ASL_STRING_WARNING;
      break;
    case Level::INFO:
    default:
      asl_level_string = ASL_STRING_INFO;
      break;
  }

  asl_set(asl_message.get(), ASL_KEY_LEVEL, asl_level_string.c_str());
  asl_set(asl_message.get(), ASL_KEY_MSG, message);
  asl_send(asl_client.get(), asl_message.get());

  if (level == Level::FATAL) {
    abort_report_np(message);
  }
}

#else

void SendOsLog(Level level, const char* message) {
  const class OSLog {
   public:
    explicit OSLog()
        : os_log_(os_log_create("org.chromium.sandbox", "chromium_logging")) {}
    OSLog(const OSLog&) = delete;
    OSLog& operator=(const OSLog&) = delete;
    ~OSLog() { os_release(os_log_); }
    os_log_t get() const { return os_log_; }

   private:
    os_log_t os_log_;
  } log;

  const os_log_type_t os_log_type = [](Level level) {
    switch (level) {
      case Level::FATAL:
        return OS_LOG_TYPE_FAULT;
      case Level::ERR:
        return OS_LOG_TYPE_ERROR;
      case Level::WARN:
        return OS_LOG_TYPE_DEFAULT;
      case Level::INFO:
        return OS_LOG_TYPE_INFO;
    }
  }(level);

  os_log_with_type(log.get(), os_log_type, "%{public}s", message);

  if (level == Level::FATAL) {
    abort_report_np(message);
  }
}

#endif  // defined(USE_ASL)

// |error| is strerror(errno) when a P* logging function is called. Pass
// |nullptr| if no errno is set.
void DoLogging(Level level,
               const char* fmt,
               va_list args,
               const std::string* error) {
  char message[4096];
  int ret = vsnprintf(message, sizeof(message), fmt, args);

  if (ret < 0) {
#if defined(USE_ASL)
    SendAslLog(level, "warning: log message could not be formatted");
#else
    SendOsLog(level, "warning: log message could not be formatted");
#endif  // defined(USE_ASL)
    return;
  }

  // int |ret| is not negative so casting to a larger type is safe.
  bool truncated = static_cast<unsigned long>(ret) > sizeof(message) - 1;

  std::string final_message = message;
  if (error)
    final_message += ": " + *error;

#if defined(USE_ASL)
  SendAslLog(level, final_message.c_str());
#else
  SendOsLog(level, final_message.c_str());
#endif  // defined(USE_ASL)

  if (truncated) {
#if defined(USE_ASL)
    SendAslLog(level, "warning: previous log message truncated");
#else
    SendOsLog(level, "warning: previous log message truncated");
#endif  // defined(USE_ASL)
  }
}

}  // namespace

void Info(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  DoLogging(Level::INFO, fmt, args, nullptr);
  va_end(args);
}

void Warning(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  DoLogging(Level::WARN, fmt, args, nullptr);
  va_end(args);
}

void Error(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  DoLogging(Level::ERR, fmt, args, nullptr);
  va_end(args);
}

void Fatal(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  DoLogging(Level::FATAL, fmt, args, nullptr);
  va_end(args);

  ABORT();
}

void PError(const char* fmt, ...) {
  std::string error = strerror(errno);
  va_list args;
  va_start(args, fmt);
  DoLogging(Level::ERR, fmt, args, &error);
  va_end(args);
}

void PFatal(const char* fmt, ...) {
  std::string error = strerror(errno);
  va_list args;
  va_start(args, fmt);
  DoLogging(Level::FATAL, fmt, args, &error);
  va_end(args);

  ABORT();
}

}  // namespace sandbox::logging
