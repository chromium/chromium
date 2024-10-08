// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-shared.h"

#ifndef CONTENT_PUBLIC_TEST_FAKE_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_FAKE_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

namespace content {

// Fake permission context which uses an in-memory map for
// [GS]etLastPickedDirectory and returns permissions which are always granted.
// Support for WellKnown directories is provided via a setter which allows
// setting custom paths in an in-memory map.
class FakeFileSystemAccessPermissionContext
    : public FileSystemAccessPermissionContext {
 public:
  static constexpr char16_t kPickerTitle[] = u"Choose something";

  FakeFileSystemAccessPermissionContext();
  ~FakeFileSystemAccessPermissionContext() override;

  scoped_refptr<FileSystemAccessPermissionGrant> GetReadPermissionGrant(
      const url::Origin& origin,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action) override;

  scoped_refptr<FileSystemAccessPermissionGrant> GetWritePermissionGrant(
      const url::Origin& origin,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action) override;

  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) override;

  void PerformAfterWriteChecks(
      std::unique_ptr<FileSystemAccessWriteItem> item,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;

  bool IsFileTypeDangerous(const base::FilePath& path,
                           const url::Origin& origin) override;

  bool CanObtainReadPermission(const url::Origin& origin) override;
  bool CanObtainWritePermission(const url::Origin& origin) override;

  void SetLastPickedDirectory(const url::Origin& origin,
                              const std::string& id,
                              const PathInfo& path_info) override;
  PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                  const std::string& id) override;

  // Establishes a mapping between a WellKnownDirectory and a custom path.
  void SetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory directory,
                                 base::FilePath path);
  // Retrieves a path which was earlier specified via SetWellKnownDirectoryPath.
  // Otherwise, returns an empty path.
  base::FilePath GetWellKnownDirectoryPath(
      blink::mojom::WellKnownDirectory directory,
      const url::Origin& origin) override;

  // Returns `kPickerTitle`.
  std::u16string GetPickerTitle(
      const blink::mojom::FilePickerOptionsPtr& options) override;

  // No-op. This class does not manage any permission grants.
  void NotifyEntryMoved(const url::Origin& origin,
                        const PathInfo& old_path,
                        const PathInfo& new_path) override;

  void OnFileCreatedFromShowSaveFilePicker(
      const GURL& file_picker_binding_context,
      const storage::FileSystemURL& url) override;

  void CheckPathsAgainstEnterprisePolicy(
      std::vector<PathInfo> entries,
      GlobalRenderFrameHostId frame_id,
      EntriesAllowedByEnterprisePolicyCallback callback) override;

 private:
  std::map<std::string, PathInfo> id_pathinfo_map_;
  std::map<blink::mojom::WellKnownDirectory, base::FilePath>
      well_known_directory_map_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
