// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "services/network/sandboxed_vfs_delegate.h"

namespace network {

SandboxedVfsDelegate::SandboxedVfsDelegate() = default;

SandboxedVfsDelegate::~SandboxedVfsDelegate() = default;

base::File SandboxedVfsDelegate::OpenFile(const base::FilePath& file_path,
                                          int sqlite_requested_flags) {
  NOTREACHED_NORETURN();
}

absl::optional<sql::SandboxedVfs::PathAccessInfo>
SandboxedVfsDelegate::GetPathAccess(const base::FilePath& file_path) {
  sql::SandboxedVfs::PathAccessInfo info;
  const char* const c_path = file_path.value().c_str();
  base::stat_wrapper_t current_stat;
  if (base::File::Stat(c_path, &current_stat) == 0) {
    info.can_read = base::FILE_PERMISSION_READ_BY_USER & current_stat.st_mode;
    info.can_write = base::FILE_PERMISSION_WRITE_BY_USER & current_stat.st_mode;
  }
  return info;
}

int SandboxedVfsDelegate::DeleteFile(const base::FilePath& file_path,
                                     bool sync_dir) {
  NOTREACHED_NORETURN();
}

bool SandboxedVfsDelegate::SetFileLength(const base::FilePath& file_path,
                                         base::File& file,
                                         size_t size) {
  if (!file.IsValid()) {
    return false;
  }
  return file.SetLength(size);
}

void SandboxedVfsDelegate::InvalidateFileBrokerPath(
    const base::FilePath& path) {
  NOTREACHED();
}

}  // namespace network
