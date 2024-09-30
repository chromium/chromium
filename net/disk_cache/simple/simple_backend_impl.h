// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/post_operation_waiter.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/disk_cache/simple/simple_index_delegate.h"

namespace net {
class PrioritizedTaskRunner;
}  // namespace net

namespace disk_cache {

// SimpleBackendImpl is a new cache backend that stores entries in individual
// files.
// See
// http://www.chromium.org/developers/design-documents/network-stack/disk-cache/very-simple-backend
//
// The SimpleBackendImpl provides safe iteration; mutating entries during
// iteration cannot cause a crash. It is undefined whether entries created or
// destroyed during the iteration will be included in any pre-existing
// iterations.
//
// The non-static functions below must be called on the sequence on which the
// SimpleBackendImpl instance is created.

class BackendCleanupTracker;
class BackendFileOperationsFactory;
class SimpleEntryImpl;
class SimpleFileTracker;
class SimpleIndex;

class NET_EXPORT_PRIVATE SimpleBackendImpl final : public Backend,
                                                   public SimpleIndexDelegate {
 public:
  // Note: only pass non-nullptr for |file_tracker| if you don't want the global
  // one (which things other than tests would want). |file_tracker| must outlive
  // the backend and all the entries, including their asynchronous close.
  // |Init()| must be called to finish the initialization process.
  SimpleBackendImpl(
      scoped_refptr<BackendFileOperationsFactory> file_operations_factory,
      const base::FilePath& path,
      scoped_refptr<BackendCleanupTracker> cleanup_tracker,
      SimpleFileTracker* file_tracker,
      int64_t max_bytes,
      net::CacheType cache_type,
      net::NetLog* net_log);

  ~SimpleBackendImpl() override;

  SimpleIndex* index() { return index_.get(); }

  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Finishes initialization. Always asynchronous.
  void Init(CompletionOnceCallback completion_callback);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int64_t max_bytes);

  // Returns the maximum file size permitted in this backend.
  int64_t MaxFileSize() const override;

  // The entry for |entry_hash| is being doomed; the backend will not attempt
  // run new operations for this |entry_hash| until the Doom is completed.
  //
  // The return value should be used to call OnOperationComplete.
  scoped_refptr<SimplePostOperationWaiterTable> OnDoomStart(
      uint64_t entry_hash);

  // SimpleIndexDelegate:
  void DoomEntries(std::vector<uint64_t>* entry_hashes,
                   CompletionOnceCallback callback) override;

