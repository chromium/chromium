// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCK_MANAGER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCK_MANAGER_H_

#include <map>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace content {

class FileSystemAccessManagerImpl;

// This class is in charge of the creation of Locks. Locks restrict the access
// to a specific file or directory, preventing unexpected concurrent access to
// data. It is owned by the FileSystemAccessManagerImpl.
class CONTENT_EXPORT FileSystemAccessLockManager {
 public:
  using PassKey = base::PassKey<FileSystemAccessLockManager>;

  enum class LockType {
    // An exclusive lock prevents the taking of any new lock.
    kExclusive,
    // A shared lock prevents the taking of new exclusive locks, while allowing
    // the taking of shared ones.
    kShared
  };

  enum class EntryPathType {
    // A path on the local file system. Files with these paths can be operated
    // on by base::File.
    kLocal,

    // A path on an "external" file system. These paths can only be accessed via
    // the filesystem abstraction in //storage/browser/file_system, and a
    // storage::FileSystemURL of type storage::kFileSystemTypeExternal.
    kExternal,

    // A path from a sandboxed file system. These paths can be accessed by a
    // storage::FileSystemURL of type storage::kFileSystemTypeTemporary.
    kSandboxed,
  };

  struct EntryLocator {
    static EntryLocator FromFileSystemURL(const storage::FileSystemURL& url);

    EntryLocator(const EntryPathType& type,
                 const base::FilePath& path,
                 const absl::optional<storage::BucketLocator>& bucket_locator);
    EntryLocator(const EntryLocator&);
    ~EntryLocator();

    bool operator<(const EntryLocator& other) const;

    const EntryPathType type;
    const base::FilePath path;
    // Non-null iff `type` is kSandboxed.
    const absl::optional<storage::BucketLocator> bucket_locator;
  };

  // This class represents an active lock on an entry locator. The lock is
  // released on destruction.
  class CONTENT_EXPORT Lock : public base::RefCounted<Lock> {
   public:
    Lock(base::WeakPtr<FileSystemAccessLockManager> lock_manager,
         const EntryLocator& entry_locator,
         const LockType& type,
         const scoped_refptr<Lock> parent_lock,
         base::PassKey<FileSystemAccessLockManager> pass_key);

    Lock(Lock const&) = delete;
    Lock& operator=(Lock const&) = delete;

    const LockType& type() const { return type_; }

   private:
    friend class base::RefCounted<Lock>;
    // The lock is released on destruction.
    ~Lock();

    SEQUENCE_CHECKER(sequence_checker_);

    // The FileSystemAccessLockManager that created this instance. Used on
    // destruction to release the lock on the file.
    base::WeakPtr<FileSystemAccessLockManager> lock_manager_
        GUARDED_BY_CONTEXT(sequence_checker_);

    // Locator of the file or directory associated with this lock. It is used to
    // unlock the exclusive lock on closure/destruction.
    const EntryLocator entry_locator_;

    const LockType type_;

    // When a file or directory is locked, it acquires a shared lock on its
    // parent directory, which acquires a shared lock on its parent, and so
    // forth. When this instance goes away, the associated ancestor locks are
    // automatically released. May be null if this instance represents the root
    // of its file system.
    const scoped_refptr<Lock> parent_lock_;
  };

  explicit FileSystemAccessLockManager(
      base::PassKey<FileSystemAccessManagerImpl> pass_key);
  ~FileSystemAccessLockManager();

  FileSystemAccessLockManager(FileSystemAccessLockManager const&) = delete;
  FileSystemAccessLockManager& operator=(FileSystemAccessLockManager const&) =
      delete;

  // Attempts to take a lock on `url`. Returns the lock if successful.
  scoped_refptr<Lock> TakeLock(const storage::FileSystemURL& url,
                               LockType lock_type);

 private:
  scoped_refptr<Lock> TakeLockImpl(const EntryLocator& entry_locator,
                                   LockType lock_type);

  // Releases the lock on `entry_locator`. Called from the Lock destructor.
  void ReleaseLock(const EntryLocator& entry_locator);

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<EntryLocator, Lock*> locks_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessLockManager> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCK_MANAGER_H_
