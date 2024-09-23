// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/in_flight_backend_io.h"

#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/entry_impl.h"

namespace disk_cache {

namespace {

// Used to leak a strong reference to an EntryImpl to the user of disk_cache.
EntryImpl* LeakEntryImpl(scoped_refptr<EntryImpl> entry) {
  // Balanced on OP_CLOSE_ENTRY handling in BackendIO::ExecuteBackendOperation.
  if (entry)
    entry->AddRef();
  return entry.get();
}

}  // namespace

BackendIO::BackendIO(InFlightBackendIO* controller,
                     BackendImpl* backend,
                     net::CompletionOnceCallback callback)
    : BackendIO(controller, backend) {
  callback_ = std::move(callback);
}

BackendIO::BackendIO(InFlightBackendIO* controller,
                     BackendImpl* backend,
                     EntryResultCallback callback)
    : BackendIO(controller, backend) {
  entry_result_callback_ = std::move(callback);
}

BackendIO::BackendIO(InFlightBackendIO* controller,
                     BackendImpl* backend,
                     RangeResultCallback callback)
    : BackendIO(controller, backend) {
  range_result_callback_ = std::move(callback);
}

BackendIO::BackendIO(InFlightBackendIO* controller, BackendImpl* backend)
    : BackgroundIO(controller),
      backend_(backend),
      background_task_runner_(controller->background_thread()) {
  DCHECK(background_task_runner_);
  start_time_ = base::TimeTicks::Now();
}

// Runs on the background thread.
void BackendIO::ExecuteOperation() {
  if (IsEntryOperation()) {
    ExecuteEntryOperation();
  } else {
    ExecuteBackendOperation();
  }
  // Clear our pointer to entry we operated on.  We don't need it any more, and
  // it's possible by the time ~BackendIO gets destroyed on the main thread the
  // entry will have been closed and freed on the cache/background thread.
  entry_ = nullptr;
}

// Runs on the background thread.
void BackendIO::OnIOComplete(int result) {
  DCHECK(IsEntryOperation());
  DCHECK_NE(result, net::ERR_IO_PENDING);
  result_ = result;
  NotifyController();
}

// Runs on the primary thread.
void BackendIO::OnDone(bool cancel) {
  if (IsEntryOperation() && backend_->GetCacheType() == net::DISK_CACHE) {
    switch (operation_) {
      case OP_READ:
        base::UmaHistogramCustomTimes("DiskCache.0.TotalIOTimeRead",
                                      ElapsedTime(), base::Milliseconds(1),
                                      base::Seconds(10), 50);
        break;

      case OP_WRITE:
        base::UmaHistogramCustomTimes("DiskCache.0.TotalIOTimeWrite",
                                      ElapsedTime(), base::Milliseconds(1),
                                      base::Seconds(10), 50);
        break;

      default:
        // Other operations are not recorded.
        break;
    }
  }

  if (ReturnsEntry() && result_ == net::OK) {
    static_cast<EntryImpl*>(out_entry_)->OnEntryCreated(backend_);
    if (cancel)
      out_entry_.ExtractAsDangling()->Close();
  }
  ClearController();
}

bool BackendIO::IsEntryOperation() {
  return operation_ > OP_MAX_BACKEND;
}

void BackendIO::RunCallback(int result) {
  std::move(callback_).Run(result);
}

void BackendIO::RunEntryResultCallback() {
  EntryResult entry_result;
  if (result_ != net::OK) {
    entry_result = EntryResult::MakeError(static_cast<net::Error>(result()));
  } else if (out_entry_opened_) {
    entry_result = EntryResult::MakeOpened(out_entry_.ExtractAsDangling());
  } else {
    entry_result = EntryResult::MakeCreated(out_entry_.ExtractAsDangling());
  }
  std::move(entry_result_callback_).Run(std::move(entry_result));
}

void BackendIO::RunRangeResultCallback() {
  std::move(range_result_callback_).Run(range_result_);
}

void BackendIO::Init() {
  operation_ = OP_INIT;
}

void BackendIO::OpenOrCreateEntry(const std::string& key) {
  operation_ = OP_OPEN_OR_CREATE;
  key_ = key;
}

void BackendIO::OpenEntry(const std::string& key) {
  operation_ = OP_OPEN;
  key_ = key;
}

void BackendIO::CreateEntry(const std::string& key) {
  operation_ = OP_CREATE;
  key_ = key;
}

void BackendIO::DoomEntry(const std::string& key) {
  operation_ = OP_DOOM;
  key_ = key;
}

void BackendIO::DoomAllEntries() {
  operation_ = OP_DOOM_ALL;
}

void BackendIO::DoomEntriesBetween(const base::Time initial_time,
                                   const base::Time end_time) {
  operation_ = OP_DOOM_BETWEEN;
  initial_time_ = initial_time;
  end_time_ = end_time;
}

void BackendIO::DoomEntriesSince(const base::Time initial_time) {
  operation_ = OP_DOOM_SINCE;
  initial_time_ = initial_time;
}

void BackendIO::CalculateSizeOfAllEntries() {
  operation_ = OP_SIZE_ALL;
}

void BackendIO::OpenNextEntry(Rankings::Iterator* iterator) {
  operation_ = OP_OPEN_NEXT;
  iterator_ = iterator;
}

void BackendIO::EndEnumeration(std::unique_ptr<Rankings::Iterator> iterator) {
  operation_ = OP_END_ENUMERATION;
  scoped_iterator_ = std::move(iterator);
}

void BackendIO::OnExternalCacheHit(const std::string& key) {
  operation_ = OP_ON_EXTERNAL_CACHE_HIT;
  key_ = key;
}

void BackendIO::CloseEntryImpl(EntryImpl* entry) {
  operation_ = OP_CLOSE_ENTRY;
  entry_ = entry;
}

void BackendIO::DoomEntryImpl(EntryImpl* entry) {
  operation_ = OP_DOOM_ENTRY;
  entry_ = entry;
}

void BackendIO::FlushQueue() {
  operation_ = OP_FLUSH_QUEUE;
}

void BackendIO::RunTask(base::OnceClosure task) {
  operation_ = OP_RUN_TASK;
  task_ = std::move(task);
}

void BackendIO::ReadData(EntryImpl* entry, int index, int offset,
                         net::IOBuffer* buf, int buf_len) {
  operation_ = OP_READ;
  entry_ = entry;
  index_ = index;
  offset_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
}

void BackendIO::WriteData(EntryImpl* entry, int index, int offset,
                          net::IOBuffer* buf, int buf_len, bool truncate) {
  operation_ = OP_WRITE;
  entry_ = entry;
  index_ = index;
  offset_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
  truncate_ = truncate;
}

void BackendIO::ReadSparseData(EntryImpl* entry,
                               int64_t offset,
                               net::IOBuffer* buf,
                               int buf_len) {
  operation_ = OP_READ_SPARSE;
  entry_ = entry;
  offset64_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
}

void BackendIO::WriteSparseData(EntryImpl* entry,
                                int64_t offset,
                                net::IOBuffer* buf,
                                int buf_len) {
  operation_ = OP_WRITE_SPARSE;
  entry_ = entry;
  offset64_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
}

void BackendIO::GetAvailableRange(EntryImpl* entry, int64_t offset, int len) {
  operation_ = OP_GET_RANGE;
  entry_ = entry;
  offset64_ = offset;
  buf_len_ = len;
}

void BackendIO::CancelSparseIO(EntryImpl* entry) {
  operation_ = OP_CANCEL_IO;
  entry_ = entry;
}

void BackendIO::ReadyForSparseIO(EntryImpl* entry) {
  operation_ = OP_IS_READY;
  entry_ = entry;
}

BackendIO::~BackendIO() {
  if (!did_notify_controller_io_signalled() && out_entry_) {
    // At this point it's very likely the Entry does not have a
    // `background_queue_` so that Close() would do nothing. Post a task to the
    // background task runner to drop the reference, which should effectively
    // destroy if there are no more references. Destruction has to happen
    // on the background task runner.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EntryImpl::Release,
                       base::Unretained(out_entry_.ExtractAsDangling())));
  }
}

