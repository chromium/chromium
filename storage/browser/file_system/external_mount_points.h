// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_EXTERNAL_MOUNT_POINTS_H_
#define STORAGE_BROWSER_FILE_SYSTEM_EXTERNAL_MOUNT_POINTS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "storage/browser/file_system/mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class FileSystemURL;

// Manages external filesystem namespaces that are identified by 'mount name'
// and are persisted until RevokeFileSystem is called.
// Files in an external filesystem are identified by a filesystem URL like:
//
//   filesystem:<origin>/external/<mount_name>/relative/path
//
class COMPONENT_EXPORT(STORAGE_BROWSER) ExternalMountPoints
    : public base::RefCountedThreadSafe<ExternalMountPoints>,
      public MountPoints {
 public:
  static ExternalMountPoints* GetSystemInstance();
  static scoped_refptr<ExternalMountPoints> CreateRefCounted();

  static void GetDebugJSONForKey(
      std::string_view key,
      base::OnceCallback<void(std::pair<std::string_view, base::Value>)>
          callback);

  ExternalMountPoints(const ExternalMountPoints&) = delete;
  ExternalMountPoints& operator=(const ExternalMountPoints&) = delete;

  // Registers a new named external filesystem.
  // The |path| is registered as the root path of the mount point which
  // is identified by a URL "filesystem:.../external/mount_name".
  //
  // For example, if the path "/media/removable" is registered with
  // the mount_name "removable", a filesystem URL like
  // "filesystem:.../external/removable/a/b" will be resolved as
  // "/media/removable/a/b".
  //
  // The |mount_name| should NOT contain a path separator '/'.
  // Returns false if the given name is already registered.
  //
  // Overlapping mount points in a single MountPoints instance are not allowed.
  // Adding mount point whose path overlaps with an existing mount point will
  // fail except for media galleries, which do not count toward registered
  // paths for overlap calculation.
  //
  // If not empty, |path| must be absolute. It is allowed for the path to be
  // empty, but |GetVirtualPath| will not work for those mount points.
  //
  // An external file system registered by this method can be revoked
  // by calling RevokeFileSystem with |mount_name|.
  bool RegisterFileSystem(const std::string& mount_name,
                          FileSystemType type,
                          const FileSystemMountOption& mount_option,
                          const base::FilePath& path);

  // MountPoints overrides.
  bool HandlesFileSystemMountType(FileSystemType type) const override;
  bool RevokeFileSystem(const std::string& mount_name) override;
  bool GetRegisteredPath(const std::string& mount_name,
                         base::FilePath* path) const override;
  bool CrackVirtualPath(const base::FilePath& virtual_path,
                        std::string* mount_name,
                        FileSystemType* type,
                        std::string* cracked_id,
                        base::FilePath* path,
                        FileSystemMountOption* mount_option) const override;
  FileSystemURL CrackURL(const GURL& url,
                         const blink::StorageKey& storage_key) const override;
  FileSystemURL CreateCrackedFileSystemURL(
      const blink::StorageKey& storage_key,
      FileSystemType type,
      const base::FilePath& virtual_path) const override;

  // Returns a list of registered MountPointInfos (of <mount_name, path>).
  void AddMountPointInfosTo(std::vector<MountPointInfo>* mount_points) const;

  // Converts a path on a registered file system to virtual path relative to the
  // file system root. E.g. if 'Downloads' file system is mapped to
  // '/usr/local/home/Downloads', and |absolute| path is set to
  // '/usr/local/home/Downloads/foo', the method will set |virtual_path| to
  // 'Downloads/foo'.
  // Returns false if the path cannot be resolved (e.g. if the path is not
  // part of any registered filesystem).
  //
  // Media gallery type file systems do not count for this calculation. i.e.
  // if only a media gallery is registered for the path, false will be returned.
  // If a media gallery and another file system are registered for related
  // paths, only the other registration is taken into account.
  //
  // Returned virtual_path will have normalized path separators.
  bool GetVirtualPath(const base::FilePath& absolute_path,
                      base::FilePath* virtual_path) const;

  // Returns the virtual root path that looks like /<mount_name>.
  base::FilePath CreateVirtualRootPath(const std::string& mount_name) const;

  FileSystemURL CreateExternalFileSystemURL(
      const blink::StorageKey& storage_key,
      const std::string& mount_name,
      const base::FilePath& path) const;

  // Revoke all registered filesystems. Used only by testing (for clean-ups).
  void RevokeAllFileSystems();

 private:
  friend class base::RefCountedThreadSafe<ExternalMountPoints>;

  // Represents each file system instance (defined in the .cc).
  class Instance;

  using NameToInstance = std::map<std::string, std::unique_ptr<Instance>>;

  // Use |GetSystemInstance| of |CreateRefCounted| to get an instance.
  ExternalMountPoints();
  ~ExternalMountPoints() override;

  // MountPoint overrides.
  FileSystemURL CrackFileSystemURL(const FileSystemURL& url) const override;

  // Performs sanity checks on the new mount point.
  // Checks the following:
  //  - there is no registered mount point with mount_name
  //  - path does not contain a reference to a parent
  //  - path is absolute
  //  - path does not overlap with an existing mount point path unless it is a
  //    media gallery type.
  //
  // |lock_| should be taken before calling this method.
  bool ValidateNewMountPoint(const std::string& mount_name,
                             FileSystemType type,
                             const base::FilePath& path);

  // This lock needs to be obtained when accessing the instance_map_.
  mutable base::Lock lock_;

  NameToInstance instance_map_;

  // Reverse map from registered path to its corresponding mount name.
  std::map<base::FilePath, std::string> path_to_name_map_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_EXTERNAL_MOUNT_POINTS_H_
