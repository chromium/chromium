// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "net/base/cache_type.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/post_doom_waiter.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_entry_operation.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

namespace base {
class TaskRunner;
}

namespace net {
class GrowableIOBuffer;
class IOBuffer;
class NetLog;
class PrioritizedTaskRunner;
}

namespace disk_cache {

class BackendCleanupTracker;
class SimpleBackendImpl;
class SimpleEntryStat;
class SimpleFileTracker;
class SimpleSynchronousEntry;
struct SimpleEntryCreationResults;

// SimpleEntryImpl is the source task_runner interface to an entry in the very
// simple disk cache. It proxies for the SimpleSynchronousEntry, which performs
// IO on the worker thread.
class NET_EXPORT_PRIVATE SimpleEntryImpl : public Entry,
    public base::RefCounted<SimpleEntryImpl> {
  friend class base::RefCounted<SimpleEntryImpl>;
 public:
  enum OperationsMode {
    NON_OPTIMISTIC_OPERATIONS,
    OPTIMISTIC_OPERATIONS,
  };

  // The Backend provides an |ActiveEntryProxy| instance to this entry when it
  // is active, meaning it's the canonical entry for this |entry_hash_|. The
  // entry can make itself inactive by deleting its proxy.
  class ActiveEntryProxy {
   public:
    virtual ~ActiveEntryProxy() = 0;
  };

  SimpleEntryImpl(net::CacheType cache_type,
                  const base::FilePath& path,
                  scoped_refptr<BackendCleanupTracker> cleanup_tracker,
                  uint64_t entry_hash,
                  OperationsMode operations_mode,
                  SimpleBackendImpl* backend,
                  SimpleFileTracker* file_tracker,
                  net::NetLog* net_log,
                  uint32_t entry_priority);

  void SetActiveEntryProxy(
      std::unique_ptr<ActiveEntryProxy> active_entry_proxy);

  // Adds another reader/writer to this entry, if possible.
  EntryResult OpenEntry(EntryResultCallback callback);

  // Creates this entry, if possible.
  EntryResult CreateEntry(EntryResultCallback callback);

  // Opens an existing entry or creates a new one.
  EntryResult OpenOrCreateEntry(EntryResultCallback callback);

  // Identical to Backend::Doom() except that it accepts a
  // CompletionOnceCallback.
  net::Error DoomEntry(CompletionOnceCallback callback);

  const std::string& key() const { return key_; }
  uint64_t entry_hash() const { return entry_hash_; }

  // The key is not a constructor parameter to the SimpleEntryImpl, because
  // during cache iteration, it's necessary to open entries by their hash
  // alone. In that case, the SimpleSynchronousEntry will read the key from disk
  // and it will be set.
  void SetKey(const std::string& key);

  // SetCreatePendingDoom() should be called before CreateEntry() if the
  // creation should suceed optimistically but not do any I/O until
  // NotifyDoomBeforeCreateComplete() is called.
  void SetCreatePendingDoom();
  void NotifyDoomBeforeCreateComplete();

  // From Entry:
  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  // GetLastUsed() should not be called in net::APP_CACHE mode since the times
  // are not updated.
  base::Time GetLastUsed() const override;
  base::Time GetLastModified() const override;
  int32_t GetDataSize(int index) const override;
  int ReadData(int stream_index,
               int offset,
               net::IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override;
  int WriteData(int stream_index,
                int offset,
                net::IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override;
  int ReadSparseData(int64_t offset,
                     net::IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override;
  int WriteSparseData(int64_t offset,
                      net::IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override;
  int GetAvailableRange(int64_t offset,
                        int len,
                        int64_t* start,
                        CompletionOnceCallback callback) override;
  bool CouldBeSparse() const override;
  void CancelSparseIO() override;
  net::Error ReadyForSparseIO(CompletionOnceCallback callback) override;
  void SetLastUsedTimeForTest(base::Time time) override;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Changes the entry's priority in its TaskRunner.
  void SetPriority(uint32_t entry_priority);

 private:
  class ScopedOperationRunner;
  friend class ScopedOperationRunner;

  enum State {
    // The state immediately after construction, but before |synchronous_entry_|
    // has been assigned. This is the state at construction, and is one of the
    // two states (along with failure) one can destruct an entry in.
    STATE_UNINITIALIZED,

    // This entry is available for regular IO.
    STATE_READY,

    // IO is currently in flight, operations must wait for completion before
    // launching.
    STATE_IO_PENDING,

    // A failure occurred in the current or previous operation. All operations
    // after that must fail, until we receive a Close().
    STATE_FAILURE,
  };

  enum DoomState {
    // No attempt to doom the entry has been made.
    DOOM_NONE,

    // We have moved ourselves to |entries_pending_doom_| and have queued an
    // operation to actually update the disk, but haven't completed it yet.
    DOOM_QUEUED,

    // The disk has been updated. This corresponds to the state where we
    // are in neither |entries_pending_doom_| nor |active_entries_|.
    DOOM_COMPLETED,
  };

  // Used in histograms, please only add entries at the end.
  enum CheckCrcResult {
    CRC_CHECK_NEVER_READ_TO_END = 0,
    CRC_CHECK_NOT_DONE = 1,
    CRC_CHECK_DONE = 2,
    CRC_CHECK_NEVER_READ_AT_ALL = 3,
    CRC_CHECK_MAX = 4,
  };

  ~SimpleEntryImpl() override;

  // Must be used to invoke a client-provided completion callback for an
  // operation initiated through the backend (e.g. create, open, doom) so that
  // clients don't get notified after they deleted the backend (which they would
  // not expect).
  void PostClientCallback(CompletionOnceCallback callback, int result);
  void PostClientCallback(EntryResultCallback callback, EntryResult result);

  // Clears entry state enough to prepare it for re-use. This will generally
  // put it back into STATE_UNINITIALIZED, except if the entry is doomed and
  // therefore disconnected from ownership of corresponding filename, in which
  // case it will be put into STATE_FAILURE.
  void ResetEntry();

  // Adjust ownership before return of this entry to a user of the API.
  // Increments the user count.
  void ReturnEntryToCaller();

  // Like above, but for asynchronous return after the event loop runs again,
  // also invoking the callback per the usual net convention.
  // The return is cancelled if the backend is deleted in the interim.
  void ReturnEntryToCallerAsync(bool is_open, EntryResultCallback callback);

  // Portion of the above that runs off the event loop.
  void FinishReturnEntryToCallerAsync(bool is_open,
                                      EntryResultCallback callback);

  // Remove |this| from the Backend and the index, either because
  // SimpleSynchronousEntry has detected an error or because we are about to
  // be dooming it ourselves and want it to be tracked in
  // |entries_pending_doom_| instead.
  void MarkAsDoomed(DoomState doom_state);

  // Runs the next operation in the queue, if any and if there is no other
  // operation running at the moment.
  // WARNING: May delete |this|, as an operation in the queue can contain
  // the last reference.
  void RunNextOperationIfNeeded();

  void OpenEntryInternal(SimpleEntryOperation::EntryResultState result_state,
                         EntryResultCallback callback);

  void CreateEntryInternal(SimpleEntryOperation::EntryResultState result_state,
                           EntryResultCallback callback);

  void OpenOrCreateEntryInternal(
      OpenEntryIndexEnum index_state,
      SimpleEntryOperation::EntryResultState result_state,
      EntryResultCallback callback);

  void CloseInternal();

  int ReadDataInternal(bool sync_possible,
                       int index,
                       int offset,
                       net::IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback);

  void WriteDataInternal(int index,
                         int offset,
                         net::IOBuffer* buf,
                         int buf_len,
                         CompletionOnceCallback callback,
                         bool truncate);

  void ReadSparseDataInternal(int64_t sparse_offset,
                              net::IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback);

  void WriteSparseDataInternal(int64_t sparse_offset,
                               net::IOBuffer* buf,
                               int buf_len,
                               CompletionOnceCallback callback);

  void GetAvailableRangeInternal(int64_t sparse_offset,
                                 int len,
                                 int64_t* out_start,
                                 CompletionOnceCallback callback);

  void DoomEntryInternal(CompletionOnceCallback callback);

  // Called after a SimpleSynchronousEntry has completed CreateEntry() or
  // OpenEntry(). If |in_results| is used to denote whether that was successful,
  // Posts either the produced entry or an error code to |completion_callback|.
  void CreationOperationComplete(
      SimpleEntryOperation::EntryResultState result_state,
      EntryResultCallback completion_callback,
      const base::TimeTicks& start_time,
      const base::Time index_last_used_time,
      std::unique_ptr<SimpleEntryCreationResults> in_results,
      net::NetLogEventType end_event_type);

  // Called after we've closed and written the EOF record to our entry. Until
  // this point it hasn't been safe to OpenEntry() the same entry, but from this
  // point it is.
  void CloseOperationComplete(
      std::unique_ptr<SimpleEntryCloseResults> in_results);

  // Internal utility method used by other completion methods. Calls
  // |completion_callback| after updating state and dooming on errors.
  void EntryOperationComplete(CompletionOnceCallback completion_callback,
                              const SimpleEntryStat& entry_stat,
                              int result);

  // Called after an asynchronous read. Updates |crc32s_| if possible.
  void ReadOperationComplete(
      int stream_index,
      int offset,
      CompletionOnceCallback completion_callback,
      std::unique_ptr<SimpleEntryStat> entry_stat,
      std::unique_ptr<SimpleSynchronousEntry::ReadResult> read_result);

  // Called after an asynchronous write completes.
  // |buf| parameter brings back a reference to net::IOBuffer to the original
  // sequence, so that we can reduce cross thread malloc/free pair.
  // See http://crbug.com/708644 for details.
  void WriteOperationComplete(
      int stream_index,
      CompletionOnceCallback completion_callback,
      std::unique_ptr<SimpleEntryStat> entry_stat,
      std::unique_ptr<SimpleSynchronousEntry::WriteResult> result,
      net::IOBuffer* buf);

  void ReadSparseOperationComplete(CompletionOnceCallback completion_callback,
                                   std::unique_ptr<base::Time> last_used,
                                   std::unique_ptr<int> result);

  void WriteSparseOperationComplete(CompletionOnceCallback completion_callback,
                                    std::unique_ptr<SimpleEntryStat> entry_stat,
                                    std::unique_ptr<int> result);

  void GetAvailableRangeOperationComplete(
      CompletionOnceCallback completion_callback,
      std::unique_ptr<int> result);

  // Called after an asynchronous doom completes.
  void DoomOperationComplete(CompletionOnceCallback callback,
                             State state_to_restore,
                             int result);

  void RecordReadResultConsideringChecksum(
      const std::unique_ptr<SimpleSynchronousEntry::ReadResult>& read_result)
      const;

  // Called after completion of an operation, to either incoproprate file info
  // received from I/O done on the worker pool, or to simply bump the
  // timestamps. Updates the metadata both in |this| and in the index.
  // Stream size information in particular may be important for following
  // operations.
  void UpdateDataFromEntryStat(const SimpleEntryStat& entry_stat);

  int64_t GetDiskUsage() const;

  // Completes a read from the stream data kept in memory, logging metrics
  // and updating metadata. Returns the # of bytes read successfully.
  // This asumes the caller has already range-checked offset and buf_len
  // appropriately.
  int ReadFromBuffer(net::GrowableIOBuffer* in_buf,
                     int offset,
                     int buf_len,
                     net::IOBuffer* out_buf);

  // Copies data from |buf| to the internal in-memory buffer for stream 0. If
  // |truncate| is set to true, the target buffer will be truncated at |offset|
  // + |buf_len| before being written.
  int SetStream0Data(net::IOBuffer* buf,
                     int offset, int buf_len,
                     bool truncate);

  // We want all async I/O on entries to complete before recycling the dir.
  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;

  std::unique_ptr<ActiveEntryProxy> active_entry_proxy_;

  // All nonstatic SimpleEntryImpl methods should always be called on the
  // source creation sequence, in all cases. |sequence_checker_| documents and
  // enforces this.
  SEQUENCE_CHECKER(sequence_checker_);

  const base::WeakPtr<SimpleBackendImpl> backend_;
  SimpleFileTracker* const file_tracker_;
  const net::CacheType cache_type_;
  const base::FilePath path_;
  const uint64_t entry_hash_;
  const bool use_optimistic_operations_;
  bool is_initial_stream1_read_ = true;  // used for metrics only.
  std::string key_;

  // |last_used_|, |last_modified_| and |data_size_| are copied from the
  // synchronous entry at the completion of each item of asynchronous IO.
  // TODO(clamy): Unify last_used_ with data in the index.
  base::Time last_used_;
  base::Time last_modified_;
  int32_t data_size_[kSimpleEntryStreamCount];
  int32_t sparse_data_size_ = 0;

  // Number of times this object has been returned from Backend::OpenEntry() and
  // Backend::CreateEntry() without subsequent Entry::Close() calls. Used to
  // notify the backend when this entry not used by any callers.
  int open_count_ = 0;

  DoomState doom_state_ = DOOM_NONE;

  enum {
    CREATE_NORMAL,
    CREATE_OPTIMISTIC_PENDING_DOOM,
    CREATE_OPTIMISTIC_PENDING_DOOM_FOLLOWED_BY_DOOM,
  } optimistic_create_pending_doom_state_ = CREATE_NORMAL;

  State state_ = STATE_UNINITIALIZED;

  // When possible, we compute a crc32, for the data in each entry as we read or
  // write. For each stream, |crc32s_[index]| is the crc32 of that stream from
  // [0 .. |crc32s_end_offset_|). If |crc32s_end_offset_[index] == 0| then the
  // value of |crc32s_[index]| is undefined.
  // Note at this can only be done in the current implementation in the case of
  // a single entry reader that reads serially through the entire file.
  // Extending this to multiple readers is possible, but isn't currently worth
  // it; see http://crbug.com/488076#c3 for details.
  int32_t crc32s_end_offset_[kSimpleEntryStreamCount];
  uint32_t crc32s_[kSimpleEntryStreamCount];

  // If |have_written_[index]| is true, we have written to the file that
  // contains stream |index|.
  bool have_written_[kSimpleEntryStreamCount];

  // Reflects how much CRC checking has been done with the entry. This state is
  // reported on closing each entry stream.
  CheckCrcResult crc_check_state_[kSimpleEntryStreamCount];

  // The |synchronous_entry_| is the worker thread object that performs IO on
  // entries. It's owned by this SimpleEntryImpl whenever |executing_operation_|
  // is false (i.e. when an operation is not pending on the worker pool). When
  // an operation is being executed no one owns the synchronous entry. Therefore
  // SimpleEntryImpl should not be deleted while an operation is running as that
  // would leak the SimpleSynchronousEntry.
  SimpleSynchronousEntry* synchronous_entry_ = nullptr;

  scoped_refptr<net::PrioritizedTaskRunner> prioritized_task_runner_;

  base::queue<SimpleEntryOperation> pending_operations_;

  net::NetLogWithSource net_log_;

  // Unlike other streams, stream 0 data is read from the disk when the entry is
  // opened, and then kept in memory. All read/write operations on stream 0
  // affect the |stream_0_data_| buffer. When the entry is closed,
  // |stream_0_data_| is written to the disk.
  // Stream 0 is kept in memory because it is stored in the same file as stream
  // 1 on disk, to reduce the number of file descriptors and save disk space.
  // This strategy allows stream 1 to change size easily. Since stream 0 is only
  // used to write HTTP headers, the memory consumption of keeping it in memory
  // is acceptable.
  scoped_refptr<net::GrowableIOBuffer> stream_0_data_;

  // Sometimes stream 1 data is prefetched when stream 0 is first read.
  // If a write to the stream occurs on the entry the prefetch buffer is
  // discarded. It may also be null if it wasn't prefetched in the first place.
  scoped_refptr<net::GrowableIOBuffer> stream_1_prefetch_data_;

  // This is used only while a doom is pending.
  scoped_refptr<SimplePostDoomWaiterTable> post_doom_waiting_;

  // Choosing uint32_t over uint64_t for space savings. Pages have in the
  // hundres to possibly thousands of resources. Wrapping every 4 billion
  // shouldn't cause inverted priorities very often.
  uint32_t entry_priority_ = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_