bool BackendIO::ReturnsEntry() {
  return operation_ == OP_OPEN || operation_ == OP_CREATE ||
         operation_ == OP_OPEN_NEXT || operation_ == OP_OPEN_OR_CREATE;
}

base::TimeDelta BackendIO::ElapsedTime() const {
  return base::TimeTicks::Now() - start_time_;
}

// Runs on the background thread.
void BackendIO::ExecuteBackendOperation() {
  switch (operation_) {
    case OP_INIT:
      result_ = backend_->SyncInit();
      break;
    case OP_OPEN_OR_CREATE: {
      scoped_refptr<EntryImpl> entry;
      result_ = backend_->SyncOpenEntry(key_, &entry);

      if (result_ == net::OK) {
        out_entry_ = LeakEntryImpl(std::move(entry));
        out_entry_opened_ = true;
        break;
      }

      // Opening failed, create an entry instead.
      result_ = backend_->SyncCreateEntry(key_, &entry);
      out_entry_ = LeakEntryImpl(std::move(entry));
      out_entry_opened_ = false;
      break;
    }
    case OP_OPEN: {
      scoped_refptr<EntryImpl> entry;
      result_ = backend_->SyncOpenEntry(key_, &entry);
      out_entry_ = LeakEntryImpl(std::move(entry));
      out_entry_opened_ = true;
      break;
    }
    case OP_CREATE: {
      scoped_refptr<EntryImpl> entry;
      result_ = backend_->SyncCreateEntry(key_, &entry);
      out_entry_ = LeakEntryImpl(std::move(entry));
      out_entry_opened_ = false;
      break;
    }
    case OP_DOOM:
      result_ = backend_->SyncDoomEntry(key_);
      break;
    case OP_DOOM_ALL:
      result_ = backend_->SyncDoomAllEntries();
      break;
    case OP_DOOM_BETWEEN:
      result_ = backend_->SyncDoomEntriesBetween(initial_time_, end_time_);
      break;
    case OP_DOOM_SINCE:
      result_ = backend_->SyncDoomEntriesSince(initial_time_);
      break;
    case OP_SIZE_ALL:
      result_ = backend_->SyncCalculateSizeOfAllEntries();
      break;
    case OP_OPEN_NEXT: {
      scoped_refptr<EntryImpl> entry;
      result_ = backend_->SyncOpenNextEntry(iterator_, &entry);
      out_entry_ = LeakEntryImpl(std::move(entry));
      out_entry_opened_ = true;
      // `iterator_` is a proxied argument and not needed beyond this point. Set
      // it to nullptr so as to not leave a dangling pointer around.
      iterator_ = nullptr;
      break;
    }
    case OP_END_ENUMERATION:
      backend_->SyncEndEnumeration(std::move(scoped_iterator_));
      result_ = net::OK;
      break;
    case OP_ON_EXTERNAL_CACHE_HIT:
      backend_->SyncOnExternalCacheHit(key_);
      result_ = net::OK;
      break;
    case OP_CLOSE_ENTRY:
      // Collect the reference to |entry_| to balance with the AddRef() in
      // LeakEntryImpl.
      entry_.ExtractAsDangling()->Release();
      result_ = net::OK;
      break;
    case OP_DOOM_ENTRY:
      entry_->DoomImpl();
      result_ = net::OK;
      break;
    case OP_FLUSH_QUEUE:
      result_ = net::OK;
      break;
    case OP_RUN_TASK:
      std::move(task_).Run();
      result_ = net::OK;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid Operation";
      result_ = net::ERR_UNEXPECTED;
  }
  DCHECK_NE(net::ERR_IO_PENDING, result_);
  NotifyController();
  backend_->OnSyncBackendOpComplete();
}

