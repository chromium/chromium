// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache_writers.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache_transaction.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/partial_data.h"

namespace net {

namespace {

bool IsValidResponseForWriter(bool is_partial,
                              const HttpResponseInfo* response_info) {
  if (!response_info->headers.get()) {
    return false;
  }

  // Return false if the response code sent by the server is garbled.
  // Both 200 and 304 are valid since concurrent writing is supported.
  if (!is_partial &&
      (response_info->headers->response_code() != HTTP_OK &&
       response_info->headers->response_code() != HTTP_NOT_MODIFIED)) {
    return false;
  }

  return true;
}

}  // namespace

HttpCache::Writers::TransactionInfo::TransactionInfo(PartialData* partial_data,
                                                     const bool is_truncated,
                                                     HttpResponseInfo info)
    : partial(partial_data), truncated(is_truncated), response_info(info) {}

HttpCache::Writers::TransactionInfo::~TransactionInfo() = default;

HttpCache::Writers::TransactionInfo::TransactionInfo(const TransactionInfo&) =
    default;

HttpCache::Writers::Writers(HttpCache* cache,
                            scoped_refptr<HttpCache::ActiveEntry> entry)
    : cache_(cache), entry_(entry) {
  DCHECK(cache_);
  DCHECK(entry_);
}

HttpCache::Writers::~Writers() = default;

int HttpCache::Writers::Read(scoped_refptr<IOBuffer> buf,
                             int buf_len,
                             CompletionOnceCallback callback,
                             Transaction* transaction) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(!callback.is_null());
  DCHECK(transaction);

  // If another transaction invoked a Read which is currently ongoing, then
  // this transaction waits for the read to complete and gets its buffer filled
  // with the data returned from that read.
  if (next_state_ != State::NONE) {
    WaitingForRead read_info(buf, buf_len, std::move(callback));
    waiting_for_read_.emplace(transaction, std::move(read_info));
    return ERR_IO_PENDING;
  }

  DCHECK_EQ(next_state_, State::NONE);
  DCHECK(callback_.is_null());
  DCHECK_EQ(nullptr, active_transaction_);
  DCHECK(HasTransaction(transaction));
  active_transaction_ = transaction;

  read_buf_ = std::move(buf);
  io_buf_len_ = buf_len;
  next_state_ = State::NETWORK_READ;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv;
}

bool HttpCache::Writers::StopCaching(bool keep_entry) {
  // If this is the only transaction in Writers, then stopping will be
  // successful. If not, then we will not stop caching since there are
  // other consumers waiting to read from the cache.
  if (all_writers_.size() != 1) {
    return false;
  }

  network_read_only_ = true;
  if (!keep_entry) {
    should_keep_entry_ = false;
    cache_->WritersDoomEntryRestartTransactions(entry_.get());
  }

  return true;
}

void HttpCache::Writers::AddTransaction(
    Transaction* transaction,
    ParallelWritingPattern initial_writing_pattern,
    RequestPriority priority,
    const TransactionInfo& info) {
  DCHECK(transaction);
  ParallelWritingPattern writers_pattern;
  DCHECK(CanAddWriters(&writers_pattern));

  DCHECK_EQ(0u, all_writers_.count(transaction));

  // Set truncation related information.
  response_info_truncation_ = info.response_info;
  should_keep_entry_ =
      IsValidResponseForWriter(info.partial != nullptr, &(info.response_info));

  if (all_writers_.empty()) {
    DCHECK_EQ(PARALLEL_WRITING_NONE, parallel_writing_pattern_);
    parallel_writing_pattern_ = initial_writing_pattern;
    if (parallel_writing_pattern_ != PARALLEL_WRITING_JOIN) {
      is_exclusive_ = true;
    }
  } else {
    DCHECK_EQ(PARALLEL_WRITING_JOIN, parallel_writing_pattern_);
  }

  if (info.partial && !info.truncated) {
    DCHECK(!partial_do_not_truncate_);
    partial_do_not_truncate_ = true;
  }

  std::pair<Transaction*, TransactionInfo> writer(transaction, info);
  all_writers_.insert(writer);

  priority_ = std::max(priority, priority_);
  if (network_transaction_) {
    network_transaction_->SetPriority(priority_);
  }
}

