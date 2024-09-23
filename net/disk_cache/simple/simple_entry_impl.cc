// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/simple/simple_entry_impl.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/prioritized_task_runner.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/net_log_parameters.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_net_log_parameters.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source_type.h"
#include "third_party/zlib/zlib.h"

namespace disk_cache {
namespace {

// An entry can store sparse data taking up to 1 / kMaxSparseDataSizeDivisor of
// the cache.
const int64_t kMaxSparseDataSizeDivisor = 10;

OpenEntryIndexEnum ComputeIndexState(SimpleBackendImpl* backend,
                                     uint64_t entry_hash) {
  if (!backend->index()->initialized())
    return INDEX_NOEXIST;
  if (backend->index()->Has(entry_hash))
    return INDEX_HIT;
  return INDEX_MISS;
}

void RecordOpenEntryIndexState(net::CacheType cache_type,
                               OpenEntryIndexEnum state) {
  SIMPLE_CACHE_UMA(ENUMERATION, "OpenEntryIndexState", cache_type, state,
                   INDEX_MAX);
}

void RecordHeaderSize(net::CacheType cache_type, int size) {
  SIMPLE_CACHE_UMA(COUNTS_10000, "HeaderSize", cache_type, size);
}

void InvokeCallbackIfBackendIsAlive(
    const base::WeakPtr<SimpleBackendImpl>& backend,
    net::CompletionOnceCallback completion_callback,
    int result) {
  DCHECK(!completion_callback.is_null());
  if (!backend.get())
    return;
  std::move(completion_callback).Run(result);
}

void InvokeEntryResultCallbackIfBackendIsAlive(
    const base::WeakPtr<SimpleBackendImpl>& backend,
    EntryResultCallback completion_callback,
    EntryResult result) {
  DCHECK(!completion_callback.is_null());
  if (!backend.get())
    return;
  std::move(completion_callback).Run(std::move(result));
}

// If |sync_possible| is false, and callback is available, posts rv to it and
// return net::ERR_IO_PENDING; otherwise just passes through rv.
int PostToCallbackIfNeeded(bool sync_possible,
                           net::CompletionOnceCallback callback,
                           int rv) {
  if (!sync_possible && !callback.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), rv));
    return net::ERR_IO_PENDING;
  } else {
    return rv;
  }
}

}  // namespace

using base::OnceClosure;
using base::FilePath;
using base::Time;
using base::TaskRunner;

// A helper class to insure that RunNextOperationIfNeeded() is called when
// exiting the current stack frame.
class SimpleEntryImpl::ScopedOperationRunner {
 public:
  explicit ScopedOperationRunner(SimpleEntryImpl* entry) : entry_(entry) {
  }

  ~ScopedOperationRunner() {
    entry_->RunNextOperationIfNeeded();
  }

 private:
  const raw_ptr<SimpleEntryImpl> entry_;
};

SimpleEntryImpl::ActiveEntryProxy::~ActiveEntryProxy() = default;

SimpleEntryImpl::SimpleEntryImpl(
    net::CacheType cache_type,
    const FilePath& path,
    scoped_refptr<BackendCleanupTracker> cleanup_tracker,
    const uint64_t entry_hash,
    OperationsMode operations_mode,
    SimpleBackendImpl* backend,
    SimpleFileTracker* file_tracker,
    scoped_refptr<BackendFileOperationsFactory> file_operations_factory,
    net::NetLog* net_log,
    uint32_t entry_priority)
    : cleanup_tracker_(std::move(cleanup_tracker)),
      backend_(backend->AsWeakPtr()),
      file_tracker_(file_tracker),
      file_operations_factory_(std::move(file_operations_factory)),
      cache_type_(cache_type),
      path_(path),
      entry_hash_(entry_hash),
      use_optimistic_operations_(operations_mode == OPTIMISTIC_OPERATIONS),
      last_used_(Time::Now()),
      last_modified_(last_used_),
      prioritized_task_runner_(backend_->prioritized_task_runner()),
      net_log_(
          net::NetLogWithSource::Make(net_log,
                                      net::NetLogSourceType::DISK_CACHE_ENTRY)),
      stream_0_data_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      entry_priority_(entry_priority) {
  static_assert(std::extent<decltype(data_size_)>() ==
                    std::extent<decltype(crc32s_end_offset_)>(),
                "arrays should be the same size");
  static_assert(
      std::extent<decltype(data_size_)>() == std::extent<decltype(crc32s_)>(),
      "arrays should be the same size");
  static_assert(std::extent<decltype(data_size_)>() ==
                    std::extent<decltype(have_written_)>(),
                "arrays should be the same size");
  ResetEntry();
  NetLogSimpleEntryConstruction(net_log_,
                                net::NetLogEventType::SIMPLE_CACHE_ENTRY,
                                net::NetLogEventPhase::BEGIN, this);
}

void SimpleEntryImpl::SetActiveEntryProxy(
    std::unique_ptr<ActiveEntryProxy> active_entry_proxy) {
  DCHECK(!active_entry_proxy_);
  active_entry_proxy_ = std::move(active_entry_proxy);
}

EntryResult SimpleEntryImpl::OpenEntry(EntryResultCallback callback) {
  DCHECK(backend_.get());

  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_CALL);

  OpenEntryIndexEnum index_state =
      ComputeIndexState(backend_.get(), entry_hash_);
  RecordOpenEntryIndexState(cache_type_, index_state);

  // If entry is not known to the index, initiate fast failover to the network.
  if (index_state == INDEX_MISS) {
    net_log_.AddEventWithNetErrorCode(
        net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_END, net::ERR_FAILED);
    return EntryResult::MakeError(net::ERR_FAILED);
  }

  pending_operations_.push(SimpleEntryOperation::OpenOperation(
      this, SimpleEntryOperation::ENTRY_NEEDS_CALLBACK, std::move(callback)));
  RunNextOperationIfNeeded();
  return EntryResult::MakeError(net::ERR_IO_PENDING);
}

EntryResult SimpleEntryImpl::CreateEntry(EntryResultCallback callback) {
  DCHECK(backend_.get());
  DCHECK_EQ(entry_hash_, simple_util::GetEntryHashKey(*key_));

  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_CREATE_CALL);

  EntryResult result = EntryResult::MakeError(net::ERR_IO_PENDING);
  if (use_optimistic_operations_ &&
      state_ == STATE_UNINITIALIZED && pending_operations_.size() == 0) {
    net_log_.AddEvent(
        net::NetLogEventType::SIMPLE_CACHE_ENTRY_CREATE_OPTIMISTIC);

    ReturnEntryToCaller();
    result = EntryResult::MakeCreated(this);
    pending_operations_.push(SimpleEntryOperation::CreateOperation(
        this, SimpleEntryOperation::ENTRY_ALREADY_RETURNED,
        EntryResultCallback()));

    // If we are optimistically returning before a preceeding doom, we need to
    // wait for that IO, about which we will be notified externally.
    if (optimistic_create_pending_doom_state_ != CREATE_NORMAL) {
      CHECK_EQ(CREATE_OPTIMISTIC_PENDING_DOOM,
               optimistic_create_pending_doom_state_);
      state_ = STATE_IO_PENDING;
    }
  } else {
    pending_operations_.push(SimpleEntryOperation::CreateOperation(
        this, SimpleEntryOperation::ENTRY_NEEDS_CALLBACK, std::move(callback)));
  }

  // We insert the entry in the index before creating the entry files in the
  // SimpleSynchronousEntry, because this way the worst scenario is when we
  // have the entry in the index but we don't have the created files yet, this
  // way we never leak files. CreationOperationComplete will remove the entry
  // from the index if the creation fails.
  backend_->index()->Insert(entry_hash_);

  RunNextOperationIfNeeded();
  return result;
}