// Runs on the background thread.
void BackendIO::ExecuteEntryOperation() {
  switch (operation_) {
    case OP_READ:
      result_ =
          entry_->ReadDataImpl(index_, offset_, buf_.get(), buf_len_,
                               base::BindOnce(&BackendIO::OnIOComplete, this));
      break;
    case OP_WRITE:
      result_ = entry_->WriteDataImpl(
          index_, offset_, buf_.get(), buf_len_,
          base::BindOnce(&BackendIO::OnIOComplete, this), truncate_);
      break;
    case OP_READ_SPARSE:
      result_ = entry_->ReadSparseDataImpl(
          offset64_, buf_.get(), buf_len_,
          base::BindOnce(&BackendIO::OnIOComplete, this));
      break;
    case OP_WRITE_SPARSE:
      result_ = entry_->WriteSparseDataImpl(
          offset64_, buf_.get(), buf_len_,
          base::BindOnce(&BackendIO::OnIOComplete, this));
      break;
    case OP_GET_RANGE:
      range_result_ = entry_->GetAvailableRangeImpl(offset64_, buf_len_);
      result_ = range_result_.net_error;
      break;
    case OP_CANCEL_IO:
      entry_->CancelSparseIOImpl();
      result_ = net::OK;
      break;
    case OP_IS_READY:
      result_ = entry_->ReadyForSparseIOImpl(
          base::BindOnce(&BackendIO::OnIOComplete, this));
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid Operation";
      result_ = net::ERR_UNEXPECTED;
  }
  buf_ = nullptr;
  if (result_ != net::ERR_IO_PENDING)
    NotifyController();
}

InFlightBackendIO::InFlightBackendIO(
    BackendImpl* backend,
    const scoped_refptr<base::SingleThreadTaskRunner>& background_thread)
    : backend_(backend), background_thread_(background_thread) {}

InFlightBackendIO::~InFlightBackendIO() = default;

