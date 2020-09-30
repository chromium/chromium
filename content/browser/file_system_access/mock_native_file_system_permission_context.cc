// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/mock_native_file_system_permission_context.h"

namespace content {

MockNativeFileSystemPermissionContext::MockNativeFileSystemPermissionContext() =
    default;
MockNativeFileSystemPermissionContext::
    ~MockNativeFileSystemPermissionContext() = default;

void MockNativeFileSystemPermissionContext::ConfirmSensitiveDirectoryAccess(
    const url::Origin& origin,
    const std::vector<base::FilePath>& paths,
    HandleType handle_type,
    GlobalFrameRoutingId frame_id,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback) {
  ConfirmSensitiveDirectoryAccess_(origin, paths, handle_type, frame_id,
                                   callback);
}

void MockNativeFileSystemPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<NativeFileSystemWriteItem> item,
    GlobalFrameRoutingId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  PerformAfterWriteChecks_(item.get(), frame_id, callback);
}

}  // namespace content
