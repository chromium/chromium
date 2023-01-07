// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_error.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "crazy_linker_debug.h"

namespace crazy {

void Error::Set(const char* message) {
  if (!message)
    message = "";
  strlcpy(buff_, message, sizeof(buff_));

  LOG("--- ERROR: %s", buff_);
}

void Error::Append(const char* message) {
  if (!message)
    return;
  strlcat(buff_, message, sizeof(buff_));

  LOG("--- ERROR: %s", buff_);
}

void Error::Format(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(buff_, sizeof(buff_), fmt, args);
  va_end(args);

  LOG("--- ERROR: %s", buff_);
}

void Error::AppendFormat(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  size_t buff_len = strlen(buff_);
  vsnprintf(buff_ + buff_len, sizeof(buff_) - buff_len, fmt, args);
  va_end(args);

  LOG("--- ERROR: %s", buff_);
}

}  // namespace crazy