EntryResult SimpleEntryImpl::OpenOrCreateEntry(EntryResultCallback callback) {
  DCHECK(backend_.get());
  DCHECK_EQ(entry_hash_, simple_util::GetEntryHashKey(*key_));

  net_log_.AddEvent(
      net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_CALL);

  OpenEntryIndexEnum index_state =
      ComputeIndexState(backend_.get(), entry_hash_);
  RecordOpenEntryIndexState(cache_type_, index_state);

  EntryResult result = EntryResult::MakeError(net::ERR_IO_PENDING);
  if (index_state == INDEX_MISS && use_optimistic_operations_ &&
      state_ == STATE_UNINITIALIZED && pending_operations_.size() == 0) {
    net_log_.AddEvent(
        net::NetLogEventType::SIMPLE_CACHE_ENTRY_CREATE_OPTIMISTIC);

    ReturnEntryToCaller();
    result = EntryResult::MakeCreated(this);
    pending_operations_.push(SimpleEntryOperation::OpenOrCreateOperation(
        this, index_state, SimpleEntryOperation::ENTRY_ALREADY_RETURNED,
        EntryResultCallback()));

    // The post-doom stuff should go through CreateEntry, not here.
    CHECK_EQ(CREATE_NORMAL, optimistic_create_pending_doom_state_);
  } else {
    pending_operations_.push(SimpleEntryOperation::OpenOrCreateOperation(
        this, index_state, SimpleEntryOperation::ENTRY_NEEDS_CALLBACK,
        std::move(callback)));
  }

  // We insert the entry in the index before creating the entry files in the
  // SimpleSynchronousEntry, because this way the worst scenario is when we
  // have the entry in the index but we don't have the created files yet, this
  // way we never leak files. CreationOperationComplete will remove the entry
  // from the index if the creation fails.
  backend_->index()->Insert(entry_hash_);

  RunNextOperationIfNeeded();
  return result;
}

net::Error SimpleEntryImpl::DoomEntry(net::CompletionOnceCallback callback) {
  if (doom_state_ != DOOM_NONE)
    return net::OK;
  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_DOOM_CALL);
  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_DOOM_BEGIN);

  MarkAsDoomed(DOOM_QUEUED);
  if (backend_.get()) {
    if (optimistic_create_pending_doom_state_ == CREATE_NORMAL) {
      post_doom_waiting_ = backend_->OnDoomStart(entry_hash_);
    } else {
      CHECK_EQ(STATE_IO_PENDING, state_);
      CHECK_EQ(CREATE_OPTIMISTIC_PENDING_DOOM,
               optimistic_create_pending_doom_state_);
      // If we are in this state, we went ahead with making the entry even
      // though the backend was already keeping track of a doom, so it can't
      // keep track of ours. So we delay notifying it until
      // NotifyDoomBeforeCreateComplete is called.  Since this path is invoked
      // only when the queue of post-doom callbacks was previously empty, while
      // the CompletionOnceCallback for the op is posted,
      // NotifyDoomBeforeCreateComplete() will be the first thing running after
      // the previous doom completes, so at that point we can immediately grab
      // a spot in entries_pending_doom_.
      optimistic_create_pending_doom_state_ =
          CREATE_OPTIMISTIC_PENDING_DOOM_FOLLOWED_BY_DOOM;
    }
  }
  pending_operations_.push(
      SimpleEntryOperation::DoomOperation(this, std::move(callback)));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

void SimpleEntryImpl::SetCreatePendingDoom() {
  CHECK_EQ(CREATE_NORMAL, optimistic_create_pending_doom_state_);
  optimistic_create_pending_doom_state_ = CREATE_OPTIMISTIC_PENDING_DOOM;
}

void SimpleEntryImpl::NotifyDoomBeforeCreateComplete() {
  CHECK_EQ(STATE_IO_PENDING, state_);
  CHECK_NE(CREATE_NORMAL, optimistic_create_pending_doom_state_);
  if (backend_.get() && optimistic_create_pending_doom_state_ ==
                            CREATE_OPTIMISTIC_PENDING_DOOM_FOLLOWED_BY_DOOM)
    post_doom_waiting_ = backend_->OnDoomStart(entry_hash_);

  state_ = STATE_UNINITIALIZED;
  optimistic_create_pending_doom_state_ = CREATE_NORMAL;
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::SetKey(const std::string& key) {
  key_ = key;
  net_log_.AddEventWithStringParams(
      net::NetLogEventType::SIMPLE_CACHE_ENTRY_SET_KEY, "key", key);
}

void SimpleEntryImpl::Doom() {
  DoomEntry(CompletionOnceCallback());
}

void SimpleEntryImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_LT(0, open_count_);

  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_CLOSE_CALL);

  if (--open_count_ > 0) {
    DCHECK(!HasOneRef());
    Release();  // Balanced in ReturnEntryToCaller().
    return;
  }

  pending_operations_.push(SimpleEntryOperation::CloseOperation(this));
  DCHECK(!HasOneRef());
  Release();  // Balanced in ReturnEntryToCaller().
  RunNextOperationIfNeeded();
}

std::string SimpleEntryImpl::GetKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *key_;
}

Time SimpleEntryImpl::GetLastUsed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cache_type_ != net::APP_CACHE);
  return last_used_;
}

Time SimpleEntryImpl::GetLastModified() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_modified_;
}

int32_t SimpleEntryImpl::GetDataSize(int stream_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(0, data_size_[stream_index]);
  return data_size_[stream_index];
}