  // Backend:
  int32_t GetEntryCount() const override;
  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority request_priority,
                        EntryResultCallback callback) override;
  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority request_priority,
                          EntryResultCallback callback) override;
  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority priority,
                                EntryResultCallback callback) override;
  net::Error DoomEntry(const std::string& key,
                       net::RequestPriority priority,
                       CompletionOnceCallback callback) override;
  net::Error DoomAllEntries(CompletionOnceCallback callback) override;
  net::Error DoomEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                CompletionOnceCallback callback) override;
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      Int64CompletionOnceCallback callback) override;
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(base::StringPairs* stats) override;
  void OnExternalCacheHit(const std::string& key) override;
  uint8_t GetEntryInMemoryData(const std::string& key) override;
  void SetEntryInMemoryData(const std::string& key, uint8_t data) override;

  net::PrioritizedTaskRunner* prioritized_task_runner() const {
    return prioritized_task_runner_.get();
  }

  static constexpr base::TaskTraits kWorkerPoolTaskTraits = {
      base::MayBlock(), base::WithBaseSyncPrimitives(),
      base::TaskPriority::USER_BLOCKING,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

#if BUILDFLAG(IS_ANDROID)
  // Note: null callback is OK, and will make the cache use a
  // base::android::ApplicationStatusListener. Callback returning nullptr
  // means to not use an app status listener at all.
  void set_app_status_listener_getter(
      ApplicationStatusListenerGetter app_status_listener_getter) {
    app_status_listener_getter_ = std::move(app_status_listener_getter);
  }
#endif

  base::WeakPtr<SimpleBackendImpl> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class SimpleIterator;
  friend class SimpleIterator;

  using EntryMap =
      std::unordered_map<uint64_t, raw_ptr<SimpleEntryImpl, CtnExperimental>>;

  class ActiveEntryProxy;
  friend class ActiveEntryProxy;

  // Return value of InitCacheStructureOnDisk().
  struct DiskStatResult {
    base::Time cache_dir_mtime;
    uint64_t max_size;
    bool detected_magic_number_mismatch;
    int net_error;
  };

  enum class PostOperationQueue { kNone, kPostDoom, kPostOpenByHash };

  void InitializeIndex(CompletionOnceCallback callback,
                       const DiskStatResult& result);

  // Dooms all entries previously accessed between |initial_time| and
  // |end_time|. Invoked when the index is ready.
  void IndexReadyForDoom(base::Time initial_time,
                         base::Time end_time,
                         CompletionOnceCallback callback,
                         int result);

  // Calculates the size of the entire cache. Invoked when the index is ready.
  void IndexReadyForSizeCalculation(Int64CompletionOnceCallback callback,
                                    int result);

  // Calculates the size all cache entries between |initial_time| and
  // |end_time|. Invoked when the index is ready.
  void IndexReadyForSizeBetweenCalculation(base::Time initial_time,
                                           base::Time end_time,
                                           Int64CompletionOnceCallback callback,
                                           int result);

  // Try to create the directory if it doesn't exist. This must run on the
  // sequence on which SimpleIndexFile is running disk I/O.
  static DiskStatResult InitCacheStructureOnDisk(
      std::unique_ptr<BackendFileOperations> file_operations,
      const base::FilePath& path,
      uint64_t suggested_max_size,
      net::CacheType cache_type);

  // Looks at current state of `post_doom_waiting_`,
  // `post_open_by_hash_waiting_` and `active_entries_` relevant to
  // `entry_hash`, and, as appropriate, either returns a valid entry matching
  // `entry_hash` and `key`, or returns nullptr and sets `post_operation` to
  // point to a vector of closures which will be invoked when it's an
  // appropriate time to try again. The caller is expected to append its retry
  // closure to that vector. `post_operation_queue` will be set exactly when
  // `post_operation` is.
  scoped_refptr<SimpleEntryImpl> CreateOrFindActiveOrDoomedEntry(
      uint64_t entry_hash,
      const std::string& key,
      net::RequestPriority request_priority,
      std::vector<base::OnceClosure>*& post_operation,
      PostOperationQueue& post_operation_queue);

  // If post-doom and settings indicates that optimistically succeeding a create
  // due to being immediately after a doom is possible, sets up an entry for
  // that, and returns a non-null pointer. (CreateEntry still needs to be called
  // to actually do the creation operation). Otherwise returns nullptr.
  //
  // Pre-condition: |post_doom| is non-null.
  scoped_refptr<SimpleEntryImpl> MaybeOptimisticCreateForPostDoom(
      uint64_t entry_hash,
      const std::string& key,
      net::RequestPriority request_priority,
      std::vector<base::OnceClosure>* post_doom);

  // Given a hash, will try to open the corresponding Entry. If we have an Entry
  // corresponding to |hash| in the map of active entries, opens it. Otherwise,
  // a new empty Entry will be created, opened and filled with information from
  // the disk.
  EntryResult OpenEntryFromHash(uint64_t entry_hash,
                                EntryResultCallback callback);

  // Doom the entry corresponding to |entry_hash|, if it's active or currently
  // pending doom. This function does not block if there is an active entry,
  // which is very important to prevent races in DoomEntries() above.
  net::Error DoomEntryFromHash(uint64_t entry_hash,
                               CompletionOnceCallback callback);

  // Called when we tried to open an entry with hash alone. When a blank entry
  // has been created and filled in with information from the disk - based on a
  // hash alone - this resumes operations that were waiting on the key
  // information to have been loaded.
  void OnEntryOpenedFromHash(uint64_t hash,
                             EntryResultCallback callback,
                             EntryResult result);

  // A callback thunk used by DoomEntries to clear the |entries_pending_doom_|
  // after a mass doom.
  void DoomEntriesComplete(std::unique_ptr<std::vector<uint64_t>> entry_hashes,
                           CompletionOnceCallback callback,
                           int result);

  // Calculates and returns a new entry's worker pool priority.
  uint32_t GetNewEntryPriority(net::RequestPriority request_priority);

  scoped_refptr<BackendFileOperationsFactory> file_operations_factory_;

  // We want this destroyed after every other field.
  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;

  const raw_ptr<SimpleFileTracker> file_tracker_;

  const base::FilePath path_;
  std::unique_ptr<SimpleIndex> index_;

  // This is used for all the entry I/O.
  scoped_refptr<net::PrioritizedTaskRunner> prioritized_task_runner_;

  int64_t orig_max_size_;
  const SimpleEntryImpl::OperationsMode entry_operations_mode_;

  EntryMap active_entries_;

  // The set of all entries which are currently being doomed. To avoid races,
  // these entries cannot have Doom/Create/Open operations run until the doom
  // is complete. They store a vector of base::OnceClosure's of deferred
  // operations to be run at the completion of the Doom.
  scoped_refptr<SimplePostOperationWaiterTable> post_doom_waiting_;

  // Set of entries which are being opened by hash. We don't have their key,
  // so can't check for collisions on Doom/Open/Create ops until that is
  // complete.  This stores a vector of base::OnceClosure's of deferred
  // operations to be run at the completion of the Doom.
  scoped_refptr<SimplePostOperationWaiterTable> post_open_by_hash_waiting_;

  const raw_ptr<net::NetLog> net_log_;

  uint32_t entry_count_ = 0;

#if BUILDFLAG(IS_ANDROID)
  ApplicationStatusListenerGetter app_status_listener_getter_;
#endif

  base::WeakPtrFactory<SimpleBackendImpl> weak_ptr_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_IMPL_H_
