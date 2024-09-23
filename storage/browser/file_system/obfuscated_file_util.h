// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/obfuscated_file_util_delegate.h"
#include "storage/browser/file_system/sandbox_directory_database.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/common/file_system/file_system_types.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class FileSystemOperationContext;
class ObfuscatedFileUtilTest;
class QuotaBackendImplTest;
class SandboxOriginDatabaseInterface;
class SpecialStoragePolicy;

// Class representing the key for directories_. NOTE: The `bucket` value is
// optional due to usage of ObfuscatedFileUtil where the type is not kTemporary
// (i.e. kPersistent or kSyncable). For all non-temporary types, expect the
// bucket member value to be std::nullopt. The class is implemented as such to
// avoid mapping the same StorageKey to potentially different bucket values,
// which would cause directories_ lookup errors. NOTE: The `type_string` value
// is empty when designating a "top-level directory" or a directory that
// contains one or more subdirectories with a non-empty type. This class stores
// a string rather than the FileSystemType itself because multiple
// FileSystemTypes can map to the same `type_string`, and preserving this
// behavior is necessary to retrieving and deleting ObfuscatedFilePaths
// correctly.
class DatabaseKey {
 public:
  DatabaseKey();
  ~DatabaseKey();

  // Copyable and movable
  DatabaseKey(const DatabaseKey& other);
  DatabaseKey& operator=(const DatabaseKey& other);
  DatabaseKey(DatabaseKey&& other);
  DatabaseKey& operator=(DatabaseKey&& other);

  DatabaseKey(const blink::StorageKey& storage_key,
              const std::optional<BucketLocator>& bucket,
              const std::string& type_string);

  const blink::StorageKey& storage_key() const { return storage_key_; }
  const std::optional<BucketLocator>& bucket() const { return bucket_; }
  const std::string& type() const { return type_; }

  bool operator==(const DatabaseKey& other) const;
  bool operator!=(const DatabaseKey& other) const;
  bool operator<(const DatabaseKey& other) const;

 private:
  blink::StorageKey storage_key_;
  std::optional<BucketLocator> bucket_;
  std::string type_;
};

