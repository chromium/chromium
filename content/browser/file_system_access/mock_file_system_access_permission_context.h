// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <string>

#include "base/files/file_path.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"

namespace content {
// Mock FileSystemAccessPermissionContext implementation.
class MockFileSystemAccessPermissionContext
    : public FileSystemAccessPermissionContext {
 public:
  MockFileSystemAccessPermissionContext();
  ~MockFileSystemAccessPermissionContext() override;

  MOCK_METHOD(scoped_refptr<FileSystemAccessPermissionGrant>,
              GetReadPermissionGrant,
              (const url::Origin& origin,
               const PathInfo& path_info,
               HandleType handle_type,
               FileSystemAccessPermissionContext::UserAction user_action),
              (override));

  MOCK_METHOD(scoped_refptr<FileSystemAccessPermissionGrant>,
              GetWritePermissionGrant,
              (const url::Origin& origin,
               const PathInfo& path_info,
               HandleType handle_type,
               FileSystemAccessPermissionContext::UserAction user_action),
              (override));

  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) override;
  MOCK_METHOD(void,
              ConfirmSensitiveEntryAccess_,
              (const url::Origin& origin,
               const PathInfo& path_info,
               HandleType handle_type,
               UserAction user_action,
               GlobalRenderFrameHostId frame_id,
               base::OnceCallback<void(SensitiveEntryResult)>& callback));

  void PerformAfterWriteChecks(
      std::unique_ptr<FileSystemAccessWriteItem> item,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;
  MOCK_METHOD(void,
              PerformAfterWriteChecks_,
              (FileSystemAccessWriteItem * item,
               GlobalRenderFrameHostId frame_id,
               base::OnceCallback<void(AfterWriteCheckResult)>& callback));

  bool IsFileTypeDangerous(const base::FilePath& path,
                           const url::Origin& origin) override;
  MOCK_METHOD(bool,
              IsFileTypeDangerous_,
              (const base::FilePath& path, const url::Origin& origin));

  MOCK_METHOD(bool,
              CanObtainReadPermission,
              (const url::Origin& origin),
              (override));
  MOCK_METHOD(bool,
              CanObtainWritePermission,
              (const url::Origin& origin),
              (override));

  MOCK_METHOD(void,
              SetLastPickedDirectory,
              (const url::Origin& origin,
               const std::string& id,
               const PathInfo& path_info),
              (override));
  MOCK_METHOD(PathInfo,
              GetLastPickedDirectory,
              (const url::Origin& origin, const std::string& id),
              (override));

  MOCK_METHOD(base::FilePath,
              GetWellKnownDirectoryPath,
              (blink::mojom::WellKnownDirectory directory,
               const url::Origin& origin),
              (override));

  MOCK_METHOD(std::u16string,
              GetPickerTitle,
              (const blink::mojom::FilePickerOptionsPtr& options),
              (override));

  MOCK_METHOD(void,
              NotifyEntryMoved,
              (const url::Origin& origin,
               const PathInfo& old_path,
               const PathInfo& new_path),
              (override));

  MOCK_METHOD(void,
              OnFileCreatedFromShowSaveFilePicker,
              (const GURL& file_picker_binding_context,
               const storage::FileSystemURL& url),
              (override));

  MOCK_METHOD(void,
              CheckPathsAgainstEnterprisePolicy,
              (std::vector<PathInfo> entries,
               GlobalRenderFrameHostId frame_id,
               EntriesAllowedByEnterprisePolicyCallback callback),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
