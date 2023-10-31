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
class RootLock;

// This class is in charge of the creation of Locks. Locks restrict the access
// to a specific file or directory, preventing unexpected concurrent access to
// data. It is owned by the FileSystemAccessManagerImpl.
class CONTENT_EXPORT FileSystemAccessLockManager {
 public:
  class LockHandle;

  using PassKey = base::PassKey<FileSystemAccessLockManager>;

  // This type represents a locking type used to prevent other locking types
  // from acquiring a lock.
  using LockType = base::IdType32<class LockTypeTag>;

  using TakeLockCallback = base::OnceCallback<void(scoped_refptr<LockHandle>)>;

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

    LockHandle(base::WeakPtr<Lock> lock,
               scoped_refptr<LockHandle> parent_lock_handle);

    // On destruction, lets its `lock_` know it is no longer held.
    ~LockHandle();

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtr<Lock> lock_ GUARDED_BY_CONTEXT(sequence_checker_);
    const LockType type_;
    const bool is_exclusive_;

    const scoped_refptr<LockHandle> parent_lock_handle_;
  };

  explicit FileSystemAccessLockManager(
      base::PassKey<FileSystemAccessManagerImpl> pass_key);
  ~FileSystemAccessLockManager();

  FileSystemAccessLockManager(FileSystemAccessLockManager const&) = delete;
  FileSystemAccessLockManager& operator=(FileSystemAccessLockManager const&) =
      delete;

  // Attempts to take a lock of `lock_type` on `url`. Passes a handle of the
  // lock to `callback` if successful. The lock is released when there are no
  // handles to it.
  void TakeLock(const storage::FileSystemURL& url,
                LockType lock_type,
                TakeLockCallback callback);

  // Returns true if there is not an existing lock on `url` that is contentious
  // with `lock_type`.
  bool IsContentious(const storage::FileSystemURL& url, LockType lock_type);

  // Creates a new shared lock type.
  [[nodiscard]] LockType CreateSharedLockType();

  // Gets the exclusive lock type.
  [[nodiscard]] LockType GetExclusiveLockType();

  // Gets the `ancestor_lock_type_` for testing.
  [[nodiscard]] LockType GetAncestorLockTypeForTesting();

 private:
  friend RootLock;

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

  struct RootLocator {
    static RootLocator FromFileSystemURL(const storage::FileSystemURL& url);

    RootLocator(const EntryPathType& type,
                const absl::optional<storage::BucketLocator>& bucket_locator);
    RootLocator(const RootLocator&);
    ~RootLocator();

    bool operator<(const RootLocator& other) const;

    const EntryPathType type;
    // Non-null iff `type` is kSandboxed.
    const absl::optional<storage::BucketLocator> bucket_locator;
  };

  // Releases the root lock for `root_locator`. Called from the RootLock.
  void ReleaseRoot(const RootLocator& root_locator);

  RootLock* GetRootLock(const RootLocator& root_locator);
  RootLock* GetOrCreateRootLock(const RootLocator& root_locator);

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<RootLocator, std::unique_ptr<RootLock>> root_locks_
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
