// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_FILE_TRACKER_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_FILE_TRACKER_H_

#include <stdint.h>
#include <algorithm>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/base/net_export.h"
#include "net/disk_cache/simple/simple_entry_format.h"

namespace disk_cache {

class SimpleSynchronousEntry;

// This keeps track of all the files SimpleCache has open, across all the
// backend instancess, in order to prevent us from running out of file
// descriptors.
// TODO(morlovich): Actually implement closing and re-opening of things if we
// run out.
//
// This class is thread-safe.
class NET_EXPORT_PRIVATE SimpleFileTracker {
 public:
  enum class SubFile { FILE_0, FILE_1, FILE_SPARSE };

  // A RAII helper that guards access to a file grabbed for use from
  // SimpleFileTracker::Acquire().  While it's still alive, if IsOK() is true,
  // then using the underlying base::File via get() or the -> operator will be
  // safe.
  //
  // This class is movable but not copyable.  It should only be used from a
  // single logical sequence of execution, and should not outlive the
  // corresponding SimpleSynchronousEntry.
  class NET_EXPORT_PRIVATE FileHandle {
   public:
    FileHandle();
    FileHandle(FileHandle&& other);
    ~FileHandle();
    FileHandle& operator=(FileHandle&& other);
    base::File* operator->() const;
    base::File* get() const;
    // Returns true if this handle points to a valid file. This should normally
    // be the first thing called on the object, after getting it from
    // SimpleFileTracker::Acquire.
    bool IsOK() const;

   private:
    friend class SimpleFileTracker;
    FileHandle(SimpleFileTracker* file_tracker,
               const SimpleSynchronousEntry* entry,
               SimpleFileTracker::SubFile subfile,
               base::File* file);

    // All the pointer fields are nullptr in the default/moved away from form.
    SimpleFileTracker* file_tracker_ = nullptr;
    const SimpleSynchronousEntry* entry_ = nullptr;
    SimpleFileTracker::SubFile subfile_;
    base::File* file_ = nullptr;
    DISALLOW_COPY_AND_ASSIGN(FileHandle);
  };

  struct EntryFileKey {
    EntryFileKey() {}
    explicit EntryFileKey(uint64_t hash) : entry_hash(hash) {}

    uint64_t entry_hash = 0;

    // 0 means this a non-doomed, active entry, for its backend that will be
    // checked on OpenEntry(key) where hash(key) = entry_hash.  Other values of
    // |doom_generation| are used to generate distinct file names for entries
    // that have been Doom()ed, either by explicit API call by the client or
    // internal operation (eviction, collisions, etc.)
    uint64_t doom_generation = 0;
  };

  // The default limit here is half of what's available on our target OS where
  // Chrome has the lowest limit.
  SimpleFileTracker(int file_limit = 512);
  ~SimpleFileTracker();

  // Established |file| as what's backing |subfile| for |owner|. This is
  // intended to be called when SimpleSynchronousEntry first sets up the file to
  // transfer its ownership to SimpleFileTracker. Any Register() call must be
  // eventually followed by a corresponding Close() call before the |owner| is
  // destroyed. |file->IsValid()| must be true.
  void Register(const SimpleSynchronousEntry* owner,
                SubFile subfile,
                std::unique_ptr<base::File> file);

  // Lends out a file to SimpleSynchronousEntry for use. SimpleFileTracker
  // will ensure that it doesn't close the file until the handle is destroyed.
  // The caller should check .IsOK() on the returned value before using it, as
  // it's possible that the file had to be closed and re-opened due to FD
  // pressure, and that open may have failed. This should not be called twice
  // with the exact same arguments until the handle returned from the previous
  // such call is destroyed.
  FileHandle Acquire(const SimpleSynchronousEntry* owner, SubFile subfile);

  // Tells SimpleFileTracker that SimpleSynchronousEntry will not be interested
  // in the file further, so it can be closed and forgotten about.  It's OK to
  // call this while a handle to the file is alive, in which case the effect
  // takes place after the handle is destroyed.
  // If Close() has been called and the handle to the file is no longer alive,
  // a new backing file can be established by calling Register() again.
  void Close(const SimpleSynchronousEntry* owner, SubFile file);

