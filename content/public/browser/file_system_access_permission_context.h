// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <string>
#include <vector>

#if defined(UNIT_TEST)
#include <ostream>
#endif

#include "base/files/file_path.h"
#include "content/public/browser/file_system_access_permission_grant.h"
#include "content/public/browser/file_system_access_write_item.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-shared.h"
#include "url/origin.h"

class GURL;

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace content {

// These values are used in json serialization. Entries should not be
// renumbered and numeric values should never be reused.
enum class PathType {
  // A path on the local file system. Files with these paths can be operated
  // on by base::File.
  kLocal = 0,

  // A path on an "external" file system. These paths can only be accessed via
  // the filesystem abstraction in //storage/browser/file_system, and a
  // storage::FileSystemURL of type storage::kFileSystemTypeExternal.
  // This path type should be used for paths retrieved via the `virtual_path`
  // member of a ui::SelectedFileInfo struct.
  kExternal = 1
};

struct PathInfo {
  PathType type = PathType::kLocal;
  // Full path of file or directory.
  base::FilePath path;
  // Display name of file or directory, must not be empty. This is usually
  // path.BaseName(), but in some cases such as android content-URIs the path is
  // unrelated to the display name.
  std::string display_name;

  PathInfo() = default;
  explicit PathInfo(base::FilePath path)
      : path(std::move(path)),
        display_name(this->path.BaseName().AsUTF8Unsafe()) {
    CHECK(!this->path.empty());
    CHECK(!this->display_name.empty());
  }
  explicit PathInfo(base::FilePath::StringPieceType path)
      : PathInfo(base::FilePath(path)) {
    CHECK(!this->path.empty());
    CHECK(!this->display_name.empty());
  }
  PathInfo(PathType type, base::FilePath path)
      : type(type),
        path(std::move(path)),
        display_name(this->path.BaseName().AsUTF8Unsafe()) {
    CHECK(!this->path.empty());
    CHECK(!this->display_name.empty());
  }
  PathInfo(base::FilePath path, std::string display_name)
      : path(std::move(path)), display_name(std::move(display_name)) {
    CHECK(!this->path.empty());
    CHECK(!this->display_name.empty());
  }
  PathInfo(PathType type, base::FilePath path, std::string display_name)
      : type(type),
        path(std::move(path)),
        display_name(std::move(display_name)) {
    CHECK(!this->path.empty());
    CHECK(!this->display_name.empty());
  }

  bool operator==(const PathInfo& other) const = default;
};

// For testing only.
#if defined(UNIT_TEST)
inline std::ostream& operator<<(std::ostream& os, const PathInfo& path_info) {
  return os << (int)path_info.type << ':' << path_info.path << ':'
            << path_info.display_name;
}
#endif

// Entry point to an embedder implemented permission context for the File System
// Access API. Instances of this class can be retrieved via a BrowserContext.
// All these methods must always be called on the UI thread.
class FileSystemAccessPermissionContext {
 public:
  // The type of action a user took that resulted in needing a permission grant
  // for a particular path. This is used to signal to the permission context if
  // the path was the result of a "save" operation, which an implementation can
  // use to automatically grant write access to the path.
  enum class UserAction {
    // The path for which a permission grant is requested was the result of a
    // "open" dialog. As such, only read access to files should be automatically
    // granted, but read access to directories as well as write access to files
    // or directories should not be granted without needing to request it.
    kOpen,
    // The path for which a permission grant is requested was the result of a
    // "save" dialog, and as such it could make sense to return a grant that
    // immediately allows write access without needing to request it.
    kSave,
    // The path for which a permission grant is requested was the result of
    // loading a handle from storage. As such the grant should not start out
    // as granted, even for read access.
    kLoadFromStorage,
    // The path for which a permission grant is requested was the result of a
    // drag&drop operation. Read access should start out granted, but write
    // access will require a prompt.
    kDragAndDrop,
    // The path for which a permission grant is requested was not the result of
    // a user action. This is used for checking additional blocklist check of
    // a path when obtaining a handle, therefore no prompt needs to be shown.
    kNone,
  };

  // This enum helps distinguish between file or directory File System Access
  // handles.
  enum class HandleType { kFile, kDirectory };

  using EntriesAllowedByEnterprisePolicyCallback =
      base::OnceCallback<void(std::vector<PathInfo>)>;

  // Returns the read permission grant to use for a particular path.
  virtual scoped_refptr<FileSystemAccessPermissionGrant> GetReadPermissionGrant(
      const url::Origin& origin,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action) = 0;

  // Returns the permission grant to use for a particular path. This could be a
  // grant that applies to more than just the path passed in, for example if a
  // user has already granted write access to a directory, this method could
  // return that existing grant when figuring the grant to use for a file in
  // that directory.
  virtual scoped_refptr<FileSystemAccessPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const PathInfo& path_info,
                          HandleType handle_type,
                          UserAction user_action) = 0;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SensitiveEntryResult {
    kAllowed = 0,   // Access to entry is okay.
    kTryAgain = 1,  // User should pick a different entry.
    kAbort = 2,     // Abandon entirely, as if picking was cancelled.
    kMaxValue = kAbort
  };
  // Checks if access to the given `path` should be allowed or blocked. This is
  // used to implement blocks for certain sensitive directories such as the
  // "Windows" system directory, as well as the root of the "home" directory.
  // For downloads ("Save as") it also checks the file extension. Calls
  // `callback` with the result of the check, after potentially showing some UI
  // to the user if the path is dangerous or should not be accessed.
  virtual void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      const PathInfo& path_info,
      HandleType handle_type,
      UserAction user_action,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) = 0;

  enum class AfterWriteCheckResult { kAllow, kBlock };
  // Runs a recently finished write operation through checks such as malware
  // or other security checks to determine if the write should be allowed.
  virtual void PerformAfterWriteChecks(
      std::unique_ptr<FileSystemAccessWriteItem> item,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) = 0;

  // Returns whether the file type is considered dangerous. This is used to
  // block file operations from creating or accessing these file types.
  virtual bool IsFileTypeDangerous(const base::FilePath& path,
                                   const url::Origin& origin) = 0;

  // Returns whether the give |origin| already allows read permission, or it is
  // possible to request one. This is used to block file dialogs from being
  // shown if permission won't be granted anyway.
  virtual bool CanObtainReadPermission(const url::Origin& origin) = 0;

  // Returns whether the give |origin| already allows write permission, or it is
  // possible to request one. This is used to block save file dialogs from being
  // shown if there is no need to ask for it.
  virtual bool CanObtainWritePermission(const url::Origin& origin) = 0;

  // Store the directory recently chosen by a file picker. This can later be
  // retrieved via a call to |GetLastPickedDirectory| with the corresponding
  // |origin| and |id|.
  virtual void SetLastPickedDirectory(const url::Origin& origin,
                                      const std::string& id,
                                      const content::PathInfo& path_info) = 0;
  // Returns the directory recently chosen by a file picker for a given
  // |origin| and |id|.
  virtual PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                          const std::string& id) = 0;

  // Return the path associated with well-known directories such as "desktop"
  // and "music", or a default path if the |directory| cannot be matched to a
  // well-known directory. When |directory| is WellKnownDirectory.DIR_DOWNLOADS,
  // |origin| is used to determine if browser-specified download directory
  // should be returned instead of OS default download directory.
  virtual base::FilePath GetWellKnownDirectoryPath(
      blink::mojom::WellKnownDirectory directory,
      const url::Origin& origin) = 0;

  // Return the desired title of the file picker for the given `options`.
  virtual std::u16string GetPickerTitle(
      const blink::mojom::FilePickerOptionsPtr& options) = 0;

  // Notifies that the underlying file or directory has been moved and updates
  // permission grants accordingly.
  virtual void NotifyEntryMoved(const url::Origin& origin,
                                const PathInfo& old_path,
                                const PathInfo& new_path) = 0;

  // Invoked on file creation events originating from
  // `window.showSaveFilePicker()`.
  //
  // See `FileSystemAccessEntryFactory::BindingContext`.
  virtual void OnFileCreatedFromShowSaveFilePicker(
      const GURL& file_picker_binding_context,
      const storage::FileSystemURL& url) = 0;

  // Checks the paths listed in `entries` to determine if they should be allowed
  // or blocked within this context, for the given render frame host, based on
  // enterprise policies. Invokes `callback` with the list of entries which are
  // allowed.
  virtual void CheckPathsAgainstEnterprisePolicy(
      std::vector<PathInfo> entries,
      GlobalRenderFrameHostId frame_id,
      EntriesAllowedByEnterprisePolicyCallback callback) = 0;

 protected:
  virtual ~FileSystemAccessPermissionContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
