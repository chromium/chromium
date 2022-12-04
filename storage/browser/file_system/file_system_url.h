// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

// A class representing a filesystem URL which consists of origin URL,
// type and an internal path used inside the filesystem.
//
// When a FileSystemURL instance is created for a GURL (for filesystem: scheme),
// each accessor method would return following values:
//
// Example: For a URL 'filesystem:http://foo.com/temporary/foo/bar':
//   origin() returns 'http://foo.com',
//   mount_type() returns kFileSystemTypeTemporary,
//   virtual_path() returns 'foo/bar',
//   type() returns the same value as mount_type(),
//   path() returns the same value as virtual_path(),
//   bucket() returns nullopt unless explicitly set with SetBucket(),
//
// All other accessors return empty or invalid value.
//
// FileSystemURL can also be created to represent a 'cracked' filesystem URL if
// the original URL's type/path is pointing to a mount point which can be
// further resolved to a lower filesystem type/path.
//
// Example: Assume a path '/media/removable' is mounted at mount name
// 'mount_name' with type kFileSystemTypeFoo as an external file system.
//
// The original URL would look like:
//     'filesystem:http://bar.com/external/mount_name/foo/bar':
//
// FileSystemURL('http://bar.com',
//               kFileSystemTypeExternal,
//               'mount_name/foo/bar'
//               'mount_name',
//               kFileSystemTypeFoo,
//               '/media/removable/foo/bar');
// would create a FileSystemURL whose accessors return:
//
//   origin() returns 'http://bar.com',
//   mount_type() returns kFileSystemTypeExternal,
//   virtual_path() returns 'mount_name/foo/bar',
//   type() returns the kFileSystemTypeFoo,
//   path() returns '/media/removable/foo/bar',
//
// Note that in either case virtual_path() always returns the path part after
// 'type' part in the original URL, and mount_type() always returns the 'type'
// part in the original URL.
//
// Additionally, following accessors would return valid values:
//   filesystem_id() returns 'mount_name'.
//
// It is impossible to directly create a valid FileSystemURL instance (except by
// using CreatedForTest methods, which should not be used in production code).
// To get a valid FileSystemURL, one of the following methods can be used:
// <Friend>::CrackURL, <Friend>::CreateCrackedFileSystemURL, where <Friend> is
// one of the friended classes.
//
// TODO(ericu): Look into making virtual_path() [and all FileSystem API virtual
// paths] just an std::string, to prevent platform-specific base::FilePath
// behavior from getting invoked by accident. Currently the base::FilePath
// returned here needs special treatment, as it may contain paths that are
// illegal on the current platform.
// To avoid problems, use VirtualPath::BaseName and
// VirtualPath::GetComponents instead of the base::FilePath methods.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemURL {
 public:
  FileSystemURL();

  FileSystemURL(const FileSystemURL&);
  FileSystemURL(FileSystemURL&&) noexcept;
  FileSystemURL& operator=(const FileSystemURL&);
  FileSystemURL& operator=(FileSystemURL&&) noexcept;

  ~FileSystemURL();

  // Methods for creating FileSystemURL without attempting to crack them.
  // Should be used only in tests.
  static FileSystemURL CreateForTest(const GURL& url);
  static FileSystemURL CreateForTest(const blink::StorageKey& storage_key,
                                     FileSystemType mount_type,
                                     const base::FilePath& virtual_path);
  static FileSystemURL CreateForTest(const blink::StorageKey& storage_key,
                                     FileSystemType mount_type,
                                     const base::FilePath& virtual_path,
                                     const std::string& mount_filesystem_id,
                                     FileSystemType cracked_type,
                                     const base::FilePath& cracked_path,
                                     const std::string& filesystem_id,
                                     const FileSystemMountOption& mount_option);

  // Returns true if this instance represents a valid FileSystem URL.
  bool is_valid() const { return is_valid_; }

  // Returns the storage key. See the class comment for details.
  const blink::StorageKey& storage_key() const { return storage_key_; }

  // Returns the origin part of this URL. See the class comment for details.
  const url::Origin& origin() const { return storage_key_.origin(); }

  // Returns the type part of this URL. See the class comment for details.
  FileSystemType type() const { return type_; }

  // Returns the cracked path of this URL. See the class comment for details.
  const base::FilePath& path() const { return path_; }

  // Returns the original path part of this URL.
  // See the class comment for details.
  // TODO(kinuko): this must return std::string.
  const base::FilePath& virtual_path() const { return virtual_path_; }

  // Returns the filesystem ID/mount name for isolated/external filesystem URLs.
  // See the class comment for details.
  const std::string& filesystem_id() const { return filesystem_id_; }
  const std::string& mount_filesystem_id() const {
    return mount_filesystem_id_;
  }

  FileSystemType mount_type() const { return mount_type_; }

  const FileSystemMountOption& mount_option() const { return mount_option_; }

  // Returns the BucketLocator for this URL's partitioned file location. In
  // the majority of cases, this will not be populated and the default storage
  // bucket will be used.
  const absl::optional<BucketLocator>& bucket() const { return bucket_; }
  void SetBucket(const BucketLocator& bucket) { bucket_ = bucket; }

  // Returns either `bucket_` or a BucketLocator corresponding to the default
  // bucket for `storage_key_`.
  BucketLocator GetBucket() const;

  // Returns the formatted URL of this instance.
  GURL ToGURL() const;

  std::string DebugString() const;

  // Returns true if this URL is a strict parent of the |child|.
  bool IsParent(const FileSystemURL& child) const;

  bool IsInSameFileSystem(const FileSystemURL& other) const;

  bool operator==(const FileSystemURL& that) const;

  bool operator!=(const FileSystemURL& that) const { return !(*this == that); }

  struct COMPONENT_EXPORT(STORAGE_BROWSER) Comparator {
    bool operator()(const FileSystemURL& lhs, const FileSystemURL& rhs) const;
  };

 private:
  friend class FileSystemContext;
  friend class ExternalMountPoints;
  friend class IsolatedContext;

  FileSystemURL(const GURL& filesystem_url,
                const blink::StorageKey& storage_key);
  FileSystemURL(const blink::StorageKey& storage_key,
                FileSystemType mount_type,
                const base::FilePath& virtual_path);
  // Creates a cracked FileSystemURL.
  FileSystemURL(const blink::StorageKey& storage_key,
                FileSystemType mount_type,
                const base::FilePath& virtual_path,
                const std::string& mount_filesystem_id,
                FileSystemType cracked_type,
                const base::FilePath& cracked_path,
                const std::string& filesystem_id,
                const FileSystemMountOption& mount_option);

  // Used to determine if a FileSystemURL was default constructed.
  bool is_null_ = false;

  bool is_valid_;

  // Values parsed from the original URL.
  blink::StorageKey storage_key_;
  FileSystemType mount_type_;
  base::FilePath virtual_path_;

  // Values obtained by cracking URLs.
  // |mount_filesystem_id_| is retrieved from the first round of cracking,
  // and the rest of the fields are from recursive cracking. Permission
  // checking on the top-level mount information should be done with the former,
  // and the low-level file operation should be implemented with the latter.
  std::string mount_filesystem_id_;
  FileSystemType type_;
  base::FilePath path_;
  std::string filesystem_id_;
  FileSystemMountOption mount_option_;

  // Values that must be explicitly set.
  absl::optional<BucketLocator> bucket_;
};

using FileSystemURLSet = std::set<FileSystemURL, FileSystemURL::Comparator>;

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_H_