int SimpleEntryImpl::ReadData(int stream_index,
                              int offset,
                              net::IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_CALL,
        net::NetLogEventPhase::NONE, stream_index, offset, buf_len, false);
  }

  if (stream_index < 0 || stream_index >= kSimpleEntryStreamCount ||
      buf_len < 0) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_END,
          net::NetLogEventPhase::NONE, net::ERR_INVALID_ARGUMENT);
    }

    return net::ERR_INVALID_ARGUMENT;
  }

  // If this is the only operation, bypass the queue, and also see if there is
  // in-memory data to handle it synchronously. In principle, multiple reads can
  // be parallelized, but past studies have shown that parallelizable ones
  // happen <1% of the time, so it's probably not worth the effort.
  bool alone_in_queue =
      pending_operations_.size() == 0 && state_ == STATE_READY;

  if (alone_in_queue) {
    return ReadDataInternal(/*sync_possible = */ true, stream_index, offset,
                            buf, buf_len, std::move(callback));
  }

  pending_operations_.push(SimpleEntryOperation::ReadOperation(
      this, stream_index, offset, buf_len, buf, std::move(callback)));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::WriteData(int stream_index,
                               int offset,
                               net::IOBuffer* buf,
                               int buf_len,
                               CompletionOnceCallback callback,
                               bool truncate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_CALL,
        net::NetLogEventPhase::NONE, stream_index, offset, buf_len, truncate);
  }

  if (stream_index < 0 || stream_index >= kSimpleEntryStreamCount ||
      offset < 0 || buf_len < 0) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_END,
          net::NetLogEventPhase::NONE, net::ERR_INVALID_ARGUMENT);
    }
    return net::ERR_INVALID_ARGUMENT;
  }
  int end_offset;
  if (!base::CheckAdd(offset, buf_len).AssignIfValid(&end_offset) ||
      (backend_.get() && end_offset > backend_->MaxFileSize())) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_END,
          net::NetLogEventPhase::NONE, net::ERR_FAILED);
    }
    return net::ERR_FAILED;
  }
  ScopedOperationRunner operation_runner(this);

  // Stream 0 data is kept in memory, so can be written immediatly if there are
  // no IO operations pending.
  if (stream_index == 0 && state_ == STATE_READY &&
      pending_operations_.size() == 0) {
    state_ = STATE_IO_PENDING;
    SetStream0Data(buf, offset, buf_len, truncate);
    state_ = STATE_READY;
    return buf_len;
  }

  // We can only do optimistic Write if there is no pending operations, so
  // that we are sure that the next call to RunNextOperationIfNeeded will
  // actually run the write operation that sets the stream size. It also
  // prevents from previous possibly-conflicting writes that could be stacked
  // in the |pending_operations_|. We could optimize this for when we have
  // only read operations enqueued, but past studies have shown that that such
  // parallelizable cases are very rare.
  const bool optimistic =
      (use_optimistic_operations_ && state_ == STATE_READY &&
       pending_operations_.size() == 0);
  CompletionOnceCallback op_callback;
  scoped_refptr<net::IOBuffer> op_buf;
  int ret_value = net::ERR_FAILED;
  if (!optimistic) {
    op_buf = buf;
    op_callback = std::move(callback);
    ret_value = net::ERR_IO_PENDING;
  } else {
    // TODO(morlovich,pasko): For performance, don't use a copy of an IOBuffer
    // here to avoid paying the price of the RefCountedThreadSafe atomic
    // operations.
    if (buf) {
      op_buf = base::MakeRefCounted<net::IOBufferWithSize>(buf_len);
      std::copy(buf->data(), buf->data() + buf_len, op_buf->data());
    }
    op_callback = CompletionOnceCallback();
    ret_value = buf_len;
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_OPTIMISTIC,
          net::NetLogEventPhase::NONE, buf_len);
    }
  }

  pending_operations_.push(SimpleEntryOperation::WriteOperation(
      this, stream_index, offset, buf_len, op_buf.get(), truncate, optimistic,
      std::move(op_callback)));
  return ret_value;
}

int SimpleEntryImpl::ReadSparseData(int64_t offset,
                                    net::IOBuffer* buf,
                                    int buf_len,
                                    CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_SPARSE_CALL,
        net::NetLogEventPhase::NONE, offset, buf_len);
  }

  if (offset < 0 || buf_len < 0) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_SPARSE_END,
          net::NetLogEventPhase::NONE, net::ERR_INVALID_ARGUMENT);
    }
    return net::ERR_INVALID_ARGUMENT;
  }

  // Truncate |buf_len| to make sure that |offset + buf_len| does not overflow.
  // This is OK since one can't write that far anyway.
  // The result of std::min is guaranteed to fit into int since |buf_len| did.
  buf_len = std::min(static_cast<int64_t>(buf_len),
                     std::numeric_limits<int64_t>::max() - offset);

  ScopedOperationRunner operation_runner(this);
  pending_operations_.push(SimpleEntryOperation::ReadSparseOperation(
      this, offset, buf_len, buf, std::move(callback)));
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::WriteSparseData(int64_t offset,
                                     net::IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_SPARSE_CALL,
        net::NetLogEventPhase::NONE, offset, buf_len);
  }

  if (offset < 0 || buf_len < 0 || !base::CheckAdd(offset, buf_len).IsValid()) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_SPARSE_END,
          net::NetLogEventPhase::NONE, net::ERR_INVALID_ARGUMENT);
    }
    return net::ERR_INVALID_ARGUMENT;
  }

  ScopedOperationRunner operation_runner(this);
  pending_operations_.push(SimpleEntryOperation::WriteSparseOperation(
      this, offset, buf_len, buf, std::move(callback)));
  return net::ERR_IO_PENDING;
}

RangeResult SimpleEntryImpl::GetAvailableRange(int64_t offset,
                                               int len,
                                               RangeResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (offset < 0 || len < 0)
    return RangeResult(net::ERR_INVALID_ARGUMENT);

  // Truncate |len| to make sure that |offset + len| does not overflow.
  // This is OK since one can't write that far anyway.
  // The result of std::min is guaranteed to fit into int since |len| did.
  len = std::min(static_cast<int64_t>(len),
                 std::numeric_limits<int64_t>::max() - offset);

  ScopedOperationRunner operation_runner(this);
  pending_operations_.push(SimpleEntryOperation::GetAvailableRangeOperation(
      this, offset, len, std::move(callback)));
  return RangeResult(net::ERR_IO_PENDING);
}

bool SimpleEntryImpl::CouldBeSparse() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(morlovich): Actually check.
  return true;
}

void SimpleEntryImpl::CancelSparseIO() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The Simple Cache does not return distinct objects for the same non-doomed
  // entry, so there's no need to coordinate which object is performing sparse
  // I/O.  Therefore, CancelSparseIO and ReadyForSparseIO succeed instantly.
}

net::Error SimpleEntryImpl::ReadyForSparseIO(CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The simple Cache does not return distinct objects for the same non-doomed
  // entry, so there's no need to coordinate which object is performing sparse
  // I/O.  Therefore, CancelSparseIO and ReadyForSparseIO succeed instantly.
  return net::OK;
}

void SimpleEntryImpl::SetLastUsedTimeForTest(base::Time time) {
  last_used_ = time;
  backend_->index()->SetLastUsedTimeForTest(entry_hash_, time);
}

void SimpleEntryImpl::SetPriority(uint32_t entry_priority) {
  entry_priority_ = entry_priority;
}

SimpleEntryImpl::~SimpleEntryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0U, pending_operations_.size());

  // This used to DCHECK on `state_`, but it turns out that destruction
  // happening on thread shutdown, when closures holding `this` get deleted
  // can happen in circumstances not possible during normal use, such as when
  // I/O for Close operation is keeping the entry alive in STATE_IO_PENDING, or
  // an entry that's STATE_READY has callbacks pending to hand it over to the
  // user right as the thread is shutdown (this would also have a non-null
  // `synchronous_entry_`).
  net_log_.EndEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY);
}

void SimpleEntryImpl::PostClientCallback(net::CompletionOnceCallback callback,
                                         int result) {
  if (callback.is_null())
    return;
  // Note that the callback is posted rather than directly invoked to avoid
  // reentrancy issues.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InvokeCallbackIfBackendIsAlive, backend_,
                                std::move(callback), result));
}

void SimpleEntryImpl::PostClientCallback(EntryResultCallback callback,
                                         EntryResult result) {
  if (callback.is_null())
    return;
  // Note that the callback is posted rather than directly invoked to avoid
  // reentrancy issues.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InvokeEntryResultCallbackIfBackendIsAlive, backend_,
                     std::move(callback), std::move(result)));
}

void SimpleEntryImpl::ResetEntry() {
  // If we're doomed, we can't really do anything else with the entry, since
  // we no longer own the name and are disconnected from the active entry table.
  // We preserve doom_state_ accross this entry for this same reason.
  state_ = doom_state_ == DOOM_COMPLETED ? STATE_FAILURE : STATE_UNINITIALIZED;
  std::memset(crc32s_end_offset_, 0, sizeof(crc32s_end_offset_));
  std::memset(crc32s_, 0, sizeof(crc32s_));
  std::memset(have_written_, 0, sizeof(have_written_));
  std::memset(data_size_, 0, sizeof(data_size_));
}

void SimpleEntryImpl::ReturnEntryToCaller() {
  DCHECK(backend_);
  ++open_count_;
  AddRef();  // Balanced in Close()
}

