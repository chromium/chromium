// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_logging.h"

#include <errno.h>
#include <os/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <limits>
#include <string>

#include "build/build_config.h"
#include "sandbox/mac/sandbox_crash_message.h"

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

  if (level == Level::ERR) {
    sandbox::crash_message::SetCrashMessage(message);
  }

  if (level == Level::FATAL) {
    abort_report_np(message);
  }
}

// |error| is strerror(errno) when a P* logging function is called. Pass
// |nullptr| if no errno is set.
void DoLogging(Level level,
               const char* fmt,
               va_list args,
               const std::string* error) {
  char message[4096];
  int ret = vsnprintf(message, sizeof(message), fmt, args);

  if (ret < 0) {
    SendOsLog(level, "warning: log message could not be formatted");
    return;
  }

  // int |ret| is not negative so casting to a larger type is safe.
  bool truncated = static_cast<unsigned long>(ret) > sizeof(message) - 1;

  std::string final_message = message;
  if (error)
    final_message += ": " + *error;

  SendOsLog(level, final_message.c_str());

  if (truncated) {
    SendOsLog(level, "warning: previous log message truncated");
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