void HttpCache::Writers::SetNetworkTransaction(
    Transaction* transaction,
    std::unique_ptr<HttpTransaction> network_transaction) {
  DCHECK_EQ(1u, all_writers_.count(transaction));
  DCHECK(network_transaction);
  DCHECK(!network_transaction_);
  network_transaction_ = std::move(network_transaction);
  network_transaction_->SetPriority(priority_);
}

void HttpCache::Writers::ResetNetworkTransaction() {
  DCHECK(is_exclusive_);
  DCHECK_EQ(1u, all_writers_.size());
  DCHECK(all_writers_.begin()->second.partial);
  network_transaction_.reset();
}

void HttpCache::Writers::RemoveTransaction(Transaction* transaction,
                                           bool success) {
  EraseTransaction(transaction, OK);

  if (!all_writers_.empty()) {
    return;
  }

  if (!success && ShouldTruncate()) {
    TruncateEntry();
  }

  // Destroys `this`.
  cache_->WritersDoneWritingToEntry(entry_, success, should_keep_entry_,
                                    TransactionSet());
}

void HttpCache::Writers::EraseTransaction(Transaction* transaction,
                                          int result) {
  // The transaction should be part of all_writers.
  auto it = all_writers_.find(transaction);
  CHECK(it != all_writers_.end(), base::NotFatalUntil::M130);
  EraseTransaction(it, result);
}

HttpCache::Writers::TransactionMap::iterator
HttpCache::Writers::EraseTransaction(TransactionMap::iterator it, int result) {
  Transaction* transaction = it->first;
  transaction->WriterAboutToBeRemovedFromEntry(result);

  auto return_it = all_writers_.erase(it);

  if (all_writers_.empty() && next_state_ == State::NONE) {
    // This needs to be called to handle the edge case where even before Read is
    // invoked all transactions are removed. In that case the
    // network_transaction_ will still have a valid request info and so it
    // should be destroyed before its consumer is destroyed (request info
    // is a raw pointer owned by its consumer).
    network_transaction_.reset();
  } else {
    UpdatePriority();
  }

  if (active_transaction_ == transaction) {
    active_transaction_ = nullptr;
  } else {
    // If waiting for read, remove it from the map.
    waiting_for_read_.erase(transaction);
  }
  return return_it;
}

void HttpCache::Writers::UpdatePriority() {
  // Get the current highest priority.
  RequestPriority current_highest = MINIMUM_PRIORITY;
  for (auto& writer : all_writers_) {
    Transaction* transaction = writer.first;
    current_highest = std::max(transaction->priority(), current_highest);
  }

  if (priority_ != current_highest) {
    if (network_transaction_) {
      network_transaction_->SetPriority(current_highest);
    }
    priority_ = current_highest;
  }
}

void HttpCache::Writers::CloseConnectionOnDestruction() {
  if (network_transaction_) {
    network_transaction_->CloseConnectionOnDestruction();
  }
}

bool HttpCache::Writers::ContainsOnlyIdleWriters() const {
  return waiting_for_read_.empty() && !active_transaction_;
}

bool HttpCache::Writers::CanAddWriters(ParallelWritingPattern* reason) {
  *reason = parallel_writing_pattern_;

  if (all_writers_.empty()) {
    return true;
  }

  return !is_exclusive_ && !network_read_only_;
}

void HttpCache::Writers::ProcessFailure(int error) {
  // Notify waiting_for_read_ of the failure. Tasks will be posted for all the
  // transactions.
  CompleteWaitingForReadTransactions(error);

  // Idle readers should fail when Read is invoked on them.
  RemoveIdleWriters(error);
}

