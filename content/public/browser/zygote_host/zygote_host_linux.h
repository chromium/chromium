// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_LINUX_H_
#define CONTENT_PUBLIC_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_LINUX_H_

#include <unistd.h>

#include "base/process/process.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/files/platform_file.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {

// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/zygote.md

// The zygote host is an interface, in the browser process, to the zygote
// process.
class ZygoteHost {
 public:
  // Returns the singleton instance.
  static CONTENT_EXPORT ZygoteHost* GetInstance();

  virtual ~ZygoteHost() {}

  // Returns the pid of the Zygote process.
  virtual bool IsZygotePid(pid_t pid) = 0;

  // Returns an int which is a bitmask of kSandboxLinux* values. Only valid
  // after the first render has been forked.
  virtual int GetRendererSandboxStatus() = 0;

  // Adjust the OOM score of the given renderer's PID.  The allowed
  // range for the score is [0, 1000], where higher values are more
  // likely to be killed by the OOM killer.
  virtual void AdjustRendererOOMScore(base::ProcessHandle process_handle,
                                      int score) = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Reinitialize logging for the Zygote processes. Needed on ChromeOS, which
  // switches to a log file in the user's home directory once they log in.
  virtual void ReinitializeLogging(uint32_t logging_dest,
                                   base::PlatformFile log_file_fd) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_LINUX_H_
