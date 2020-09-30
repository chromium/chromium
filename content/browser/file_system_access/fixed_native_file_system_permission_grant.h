// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FIXED_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FIXED_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_

#include "content/public/browser/native_file_system_permission_grant.h"

#include "content/common/content_export.h"

namespace content {

// NativeFileSystemPermissionGrant implementation that returns a fixed value as
// permission status. Used for example to model the permissions for sandboxed
// file systems (which can't change), as well as in tests.
// RequestPermission will immediately call the callback, leaving the status
// unchanged.
class CONTENT_EXPORT FixedNativeFileSystemPermissionGrant
    : public NativeFileSystemPermissionGrant {
 public:
  explicit FixedNativeFileSystemPermissionGrant(PermissionStatus status,
                                                base::FilePath path);

  // NativeFileSystemPermissionGrant:
  PermissionStatus GetStatus() override;
  base::FilePath GetPath() override;
  void RequestPermission(
      GlobalFrameRoutingId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override;

 protected:
  ~FixedNativeFileSystemPermissionGrant() override;

 private:
  const PermissionStatus status_;
  const base::FilePath path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FIXED_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