void HttpCache::Writers::TruncateEntry() {
  DCHECK(ShouldTruncate());
  auto data = base::MakeRefCounted<PickledIOBuffer>();
  response_info_truncation_.Persist(data->pickle(),
                                    true /* skip_transient_headers*/,
                                    true /* response_truncated */);
  data->Done();
  io_buf_len_ = data->pickle()->size();
  entry_->GetEntry()->WriteData(kResponseInfoIndex, 0, data.get(), io_buf_len_,
                                base::DoNothing(), true);
}

bool HttpCache::Writers::ShouldTruncate() {
  // Don't set the flag for sparse entries or for entries that cannot be
  // resumed.
  if (!should_keep_entry_ || partial_do_not_truncate_) {
    return false;
  }

  // Check the response headers for strong validators.
  // Note that if this is a 206, content-length was already fixed after calling
  // PartialData::ResponseHeadersOK().
  if (response_info_truncation_.headers->GetContentLength() <= 0 ||
      response_info_truncation_.headers->HasHeaderValue("Accept-Ranges",
                                                        "none") ||
      !response_info_truncation_.headers->HasStrongValidators()) {
    should_keep_entry_ = false;
    return false;
  }

  // Double check that there is something worth keeping.
  int current_size = entry_->GetEntry()->GetDataSize(kResponseContentIndex);
  if (!current_size) {
    should_keep_entry_ = false;
    return false;
  }

  if (response_info_truncation_.headers->HasHeader("Content-Encoding")) {
    should_keep_entry_ = false;
    return false;
  }

  int64_t content_length =
      response_info_truncation_.headers->GetContentLength();
  if (content_length >= 0 && content_length <= current_size) {
    return false;
  }

  return true;
}

LoadState HttpCache::Writers::GetLoadState() const {
  if (network_transaction_) {
    return network_transaction_->GetLoadState();
  }
  return LOAD_STATE_IDLE;
}

HttpCache::Writers::WaitingForRead::WaitingForRead(
    scoped_refptr<IOBuffer> buf,
    int len,
    CompletionOnceCallback consumer_callback)
    : read_buf(std::move(buf)),
      read_buf_len(len),
      callback(std::move(consumer_callback)) {
  DCHECK(read_buf);
  DCHECK_GT(len, 0);
  DCHECK(!callback.is_null());
}

HttpCache::Writers::WaitingForRead::~WaitingForRead() = default;
HttpCache::Writers::WaitingForRead::WaitingForRead(WaitingForRead&&) = default;

int HttpCache::Writers::DoLoop(int result) {
  DCHECK_NE(State::UNSET, next_state_);
  DCHECK_NE(State::NONE, next_state_);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = State::UNSET;
    switch (state) {
      case State::NETWORK_READ:
        DCHECK_EQ(OK, rv);
        rv = DoNetworkRead();
        break;
      case State::NETWORK_READ_COMPLETE:
        rv = DoNetworkReadComplete(rv);
        break;
      case State::CACHE_WRITE_DATA:
        rv = DoCacheWriteData(rv);
        break;
      case State::CACHE_WRITE_DATA_COMPLETE:
        rv = DoCacheWriteDataComplete(rv);
        break;
      case State::UNSET:
        NOTREACHED_IN_MIGRATION() << "bad state";
        rv = ERR_FAILED;
        break;
      case State::NONE:
        // Do Nothing.
        break;
    }
  } while (next_state_ != State::NONE && rv != ERR_IO_PENDING);

  if (next_state_ != State::NONE) {
    if (rv != ERR_IO_PENDING && !callback_.is_null()) {
      std::move(callback_).Run(rv);
    }
    return rv;
  }

  // Save the callback as |this| may be destroyed when |cache_callback_| is run.
  // Note that |callback_| is intentionally reset even if it is not run.
  CompletionOnceCallback callback = std::move(callback_);
  read_buf_ = nullptr;
  DCHECK(!all_writers_.empty() || cache_callback_);
  if (cache_callback_) {
    std::move(cache_callback_).Run();
  }
  // |this| may have been destroyed in the |cache_callback_|.
  if (rv != ERR_IO_PENDING && !callback.is_null()) {
    std::move(callback).Run(rv);
  }
  return rv;
}

