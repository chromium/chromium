// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"

namespace content {

MockFileSystemAccessPermissionContext::MockFileSystemAccessPermissionContext() =
    default;
MockFileSystemAccessPermissionContext::
    ~MockFileSystemAccessPermissionContext() = default;

void MockFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    const PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  ConfirmSensitiveEntryAccess_(origin, path_info, handle_type, user_action,
                               frame_id, callback);
}

void MockFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<FileSystemAccessWriteItem> item,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  PerformAfterWriteChecks_(item.get(), frame_id, callback);
}

bool MockFileSystemAccessPermissionContext::IsFileTypeDangerous(
    const base::FilePath& path,
    const url::Origin& origin) {
  return IsFileTypeDangerous_(path, origin);
}

}  // namespace content