void SimpleEntryImpl::ReturnEntryToCallerAsync(bool is_open,
                                               EntryResultCallback callback) {
  DCHECK(!callback.is_null());

  // |open_count_| must be incremented immediately, so that a Close on an alias
  // doesn't try to wrap things up.
  ++open_count_;

  // Note that the callback is posted rather than directly invoked to avoid
  // reentrancy issues.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleEntryImpl::FinishReturnEntryToCallerAsync, this,
                     is_open, std::move(callback)));
}

void SimpleEntryImpl::FinishReturnEntryToCallerAsync(
    bool is_open,
    EntryResultCallback callback) {
  AddRef();  // Balanced in Close()
  if (!backend_.get()) {
    // With backend dead, Open/Create operations are responsible for cleaning up
    // the entry --- the ownership is never transferred to the caller, and their
    // callback isn't invoked.
    Close();
    return;
  }

  std::move(callback).Run(is_open ? EntryResult::MakeOpened(this)
                                  : EntryResult::MakeCreated(this));
}

void SimpleEntryImpl::MarkAsDoomed(DoomState new_state) {
  DCHECK_NE(DOOM_NONE, new_state);
  doom_state_ = new_state;
  if (!backend_.get())
    return;
  backend_->index()->Remove(entry_hash_);
  active_entry_proxy_.reset();
}

void SimpleEntryImpl::RunNextOperationIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_operations_.empty() && state_ != STATE_IO_PENDING) {
    SimpleEntryOperation operation = std::move(pending_operations_.front());
    pending_operations_.pop();
    switch (operation.type()) {
      case SimpleEntryOperation::TYPE_OPEN:
        OpenEntryInternal(operation.entry_result_state(),
                          operation.ReleaseEntryResultCallback());
        break;
      case SimpleEntryOperation::TYPE_CREATE:
        CreateEntryInternal(operation.entry_result_state(),
                            operation.ReleaseEntryResultCallback());
        break;
      case SimpleEntryOperation::TYPE_OPEN_OR_CREATE:
        OpenOrCreateEntryInternal(operation.index_state(),
                                  operation.entry_result_state(),
                                  operation.ReleaseEntryResultCallback());
        break;
      case SimpleEntryOperation::TYPE_CLOSE:
        CloseInternal();
        break;
      case SimpleEntryOperation::TYPE_READ:
        ReadDataInternal(/* sync_possible= */ false, operation.index(),
                         operation.offset(), operation.buf(),
                         operation.length(), operation.ReleaseCallback());
        break;
      case SimpleEntryOperation::TYPE_WRITE:
        WriteDataInternal(operation.index(), operation.offset(),
                          operation.buf(), operation.length(),
                          operation.ReleaseCallback(), operation.truncate());
        break;
      case SimpleEntryOperation::TYPE_READ_SPARSE:
        ReadSparseDataInternal(operation.sparse_offset(), operation.buf(),
                               operation.length(), operation.ReleaseCallback());
        break;
      case SimpleEntryOperation::TYPE_WRITE_SPARSE:
        WriteSparseDataInternal(operation.sparse_offset(), operation.buf(),
                                operation.length(),
                                operation.ReleaseCallback());
        break;
      case SimpleEntryOperation::TYPE_GET_AVAILABLE_RANGE:
        GetAvailableRangeInternal(operation.sparse_offset(), operation.length(),
                                  operation.ReleaseRangeResultCalback());
        break;
      case SimpleEntryOperation::TYPE_DOOM:
        DoomEntryInternal(operation.ReleaseCallback());
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    // |this| may have been deleted.
  }
}