// This file util stores directory information in either LevelDB or
// StorageBuckets to obfuscate and to neutralize virtual file paths given by
// arbitrary apps. Files are stored with three-level isolation: (1)
// per-StorageKey, (2) per-bucket, and (3) per-type. The isolation is done by
// storing data in separate directory partitions. For example, a file in
// Temporary file system for origin 'www.example.com' is stored in a different
// partition from a file in Persistent file system for the same origin, or from
// a file in a Temporary file system for another origin. Similarly, a file in a
// Temporary file system for origin 'www.foo.com' with a default bucket is
// stored in a different partition from a non-default bucket for the same origin
// and Temporary file system.
//
// * For default first-party StorageKeys, per-origin directory name information
//   is stored in a separate LevelDB, which is maintained by
//   SandboxOriginDatabase. For per-type information, we use a small static
//   mapping (e.g. 't' for Temporary type) for regular sandbox filesystems.
//   NOTE/TODO(crbug.com/40855748): the goal is to eventually deprecate
//   SandboxOriginDatabase and rely entirely on Storage Buckets.
// * For all other StorageKeys, we rely on quota management of Storage Buckets
//   in addition to the same static mapping of per-type information described
//   above.
//
// The overall implementation philosophy of this class is that partial failures
// should leave us with an intact database; we'd prefer to leak the occasional
// backing file than have a database entry whose backing file is missing.  When
// doing FSCK operations, if you find a loose backing file with no reference,
// you may safely delete it.
//
// This class must be deleted on the FILE thread, because that's where
// DropDatabases needs to be called.
class COMPONENT_EXPORT(STORAGE_BROWSER) ObfuscatedFileUtil
    : public FileSystemFileUtil {
 public:
  // StorageKey enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  // NOTE: currently, ObfuscatedFileUtil still relies on SandboxOriginDatabases
  // for first-party StorageKeys/default buckets. The
  // AbstractStorageKeyEnumerator is only used in these cases. While this class
  // stores StorageKeys, it ultimately relies on only the origin information to
  // access the appropriate SandboxOriginDatabase.
  class AbstractStorageKeyEnumerator {
   public:
    virtual ~AbstractStorageKeyEnumerator() = default;

    // Returns the next StorageKey. Returns std::nullopt if there are no more
    // StorageKeys.
    virtual std::optional<blink::StorageKey> Next() = 0;

    // Returns the current StorageKey's information.
    // `type_string` must be ascii string.
    virtual bool HasTypeDirectory(const std::string& type_string) const = 0;
  };

  // The FileSystem directory component.
  static const base::FilePath::CharType kFileSystemDirectory[];

  // The type string is used to provide per-type isolation in the sandboxed
  // filesystem directory. `known_type_strings` are known type string names that
  // this file system should care about. This info is used to determine whether
  // we could delete the entire origin directory or not in
  // DeleteDirectoryForBucketAndType. If no directory for any known type
  // exists the origin directory may get deleted when one StorageKey/type pair
  // is deleted. NOTE: type strings are not mapped 1-to-1 with FileSystemType,
  // and as a result, directories should only be directly compared using type
  // string values.
  ObfuscatedFileUtil(scoped_refptr<SpecialStoragePolicy> special_storage_policy,
                     const base::FilePath& profile_path,
                     leveldb::Env* env_override,
                     const std::set<std::string>& known_type_strings,
                     SandboxFileSystemBackendDelegate* sandbox_delegate,
                     bool is_incognito);

  ObfuscatedFileUtil(const ObfuscatedFileUtil&) = delete;
  ObfuscatedFileUtil& operator=(const ObfuscatedFileUtil&) = delete;

  ~ObfuscatedFileUtil() override;

  // FileSystemFileUtil overrides.
  base::File CreateOrOpen(FileSystemOperationContext* context,
                          const FileSystemURL& url,
                          int file_flags) override;
  base::File::Error EnsureFileExists(FileSystemOperationContext* context,
                                     const FileSystemURL& url,
                                     bool* created) override;
  base::File::Error CreateDirectory(FileSystemOperationContext* context,
                                    const FileSystemURL& url,
                                    bool exclusive,
                                    bool recursive) override;
  base::File::Error GetFileInfo(FileSystemOperationContext* context,
                                const FileSystemURL& url,
                                base::File::Info* file_info,
                                base::FilePath* platform_file) override;
  std::unique_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) override;
  base::File::Error GetLocalFilePath(FileSystemOperationContext* context,
                                     const FileSystemURL& file_system_url,
                                     base::FilePath* local_path) override;
  base::File::Error Touch(FileSystemOperationContext* context,
                          const FileSystemURL& url,
                          const base::Time& last_access_time,
                          const base::Time& last_modified_time) override;
  base::File::Error Truncate(FileSystemOperationContext* context,
                             const FileSystemURL& url,
                             int64_t length) override;
  base::File::Error CopyOrMoveFile(FileSystemOperationContext* context,
                                   const FileSystemURL& src_url,
                                   const FileSystemURL& dest_url,
                                   CopyOrMoveOptionSet options,
                                   bool copy) override;
  base::File::Error CopyInForeignFile(FileSystemOperationContext* context,
                                      const base::FilePath& src_file_path,
                                      const FileSystemURL& dest_url) override;
  base::File::Error DeleteFile(FileSystemOperationContext* context,
                               const FileSystemURL& url) override;
  base::File::Error DeleteDirectory(FileSystemOperationContext* context,
                                    const FileSystemURL& url) override;
  ScopedFile CreateSnapshotFile(FileSystemOperationContext* context,
                                const FileSystemURL& url,
                                base::File::Error* error,
                                base::File::Info* file_info,
                                base::FilePath* platform_path) override;

  // Returns true if the directory `url` is empty.
  bool IsDirectoryEmpty(FileSystemOperationContext* context,
                        const FileSystemURL& url);

  // Gets the topmost directory specific to this BucketLocator and type. This
  // will contain both the directory database's files and all the backing file
  // subdirectories.
  // Returns the topmost origin directory if `type` is empty.
  // Returns a base::FileError if the directory is undefined.
  base::FileErrorOr<base::FilePath> GetDirectoryForBucketAndType(
      const BucketLocator& bucket_locator,
      const std::optional<FileSystemType>& type,
      bool create);

  // Gets the topmost directory specific to this StorageKey and type.  This will
  // contain both the directory database's files and all the backing file
  // subdirectories.
  // Returns the topmost origin directory if `type` is empty.
  // Returns an empty path if the directory is undefined.
  // If the directory is defined, it will be returned, even if
  // there is a file system error (e.g. the directory doesn't exist on disk and
  // `create` is false).
  base::FileErrorOr<base::FilePath> GetDirectoryForStorageKeyAndType(
      const blink::StorageKey& storage_key,
      const std::optional<FileSystemType>& type,
      bool create);

  // Deletes the topmost directory specific to this BucketLocator and type. This
  // will delete its directory database. Deletes the topmost bucket
  // directory if `type` is std::nullopt.
  bool DeleteDirectoryForBucketAndType(
      const BucketLocator& bucket_locator,
      const std::optional<FileSystemType>& type);

  // This method and all methods of its returned class must be called only on
  // the FILE thread.  The caller is responsible for deleting the returned
  // object.
  std::unique_ptr<AbstractStorageKeyEnumerator> CreateStorageKeyEnumerator();

  // Deletes a directory database from the database list and destroys the
  // database on the disk corresponding to the provided bucket locator and type.
  void DestroyDirectoryDatabaseForBucket(
      const BucketLocator& bucket_locator,
      const std::optional<FileSystemType>& type);

  // Computes a cost for storing a given file in the obfuscated FSFU.
  // As the cost of a file is independent of the cost of its parent directories,
  // this ignores all but the BaseName of the supplied path.  In order to
  // compute the cost of adding a multi-segment directory recursively, call this
  // on each path segment and add the results.
  static int64_t ComputeFilePathCost(const base::FilePath& path);

  // This will rewrite the databases to remove traces of deleted data from disk.
  void RewriteDatabases();

  // This function removes the key-value pair from default_buckets_ keyed at
  // `storage_key`. Called when a default bucket is deleted from Quota
  // management and the default_buckets_ cache needs to be updated to reflect
  // that change in state.
  void DeleteDefaultBucketForStorageKey(const blink::StorageKey& storage_key);

  bool is_incognito() { return is_incognito_; }

  ObfuscatedFileUtilDelegate* delegate() { return delegate_.get(); }
  // Not owned.
  SandboxFileSystemBackendDelegate* sandbox_delegate() {
    return sandbox_delegate_;
  }

 private:
  using FileId = SandboxDirectoryDatabase::FileId;
  using FileInfo = SandboxDirectoryDatabase::FileInfo;

  friend class ObfuscatedFileEnumerator;
  friend class ObfuscatedFileUtilTest;
  friend class QuotaBackendImplTest;
  friend class SandboxFileSystemBackendDelegate;

  // Helper method to create an obfuscated file util for regular
  // (temporary, persistent) file systems. Used only for testing.
  // Note: this is implemented in sandbox_file_system_backend_delegate.cc.
  static std::unique_ptr<ObfuscatedFileUtil> CreateForTesting(
      scoped_refptr<SpecialStoragePolicy> special_storage_policy,
      const base::FilePath& file_system_directory,
      leveldb::Env* env_override,
      bool is_incognito);

  base::FileErrorOr<base::FilePath> GetDirectoryForURL(const FileSystemURL& url,
                                                       bool create);

  base::File::Error GetFileInfoInternal(SandboxDirectoryDatabase* db,
                                        FileSystemOperationContext* context,
                                        const FileSystemURL& url,
                                        FileId file_id,
                                        FileInfo* local_info,
                                        base::File::Info* file_info,
                                        base::FilePath* platform_file_path);

  // Creates a new file, both the underlying backing file and the entry in the
  // database.  `dest_file_info` is an in-out parameter.  Supply the name and
  // parent_id; data_path is ignored.  On success, data_path will
  // always be set to the relative path [from the root of the type-specific
  // filesystem directory] of a NEW backing file.  Returns the new file.
  base::File CreateAndOpenFile(FileSystemOperationContext* context,
                               const FileSystemURL& dest_url,
                               FileInfo* dest_file_info,
                               int file_flags);

  // The same as CreateAndOpenFile except that a file is not returned and if a
  // path is provided in `source_path`, it will be used as a source from which
  // to COPY data. If `foreign_source` is true, the source file is considered
  // from another (on disk) file system and its path is considered not
  // obfuscated.
  base::File::Error CreateFile(FileSystemOperationContext* context,
                               const base::FilePath& source_file_path,
                               bool foreign_source,
                               const FileSystemURL& dest_url,
                               FileInfo* dest_file_info);

  // Updates `db` and `dest_file_info` at the end of creating a new file.
  base::File::Error CommitCreateFile(const base::FilePath& root,
                                     const base::FilePath& local_path,
                                     SandboxDirectoryDatabase* db,
                                     FileInfo* dest_file_info);

  // This converts from a relative path [as is stored in the FileInfo.data_path
  // field] to an absolute platform path that can be given to the native
  // filesystem.
  base::FilePath DataPathToLocalPath(const FileSystemURL& url,
                                     const base::FilePath& data_file_path);

  // This returns nullptr if `create` flag is false and a filesystem does not
  // exist for the given `url`.
  // For read operations `create` should be false.
  SandboxDirectoryDatabase* GetDirectoryDatabase(const FileSystemURL& url,
                                                 bool create);

  // Gets the topmost directory specific to this StorageKey. This will
  // contain both of the filesystem type subdirectories.
  // NOTE: this function uses base::ScopedAllowBaseSyncPrimitives and
  // calls QuotaManagerProxy::GetOrCreateBucketSync() which relies on a
  // blocking base::WaitableEvent.
  base::FileErrorOr<base::FilePath> GetDirectoryForStorageKey(
      const blink::StorageKey& storage_key,
      bool create);

  // A helper function used by the GetDirectoryFor* methods to ensure that
  // `path` is a valid directory or that a valid directory can be constructed.
  base::File::Error GetDirectoryHelper(const base::FilePath& path, bool create);

  void InvalidateUsageCache(FileSystemOperationContext* context,
                            const blink::StorageKey& storage_key,
                            FileSystemType type);

  // Given a StorageKey, retrieve its default bucket either from the
  // default_buckets_ in-memory structure or via GetOrCreateBucketSync(). NOTE:
  // this function may use base::ScopedAllowBaseSyncPrimitives and call
  // QuotaManagerProxy::GetOrCreateBucketSync() which relies on a blocking
  // base::WaitableEvent.
  QuotaErrorOr<BucketLocator> GetOrCreateDefaultBucket(
      const blink::StorageKey& storage_key);

  void MarkUsed();
  void DropDatabases();

  // Initializes the origin/type database. `origin_hint` may be used as a
  // hint for initializing database if it's not empty.
  bool InitOriginDatabase(const url::Origin& origin_hint, bool create);

  base::File::Error GenerateNewLocalPath(SandboxDirectoryDatabase* db,
                                         FileSystemOperationContext* context,
                                         const FileSystemURL& url,
                                         base::FilePath* root,
                                         base::FilePath* local_path);

  base::File CreateOrOpenInternal(FileSystemOperationContext* context,
                                  const FileSystemURL& url,
                                  int file_flags);

  bool HasIsolatedStorage(const blink::StorageKey& storage_key);

  SEQUENCE_CHECKER(sequence_checker_);

  // Keeps tracks of previously-seen default buckets mapped to their
  // corresponding StorageKey. Should remain in parallel with directories_.
  std::map<blink::StorageKey, BucketLocator> default_buckets_;
  std::map<DatabaseKey, std::unique_ptr<SandboxDirectoryDatabase>> directories_;
  std::unique_ptr<SandboxOriginDatabaseInterface> origin_database_;
  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;
  base::FilePath file_system_directory_;
  raw_ptr<leveldb::Env> env_override_;
  bool is_incognito_;

  // Used to delete database after a certain period of inactivity.
  int64_t db_flush_delay_seconds_;

  base::OneShotTimer timer_;

  std::set<std::string> known_type_strings_;

  // Not owned.
  raw_ptr<SandboxFileSystemBackendDelegate> sandbox_delegate_;

  std::unique_ptr<ObfuscatedFileUtilDelegate> delegate_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_H_
