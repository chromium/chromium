// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/proc_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>

#include "base/check_op.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"

namespace sandbox {
namespace {

struct DIRCloser {
  void operator()(DIR* d) const {
    DCHECK(d);
    PCHECK(0 == closedir(d));
  }
};

typedef std::unique_ptr<DIR, DIRCloser> ScopedDIR;

base::ScopedFD OpenDirectory(const char* path) {
  DCHECK(path);
  base::ScopedFD directory_fd(
      HANDLE_EINTR(open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  PCHECK(directory_fd.is_valid());
  return directory_fd;
}

}  // namespace

int ProcUtil::CountOpenFds(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  int proc_self_fd = HANDLE_EINTR(
      openat(proc_fd, "self/fd/", O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  PCHECK(0 <= proc_self_fd);

  // Ownership of proc_self_fd is transferred here, it must not be closed
  // or modified afterwards except via dir.
  ScopedDIR dir(fdopendir(proc_self_fd));
  CHECK(dir);

  int count = 0;
  struct dirent* de;
  while ((de = readdir(dir.get()))) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
      continue;
    }

    int fd_num;
    CHECK(base::StringToInt(de->d_name, &fd_num));
    if (fd_num == proc_fd || fd_num == proc_self_fd) {
      continue;
    }

    ++count;
  }
  return count;
}

bool ProcUtil::HasOpenDirectory(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  int proc_self_fd =
      openat(proc_fd, "self/fd/", O_DIRECTORY | O_RDONLY | O_CLOEXEC);

  PCHECK(0 <= proc_self_fd);

  // Ownership of proc_self_fd is transferred here, it must not be closed
  // or modified afterwards except via dir.
  ScopedDIR dir(fdopendir(proc_self_fd));
  CHECK(dir);

  struct dirent* de;
  while ((de = readdir(dir.get()))) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
      continue;
    }

    int fd_num;
    CHECK(base::StringToInt(de->d_name, &fd_num));
    if (fd_num == proc_fd || fd_num == proc_self_fd) {
      continue;
    }

    struct stat s;
    // It's OK to use proc_self_fd here, fstatat won't modify it.
    PCHECK(fstatat(proc_self_fd, de->d_name, &s, 0) == 0);
    if (S_ISDIR(s.st_mode)) {
      return true;
    }
  }

  // No open unmanaged directories found.
  return false;
}

bool ProcUtil::HasOpenDirectory() {
  base::ScopedFD proc_fd(
      HANDLE_EINTR(open("/proc/", O_DIRECTORY | O_RDONLY | O_CLOEXEC)));
  return HasOpenDirectory(proc_fd.get());
}

// static
base::ScopedFD ProcUtil::OpenProc() {
  return OpenDirectory("/proc/");
}

}  // namespace sandbox