void SimpleEntryImpl::OpenEntryInternal(
    SimpleEntryOperation::EntryResultState result_state,
    EntryResultCallback callback) {
  ScopedOperationRunner operation_runner(this);

  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_BEGIN);

  // No optimistic sync return possible on open.
  DCHECK_EQ(SimpleEntryOperation::ENTRY_NEEDS_CALLBACK, result_state);

  if (state_ == STATE_READY) {
    ReturnEntryToCallerAsync(/* is_open = */ true, std::move(callback));
    NetLogSimpleEntryCreation(net_log_,
                              net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_END,
                              net::NetLogEventPhase::NONE, this, net::OK);
    return;
  }
  if (state_ == STATE_FAILURE) {
    PostClientCallback(std::move(callback),
                       EntryResult::MakeError(net::ERR_FAILED));
    NetLogSimpleEntryCreation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_END,
        net::NetLogEventPhase::NONE, this, net::ERR_FAILED);
    return;
  }

  DCHECK_EQ(STATE_UNINITIALIZED, state_);
  DCHECK(!synchronous_entry_);
  state_ = STATE_IO_PENDING;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  auto results = std::make_unique<SimpleEntryCreationResults>(SimpleEntryStat(
      last_used_, last_modified_, data_size_, sparse_data_size_));

  int32_t trailer_prefetch_size = -1;
  base::Time last_used_time;
  if (SimpleBackendImpl* backend = backend_.get()) {
    if (cache_type_ == net::APP_CACHE) {
      trailer_prefetch_size =
          backend->index()->GetTrailerPrefetchSize(entry_hash_);
    } else {
      last_used_time = backend->index()->GetLastUsedTime(entry_hash_);
    }
  }

  base::OnceClosure task = base::BindOnce(
      &SimpleSynchronousEntry::OpenEntry, cache_type_, path_, key_, entry_hash_,
      file_tracker_, file_operations_factory_->CreateUnbound(),
      trailer_prefetch_size, results.get());

  base::OnceClosure reply = base::BindOnce(
      &SimpleEntryImpl::CreationOperationComplete, this, result_state,
      std::move(callback), start_time, last_used_time, std::move(results),
      net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_END);

  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::CreateEntryInternal(
    SimpleEntryOperation::EntryResultState result_state,
    EntryResultCallback callback) {
  ScopedOperationRunner operation_runner(this);

  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_CREATE_BEGIN);

  if (state_ != STATE_UNINITIALIZED) {
    // There is already an active normal entry.
    NetLogSimpleEntryCreation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_CREATE_END,
        net::NetLogEventPhase::NONE, this, net::ERR_FAILED);
    // If we have optimistically returned an entry, we would be the first entry
    // in queue with state_ == STATE_UNINITIALIZED.
    DCHECK_EQ(SimpleEntryOperation::ENTRY_NEEDS_CALLBACK, result_state);
    PostClientCallback(std::move(callback),
                       EntryResult::MakeError(net::ERR_FAILED));
    return;
  }
  DCHECK_EQ(STATE_UNINITIALIZED, state_);
  DCHECK(!synchronous_entry_);

  state_ = STATE_IO_PENDING;

  // Since we don't know the correct values for |last_used_| and
  // |last_modified_| yet, we make this approximation.
  last_used_ = last_modified_ = base::Time::Now();

  const base::TimeTicks start_time = base::TimeTicks::Now();
  auto results = std::make_unique<SimpleEntryCreationResults>(SimpleEntryStat(
      last_used_, last_modified_, data_size_, sparse_data_size_));

  OnceClosure task =
      base::BindOnce(&SimpleSynchronousEntry::CreateEntry, cache_type_, path_,
                     *key_, entry_hash_, file_tracker_,
                     file_operations_factory_->CreateUnbound(), results.get());
  OnceClosure reply = base::BindOnce(
      &SimpleEntryImpl::CreationOperationComplete, this, result_state,
      std::move(callback), start_time, base::Time(), std::move(results),
      net::NetLogEventType::SIMPLE_CACHE_ENTRY_CREATE_END);
  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::OpenOrCreateEntryInternal(
    OpenEntryIndexEnum index_state,
    SimpleEntryOperation::EntryResultState result_state,
    EntryResultCallback callback) {
  ScopedOperationRunner operation_runner(this);

  net_log_.AddEvent(
      net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_BEGIN);

  // result_state may be ENTRY_ALREADY_RETURNED only if an optimistic create is
  // being performed, which must be in STATE_UNINITIALIZED.
  bool optimistic_create =
      (result_state == SimpleEntryOperation::ENTRY_ALREADY_RETURNED);
  DCHECK(!optimistic_create || state_ == STATE_UNINITIALIZED);

  if (state_ == STATE_READY) {
    ReturnEntryToCallerAsync(/* is_open = */ true, std::move(callback));
    NetLogSimpleEntryCreation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_END,
        net::NetLogEventPhase::NONE, this, net::OK);
    return;
  }
  if (state_ == STATE_FAILURE) {
    PostClientCallback(std::move(callback),
                       EntryResult::MakeError(net::ERR_FAILED));
    NetLogSimpleEntryCreation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_END,
        net::NetLogEventPhase::NONE, this, net::ERR_FAILED);
    return;
  }

  DCHECK_EQ(STATE_UNINITIALIZED, state_);
  DCHECK(!synchronous_entry_);
  state_ = STATE_IO_PENDING;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  auto results = std::make_unique<SimpleEntryCreationResults>(SimpleEntryStat(
      last_used_, last_modified_, data_size_, sparse_data_size_));

  int32_t trailer_prefetch_size = -1;
  base::Time last_used_time;
  if (SimpleBackendImpl* backend = backend_.get()) {
    if (cache_type_ == net::APP_CACHE) {
      trailer_prefetch_size =
          backend->index()->GetTrailerPrefetchSize(entry_hash_);
    } else {
      last_used_time = backend->index()->GetLastUsedTime(entry_hash_);
    }
  }

  base::OnceClosure task =
      base::BindOnce(&SimpleSynchronousEntry::OpenOrCreateEntry, cache_type_,
                     path_, *key_, entry_hash_, index_state, optimistic_create,
                     file_tracker_, file_operations_factory_->CreateUnbound(),
                     trailer_prefetch_size, results.get());

  base::OnceClosure reply = base::BindOnce(
      &SimpleEntryImpl::CreationOperationComplete, this, result_state,
      std::move(callback), start_time, last_used_time, std::move(results),
      net::NetLogEventType::SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_END);

  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::CloseInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (open_count_ != 0) {
    // Entry got resurrected in between Close and CloseInternal, nothing to do
    // for now.
    return;
  }

  typedef SimpleSynchronousEntry::CRCRecord CRCRecord;
  auto crc32s_to_write = std::make_unique<std::vector<CRCRecord>>();

  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_CLOSE_BEGIN);

  if (state_ == STATE_READY) {
    DCHECK(synchronous_entry_);
    state_ = STATE_IO_PENDING;
    for (int i = 0; i < kSimpleEntryStreamCount; ++i) {
      if (have_written_[i]) {
        if (GetDataSize(i) == crc32s_end_offset_[i]) {
          int32_t crc = GetDataSize(i) == 0 ? crc32(0, Z_NULL, 0) : crc32s_[i];
          crc32s_to_write->push_back(CRCRecord(i, true, crc));
        } else {
          crc32s_to_write->push_back(CRCRecord(i, false, 0));
        }
      }
    }
  } else {
    DCHECK(STATE_UNINITIALIZED == state_ || STATE_FAILURE == state_);
  }

  auto results = std::make_unique<SimpleEntryCloseResults>();
  if (synchronous_entry_) {
    OnceClosure task = base::BindOnce(
        &SimpleSynchronousEntry::Close, base::Unretained(synchronous_entry_),
        SimpleEntryStat(last_used_, last_modified_, data_size_,
                        sparse_data_size_),
        std::move(crc32s_to_write), base::RetainedRef(stream_0_data_),
        results.get());
    OnceClosure reply = base::BindOnce(&SimpleEntryImpl::CloseOperationComplete,
                                       this, std::move(results));
    synchronous_entry_ = nullptr;
    prioritized_task_runner_->PostTaskAndReply(
        FROM_HERE, std::move(task), std::move(reply), entry_priority_);
  } else {
    CloseOperationComplete(std::move(results));
  }
}

int SimpleEntryImpl::ReadDataInternal(bool sync_possible,
                                      int stream_index,
                                      int offset,
                                      net::IOBuffer* buf,
                                      int buf_len,
                                      net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedOperationRunner operation_runner(this);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_BEGIN,
        net::NetLogEventPhase::NONE, stream_index, offset, buf_len, false);
  }

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(net_log_,
                              net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_END,
                              net::NetLogEventPhase::NONE, net::ERR_FAILED);
    }
    // Note that the API states that client-provided callbacks for entry-level
    // (i.e. non-backend) operations (e.g. read, write) are invoked even if
    // the backend was already destroyed.
    return PostToCallbackIfNeeded(sync_possible, std::move(callback),
                                  net::ERR_FAILED);
  }
  DCHECK_EQ(STATE_READY, state_);
  if (offset >= GetDataSize(stream_index) || offset < 0 || !buf_len) {
    // If there is nothing to read, we bail out before setting state_ to
    // STATE_IO_PENDING (so ScopedOperationRunner might start us on next op
    // here).
    return PostToCallbackIfNeeded(sync_possible, std::move(callback), 0);
  }

  // Truncate read to not go past end of stream.
  buf_len = std::min(buf_len, GetDataSize(stream_index) - offset);

  // Since stream 0 data is kept in memory, it is read immediately.
  if (stream_index == 0) {
    state_ = STATE_IO_PENDING;
    ReadFromBuffer(stream_0_data_.get(), offset, buf_len, buf);
    state_ = STATE_READY;
    return PostToCallbackIfNeeded(sync_possible, std::move(callback), buf_len);
  }

  // Sometimes we can read in-ram prefetched stream 1 data immediately, too.
  if (stream_index == 1) {
    if (stream_1_prefetch_data_) {
      state_ = STATE_IO_PENDING;
      ReadFromBuffer(stream_1_prefetch_data_.get(), offset, buf_len, buf);
      state_ = STATE_READY;
      return PostToCallbackIfNeeded(sync_possible, std::move(callback),
                                    buf_len);
    }
  }

  state_ = STATE_IO_PENDING;
  if (doom_state_ == DOOM_NONE && backend_.get())
    backend_->index()->UseIfExists(entry_hash_);

  SimpleSynchronousEntry::ReadRequest read_req(stream_index, offset, buf_len);
  // Figure out if we should be computing the checksum for this read,
  // and whether we should be verifying it, too.
  if (crc32s_end_offset_[stream_index] == offset) {
    read_req.request_update_crc = true;
    read_req.previous_crc32 =
        offset == 0 ? crc32(0, Z_NULL, 0) : crc32s_[stream_index];

    // We can't verify the checksum if we already overwrote part of the file.
    // (It may still make sense to compute it if the overwritten area and the
    //  about-to-read-in area are adjoint).
    read_req.request_verify_crc = !have_written_[stream_index];
  }

  auto result = std::make_unique<SimpleSynchronousEntry::ReadResult>();
  auto entry_stat = std::make_unique<SimpleEntryStat>(
      last_used_, last_modified_, data_size_, sparse_data_size_);
  OnceClosure task = base::BindOnce(
      &SimpleSynchronousEntry::ReadData, base::Unretained(synchronous_entry_),
      read_req, entry_stat.get(), base::RetainedRef(buf), result.get());
  OnceClosure reply = base::BindOnce(
      &SimpleEntryImpl::ReadOperationComplete, this, stream_index, offset,
      std::move(callback), std::move(entry_stat), std::move(result));
  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
  return net::ERR_IO_PENDING;
}

