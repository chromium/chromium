// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SCOPED_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SCOPED_LOGGER_H_

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

#if !DCHECK_IS_ON()

#define WTF_CREATE_SCOPED_LOGGER(...) ((void)0)
#define WTF_CREATE_SCOPED_LOGGER_IF(...) ((void)0)
#define WTF_APPEND_SCOPED_LOGGER(...) ((void)0)

#else

// ScopedLogger wraps log messages in parentheses, with indentation proportional
// to the number of instances. This makes it easy to see the flow of control in
// the output, particularly when instrumenting recursive functions.
//
// NOTE: This class is a debugging tool, not intended for use by checked-in
// code. Please do not remove it.
//
class WTF_EXPORT ScopedLogger {
  DISALLOW_NEW();

 public:
  // The first message is passed to the constructor.  Additional messages for
  // the same scope can be added with log(). If condition is false, produce no
  // output and do not create a scope.
  PRINTF_FORMAT(3, 4) ScopedLogger(bool condition, const char* format, ...);
  ~ScopedLogger();
  PRINTF_FORMAT(2, 3) void Log(const char* format, ...);

 private:
  FRIEND_TEST_ALL_PREFIXES(ScopedLoggerTest, ScopedLogger);
  using PrintFunctionPtr = void (*)(const char* format, va_list args);

  // Note: not thread safe.
  static void SetPrintFuncForTests(PrintFunctionPtr);

  void Init(const char* format, va_list args);
  void WriteNewlineIfNeeded();
  void Indent();
  void Print(const char* format, ...);
  void PrintIndent();
  static ScopedLogger*& Current();

  ScopedLogger* const parent_;
  bool multiline_;  // The ')' will go on the same line if there is only one
                    // entry.
  static PrintFunctionPtr print_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogger);
};

#define WTF_CREATE_SCOPED_LOGGER(name, ...) \
  WTF::ScopedLogger name(true, __VA_ARGS__)
#define WTF_CREATE_SCOPED_LOGGER_IF(name, condition, ...) \
  WTF::ScopedLogger name(condition, __VA_ARGS__)
#define WTF_APPEND_SCOPED_LOGGER(name, ...) (name.Log(__VA_ARGS__))

#endif  // !DCHECK_IS_ON()

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SCOPED_LOGGER_H_
