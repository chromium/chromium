// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_SANDBOX_SUPPORT_LINUX_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_SANDBOX_SUPPORT_LINUX_H_

#include <stddef.h>

#include "content/common/content_export.h"

class NaClListener;

namespace content {

// TODO(crbug.com/41470149): Remove this when NaCl is unshipped.
class CONTENT_EXPORT SharedMemoryIPCSupport {
 private:
  friend class ::NaClListener;

  // Returns a file descriptor for a shared memory segment.  The
  // executable flag indicates that the caller intends to use mprotect
  // with PROT_EXEC after making a mapping, but not that it intends to
  // mmap with PROT_EXEC in the first place.  (Some systems, such as
  // ChromeOS, disallow PROT_EXEC in mmap on /dev/shm files but do allow
  // PROT_EXEC in mprotect on mappings from such files.  This function
  // can yield an object that has that constraint.)
  static int MakeSharedMemorySegment(size_t length, bool executable);

  SharedMemoryIPCSupport() = delete;
};

// Gets the well-known file descriptor on which we expect to find the
// sandbox IPC channel.
CONTENT_EXPORT int GetSandboxFD();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_SANDBOX_SUPPORT_LINUX_H_