void InFlightBackendIO::Init(net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->Init();
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::OpenOrCreateEntry(const std::string& key,
                                          EntryResultCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->OpenOrCreateEntry(key);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::OpenEntry(const std::string& key,
                                  EntryResultCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->OpenEntry(key);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::CreateEntry(const std::string& key,
                                    EntryResultCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->CreateEntry(key);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::DoomEntry(const std::string& key,
                                  net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->DoomEntry(key);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::DoomAllEntries(net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->DoomAllEntries();
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::DoomEntriesBetween(
    const base::Time initial_time,
    const base::Time end_time,
    net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->DoomEntriesBetween(initial_time, end_time);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::CalculateSizeOfAllEntries(
    net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->CalculateSizeOfAllEntries();
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::DoomEntriesSince(const base::Time initial_time,
                                         net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->DoomEntriesSince(initial_time);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::OpenNextEntry(Rankings::Iterator* iterator,
                                      EntryResultCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->OpenNextEntry(iterator);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::EndEnumeration(
    std::unique_ptr<Rankings::Iterator> iterator) {
  auto operation = base::MakeRefCounted<BackendIO>(
      this, backend_, net::CompletionOnceCallback());
  operation->EndEnumeration(std::move(iterator));
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::OnExternalCacheHit(const std::string& key) {
  auto operation = base::MakeRefCounted<BackendIO>(
      this, backend_, net::CompletionOnceCallback());
  operation->OnExternalCacheHit(key);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::CloseEntryImpl(EntryImpl* entry) {
  auto operation = base::MakeRefCounted<BackendIO>(
      this, backend_, net::CompletionOnceCallback());
  operation->CloseEntryImpl(entry);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::DoomEntryImpl(EntryImpl* entry) {
  auto operation = base::MakeRefCounted<BackendIO>(
      this, backend_, net::CompletionOnceCallback());
  operation->DoomEntryImpl(entry);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::FlushQueue(net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->FlushQueue();
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::RunTask(base::OnceClosure task,
                                net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->RunTask(std::move(task));
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::ReadData(EntryImpl* entry,
                                 int index,
                                 int offset,
                                 net::IOBuffer* buf,
                                 int buf_len,
                                 net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->ReadData(entry, index, offset, buf, buf_len);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::WriteData(EntryImpl* entry,
                                  int index,
                                  int offset,
                                  net::IOBuffer* buf,
                                  int buf_len,
                                  bool truncate,
                                  net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->WriteData(entry, index, offset, buf, buf_len, truncate);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::ReadSparseData(EntryImpl* entry,
                                       int64_t offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->ReadSparseData(entry, offset, buf, buf_len);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::WriteSparseData(EntryImpl* entry,
                                        int64_t offset,
                                        net::IOBuffer* buf,
                                        int buf_len,
                                        net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->WriteSparseData(entry, offset, buf, buf_len);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::GetAvailableRange(EntryImpl* entry,
                                          int64_t offset,
                                          int len,
                                          RangeResultCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->GetAvailableRange(entry, offset, len);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::CancelSparseIO(EntryImpl* entry) {
  auto operation = base::MakeRefCounted<BackendIO>(
      this, backend_, net::CompletionOnceCallback());
  operation->CancelSparseIO(entry);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::ReadyForSparseIO(EntryImpl* entry,
                                         net::CompletionOnceCallback callback) {
  auto operation =
      base::MakeRefCounted<BackendIO>(this, backend_, std::move(callback));
  operation->ReadyForSparseIO(entry);
  PostOperation(FROM_HERE, operation.get());
}

void InFlightBackendIO::WaitForPendingIO() {
  InFlightIO::WaitForPendingIO();
}

void InFlightBackendIO::OnOperationComplete(BackgroundIO* operation,
                                            bool cancel) {
  BackendIO* op = static_cast<BackendIO*>(operation);
  op->OnDone(cancel);

  if (op->has_callback() && (!cancel || op->IsEntryOperation()))
    op->RunCallback(op->result());

  if (op->has_range_result_callback()) {
    DCHECK(op->IsEntryOperation());
    op->RunRangeResultCallback();
  }

  if (op->has_entry_result_callback() && !cancel) {
    DCHECK(!op->IsEntryOperation());
    op->RunEntryResultCallback();
  }
}

void InFlightBackendIO::PostOperation(const base::Location& from_here,
                                      BackendIO* operation) {
  background_thread_->PostTask(
      from_here, base::BindOnce(&BackendIO::ExecuteOperation, operation));
  OnOperationPosted(operation);
}

base::WeakPtr<InFlightBackendIO> InFlightBackendIO::GetWeakPtr() {
  return ptr_factory_.GetWeakPtr();
}

}  // namespace disk_cache
