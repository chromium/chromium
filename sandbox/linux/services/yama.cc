// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/yama.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/system_headers/linux_prctl.h"

namespace sandbox {

namespace {

// Enable or disable the Yama ptracers restrictions.
// Return false if Yama is not present on this kernel.
bool SetYamaPtracersRestriction(bool enable_restrictions) {
  unsigned long set_ptracer_arg;
  if (enable_restrictions) {
    set_ptracer_arg = 0;
  } else {
    set_ptracer_arg = PR_SET_PTRACER_ANY;
  }

  const int ret = prctl(PR_SET_PTRACER, set_ptracer_arg);
  const int prctl_errno = errno;

  if (0 == ret) {
    return true;
  } else {
    // ENOSYS or EINVAL means Yama is not in the current kernel.
    CHECK(ENOSYS == prctl_errno || EINVAL == prctl_errno);
    return false;
  }
}

bool CanAccessProcFS() {
  static const char kProcfsKernelSysPath[] = "/proc/sys/kernel/";
  int ret = access(kProcfsKernelSysPath, F_OK);
  if (ret) {
    return false;
  }
  return true;
}

}  // namespace

// static
bool Yama::RestrictPtracersToAncestors() {
  return SetYamaPtracersRestriction(true /* enable_restrictions */);
}

// static
bool Yama::DisableYamaRestrictions() {
  return SetYamaPtracersRestriction(false /* enable_restrictions */);
}

// static
int Yama::GetStatus() {
  if (!CanAccessProcFS()) {
    return 0;
  }

  static const char kPtraceScopePath[] = "/proc/sys/kernel/yama/ptrace_scope";

  base::ScopedFD yama_scope(HANDLE_EINTR(open(kPtraceScopePath, O_RDONLY)));

  if (!yama_scope.is_valid()) {
    const int open_errno = errno;
    DCHECK(ENOENT == open_errno);
    // The status is known, yama is not present.
    return STATUS_KNOWN;
  }

  char yama_scope_value = 0;
  ssize_t num_read = HANDLE_EINTR(read(yama_scope.get(), &yama_scope_value, 1));
  PCHECK(1 == num_read);

  switch (yama_scope_value) {
    case '0':
      return STATUS_KNOWN | STATUS_PRESENT;
    case '1':
      return STATUS_KNOWN | STATUS_PRESENT | STATUS_ENFORCING;
    case '2':
    case '3':
      return STATUS_KNOWN | STATUS_PRESENT | STATUS_ENFORCING |
             STATUS_STRICT_ENFORCING;
    default:
      NOTREACHED();
  }
}

// static
bool Yama::IsPresent() { return GetStatus() & STATUS_PRESENT; }

// static
bool Yama::IsEnforcing() { return GetStatus() & STATUS_ENFORCING; }

}  // namespace sandbox
