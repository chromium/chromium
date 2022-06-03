// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WRITE_LOCK_MANAGER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WRITE_LOCK_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class FileSystemAccessManagerImpl;

// This class is in charge of the creation of WriteLocks. WriteLocks restrict
// the access to a specific file, preventing unexpected concurrent access to
// data. It is owned by the FileSystemAccessManagerImpl.
class CONTENT_EXPORT FileSystemAccessWriteLockManager {
 public:
  using PassKey = base::PassKey<FileSystemAccessWriteLockManager>;

  enum class WriteLockType {
    // An exclusive lock prevents the taking of any new lock.
    kExclusive,
    // A shared lock prevents the taking of new exclusive locks, while allowing
    // the taking of shared ones.
    kShared
  };

  // This class represents an active write lock on a file. The lock is released
  // on destruction.
  class CONTENT_EXPORT WriteLock : public base::RefCounted<WriteLock> {
   public:
    WriteLock(base::WeakPtr<FileSystemAccessWriteLockManager> lock_manager,
              const storage::FileSystemURL& url,
              const WriteLockType& type,
              base::PassKey<FileSystemAccessWriteLockManager> pass_key);

    WriteLock(WriteLock const&) = delete;
    WriteLock& operator=(WriteLock const&) = delete;

    const WriteLockType& type() const { return type_; }

   private:
    friend class base::RefCounted<WriteLock>;
    // The lock is released on destruction.
    ~WriteLock();

    // The FileSystemAccessWriteLockManager that created this instance. Used on
    // destruction to release the lock on the file.
    base::WeakPtr<FileSystemAccessWriteLockManager> lock_manager_;

    // URL of the file associated with this lock. It is used to unlock the
    // exclusive write lock on closure/destruction.
    const storage::FileSystemURL url_;

    const WriteLockType type_;
  };

  explicit FileSystemAccessWriteLockManager(
      base::PassKey<FileSystemAccessManagerImpl> pass_key);
  ~FileSystemAccessWriteLockManager();

  FileSystemAccessWriteLockManager(FileSystemAccessWriteLockManager const&) =
      delete;
  FileSystemAccessWriteLockManager& operator=(
      FileSystemAccessWriteLockManager const&) = delete;

  // Attempts to take a lock on `url`. Returns the lock if successful.
  absl::optional<scoped_refptr<FileSystemAccessWriteLockManager::WriteLock>>
  TakeLock(const storage::FileSystemURL& url, WriteLockType lock_type);

 private:
  // Releases the lock on `url`. Called from the WriteLock destructor.
  void ReleaseLock(const storage::FileSystemURL& url);

  std::map<storage::FileSystemURL,
           FileSystemAccessWriteLockManager::WriteLock*,
           storage::FileSystemURL::Comparator>
      locks_;

  base::WeakPtrFactory<FileSystemAccessWriteLockManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WRITE_LOCK_MANAGER_H_
