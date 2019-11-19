// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_DEBUG_H
#define CRAZY_LINKER_DEBUG_H

// Set CRAZY_DEBUG on the command-line to 1 to enable debugging support.
// This really means adding traces that will be sent to both stderr
// and the logcat during execution.
#ifndef CRAZY_DEBUG
#define CRAZY_DEBUG 0
#endif

// Define COMPILE_ASSERT using the implementation described in:
// http://chromium.googlesource.com/chromium/src.git/+/539fc831/base/basictypes.h
#if __cplusplus >= 201103L

#define COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)

#else

template <bool> struct CompileAssert { };
#define COMPILE_ASSERT(expr, msg) \
  typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1] \
  __attribute__((unused))

#endif  // __cplusplus >= 201103L

namespace crazy {

#if CRAZY_DEBUG

void Log(const char* location, const char* fmt, ...);
void LogErrno(const char* location, const char* fmt, ...);
void AssertionFailure(const char* location, const char* fmt, ...);

#define LOG(...) ::crazy::Log(__PRETTY_FUNCTION__, __VA_ARGS__)
#define LOG_ERRNO(...) ::crazy::LogErrno(__PRETTY_FUNCTION__, __VA_ARGS__)

// NOTE: This form of ASSERT() that can be used within constexpr methods, inside
// a comma operation, as in:
//   return ASSERT(cond), <result>;
// Which will be equivalent to:
//   ASSERT(cond);
//   return <result>;
#define ASSERT(cond, ...)                                                \
  (!(cond) ? ::crazy::AssertionFailure(__PRETTY_FUNCTION__, __VA_ARGS__) \
           : (void)0)

#else  // !CRAZY_DEBUG

#define LOG(...) ((void)0)
#define LOG_ERRNO(...) ((void)0)
#define ASSERT(cond, ...) ((void)(cond))

#endif  // !CRAZY_DEBUG

// Conditional logging.
#define LOG_IF(cond, ...) \
  do {                    \
    if ((cond))           \
      LOG(__VA_ARGS__);   \
  } while (0)

#define LOG_ERRNO_IF(cond, ...) \
  do {                          \
    if ((cond))                 \
      LOG_ERRNO(__VA_ARGS__);   \
  } while (0)

}  // namespace crazy

#endif  // CRAZY_LINKER_DEBUG_H
