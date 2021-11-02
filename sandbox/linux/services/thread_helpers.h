// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_THREAD_HELPERS_H_
#define SANDBOX_LINUX_SERVICES_THREAD_HELPERS_H_

#include "base/macros.h"
#include "sandbox/sandbox_export.h"

namespace base { class Thread; }

namespace sandbox {

class SANDBOX_EXPORT ThreadHelpers {
 public:
  // Checks whether the current process is single threaded. |proc_fd|
  // must be a file descriptor to /proc/ and remains owned by the
  // caller.
  static bool IsSingleThreaded(int proc_fd);
  static bool IsSingleThreaded();

  // Crashes if the current process is not single threaded. This will wait
  // on /proc to be updated. In the case where this doesn't crash, this will
  // return promptly. In the case where this does crash, this will first wait
  // for a few ms in Debug mode, a few seconds in Release mode.
  static void AssertSingleThreaded(int proc_fd);
  static void AssertSingleThreaded();

  // Starts |thread| and ensure that it has an entry in /proc/self/task/ from
  // the point of view of the current thread.
  static bool StartThreadAndWatchProcFS(int proc_fd, base::Thread* thread);

  // Stops |thread| and ensure that it does not have an entry in
  // /proc/self/task/ from the point of view of the current thread. This is
  // the way to stop threads before calling IsSingleThreaded().
  static bool StopThreadAndWatchProcFS(int proc_fd, base::Thread* thread);

  static const char* GetAssertSingleThreadedErrorMessageForTests();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ThreadHelpers);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_THREAD_HELPERS_H_
