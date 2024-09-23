// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/vfs_wrapper_fuchsia.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "sql/vfs_wrapper.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

struct FileLock {
  int lock_level;
  // Used to track the pointers to different VfsFile instances that hold shared
  // locks on the same underlying file. The pointer is only used as a unique id
  // for the VfsFile instance. The contents are never accessed.
  base::flat_set<VfsFile*> readers = {};
  // Used to track a VfsFile instance that holds a reserved/pending/exclusive
  // lock for writing. The pointer is only used as a unique id for the VfsFile
  // instance. The contents are never accessed.
  VfsFile* writer = nullptr;
};

// Singleton that stores and mutates state as described in
// https://www.sqlite.org/lockingv3.html
class FuchsiaFileLockManager {
 public:
  FuchsiaFileLockManager() = default;

  // Returns lock manager for the current process.
  static FuchsiaFileLockManager* Instance() {
    static base::NoDestructor<FuchsiaFileLockManager> lock_manager;
    return lock_manager.get();
  }

  int Lock(VfsFile* vfs_file, int requested_lock) {
    DCHECK_GT(requested_lock, SQLITE_LOCK_NONE)
        << "SQLITE_LOCK_NONE can only be set via Unlock";
    base::AutoLock lock(lock_);
    const auto file_lock_state = GetFileLockStateLocked(vfs_file);

    // Allow any lock level since the lock isn't held.
    if (file_lock_state.readers.empty() && file_lock_state.writer == nullptr) {
      if (requested_lock == SQLITE_LOCK_SHARED) {
        locked_files_[vfs_file->file_name] = {.lock_level = requested_lock,
                                              .readers = {vfs_file}};
      } else {
        locked_files_[vfs_file->file_name] = {.lock_level = requested_lock,
                                              .writer = vfs_file};
      }

      return SQLITE_OK;
    }

    if (requested_lock == SQLITE_LOCK_SHARED) {
      if (file_lock_state.lock_level >= SQLITE_LOCK_PENDING) {
        DVLOG(1) << "lock for file " << vfs_file->file_name
                 << " is held by a writer and cannot be shared.";
        return SQLITE_BUSY;
      }

      locked_files_[vfs_file->file_name].readers.insert(vfs_file);
      return SQLITE_OK;
    }

    if (file_lock_state.writer != nullptr &&
        file_lock_state.writer != vfs_file) {
      DVLOG(1) << "lock for file " << vfs_file->file_name
               << " is already held by another writer.";
      return SQLITE_BUSY;
    }

    if (requested_lock == SQLITE_LOCK_EXCLUSIVE &&
        (file_lock_state.readers.size() > 1 ||
         (file_lock_state.readers.size() == 1 &&
          !file_lock_state.readers.contains(vfs_file)))) {
      DVLOG(1) << "lock for file " << vfs_file->file_name
               << " is held by readers and can't yet be upgraded to exclusive.";
      return SQLITE_BUSY;
    }

    DCHECK(file_lock_state.writer == nullptr ||
           file_lock_state.writer == vfs_file);
    locked_files_[vfs_file->file_name].lock_level = requested_lock;
    locked_files_[vfs_file->file_name].writer = vfs_file;
    locked_files_[vfs_file->file_name].readers.erase(vfs_file);
    DCHECK(locked_files_[vfs_file->file_name].lock_level <
               SQLITE_LOCK_EXCLUSIVE ||
           locked_files_[vfs_file->file_name].readers.empty());
    return SQLITE_OK;
  }