void SimpleEntryImpl::WriteDataInternal(int stream_index,
                                        int offset,
                                        net::IOBuffer* buf,
                                        int buf_len,
                                        net::CompletionOnceCallback callback,
                                        bool truncate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedOperationRunner operation_runner(this);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_BEGIN,
        net::NetLogEventPhase::NONE, stream_index, offset, buf_len, truncate);
  }

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_END,
          net::NetLogEventPhase::NONE, net::ERR_FAILED);
    }
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_FAILED));
    }
    // |this| may be destroyed after return here.
    return;
  }

  DCHECK_EQ(STATE_READY, state_);

  // Since stream 0 data is kept in memory, it will be written immediatly.
  if (stream_index == 0) {
    state_ = STATE_IO_PENDING;
    SetStream0Data(buf, offset, buf_len, truncate);
    state_ = STATE_READY;
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), buf_len));
    }
    return;
  }

  // Ignore zero-length writes that do not change the file size.
  if (buf_len == 0) {
    int32_t data_size = data_size_[stream_index];
    if (truncate ? (offset == data_size) : (offset <= data_size)) {
      if (!callback.is_null()) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), 0));
      }
      return;
    }
  }
  state_ = STATE_IO_PENDING;
  if (doom_state_ == DOOM_NONE && backend_.get())
    backend_->index()->UseIfExists(entry_hash_);

  // Any stream 1 write invalidates the prefetched data.
  if (stream_index == 1)
    stream_1_prefetch_data_ = nullptr;

  bool request_update_crc = false;
  uint32_t initial_crc = 0;

  if (offset < crc32s_end_offset_[stream_index]) {
    // If a range for which the crc32 was already computed is rewritten, the
    // computation of the crc32 need to start from 0 again.
    crc32s_end_offset_[stream_index] = 0;
  }

  if (crc32s_end_offset_[stream_index] == offset) {
    request_update_crc = true;
    initial_crc = (offset != 0) ? crc32s_[stream_index] : crc32(0, Z_NULL, 0);
  }

  // |entry_stat| needs to be initialized before modifying |data_size_|.
  auto entry_stat = std::make_unique<SimpleEntryStat>(
      last_used_, last_modified_, data_size_, sparse_data_size_);
  if (truncate) {
    data_size_[stream_index] = offset + buf_len;
  } else {
    data_size_[stream_index] = std::max(offset + buf_len,
                                        GetDataSize(stream_index));
  }

  auto write_result = std::make_unique<SimpleSynchronousEntry::WriteResult>();

  // Since we don't know the correct values for |last_used_| and
  // |last_modified_| yet, we make this approximation.
  last_used_ = last_modified_ = base::Time::Now();

  have_written_[stream_index] = true;
  // Writing on stream 1 affects the placement of stream 0 in the file, the EOF
  // record will have to be rewritten.
  if (stream_index == 1)
    have_written_[0] = true;

  // Retain a reference to |buf| in |reply| instead of |task|, so that we can
  // reduce cross thread malloc/free pairs. The cross thread malloc/free pair
  // increases the apparent memory usage due to the thread cached free list.
  // TODO(morlovich): Remove the doom_state_ argument to WriteData, since with
  // renaming rather than delete, creating a new stream 2 of doomed entry will
  // just work.
  OnceClosure task = base::BindOnce(
      &SimpleSynchronousEntry::WriteData, base::Unretained(synchronous_entry_),
      SimpleSynchronousEntry::WriteRequest(
          stream_index, offset, buf_len, initial_crc, truncate,
          doom_state_ != DOOM_NONE, request_update_crc),
      base::Unretained(buf), entry_stat.get(), write_result.get());
  OnceClosure reply =
      base::BindOnce(&SimpleEntryImpl::WriteOperationComplete, this,
                     stream_index, std::move(callback), std::move(entry_stat),
                     std::move(write_result), base::RetainedRef(buf));
  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::ReadSparseDataInternal(
    int64_t sparse_offset,
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedOperationRunner operation_runner(this);

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_SPARSE_BEGIN,
        net::NetLogEventPhase::NONE, sparse_offset, buf_len);
  }

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_SPARSE_END,
          net::NetLogEventPhase::NONE, net::ERR_FAILED);
    }
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_FAILED));
    }
    // |this| may be destroyed after return here.
    return;
  }

  DCHECK_EQ(STATE_READY, state_);
  state_ = STATE_IO_PENDING;

  auto result = std::make_unique<int>();
  auto last_used = std::make_unique<base::Time>();
  OnceClosure task = base::BindOnce(
      &SimpleSynchronousEntry::ReadSparseData,
      base::Unretained(synchronous_entry_),
      SimpleSynchronousEntry::SparseRequest(sparse_offset, buf_len),
      base::RetainedRef(buf), last_used.get(), result.get());
  OnceClosure reply = base::BindOnce(
      &SimpleEntryImpl::ReadSparseOperationComplete, this, std::move(callback),
      std::move(last_used), std::move(result));
  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::WriteSparseDataInternal(
    int64_t sparse_offset,
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedOperationRunner operation_runner(this);

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_SPARSE_BEGIN,
        net::NetLogEventPhase::NONE, sparse_offset, buf_len);
  }

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(
          net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_SPARSE_END,
          net::NetLogEventPhase::NONE, net::ERR_FAILED);
    }
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_FAILED));
    }
    // |this| may be destroyed after return here.
    return;
  }

  DCHECK_EQ(STATE_READY, state_);
  state_ = STATE_IO_PENDING;

  uint64_t max_sparse_data_size = std::numeric_limits<int64_t>::max();
  if (backend_.get()) {
    uint64_t max_cache_size = backend_->index()->max_size();
    max_sparse_data_size = max_cache_size / kMaxSparseDataSizeDivisor;
  }

  auto entry_stat = std::make_unique<SimpleEntryStat>(
      last_used_, last_modified_, data_size_, sparse_data_size_);

  last_used_ = last_modified_ = base::Time::Now();

  auto result = std::make_unique<int>();
  OnceClosure task = base::BindOnce(
      &SimpleSynchronousEntry::WriteSparseData,
      base::Unretained(synchronous_entry_),
      SimpleSynchronousEntry::SparseRequest(sparse_offset, buf_len),
      base::RetainedRef(buf), max_sparse_data_size, entry_stat.get(),
      result.get());
  OnceClosure reply = base::BindOnce(
      &SimpleEntryImpl::WriteSparseOperationComplete, this, std::move(callback),
      std::move(entry_stat), std::move(result));
  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::GetAvailableRangeInternal(int64_t sparse_offset,
                                                int len,
                                                RangeResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedOperationRunner operation_runner(this);

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), RangeResult(net::ERR_FAILED)));
    }
    // |this| may be destroyed after return here.
    return;
  }

  DCHECK_EQ(STATE_READY, state_);
  state_ = STATE_IO_PENDING;

  auto result = std::make_unique<RangeResult>();
  OnceClosure task = base::BindOnce(
      &SimpleSynchronousEntry::GetAvailableRange,
      base::Unretained(synchronous_entry_),
      SimpleSynchronousEntry::SparseRequest(sparse_offset, len), result.get());
  OnceClosure reply =
      base::BindOnce(&SimpleEntryImpl::GetAvailableRangeOperationComplete, this,
                     std::move(callback), std::move(result));
  prioritized_task_runner_->PostTaskAndReply(FROM_HERE, std::move(task),
                                             std::move(reply), entry_priority_);
}

