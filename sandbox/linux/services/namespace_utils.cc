// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/namespace_utils.h"

#include <fcntl.h>
#include <sched.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/safe_sprintf.h"

namespace sandbox {

namespace {
const char kProcSelfSetgroups[] = "/proc/self/setgroups";
}  // namespace

// static
bool NamespaceUtils::WriteToIdMapFile(const char* map_file, generic_id_t id) {
  // This function needs to be async-signal-safe, as it may be called in between
  // fork and exec.

  int fd = HANDLE_EINTR(open(map_file, O_WRONLY));
  if (fd == -1) {
    return false;
  }

  const generic_id_t inside_id = id;
  const generic_id_t outside_id = id;

  char mapping[64];
  const ssize_t len =
      base::strings::SafeSPrintf(mapping, "%d %d 1\n", inside_id, outside_id);
  const ssize_t rc = HANDLE_EINTR(write(fd, mapping, len));
  RAW_CHECK(IGNORE_EINTR(close(fd)) == 0);
  return rc == len;
}

// static
bool NamespaceUtils::KernelSupportsUnprivilegedNamespace(int type) {
  // As of Linux 3.8, /proc/self/ns/* files exist for all namespace types. Since
  // user namespaces were added in 3.8, it is OK to rely on the existence of
  // /proc/self/ns/*.
  if (!base::PathExists(base::FilePath("/proc/self/ns/user"))) {
    return false;
  }

  const char* path;
  switch (type) {
    case CLONE_NEWUSER:
      return true;
    case CLONE_NEWIPC:
      path = "/proc/self/ns/ipc";
      break;
    case CLONE_NEWNET:
      path = "/proc/self/ns/net";
      break;
    case CLONE_NEWNS:
      path = "/proc/self/ns/mnt";
      break;
    case CLONE_NEWPID:
      path = "/proc/self/ns/pid";
      break;
    case CLONE_NEWUTS:
      path = "/proc/self/ns/uts";
      break;
    default:
      NOTREACHED();
      return false;
  }

  return base::PathExists(base::FilePath(path));
}

// static
bool NamespaceUtils::KernelSupportsDenySetgroups() {
  return base::PathExists(base::FilePath(kProcSelfSetgroups));
}

// static
bool NamespaceUtils::DenySetgroups() {
  // This function needs to be async-signal-safe.
  int fd = HANDLE_EINTR(open(kProcSelfSetgroups, O_WRONLY));
  if (fd == -1) {
    return false;
  }

  static const char kDeny[] = "deny";
  const ssize_t len = sizeof(kDeny) - 1;
  const ssize_t rc = HANDLE_EINTR(write(fd, kDeny, len));
  RAW_CHECK(IGNORE_EINTR(close(fd)) == 0);
  return rc == len;
}

}  // namespace sandbox
