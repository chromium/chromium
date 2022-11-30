// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_DIE_H__
#define SANDBOX_LINUX_SECCOMP_BPF_DIE_H__

#include "sandbox/sandbox_export.h"

namespace sandbox {

// This is the main API for using this file. Prints a error message and
// exits with a fatal error. This is not async-signal safe.
#define SANDBOX_DIE(m) sandbox::Die::SandboxDie(m, __FILE__, __LINE__)

// An async signal safe version of the same API. Won't print the filename
// and line numbers.
#define RAW_SANDBOX_DIE(m) sandbox::Die::RawSandboxDie(m)

// Adds an informational message to the log file or stderr as appropriate.
#define SANDBOX_INFO(m) sandbox::Die::SandboxInfo(m, __FILE__, __LINE__)

class SANDBOX_EXPORT Die {
 public:
  Die() = delete;
  Die(const Die&) = delete;
  Die& operator=(const Die&) = delete;

  // Terminate the program, even if the current sandbox policy prevents some
  // of the more commonly used functions used for exiting.
  // Most users would want to call SANDBOX_DIE() instead, as it logs extra
  // information. But calling ExitGroup() is correct and in some rare cases
  // preferable. So, we make it part of the public API.
  static void ExitGroup() __attribute__((noreturn));

  // This method gets called by SANDBOX_DIE(). There is normally no reason
  // to call it directly unless you are defining your own exiting macro.
  static void SandboxDie(const char* msg, const char* file, int line)
      __attribute__((noreturn));

  static void RawSandboxDie(const char* msg) __attribute__((noreturn));

  // This method gets called by SANDBOX_INFO(). There is normally no reason
  // to call it directly unless you are defining your own logging macro.
  static void SandboxInfo(const char* msg, const char* file, int line);

  // Writes a message to stderr. Used as a fall-back choice, if we don't have
  // any other way to report an error.
  static void LogToStderr(const char* msg, const char* file, int line);

  // We generally want to run all exit handlers. This means, on SANDBOX_DIE()
  // we should be calling LOG(FATAL). But there are some situations where
  // we just need to print a message and then terminate. This would typically
  // happen in cases where we consume the error message internally (e.g. in
  // unit tests or in the supportsSeccompSandbox() method).
  static void EnableSimpleExit() { simple_exit_ = true; }

  // Sometimes we need to disable all informational messages (e.g. from within
  // unittests).
  static void SuppressInfoMessages(bool flag) { suppress_info_ = flag; }

 private:
  static bool simple_exit_;
  static bool suppress_info_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_DIE_H__
