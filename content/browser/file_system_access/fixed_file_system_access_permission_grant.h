// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FIXED_FILE_SYSTEM_ACCESS_PERMISSION_GRANT_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FIXED_FILE_SYSTEM_ACCESS_PERMISSION_GRANT_H_

#include "content/public/browser/file_system_access_permission_grant.h"

#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

// FileSystemAccessPermissionGrant implementation that returns a fixed value as
// permission status. Used for example to model the permissions for sandboxed
// file systems (which can't change), as well as in tests.
// RequestPermission will immediately call the callback, leaving the status
// unchanged.
class CONTENT_EXPORT FixedFileSystemAccessPermissionGrant
    : public FileSystemAccessPermissionGrant {
 public:
  explicit FixedFileSystemAccessPermissionGrant(PermissionStatus status,
                                                base::FilePath path);

  // FileSystemAccessPermissionGrant:
  PermissionStatus GetStatus() override;
  base::FilePath GetPath() override;
  void RequestPermission(
      GlobalRenderFrameHostId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override;

 protected:
  ~FixedFileSystemAccessPermissionGrant() override;

 private:
  const PermissionStatus status_;
  const base::FilePath path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FIXED_FILE_SYSTEM_ACCESS_PERMISSION_GRANT_H_
