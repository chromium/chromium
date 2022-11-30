// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_PROC_UTIL_H_
#define SANDBOX_LINUX_SERVICES_PROC_UTIL_H_

#include "base/files/scoped_file.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

class SANDBOX_EXPORT ProcUtil {
 public:
  ProcUtil() = delete;
  ProcUtil(const ProcUtil&) = delete;
  ProcUtil& operator=(const ProcUtil&) = delete;

  // Returns the number of file descriptors in the current process's FD
  // table, excluding |proc_fd|, which should be a file descriptor for
  // /proc/.
  static int CountOpenFds(int proc_fd);

  // Checks whether the current process has any directory file descriptor open.
  // Directory file descriptors are "capabilities" that would let a process use
  // system calls such as openat() to bypass restrictions such as
  // DropFileSystemAccess().
  // Sometimes it's useful to call HasOpenDirectory() after file system access
  // has been dropped. In this case, |proc_fd| should be a file descriptor to
  // /proc/. The file descriptor in |proc_fd| will be ignored by
  // HasOpenDirectory() and remains owned by the caller. It is very important
  // for the caller to close it.
  [[nodiscard]] static bool HasOpenDirectory(int proc_fd);
  [[nodiscard]] static bool HasOpenDirectory();

  // Open /proc/ or crash if not possible.
  static base::ScopedFD OpenProc();
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_PROC_UTIL_H_
