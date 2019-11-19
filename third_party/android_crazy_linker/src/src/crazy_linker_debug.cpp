// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_debug.h"

#include <errno.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace crazy {

#if CRAZY_DEBUG

namespace {

struct LogBuffer {
  LogBuffer() = default;

  void Append(const char* str) {
    int avail = kSize - pos_;
    int ret = snprintf(buffer_ + pos_, avail, "%s", str);
    if (ret >= avail) {
      pos_ = kSize - 1;
    } else {
      pos_ += ret;
    }
  }

  void AppendV(const char* fmt, va_list args) {
    int avail = kSize - pos_;
    int ret = vsnprintf(buffer_ + pos_, kSize - pos_, fmt, args);
    if (ret >= avail) {
      pos_ = kSize - 1;
    } else {
      pos_ += ret;
    }
  }

  void Print() {
    // First, send to stderr.
    fprintf(stderr, "%.*s\n", pos_, buffer_);

#ifdef __ANDROID__
    // Then to the Android log.
    __android_log_write(ANDROID_LOG_INFO, "crazy_linker", buffer_);
#endif
  }

 private:
  static constexpr int kSize = 4096;
  int pos_ = 0;
  char buffer_[kSize];
};

}  // namespace

void Log(const char* location, const char* fmt, ...) {
  int old_errno = errno;
  va_list args;
  va_start(args, fmt);
  {
    LogBuffer log;
    log.Append(location);
    log.Append(": ");
    log.AppendV(fmt, args);
    log.Print();
  }
  va_end(args);
  errno = old_errno;
}

void LogErrno(const char* location, const char* fmt, ...) {
  int old_errno = errno;
  va_list args;
  va_start(args, fmt);
  {
    LogBuffer log;
    log.Append(location);
    log.Append(": ");
    log.AppendV(fmt, args);
    log.Append(": ");
    log.Append(strerror(old_errno));
    log.Print();
  }
  va_end(args);
  errno = old_errno;
}

void AssertionFailure(const char* location, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  {
    LogBuffer log;
    log.Append(location);
    log.Append(": ");
    log.AppendV(fmt, args);
    log.Print();
  }
  va_end(args);
  exit(1);
}

#endif  // CRAZY_DEBUG

}  // namespace crazy
