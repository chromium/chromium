// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/scoped_logger.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

#if DCHECK_IS_ON()

namespace WTF {

ScopedLogger::ScopedLogger(bool condition, const char* format, ...)
    : parent_(condition ? Current() : nullptr), multiline_(false) {
  if (!condition)
    return;

  va_list args;
  va_start(args, format);
  Init(format, args);
  va_end(args);
}

ScopedLogger::~ScopedLogger() {
  if (Current() == this) {
    if (multiline_)
      Indent();
    else
      Print(" ");
    Print(")\n");
    Current() = parent_;
  }
}

void ScopedLogger::SetPrintFuncForTests(PrintFunctionPtr ptr) {
  print_func_ = ptr;
}

void ScopedLogger::Init(const char* format, va_list args) {
  Current() = this;
  if (parent_)
    parent_->WriteNewlineIfNeeded();
  Indent();
  Print("( ");
  print_func_(format, args);
}

void ScopedLogger::WriteNewlineIfNeeded() {
  if (!multiline_) {
    Print("\n");
    multiline_ = true;
  }
}

void ScopedLogger::Indent() {
  if (parent_) {
    parent_->Indent();
    PrintIndent();
  }
}

void ScopedLogger::Log(const char* format, ...) {
  if (Current() != this)
    return;

  va_list args;
  va_start(args, format);

  WriteNewlineIfNeeded();
  Indent();
  PrintIndent();
  print_func_(format, args);
  Print("\n");

  va_end(args);
}

void ScopedLogger::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  print_func_(format, args);
  va_end(args);
}

void ScopedLogger::PrintIndent() {
  Print("  ");
}

ScopedLogger*& ScopedLogger::Current() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ScopedLogger*>, ref, ());
  return *ref;
}

ScopedLogger::PrintFunctionPtr ScopedLogger::print_func_ =
    vprintf_stderr_common;

}  // namespace WTF

#endif  // DCHECK_IS_ON
