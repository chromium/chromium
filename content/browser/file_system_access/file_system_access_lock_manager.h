// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCK_MANAGER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCK_MANAGER_H_

#include <map>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace content {

class FileSystemAccessManagerImpl;
class Lock;

// This class is in charge of the creation of Locks. Locks restrict the access
// to a specific file or directory, preventing unexpected concurrent access to
// data. It is owned by the FileSystemAccessManagerImpl.
class CONTENT_EXPORT FileSystemAccessLockManager {
 public:
  using PassKey = base::PassKey<FileSystemAccessLockManager>;

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

  // This type represents a locking type used to prevent other locking types
  // from acquiring a lock.
  using LockType = base::IdType32<class LockTypeTag>;

  // A handle to a `Lock` passed to the frame that holds the lock. The `Lock` is
  // kept alive as long as `LockHandle` is kept alive.
  class CONTENT_EXPORT LockHandle : public base::RefCounted<LockHandle> {
   public:
    LockHandle(LockHandle const&) = delete;
    LockHandle& operator=(LockHandle const&) = delete;

    const LockType& type() const { return type_; }

    bool IsExclusive() const { return is_exclusive_; }

   private:
    friend class Lock;
    friend class base::RefCounted<LockHandle>;

    explicit LockHandle(base::WeakPtr<Lock> lock);

    // On destruction, lets its `lock_` know it is no longer held.
    ~LockHandle();

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtr<Lock> lock_ GUARDED_BY_CONTEXT(sequence_checker_);
    const LockType type_;
    const bool is_exclusive_;
  };

  explicit FileSystemAccessLockManager(
      base::PassKey<FileSystemAccessManagerImpl> pass_key);
  ~FileSystemAccessLockManager();

  FileSystemAccessLockManager(FileSystemAccessLockManager const&) = delete;
  FileSystemAccessLockManager& operator=(FileSystemAccessLockManager const&) =
      delete;

  // Attempts to take a lock of `lock_type` on `url`. Returns a handle to the
  // lock if successful. The lock is released when there are no handles to it.
  scoped_refptr<LockHandle> TakeLock(const storage::FileSystemURL& url,
                                     LockType lock_type);

  // Creates a new shared lock type.
  [[nodiscard]] LockType CreateSharedLockType();

  // Gets the exclusive lock type.
  [[nodiscard]] LockType GetExclusiveLockType();

  // Gets the `ancestor_lock_type_` for testing.
  [[nodiscard]] LockType GetAncestorLockTypeForTesting();

 private:
  friend Lock;

  scoped_refptr<LockHandle> TakeLockImpl(const EntryLocator& entry_locator,
                                         LockType lock_type);

  // Releases the lock on `entry_locator`. Called from the Lock destructor.
  void ReleaseLock(const EntryLocator& entry_locator);

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<EntryLocator, std::unique_ptr<Lock>> locks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  LockType::Generator lock_type_generator_;

  LockType exclusive_lock_type_ = lock_type_generator_.GenerateNextId();

  // The shared lock type that the lock manager uses to lock ancestors of locked
  // entry locators. Should not be used outside of the lock manager or testing.
  LockType ancestor_lock_type_ = lock_type_generator_.GenerateNextId();

  base::WeakPtrFactory<FileSystemAccessLockManager> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCK_MANAGER_H_
