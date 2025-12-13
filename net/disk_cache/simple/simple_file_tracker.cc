// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_file_tracker.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"

namespace disk_cache {

namespace {

void RecordFileDescripterLimiterOp(FileDescriptorLimiterOp op) {
  UMA_HISTOGRAM_ENUMERATION("SimpleCache.FileDescriptorLimiterAction", op,
                            FD_LIMIT_OP_MAX);
}

}  // namespace

bool SimpleFileTracker::TrackedFiles::InLRUList() const {
  // Either both should be set, or neither.
  DCHECK((next() && previous()) || (!next() && !previous()));
  return next();
}

SimpleFileTracker::SimpleFileTracker(int file_limit)
    : file_limit_(file_limit) {}

SimpleFileTracker::~SimpleFileTracker() {
  DCHECK(lru_.empty());
  DCHECK(tracked_files_.empty());
}
void SimpleFileTracker::TrackedFiles::RemoveIfLinked() {
  if (InLRUList()) {
    RemoveFromList();
  }
}

void SimpleFileTracker::Register(const SimpleSynchronousEntry* owner,
                                 SubFile subfile,
                                 std::unique_ptr<CacheFile> file) {
  DCHECK(file->IsValid());
  std::vector<std::unique_ptr<CacheFile>> files_to_close;

  {
    base::AutoLock hold_lock(lock_);

    // Make sure the list of everything with given hash exists.
    auto insert_status =
        tracked_files_.emplace(owner->entry_file_key().entry_hash,
                               std::vector<std::unique_ptr<TrackedFiles>>());

    std::vector<std::unique_ptr<TrackedFiles>>& candidates =
        insert_status.first->second;

    // See if entry for |owner| already exists, if not append.
    TrackedFiles* owners_files = nullptr;
    for (const std::unique_ptr<TrackedFiles>& candidate : candidates) {
      if (candidate->owner == owner) {
        owners_files = candidate.get();
        break;
      }
    }

    if (!owners_files) {
      candidates.emplace_back(std::make_unique<TrackedFiles>());
      owners_files = candidates.back().get();
      owners_files->owner = owner;
      owners_files->key = owner->entry_file_key();
    }

    EnsureInFrontOfLRU(owners_files);

    int file_index = static_cast<int>(subfile);
    DCHECK_EQ(TrackedFiles::TF_NO_REGISTRATION,
              owners_files->state[file_index]);
    owners_files->files[file_index] = std::move(file);
    owners_files->state[file_index] = TrackedFiles::TF_REGISTERED;
    ++open_files_;
    CloseFilesIfTooManyOpen(&files_to_close);
  }
}

SimpleFileTracker::FileHandle SimpleFileTracker::Acquire(
    BackendFileOperations* file_operations,
    const SimpleSynchronousEntry* owner,
    SubFile subfile) {
  std::vector<std::unique_ptr<CacheFile>> files_to_close;

  {
    base::AutoLock hold_lock(lock_);
    TrackedFiles* owners_files = Find(owner);
    int file_index = static_cast<int>(subfile);

    DCHECK_EQ(TrackedFiles::TF_REGISTERED, owners_files->state[file_index]);
    owners_files->state[file_index] = TrackedFiles::TF_ACQUIRED;
    EnsureInFrontOfLRU(owners_files);

    // Check to see if we have to reopen the file. That might push us over the
    // fd limit.  CloseFilesIfTooManyOpen will not close anything in
    // |*owners_files| since it's already in the the TF_ACQUIRED state.
    if (owners_files->files[file_index] == nullptr) {
      ReopenFile(file_operations, owners_files, subfile);
      CloseFilesIfTooManyOpen(&files_to_close);
    }

    return FileHandle(this, owner, subfile,
                      owners_files->files[file_index].get());
  }
}

SimpleFileTracker::TrackedFiles::TrackedFiles() {
  std::ranges::fill(state, TF_NO_REGISTRATION);
}

SimpleFileTracker::TrackedFiles::~TrackedFiles() = default;

bool SimpleFileTracker::TrackedFiles::Empty() const {
  for (State s : state)
    if (s != TF_NO_REGISTRATION)
      return false;
  return true;
}

bool SimpleFileTracker::TrackedFiles::HasOpenFiles() const {
  for (const std::unique_ptr<CacheFile>& file : files) {
    if (file != nullptr)
      return true;
  }
  return false;
}

void SimpleFileTracker::Release(const SimpleSynchronousEntry* owner,
                                SubFile subfile) {
  std::vector<std::unique_ptr<CacheFile>> files_to_close;

  {
    base::AutoLock hold_lock(lock_);
    TrackedFiles* owners_files = Find(owner);
    int file_index = static_cast<int>(subfile);

    DCHECK(owners_files->state[file_index] == TrackedFiles::TF_ACQUIRED ||
           owners_files->state[file_index] ==
               TrackedFiles::TF_ACQUIRED_PENDING_CLOSE);

    // Prepare to executed deferred close, if any.
    if (owners_files->state[file_index] ==
        TrackedFiles::TF_ACQUIRED_PENDING_CLOSE) {
      files_to_close.push_back(PrepareClose(owners_files, file_index));
    } else {
      owners_files->state[file_index] = TrackedFiles::TF_REGISTERED;
    }

    // It's possible that we were over limit and couldn't do much about it
    // since everything was lent out, so now may be the time to close extra
    // stuff.
    CloseFilesIfTooManyOpen(&files_to_close);
  }
}

void SimpleFileTracker::Close(const SimpleSynchronousEntry* owner,
                              SubFile subfile) {
  std::unique_ptr<CacheFile> file_to_close;

  {
    base::AutoLock hold_lock(lock_);
    TrackedFiles* owners_files = Find(owner);
    int file_index = static_cast<int>(subfile);

    DCHECK(owners_files->state[file_index] == TrackedFiles::TF_ACQUIRED ||
           owners_files->state[file_index] == TrackedFiles::TF_REGISTERED);

    if (owners_files->state[file_index] == TrackedFiles::TF_ACQUIRED) {
      // The FD is currently acquired, so we can't clean up the TrackedFiles,
      // just yet; even if this is the last close, so delay the close until it
      // gets released.
      owners_files->state[file_index] = TrackedFiles::TF_ACQUIRED_PENDING_CLOSE;
    } else {
      file_to_close = PrepareClose(owners_files, file_index);
    }
  }
}

void SimpleFileTracker::Doom(const SimpleSynchronousEntry* owner,
                             EntryFileKey* key) {
  base::AutoLock hold_lock(lock_);
  auto iter = tracked_files_.find(key->entry_hash);
  CHECK(iter != tracked_files_.end());

  uint64_t max_doom_gen = 0;
  for (const std::unique_ptr<TrackedFiles>& file_with_same_hash :
       iter->second) {
    max_doom_gen =
        std::max(max_doom_gen, file_with_same_hash->key.doom_generation);
  }

  // It would take >502 years to doom the same hash enough times (at 10^9 dooms
  // per second) to wrap the 64 bit counter. Still, if it does wrap around,
  // there is a security risk since we could confuse different keys.
  CHECK_NE(max_doom_gen, std::numeric_limits<uint64_t>::max());
  uint64_t new_doom_gen = max_doom_gen + 1;

  // Update external key.
  key->doom_generation = new_doom_gen;

  // Update our own.
  for (const std::unique_ptr<TrackedFiles>& file_with_same_hash :
       iter->second) {
    if (file_with_same_hash->owner == owner)
      file_with_same_hash->key.doom_generation = new_doom_gen;
  }
}

bool SimpleFileTracker::IsEmptyForTesting() {
  base::AutoLock hold_lock(lock_);
  return tracked_files_.empty() && lru_.empty();
}

SimpleFileTracker::TrackedFiles* SimpleFileTracker::Find(
    const SimpleSynchronousEntry* owner) {
  auto candidates = tracked_files_.find(owner->entry_file_key().entry_hash);
  CHECK(candidates != tracked_files_.end());
  for (const auto& candidate : candidates->second) {
    if (candidate->owner == owner) {
      return candidate.get();
    }
  }
  LOG(DFATAL) << "SimpleFileTracker operation on non-found entry";
  return nullptr;
}

std::unique_ptr<CacheFile> SimpleFileTracker::PrepareClose(
    TrackedFiles* owners_files,
    int file_index) {
  std::unique_ptr<CacheFile> file_out =
      std::move(owners_files->files[file_index]);
  owners_files->state[file_index] = TrackedFiles::TF_NO_REGISTRATION;
  if (owners_files->Empty()) {
    auto iter = tracked_files_.find(owners_files->key.entry_hash);
    for (auto i = iter->second.begin(); i != iter->second.end(); ++i) {
      if ((*i).get() == owners_files) {
        owners_files->RemoveIfLinked();
        iter->second.erase(i);
        break;
      }
    }
    if (iter->second.empty())
      tracked_files_.erase(iter);
  }
  if (file_out != nullptr)
    --open_files_;
  return file_out;
}

void SimpleFileTracker::CloseFilesIfTooManyOpen(
    std::vector<std::unique_ptr<CacheFile>>* files_to_close) {
  TrackedFiles* node = lru_.tail()->value();
  while (open_files_ > file_limit_ && node != lru_.end()) {
    // Grab the previous node *before* we possibly remove |node| from the list.
    TrackedFiles* previous = node->previous()->value();
    DCHECK(node->InLRUList());
    // Close TF_REGISTERED subfiles for this node.
    for (int j = 0; j < kSimpleEntryTotalFileCount; ++j) {
      if (node->state[j] == TrackedFiles::TF_REGISTERED &&
          node->files[j] != nullptr) {
        files_to_close->push_back(std::move(node->files[j]));
        --open_files_;
        RecordFileDescripterLimiterOp(FD_LIMIT_CLOSE_FILE);
      }
    }

    if (!node->HasOpenFiles()) {
      // If there is nothing here that can possibly be closed, remove this from
      // LRU for now so we don't have to rescan it next time we are here. If the
      // files get re-opened (in Acquire), it will get added back in.
      DCHECK(node->InLRUList());
      node->RemoveIfLinked();
    }
    // Move to the previous item in the list
    node = previous;
  }
}

void SimpleFileTracker::ReopenFile(BackendFileOperations* file_operations,
                                   TrackedFiles* owners_files,
                                   SubFile subfile) {
  int file_index = static_cast<int>(subfile);
  DCHECK(owners_files->files[file_index] == nullptr);
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  base::FilePath file_path =
      owners_files->owner->GetFilenameForSubfile(subfile);
  owners_files->files[file_index] = file_operations->OpenFile(file_path, flags);
  if (owners_files->files[file_index]->IsValid()) {
    RecordFileDescripterLimiterOp(FD_LIMIT_REOPEN_FILE);

    ++open_files_;
  } else {
    owners_files->files[file_index] = nullptr;
    RecordFileDescripterLimiterOp(FD_LIMIT_FAIL_REOPEN_FILE);
  }
}

void SimpleFileTracker::EnsureInFrontOfLRU(TrackedFiles* owners_files) {
  if (lru_.head() == owners_files) {
    return;
  }
  owners_files->RemoveIfLinked();
  if (lru_.empty()) {
    lru_.Append(owners_files);
  } else {
    auto* head = lru_.head()->value();
    owners_files->InsertBefore(head);
  }
  DCHECK_EQ(lru_.head(), owners_files);
}

SimpleFileTracker::FileHandle::FileHandle() = default;

SimpleFileTracker::FileHandle::FileHandle(SimpleFileTracker* file_tracker,
                                          const SimpleSynchronousEntry* entry,
                                          SimpleFileTracker::SubFile subfile,
                                          CacheFile* file)
    : file_tracker_(file_tracker),
      entry_(entry),
      subfile_(subfile),
      file_(file) {}

SimpleFileTracker::FileHandle::FileHandle(FileHandle&& other) {
  *this = std::move(other);
}

SimpleFileTracker::FileHandle::~FileHandle() {
  file_ = nullptr;
  if (entry_) {
    file_tracker_->Release(entry_.ExtractAsDangling(), subfile_);
  }
}

SimpleFileTracker::FileHandle& SimpleFileTracker::FileHandle::operator=(
    FileHandle&& other) {
  file_tracker_ = other.file_tracker_;
  entry_ = other.entry_;
  subfile_ = other.subfile_;
  file_ = other.file_;
  other.file_tracker_ = nullptr;
  other.entry_ = nullptr;
  other.file_ = nullptr;
  return *this;
}

CacheFile* SimpleFileTracker::FileHandle::operator->() const {
  return file_;
}

CacheFile* SimpleFileTracker::FileHandle::get() const {
  return file_;
}

bool SimpleFileTracker::FileHandle::IsOK() const {
  return file_ && file_->IsValid();
}

}  // namespace disk_cache
