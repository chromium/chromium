// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_MOUNT_POINTS_H_
#define STORAGE_BROWSER_FILE_SYSTEM_MOUNT_POINTS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "storage/common/file_system/file_system_util.h"

class GURL;

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class FileSystemMountOption;
class FileSystemURL;

// Represents a set of mount points for File API.
class COMPONENT_EXPORT(STORAGE_BROWSER) MountPoints {
 public:
  struct COMPONENT_EXPORT(STORAGE_BROWSER) MountPointInfo {
    MountPointInfo();
    MountPointInfo(const std::string& name, const base::FilePath& path);

    // The name to be used to register the path. The registered file can
    // be referred by a virtual path /<filesystem_id>/<name>.
    // The name should NOT contain a path separator '/'.
    std::string name;

    // The path of the file.
    base::FilePath path;

    // For STL operation.
    bool operator<(const MountPointInfo& that) const {
      return name < that.name;
    }
  };

  MountPoints() = default;
  MountPoints(const MountPoints&) = delete;
  MountPoints& operator=(const MountPoints&) = delete;
  virtual ~MountPoints() = default;

  // Revokes a mount point identified by |mount_name|.
  // Returns false if the |mount_name| is not (no longer) registered.
  // TODO(kinuko): Probably this should be rather named RevokeMountPoint.
  virtual bool RevokeFileSystem(const std::string& mount_name) = 0;

  // Returns true if the MountPoints implementation handles filesystems with
  // the given mount type.
  virtual bool HandlesFileSystemMountType(FileSystemType type) const = 0;

  // TODO(crbug.com/40194313): Determine if MountPoints::CrackURL()
  // and its overrides in child classes should be removed and replaced with
  // FileSystemContext::CrackURL().
  //
  // Same as CreateCrackedFileSystemURL, but cracks a FileSystemURL created
  // from `url` and `storage_key`.
  virtual FileSystemURL CrackURL(
      const GURL& url,
      const blink::StorageKey& storage_key) const = 0;

  // Creates a FileSystemURL with the given `storage_key`, `type`, and
  // `virtual_path` and tries to crack it as a part of one of the registered
  // mount points. If the the URL is not valid or does not belong to any of the
  // mount points registered in this context, returns empty, invalid
  // FileSystemURL.
  virtual FileSystemURL CreateCrackedFileSystemURL(
      const blink::StorageKey& storage_key,
      FileSystemType type,
      const base::FilePath& virtual_path) const = 0;

  // Returns the mount point root path registered for a given |mount_name|.
  // Returns false if the given |mount_name| is not valid.
  virtual bool GetRegisteredPath(const std::string& mount_name,
                                 base::FilePath* path) const = 0;

  // Cracks the given |virtual_path| (which is the path part of a filesystem URL
  // without '/external' or '/isolated' prefix part) and populates the
  // |mount_name|, |type|, and |path| if the <mount_name> part embedded in
  // the |virtual_path| (i.e. the first component of the |virtual_path|) is a
  // valid registered filesystem ID or mount name for an existing mount point.
  //
  // Returns false if the given virtual_path cannot be cracked.
  //
  // Note that |path| is set to empty paths if the filesystem type is isolated
  // and |virtual_path| has no <relative_path> part (i.e. pointing to the
  // virtual root).
  virtual bool CrackVirtualPath(const base::FilePath& virtual_path,
                                std::string* mount_name,
                                FileSystemType* type,
                                std::string* cracked_id,
                                base::FilePath* path,
                                FileSystemMountOption* mount_option) const = 0;

 protected:
  friend class FileSystemContext;

  // Same as CrackURL and CreateCrackedFileSystemURL, but cracks the url already
  // instantiated as the FileSystemURL class. This is internally used for nested
  // URL cracking in FileSystemContext.
  virtual FileSystemURL CrackFileSystemURL(const FileSystemURL& url) const = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_MOUNT_POINTS_H_