  int Unlock(VfsFile* vfs_file, int requested_lock) {
    base::AutoLock lock(lock_);
    const auto file_lock_state = GetFileLockStateLocked(vfs_file);

    DCHECK_LE(requested_lock, file_lock_state.lock_level)
        << "Attempted to unlock to a higher lock level, unlock can only "
           "decrement.";

    // Shortcut if the caller doesn't currently hold a lock.
    if (!file_lock_state.readers.contains(vfs_file) &&
        file_lock_state.writer != vfs_file) {
      DVLOG(1) << "caller can't unlock because it doesn't currently "
               << "hold a lock for file " << vfs_file->file_name;
      return SQLITE_OK;
    }

    if (requested_lock == SQLITE_LOCK_NONE) {
      locked_files_[vfs_file->file_name].readers.erase(vfs_file);
    } else if (requested_lock == SQLITE_LOCK_SHARED) {
      locked_files_[vfs_file->file_name].readers.insert(vfs_file);
    }

    if (requested_lock < SQLITE_LOCK_RESERVED &&
        file_lock_state.writer == vfs_file) {
      locked_files_[vfs_file->file_name].writer = nullptr;
    }

    // Check that `vfs_file` is correctly tracked given the `requested_lock`.
    DCHECK(requested_lock == SQLITE_LOCK_SHARED ||
           !locked_files_[vfs_file->file_name].readers.contains(vfs_file));
    DCHECK_EQ(requested_lock > SQLITE_LOCK_SHARED,
              locked_files_[vfs_file->file_name].writer == vfs_file);

    // Mark lock level as shared if there are only shared usages.
    if (!file_lock_state.readers.empty() && file_lock_state.writer == nullptr) {
      locked_files_[vfs_file->file_name].lock_level = SQLITE_LOCK_SHARED;
      return SQLITE_OK;
    }

    // Remove lock if there are no usages left.
    if (file_lock_state.readers.empty() && file_lock_state.writer == nullptr) {
      DCHECK_EQ(requested_lock, SQLITE_LOCK_NONE);
      locked_files_.erase(vfs_file->file_name);
      return SQLITE_OK;
    }

    if (file_lock_state.writer != vfs_file) {
      DCHECK_GE(file_lock_state.lock_level, SQLITE_LOCK_RESERVED);
      DCHECK_LE(requested_lock, SQLITE_LOCK_SHARED);
      return SQLITE_OK;
    }

    locked_files_[vfs_file->file_name].lock_level = requested_lock;
    return SQLITE_OK;
  }

  int CheckReservedLock(VfsFile* vfs_file, int* result) {
    base::AutoLock lock(lock_);
    const auto file_lock_state = GetFileLockStateLocked(vfs_file);

    switch (file_lock_state.lock_level) {
      case SQLITE_LOCK_NONE:
      case SQLITE_LOCK_SHARED:
        *result = 0;
        return SQLITE_OK;
      case SQLITE_LOCK_RESERVED:
      case SQLITE_LOCK_PENDING:
      case SQLITE_LOCK_EXCLUSIVE:
        *result = 1;
        return SQLITE_OK;
      default:
        return SQLITE_IOERR_CHECKRESERVEDLOCK;
    }
  }

 private:
  ~FuchsiaFileLockManager() = delete;

  const FileLock& GetFileLockStateLocked(VfsFile* vfs_file)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    static const FileLock kUnlockedFileLock = {.lock_level = SQLITE_LOCK_NONE};
    const auto file_lock_state_iter = locked_files_.find(vfs_file->file_name);
    if (file_lock_state_iter == locked_files_.end()) {
      return kUnlockedFileLock;
    }

    return file_lock_state_iter->second;
  }

  base::Lock lock_;

  // Set of all currently locked files.
  base::flat_map<std::string, FileLock> locked_files_ GUARDED_BY(lock_);
};

}  // namespace

int Lock(sqlite3_file* sqlite_file, int file_lock) {
  DCHECK(file_lock == SQLITE_LOCK_SHARED || file_lock == SQLITE_LOCK_RESERVED ||
         file_lock == SQLITE_LOCK_PENDING ||
         file_lock == SQLITE_LOCK_EXCLUSIVE);

  auto* vfs_file = reinterpret_cast<VfsFile*>(sqlite_file);
  return FuchsiaFileLockManager::Instance()->Lock(vfs_file, file_lock);
}

int Unlock(sqlite3_file* sqlite_file, int file_lock) {
  auto* vfs_file = reinterpret_cast<VfsFile*>(sqlite_file);
  return FuchsiaFileLockManager::Instance()->Unlock(vfs_file, file_lock);
}

int CheckReservedLock(sqlite3_file* sqlite_file, int* result) {
  auto* vfs_file = reinterpret_cast<VfsFile*>(sqlite_file);
  return FuchsiaFileLockManager::Instance()->CheckReservedLock(vfs_file,
                                                               result);
}

}  // namespace sql