  // Updates key->doom_generation to one not in use for the hash; it's the
  // caller's responsibility to update file names accordingly, and to prevent
  // others from stomping on things while this is going on. SimpleBackendImpl's
  // entries_pending_doom_ is the mechanism for protecting this action from
  // races.
  void Doom(const SimpleSynchronousEntry* owner, EntryFileKey* key);

  // Returns true if there is no in-memory state around, e.g. everything got
  // cleaned up. This is a test-only method since this object is expected to be
  // shared between multiple threads, in which case its return value may be
  // outdated the moment it's returned.
  bool IsEmptyForTesting();

 private:
  struct TrackedFiles {
    // We can potentially run through this state machine multiple times for
    // FILE_1, as that's often missing, so SimpleSynchronousEntry can sometimes
    // close and remove the file for an empty stream, then re-open it on actual
    // data.
    enum State {
      TF_NO_REGISTRATION = 0,
      TF_REGISTERED = 1,
      TF_ACQUIRED = 2,
      TF_ACQUIRED_PENDING_CLOSE = 3,
    };

    NET_EXPORT_PRIVATE TrackedFiles();
    NET_EXPORT_PRIVATE ~TrackedFiles();

    // True if this isn't keeping track of anything any more.
    bool Empty() const;

    // True if this has open files. Note that this is not the same as !Empty()
    // as this may be false when an entry had its files temporarily closed, but
    // is still relevant.
    bool HasOpenFiles() const;

    // We use pointers to SimpleSynchronousEntry two ways:
    // 1) As opaque keys. This is handy as it avoids having to compare paths in
    //    case multiple backends use the same key. Since we access the
    //    bookkeeping under |lock_|
    //
    // 2) To get info on the caller of our operation.
    //    Accessing |owner| from any other TrackedFiles would be unsafe (as it
    //    may be doing its own thing in a different thread).
    const SimpleSynchronousEntry* owner;
    EntryFileKey key;

    // Some of these may be nullptr, if they are not open. Non-null pointers
    // to files that are not valid will not be stored here.
    // Note that these are stored indirect since we hand out pointers to these,
    // and we don't want those to become invalid if some other thread appends
    // things here.
    std::unique_ptr<base::File> files[kSimpleEntryTotalFileCount];

    State state[kSimpleEntryTotalFileCount];
    std::list<TrackedFiles*>::iterator position_in_lru;

    // true if position_in_lru is valid. For entries where we closed everything,
    // we try not to keep them in the LRU so that we don't have to constantly
    // rescan them.
    bool in_lru;
  };

  // Marks the file that was previously returned by Acquire as eligible for
  // closing again. Called by ~FileHandle.
  void Release(const SimpleSynchronousEntry* owner, SubFile subfile);

  // Precondition: entry for given |owner| must already be in tracked_files_
  TrackedFiles* Find(const SimpleSynchronousEntry* owner);

  // Handles state transition of closing file (when we are not deferring it),
  // and moves the file out. Note that this may delete |*owners_files|.
  std::unique_ptr<base::File> PrepareClose(TrackedFiles* owners_files,
                                           int file_index);

  // If too many files are open, picks some to close, and moves them to
  // |*files_to_close|, updating other state as appropriate.
  void CloseFilesIfTooManyOpen(
      std::vector<std::unique_ptr<base::File>>* files_to_close);

  // Tries to reopen given file, updating |*owners_files| if successful.
  void ReopenFile(TrackedFiles* owners_files, SubFile subfile);

  // Makes sure the entry is marked as most recently used, adding it to LRU
  // if needed.
  void EnsureInFrontOfLRU(TrackedFiles* owners_files);

  base::Lock lock_;
  std::unordered_map<uint64_t, std::vector<std::unique_ptr<TrackedFiles>>>
      tracked_files_;
  std::list<TrackedFiles*> lru_;

  int file_limit_;

  // How many actually open files we are using.
  // Note that when a thread commits to closing a file, but hasn't actually
  // executed the close yet, the file is no longer counted as open here, so this
  // might be a little off. This should be OK as long as file_limit_ is set
  // conservatively, considering SimpleCache's parallelism is bounded by a low
  // number of threads, and getting it exact would require re-acquiring the
  // lock after closing the file.
  int open_files_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SimpleFileTracker);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_FILE_TRACKER_H_
