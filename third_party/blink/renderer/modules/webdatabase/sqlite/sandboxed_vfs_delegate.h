// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SANDBOXED_VFS_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SANDBOXED_VFS_DELEGATE_H_

#include "sql/sandboxed_vfs.h"

namespace blink {

class SandboxedVfsDelegate : public sql::SandboxedVfs::Delegate {
 public:
  SandboxedVfsDelegate();
  ~SandboxedVfsDelegate() override;

  // sql::SandboxedVfs::Delegate implementation:
  base::File OpenFile(const base::FilePath& file_path,
                      int sqlite_requested_flags) override;
  int DeleteFile(const base::FilePath& file_path, bool sync_dir) override;
  std::optional<sql::SandboxedVfs::PathAccessInfo> GetPathAccess(
      const base::FilePath& file_path) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SANDBOXED_VFS_DELEGATE_H_
