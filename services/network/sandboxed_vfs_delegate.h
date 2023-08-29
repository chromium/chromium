// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SANDBOXED_VFS_DELEGATE_H_
#define SERVICES_NETWORK_SANDBOXED_VFS_DELEGATE_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "sql/sandboxed_vfs.h"

namespace network {

// This class is only used when the network service is sandboxed on Android.
// Otherwise a default VFS is used.
class COMPONENT_EXPORT(NETWORK_SERVICE) SandboxedVfsDelegate
    : public sql::SandboxedVfs::Delegate {
 public:
  SandboxedVfsDelegate();
  ~SandboxedVfsDelegate() override;

  // sql::SandboxedVfs::Delegate implementation:
  base::File OpenFile(const base::FilePath& file_path,
                      int sqlite_requested_flags) override;
  absl::optional<sql::SandboxedVfs::PathAccessInfo> GetPathAccess(
      const base::FilePath& file_path) override;
  int DeleteFile(const base::FilePath& file_path, bool sync_dir) override;
  bool SetFileLength(const base::FilePath& file_path,
                     base::File& file,
                     size_t size) override;

  void InvalidateFileBrokerPath(const base::FilePath& path);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SANDBOXED_VFS_DELEGATE_H_