int HttpCache::Writers::DoNetworkRead() {
  DCHECK(network_transaction_);
  next_state_ = State::NETWORK_READ_COMPLETE;

  // TODO(crbug.com/40089413): This is a partial mitigation. When
  // reading from the network, a valid HttpNetworkTransaction must be always
  // available.
  if (!network_transaction_) {
    return ERR_FAILED;
  }

  CompletionOnceCallback io_callback = base::BindOnce(
      &HttpCache::Writers::OnIOComplete, weak_factory_.GetWeakPtr());
  return network_transaction_->Read(read_buf_.get(), io_buf_len_,
                                    std::move(io_callback));
}

int HttpCache::Writers::DoNetworkReadComplete(int result) {
  if (result < 0) {
    next_state_ = State::NONE;
    OnNetworkReadFailure(result);
    return result;
  }

  next_state_ = State::CACHE_WRITE_DATA;
  return result;
}

void HttpCache::Writers::OnNetworkReadFailure(int result) {
  ProcessFailure(result);

  if (active_transaction_) {
    EraseTransaction(active_transaction_, result);
  }
  active_transaction_ = nullptr;

  if (ShouldTruncate()) {
    TruncateEntry();
  }

  SetCacheCallback(false, TransactionSet());
}

int HttpCache::Writers::DoCacheWriteData(int num_bytes) {
  next_state_ = State::CACHE_WRITE_DATA_COMPLETE;
  write_len_ = num_bytes;
  if (!num_bytes || network_read_only_) {
    return num_bytes;
  }

  int current_size = entry_->GetEntry()->GetDataSize(kResponseContentIndex);
  CompletionOnceCallback io_callback = base::BindOnce(
      &HttpCache::Writers::OnIOComplete, weak_factory_.GetWeakPtr());

  int rv = 0;

  PartialData* partial = nullptr;
  // The active transaction must be alive if this is a partial request, as
  // partial requests are exclusive and hence will always be the active
  // transaction.
  // TODO(shivanisha): When partial requests support parallel writing, this
  // assumption will not be true.
  if (active_transaction_) {
    partial = all_writers_.find(active_transaction_)->second.partial;
  }

  if (!partial) {
    last_disk_cache_access_start_time_ = base::TimeTicks::Now();
    rv = entry_->GetEntry()->WriteData(kResponseContentIndex, current_size,
                                       read_buf_.get(), num_bytes,
                                       std::move(io_callback), true);
  } else {
    rv = partial->CacheWrite(entry_->GetEntry(), read_buf_.get(), num_bytes,
                             std::move(io_callback));
  }
  return rv;
}

int HttpCache::Writers::DoCacheWriteDataComplete(int result) {
  DCHECK(!all_writers_.empty());
  DCHECK_GE(write_len_, 0);

  if (result != write_len_) {
    next_state_ = State::NONE;

    // Note that it is possible for cache write to fail if the size of the file
    // exceeds the per-file limit.
    OnCacheWriteFailure();

    // |active_transaction_| can continue reading from the network.
    return write_len_;
  }

  if (!last_disk_cache_access_start_time_.is_null() && active_transaction_ &&
      !all_writers_.find(active_transaction_)->second.partial) {
    active_transaction_->AddDiskCacheWriteTime(
        base::TimeTicks::Now() - last_disk_cache_access_start_time_);
    last_disk_cache_access_start_time_ = base::TimeTicks();
  }

  next_state_ = State::NONE;
  OnDataReceived(write_len_);

  return write_len_;
}

