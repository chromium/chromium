// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides integration with Google-style "base/logging.h" assertions
// for Skia SkASSERT. If you don't want this, you can link with another file
// that provides integration with the logging of your choice.

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkTypes.h"

void SkDebugf_FileLine(const char* file, int line, const char* format, ...) {
#if DCHECK_IS_ON()
  int severity = logging::LOGGING_ERROR;
#else
  int severity = logging::LOGGING_INFO;
#endif
  if (severity < logging::GetMinLogLevel())
    return;

  va_list ap;
  va_start(ap, format);

  std::string msg;
  base::StringAppendV(&msg, format, ap);
  va_end(ap);

  logging::LogMessage(file, line, severity).stream() << msg;
}

void SkAbort_FileLine(const char* file, int line, const char* format, ...) {
  int severity = logging::LOGGING_FATAL;

  va_list ap;
  va_start(ap, format);

  std::string msg;
  base::StringAppendV(&msg, format, ap);
  va_end(ap);

  logging::LogMessage(file, line, severity).stream() << msg;
  sk_abort_no_print();
  // Extra safety abort().
  abort();
}
