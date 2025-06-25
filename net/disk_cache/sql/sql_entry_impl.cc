// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_entry_impl.h"

#include "base/notimplemented.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/sql/sql_backend_impl.h"

namespace disk_cache {

SqlEntryImpl::SqlEntryImpl(base::WeakPtr<SqlBackendImpl> backend,
                           CacheEntryKey key,
                           const base::UnguessableToken& token,
                           base::Time last_used,
                           int64_t body_end,
                           scoped_refptr<net::GrowableIOBuffer> head)
    : backend_(backend),
      key_(key),
      token_(token),
      last_used_(last_used),
      body_end_(body_end),
      head_(head ? std::move(head)
                 : base::MakeRefCounted<net::GrowableIOBuffer>()) {}

SqlEntryImpl::~SqlEntryImpl() {
  if (!backend_) {
    return;
  }

  if (doomed_) {
    backend_->GetStore().DeleteDoomedEntry(
        key_, token_, base::BindOnce([](SqlPersistentStore::Error error) {}));
  }

  // TODO(crbug.com/422065015): If `last_used_` was modified, persist it to the
  // storage.

  if (doomed_) {
    backend_->ReleaseDoomedEntry(*this);
  } else {
    backend_->ReleaseActiveEntry(*this);
  }
}

void SqlEntryImpl::Doom() {
  if (doomed_ || !backend_) {
    return;
  }
  backend_->DoomActiveEntry(*this);
}

void SqlEntryImpl::Close() {
  Release();
}

std::string SqlEntryImpl::GetKey() const {
  return key_.string();
}

base::Time SqlEntryImpl::GetLastUsed() const {
  return last_used_;
}

int32_t SqlEntryImpl::GetDataSize(int index) const {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int SqlEntryImpl::ReadData(int index,
                           int offset,
                           IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int SqlEntryImpl::WriteData(int index,
                            int offset,
                            IOBuffer* buf,
                            int buf_len,
                            CompletionOnceCallback callback,
                            bool truncate) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int SqlEntryImpl::ReadSparseData(int64_t offset,
                                 IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int SqlEntryImpl::WriteSparseData(int64_t offset,
                                  IOBuffer* buf,
                                  int buf_len,
                                  CompletionOnceCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

RangeResult SqlEntryImpl::GetAvailableRange(int64_t offset,
                                            int len,
                                            RangeResultCallback callback) {
  // TODO(crbug.com/422065015): Implement this method.
  NOTIMPLEMENTED();
  return RangeResult(net::ERR_NOT_IMPLEMENTED);
}

bool SqlEntryImpl::CouldBeSparse() const {
  // SqlEntryImpl doesn't distinguish the stream 1 data and the sparse data.
  return true;
}

void SqlEntryImpl::CancelSparseIO() {
  // SqlEntryImpl doesn't distinguish the stream 1 data and the sparse data.
}

net::Error SqlEntryImpl::ReadyForSparseIO(CompletionOnceCallback callback) {
  // SqlEntryImpl doesn't distinguish the stream 1 data and the sparse data.
  return net::OK;
}

void SqlEntryImpl::SetLastUsedTimeForTest(base::Time time) {
  last_used_ = time;
}

void SqlEntryImpl::MarkAsDoomed() {
  doomed_ = true;
}

}  // namespace disk_cache