void SimpleEntryImpl::DoomEntryInternal(net::CompletionOnceCallback callback) {
  if (doom_state_ == DOOM_COMPLETED) {
    // During the time we were sitting on a queue, some operation failed
    // and cleaned our files up, so we don't have to do anything.
    DoomOperationComplete(std::move(callback), state_, net::OK);
    return;
  }

  if (!backend_) {
    // If there's no backend, we want to truncate the files rather than delete
    // or rename them. Either op will update the entry directory's mtime, which
    // will likely force a full index rebuild on the next startup; this is
    // clearly an undesirable cost. Instead, the lesser evil is to set the entry
    // files to length zero, leaving the invalid entry in the index. On the next
    // attempt to open the entry, it will fail asynchronously (since the magic
    // numbers will not be found), and the files will actually be removed.
    // Since there is no backend, new entries to conflict with us also can't be
    // created.
    prioritized_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&SimpleSynchronousEntry::TruncateEntryFiles, path_,
                       entry_hash_, file_operations_factory_->CreateUnbound()),
        base::BindOnce(&SimpleEntryImpl::DoomOperationComplete, this,
                       std::move(callback),
                       // Return to STATE_FAILURE after dooming, since no
                       // operation can succeed on the truncated entry files.
                       STATE_FAILURE),
        entry_priority_);
    state_ = STATE_IO_PENDING;
    return;
  }

  if (synchronous_entry_) {
    // If there is a backing object, we have to go through its instance methods,
    // so that it can rename itself and keep track of the altenative name.
    prioritized_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&SimpleSynchronousEntry::Doom,
                       base::Unretained(synchronous_entry_)),
        base::BindOnce(&SimpleEntryImpl::DoomOperationComplete, this,
                       std::move(callback), state_),
        entry_priority_);
  } else {
    DCHECK_EQ(STATE_UNINITIALIZED, state_);
    // If nothing is open, we can just delete the files. We know they have the
    // base names, since if we ever renamed them our doom_state_ would be
    // DOOM_COMPLETED, and we would exit at function entry.
    prioritized_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&SimpleSynchronousEntry::DeleteEntryFiles, path_,
                       cache_type_, entry_hash_,
                       file_operations_factory_->CreateUnbound()),
        base::BindOnce(&SimpleEntryImpl::DoomOperationComplete, this,
                       std::move(callback), state_),
        entry_priority_);
  }
  state_ = STATE_IO_PENDING;
}

void SimpleEntryImpl::CreationOperationComplete(
    SimpleEntryOperation::EntryResultState result_state,
    EntryResultCallback completion_callback,
    const base::TimeTicks& start_time,
    const base::Time index_last_used_time,
    std::unique_ptr<SimpleEntryCreationResults> in_results,
    net::NetLogEventType end_event_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, STATE_IO_PENDING);
  DCHECK(in_results);
  ScopedOperationRunner operation_runner(this);
  if (in_results->result != net::OK) {
    if (in_results->result != net::ERR_FILE_EXISTS) {
      // Here we keep index up-to-date, but don't remove ourselves from active
      // entries since we may have queued operations, and it would be
      // problematic to run further Creates, Opens, or Dooms if we are not
      // the active entry.  We can only do this because OpenEntryInternal
      // and CreateEntryInternal have to start from STATE_UNINITIALIZED, so
      // nothing else is going on which may be confused.
      if (backend_)
        backend_->index()->Remove(entry_hash_);
    }

    net_log_.AddEventWithNetErrorCode(end_event_type, net::ERR_FAILED);
    PostClientCallback(std::move(completion_callback),
                       EntryResult::MakeError(net::ERR_FAILED));
    ResetEntry();
    return;
  }

  // If this is a successful creation (rather than open), mark all streams to be
  // saved on close.
  if (in_results->created) {
    for (bool& have_written : have_written_)
      have_written = true;
  }

  // Make sure to keep the index up-to-date. We likely already did this when
  // CreateEntry was called, but it's possible we were sitting on a queue
  // after an op that removed us.
  if (backend_ && doom_state_ == DOOM_NONE)
    backend_->index()->Insert(entry_hash_);

  synchronous_entry_ = in_results->sync_entry;

  // Copy over any pre-fetched data and its CRCs.
  for (int stream = 0; stream < 2; ++stream) {
    const SimpleStreamPrefetchData& prefetched =
        in_results->stream_prefetch_data[stream];
    if (prefetched.data.get()) {
      if (stream == 0)
        stream_0_data_ = prefetched.data;
      else
        stream_1_prefetch_data_ = prefetched.data;

      // The crc was read in SimpleSynchronousEntry.
      crc32s_[stream] = prefetched.stream_crc32;
      crc32s_end_offset_[stream] = in_results->entry_stat.data_size(stream);
    }
  }

  // If this entry was opened by hash, key_ could still be empty. If so, update
  // it with the key read from the synchronous entry.
  if (!key_.has_value()) {
    SetKey(*synchronous_entry_->key());
  } else {
    // This should only be triggered when creating an entry. In the open case
    // the key is either copied from the arguments to open, or checked
    // in the synchronous entry.
    DCHECK_EQ(*key_, *synchronous_entry_->key());
  }

  // Prefer index last used time to disk's, since that may be pretty inaccurate.
  if (!index_last_used_time.is_null())
    in_results->entry_stat.set_last_used(index_last_used_time);

  UpdateDataFromEntryStat(in_results->entry_stat);
  if (cache_type_ == net::APP_CACHE && backend_.get() && backend_->index()) {
    backend_->index()->SetTrailerPrefetchSize(
        entry_hash_, in_results->computed_trailer_prefetch_size);
  }
  SIMPLE_CACHE_UMA(TIMES,
                   "EntryCreationTime", cache_type_,
                   (base::TimeTicks::Now() - start_time));

  net_log_.AddEvent(end_event_type);

  const bool created = in_results->created;

  // We need to release `in_results` before going out of scope, because
  // `operation_runner` destruction might call a close operation, that will
  // ultimately release `in_results->sync_entry`, and thus leading to having a
  // dangling pointer here.
  in_results = nullptr;

  state_ = STATE_READY;
  if (result_state == SimpleEntryOperation::ENTRY_NEEDS_CALLBACK) {
    ReturnEntryToCallerAsync(!created, std::move(completion_callback));
  }
}

void SimpleEntryImpl::UpdateStateAfterOperationComplete(
    const SimpleEntryStat& entry_stat,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_IO_PENDING, state_);
  if (result < 0) {
    state_ = STATE_FAILURE;
    MarkAsDoomed(DOOM_COMPLETED);
  } else {
    UpdateDataFromEntryStat(entry_stat);
    state_ = STATE_READY;
  }
}

void SimpleEntryImpl::EntryOperationComplete(
    net::CompletionOnceCallback completion_callback,
    const SimpleEntryStat& entry_stat,
    int result) {
  UpdateStateAfterOperationComplete(entry_stat, result);
  if (!completion_callback.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback), result));
  }
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::ReadOperationComplete(
    int stream_index,
    int offset,
    net::CompletionOnceCallback completion_callback,
    std::unique_ptr<SimpleEntryStat> entry_stat,
    std::unique_ptr<SimpleSynchronousEntry::ReadResult> read_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_IO_PENDING, state_);
  DCHECK(read_result);
  int result = read_result->result;

  if (read_result->crc_updated) {
    if (result > 0) {
      DCHECK_EQ(crc32s_end_offset_[stream_index], offset);
      crc32s_end_offset_[stream_index] += result;
      crc32s_[stream_index] = read_result->updated_crc32;
    }
  }

  if (result < 0) {
    crc32s_end_offset_[stream_index] = 0;
  }

  if (net_log_.IsCapturing()) {
    NetLogReadWriteComplete(net_log_,
                            net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_END,
                            net::NetLogEventPhase::NONE, result);
  }

  EntryOperationComplete(std::move(completion_callback), *entry_stat, result);
}

