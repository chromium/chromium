// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_entry_impl.h"

#include "base/notimplemented.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
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
    backend_->ReleaseDoomedEntry(*this);
  } else {
    if (previous_header_size_in_storage_.has_value()) {
      // If the entry's header was modified (i.e., a write to stream 0
      // occurred), update both the header and `last_used_` in the persistent
      // store.
      const int64_t header_size_delta = static_cast<int64_t>(head_->size()) -
                                        *previous_header_size_in_storage_;
      backend_->UpdateEntryHeaderAndLastUsed(key_, token_, last_used_, head_,
                                             header_size_delta,
                                             base::DoNothing());
    } else if (last_used_modified_) {
      // Otherwise, if only last_used was modified, update just last_used.
      backend_->UpdateEntryLastUsed(key_, token_, last_used_,
                                    base::DoNothing());
    }
    backend_->ReleaseActiveEntry(*this);
  }
}

void SqlEntryImpl::Doom() {
  if (doomed_ || !backend_) {
    return;
  }
  backend_->DoomActiveEntry(*this, base::DoNothing());
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
  if (index != 0 && index != 1) {
    return net::ERR_INVALID_ARGUMENT;
  }
  if (index == 0) {
    return head_->size();
  }
  CHECK_EQ(index, 1);
  // TODO(crbug.com/422065015): Implement this.
  return net::ERR_NOT_IMPLEMENTED;
}

int SqlEntryImpl::ReadData(int index,
                           int offset,
                           IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  UpdateLastUsed();
  if (index != 0 && index != 1) {
    return net::ERR_INVALID_ARGUMENT;
  }
  if (buf_len == 0) {
    return 0;
  }
  if (!buf || buf_len < 0 || offset < 0) {
    return net::ERR_INVALID_ARGUMENT;
  }
  if (index == 1) {
    // TODO(crbug.com/422065015): Implement this.
    return net::ERR_INVALID_ARGUMENT;
  }
  // Ensure it's stream 0 (header).
  CHECK_EQ(index, 0);
  if (head_->size() <= offset) {
    return 0;
  }
  buf_len = std::min(buf_len, head_->size() - offset);
  buf->first(buf_len).copy_from_nonoverlapping(head_->span().subspan(
      base::checked_cast<size_t>(offset), base::checked_cast<size_t>(buf_len)));
  return buf_len;
}

int SqlEntryImpl::WriteData(int index,
                            int offset,
                            IOBuffer* buf,
                            int buf_len,
                            CompletionOnceCallback callback,
                            bool truncate) {
  UpdateLastUsed();
  if ((index != 0 && index != 1) || (offset < 0) || (buf_len < 0) ||
      (!buf && buf_len > 0) || !base::CheckAdd(offset, buf_len).IsValid()) {
    return net::ERR_INVALID_ARGUMENT;
  }
  if (index == 1) {
    // TODO(crbug.com/422065015): Implement this.
    return net::ERR_INVALID_ARGUMENT;
  }
  CHECK_EQ(index, 0);

  // If this is the first write to the header, store its original size for later
  // persistence.
  if (!previous_header_size_in_storage_) {
    previous_header_size_in_storage_ = head_->size();
  }

  if (offset == 0 && truncate) {
    head_->SetCapacity(buf_len);
    if (buf_len) {
      head_->span().copy_from(buf->first(base::checked_cast<size_t>(buf_len)));
    }
  } else {
    const int original_size = head_->size();
    const int buffer_size =
        truncate ? offset + buf_len : std::max(offset + buf_len, original_size);
    head_->SetCapacity(buffer_size);

    // Fill any gap with zeros if writing beyond current size.
    const int fill_size = offset <= original_size ? 0 : offset - original_size;
    if (fill_size > 0) {
      std::ranges::fill(
          head_->span().subspan(base::checked_cast<size_t>(original_size),
                                base::checked_cast<size_t>(fill_size)),
          0);
    }
    // Copy new data into the buffer.
    if (buf) {
      head_->span()
          .subspan(base::checked_cast<size_t>(offset))
          .copy_prefix_from(buf->first(base::checked_cast<size_t>(buf_len)));
    }
  }
  return buf_len;
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
  last_used_modified_ = true;
}

void SqlEntryImpl::UpdateLastUsed() {
  last_used_ = base::Time::Now();
  last_used_modified_ = true;
}

void SqlEntryImpl::MarkAsDoomed() {
  doomed_ = true;
}

}  // namespace disk_cache