void HttpCache::Writers::OnDataReceived(int result) {
  DCHECK(!all_writers_.empty());

  auto it = all_writers_.find(active_transaction_);
  bool is_partial =
      active_transaction_ != nullptr && it->second.partial != nullptr;

  // Partial transaction will process the result, return from here.
  // This is done because partial requests handling require an awareness of both
  // headers and body state machines as they might have to go to the headers
  // phase for the next range, so it cannot be completely handled here.
  if (is_partial) {
    active_transaction_ = nullptr;
    return;
  }

  if (result == 0) {
    // Check if the response is actually completed or if not, attempt to mark
    // the entry as truncated in OnNetworkReadFailure.
    int current_size = entry_->GetEntry()->GetDataSize(kResponseContentIndex);
    DCHECK(network_transaction_);
    const HttpResponseInfo* response_info =
        network_transaction_->GetResponseInfo();
    int64_t content_length = response_info->headers->GetContentLength();
    if (content_length >= 0 && content_length > current_size) {
      OnNetworkReadFailure(result);
      return;
    }

    if (active_transaction_) {
      EraseTransaction(active_transaction_, result);
    }
    active_transaction_ = nullptr;
    CompleteWaitingForReadTransactions(write_len_);

    // Invoke entry processing.
    DCHECK(ContainsOnlyIdleWriters());
    TransactionSet make_readers;
    for (auto& writer : all_writers_) {
      make_readers.insert(writer.first);
    }
    all_writers_.clear();
    SetCacheCallback(true, make_readers);
    // We assume the set callback will be called immediately.
    DCHECK_EQ(next_state_, State::NONE);
    return;
  }

  // Notify waiting_for_read_. Tasks will be posted for all the
  // transactions.
  CompleteWaitingForReadTransactions(write_len_);

  active_transaction_ = nullptr;
}

void HttpCache::Writers::OnCacheWriteFailure() {
  DLOG(ERROR) << "failed to write response data to cache";

  ProcessFailure(ERR_CACHE_WRITE_FAILURE);

  // Now writers will only be reading from the network.
  network_read_only_ = true;

  active_transaction_ = nullptr;

  should_keep_entry_ = false;
  if (all_writers_.empty()) {
    SetCacheCallback(false, TransactionSet());
  } else {
    cache_->WritersDoomEntryRestartTransactions(entry_.get());
  }
}

void HttpCache::Writers::CompleteWaitingForReadTransactions(int result) {
  for (auto it = waiting_for_read_.begin(); it != waiting_for_read_.end();) {
    Transaction* transaction = it->first;
    int callback_result = result;

    if (result >= 0) {  // success
      // Save the data in the waiting transaction's read buffer.
      it->second.write_len = std::min(it->second.read_buf_len, result);
      memcpy(it->second.read_buf->data(), read_buf_->data(),
             it->second.write_len);
      callback_result = it->second.write_len;
    }

    // Post task to notify transaction.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(it->second.callback), callback_result));

    it = waiting_for_read_.erase(it);

    // If its response completion or failure, this transaction needs to be
    // removed from writers.
    if (result <= 0) {
      EraseTransaction(transaction, result);
    }
  }
}

void HttpCache::Writers::RemoveIdleWriters(int result) {
  // Since this is only for idle transactions, waiting_for_read_
  // should be empty.
  DCHECK(waiting_for_read_.empty());
  for (auto it = all_writers_.begin(); it != all_writers_.end();) {
    Transaction* transaction = it->first;
    if (transaction == active_transaction_) {
      it++;
      continue;
    }
    it = EraseTransaction(it, result);
  }
}

void HttpCache::Writers::SetCacheCallback(bool success,
                                          const TransactionSet& make_readers) {
  DCHECK(!cache_callback_);
  cache_callback_ = base::BindOnce(&HttpCache::WritersDoneWritingToEntry,
                                   cache_->GetWeakPtr(), entry_, success,
                                   should_keep_entry_, make_readers);
}

void HttpCache::Writers::OnIOComplete(int result) {
  DoLoop(result);
}

}  // namespace net