void SimpleEntryImpl::WriteOperationComplete(
    int stream_index,
    net::CompletionOnceCallback completion_callback,
    std::unique_ptr<SimpleEntryStat> entry_stat,
    std::unique_ptr<SimpleSynchronousEntry::WriteResult> write_result,
    net::IOBuffer* buf) {
  int result = write_result->result;
  if (net_log_.IsCapturing()) {
    NetLogReadWriteComplete(net_log_,
                            net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_END,
                            net::NetLogEventPhase::NONE, result);
  }

  if (result < 0)
    crc32s_end_offset_[stream_index] = 0;

  if (result > 0 && write_result->crc_updated) {
    crc32s_end_offset_[stream_index] += result;
    crc32s_[stream_index] = write_result->updated_crc32;
  }

  EntryOperationComplete(std::move(completion_callback), *entry_stat, result);
}

void SimpleEntryImpl::ReadSparseOperationComplete(
    net::CompletionOnceCallback completion_callback,
    std::unique_ptr<base::Time> last_used,
    std::unique_ptr<int> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(synchronous_entry_);
  DCHECK(result);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteComplete(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_READ_SPARSE_END,
        net::NetLogEventPhase::NONE, *result);
  }

  SimpleEntryStat entry_stat(*last_used, last_modified_, data_size_,
                             sparse_data_size_);
  EntryOperationComplete(std::move(completion_callback), entry_stat, *result);
}

void SimpleEntryImpl::WriteSparseOperationComplete(
    net::CompletionOnceCallback completion_callback,
    std::unique_ptr<SimpleEntryStat> entry_stat,
    std::unique_ptr<int> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(synchronous_entry_);
  DCHECK(result);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteComplete(
        net_log_, net::NetLogEventType::SIMPLE_CACHE_ENTRY_WRITE_SPARSE_END,
        net::NetLogEventPhase::NONE, *result);
  }

  EntryOperationComplete(std::move(completion_callback), *entry_stat, *result);
}

void SimpleEntryImpl::GetAvailableRangeOperationComplete(
    RangeResultCallback completion_callback,
    std::unique_ptr<RangeResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(synchronous_entry_);
  DCHECK(result);

  SimpleEntryStat entry_stat(last_used_, last_modified_, data_size_,
                             sparse_data_size_);
  UpdateStateAfterOperationComplete(entry_stat, result->net_error);
  if (!completion_callback.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback), *result));
  }
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::DoomOperationComplete(
    net::CompletionOnceCallback callback,
    State state_to_restore,
    int result) {
  state_ = state_to_restore;
  doom_state_ = DOOM_COMPLETED;
  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_DOOM_END);
  PostClientCallback(std::move(callback), result);
  RunNextOperationIfNeeded();
  if (post_doom_waiting_) {
    post_doom_waiting_->OnOperationComplete(entry_hash_);
    post_doom_waiting_ = nullptr;
  }
}

void SimpleEntryImpl::CloseOperationComplete(
    std::unique_ptr<SimpleEntryCloseResults> in_results) {
  DCHECK(!synchronous_entry_);
  DCHECK_EQ(0, open_count_);
  DCHECK(STATE_IO_PENDING == state_ || STATE_FAILURE == state_ ||
         STATE_UNINITIALIZED == state_);
  net_log_.AddEvent(net::NetLogEventType::SIMPLE_CACHE_ENTRY_CLOSE_END);
  if (cache_type_ == net::APP_CACHE &&
      in_results->estimated_trailer_prefetch_size > 0 && backend_.get() &&
      backend_->index()) {
    backend_->index()->SetTrailerPrefetchSize(
        entry_hash_, in_results->estimated_trailer_prefetch_size);
  }
  ResetEntry();
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::UpdateDataFromEntryStat(
    const SimpleEntryStat& entry_stat) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(synchronous_entry_);
  // We want to only be called in STATE_IO_PENDING so that if call to
  // SimpleIndex::UpdateEntrySize() ends up triggering eviction and queuing
  // Dooms it doesn't also run any queued operations.
  CHECK_EQ(state_, STATE_IO_PENDING);

  last_used_ = entry_stat.last_used();
  last_modified_ = entry_stat.last_modified();
  for (int i = 0; i < kSimpleEntryStreamCount; ++i) {
    data_size_[i] = entry_stat.data_size(i);
  }
  sparse_data_size_ = entry_stat.sparse_data_size();

  SimpleBackendImpl* backend_ptr = backend_.get();
  if (doom_state_ == DOOM_NONE && backend_ptr) {
    backend_ptr->index()->UpdateEntrySize(
        entry_hash_, base::checked_cast<uint32_t>(GetDiskUsage()));
  }
}

int64_t SimpleEntryImpl::GetDiskUsage() const {
  int64_t file_size = 0;
  for (int data_size : data_size_) {
    file_size += simple_util::GetFileSizeFromDataSize(key_->size(), data_size);
  }
  file_size += sparse_data_size_;
  return file_size;
}

void SimpleEntryImpl::ReadFromBuffer(net::GrowableIOBuffer* in_buf,
                                     int offset,
                                     int buf_len,
                                     net::IOBuffer* out_buf) {
  DCHECK_GE(buf_len, 0);

  std::copy(in_buf->data() + offset, in_buf->data() + offset + buf_len,
            out_buf->data());
  UpdateDataFromEntryStat(SimpleEntryStat(base::Time::Now(), last_modified_,
                                          data_size_, sparse_data_size_));
}

void SimpleEntryImpl::SetStream0Data(net::IOBuffer* buf,
                                     int offset,
                                     int buf_len,
                                     bool truncate) {
  // Currently, stream 0 is only used for HTTP headers, and always writes them
  // with a single, truncating write. Detect these writes and record the size
  // changes of the headers. Also, support writes to stream 0 that have
  // different access patterns, as required by the API contract.
  // All other clients of the Simple Cache are encouraged to use stream 1.
  have_written_[0] = true;
  int data_size = GetDataSize(0);
  if (offset == 0 && truncate) {
    stream_0_data_->SetCapacity(buf_len);
    std::copy(buf->data(), buf->data() + buf_len, stream_0_data_->data());
    data_size_[0] = buf_len;
  } else {
    const int buffer_size =
        truncate ? offset + buf_len : std::max(offset + buf_len, data_size);
    stream_0_data_->SetCapacity(buffer_size);
    // If |stream_0_data_| was extended, the extension until offset needs to be
    // zero-filled.
    const int fill_size = offset <= data_size ? 0 : offset - data_size;
    if (fill_size > 0) {
      std::fill(stream_0_data_->data() + data_size,
                stream_0_data_->data() + data_size + fill_size, 0);
    }
    if (buf) {
      std::copy(buf->data(), buf->data() + buf_len,
                stream_0_data_->data() + offset);
    }
    data_size_[0] = buffer_size;
  }
  RecordHeaderSize(cache_type_, data_size_[0]);
  base::Time modification_time = base::Time::Now();

  // Reset checksum; SimpleSynchronousEntry::Close will compute it for us,
  // and do it off the source creation sequence.
  crc32s_end_offset_[0] = 0;

  UpdateDataFromEntryStat(
      SimpleEntryStat(modification_time, modification_time, data_size_,
                      sparse_data_size_));
}

}  // namespace disk_cache
