// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_entry_impl.h"

#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/sql/entry_db_handle.h"
#include "net/disk_cache/sql/sql_backend_impl.h"

namespace disk_cache {

SqlEntryImpl::SqlEntryImpl(base::WeakPtr<SqlBackendImpl> backend,
                           CacheEntryKey key,
                           scoped_refptr<EntryDbHandle> db_handle,
                           base::Time last_used,
                           int64_t body_end,
                           scoped_refptr<net::GrowableIOBuffer> head)
    : backend_(backend),
      key_(key),
      db_handle_(std::move(db_handle)),
      last_used_(last_used),
      body_end_(body_end),
      head_(head ? std::move(head)
                 : base::MakeRefCounted<net::GrowableIOBuffer>()) {}

SqlEntryImpl::~SqlEntryImpl() {
  if (!backend_) {
    return;
  }

  if (doomed()) {
    backend_->ReleaseDoomedEntry(*this);
    return;
  }

  std::optional<int64_t> old_body_end;
  EntryWriteBuffer buffer;
  SqlWriteBufferMemoryMonitor::ScopedReservation reservation;

  if (TakeWriteBuffer(buffer, reservation)) {
    old_body_end = buffer.offset;
  }

  int64_t header_size_delta = 0;
  scoped_refptr<net::GrowableIOBuffer> head_to_send;
  bool metadata_update_needed = false;
  if (previous_header_size_in_storage_.has_value() || new_hints_.has_value()) {
    header_size_delta = static_cast<int64_t>(head_->size()) -
                        previous_header_size_in_storage_.value_or(0);
    head_to_send = head_;
    metadata_update_needed = true;
  } else if (last_used_modified_) {
    metadata_update_needed = true;
  }
  if (db_handle_->IsInitialState() || old_body_end.has_value() ||
      metadata_update_needed) {
    backend_->WriteEntryDataAndMetadata(
        key_, db_handle_, old_body_end, body_end_, std::move(buffer),
        last_used_, new_hints_, std::move(head_to_send), header_size_delta,
        base::DoNothingWithBoundArgs(std::move(reservation)));
  }

  backend_->ReleaseActiveEntry(*this);
}

void SqlEntryImpl::Doom() {
  if (doomed() || !backend_) {
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

int64_t SqlEntryImpl::GetDataSize(int index) const {
  if (index != 0 && index != 1) {
    return net::ERR_INVALID_ARGUMENT;
  }
  if (index == 0) {
    return head_->size();
  }
  CHECK_EQ(index, 1);
  return body_end_;
}

int SqlEntryImpl::ReadData(int index,
                           int64_t offset,
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
  // Unlike WriteData, there is no overflow check for `offset + buf_len` here.
  // This is intentional. The read path is designed to be permissive: even if
  // the requested range would overflow, the underlying SqlPersistentStore will
  // truncate the read length to fit within the `int64_t` range, allowing a
  // partial read up to the maximum possible offset.
  //
  // TODO(crbug.com/422065015): To enable int64_t offset writes for stream 1 in
  // the SQL backend, the check for offset against int max should be moved to
  // the index == 0 logic path, as stream 1 of SQL backend is designed to handle
  // offsets larger than int max.
  if (!buf || buf_len < 0 || offset < 0 ||
      offset > std::numeric_limits<int>::max()) {
    return net::ERR_INVALID_ARGUMENT;
  }

  if (index == 1) {
    return ReadDataInternal(offset, buf, buf_len, std::move(callback),
                            /*sparse_reading=*/false);
  }
  // Ensure it's stream 0 (header).
  CHECK_EQ(index, 0);
  if (head_->size() <= offset) {
    return 0;
  }
  buf_len = std::min(buf_len, head_->size() - static_cast<int>(offset));
  buf->first(buf_len).copy_from_nonoverlapping(head_->span().subspan(
      static_cast<size_t>(offset), static_cast<size_t>(buf_len)));
  return buf_len;
}

int SqlEntryImpl::ReadDataInternal(int64_t offset,
                                   IOBuffer* buf,
                                   int buf_len,
                                   CompletionOnceCallback callback,
                                   bool sparse_reading) {
  if (!backend_) {
    return net::ERR_FAILED;
  }
  if (body_end_ <= offset) {
    return 0;
  }
  if (read_cache_buffer_) {
    const int64_t cache_end =
        read_cache_buffer_offset_ + read_cache_buffer_->size();
    if (offset >= read_cache_buffer_offset_ && offset < cache_end) {
      const int64_t relative_offset = offset - read_cache_buffer_offset_;
      const int copy_size =
          std::min(buf_len, static_cast<int>(cache_end - offset));
      base::as_writable_bytes(buf->span())
          .first(static_cast<size_t>(copy_size))
          .copy_from(base::as_bytes(read_cache_buffer_->span())
                         .subspan(static_cast<size_t>(relative_offset),
                                  static_cast<size_t>(copy_size)));
      return copy_size;
    }
  }

  if (!write_buffer_.buffers.empty()) {
    const int64_t write_buffer_end =
        write_buffer_.offset + static_cast<int64_t>(write_buffer_.size);
    // If the request is fully within the buffer, copy from the buffer.
    if (offset >= write_buffer_.offset &&
        offset + buf_len <= write_buffer_end) {
      int64_t relative_offset = offset - write_buffer_.offset;
      CHECK_GE(relative_offset, 0);
      CHECK_LE(relative_offset + buf_len,
               static_cast<int64_t>(write_buffer_.size));
      size_t bytes_copied = 0;
      for (const auto& chunk : write_buffer_.buffers) {
        if (relative_offset < static_cast<int64_t>(chunk->size())) {
          const size_t copy_size =
              std::min(static_cast<size_t>(buf_len - bytes_copied),
                       chunk->size() - static_cast<size_t>(relative_offset));
          base::as_writable_bytes(buf->span())
              .subspan(bytes_copied, copy_size)
              .copy_from(base::as_bytes(chunk->span())
                             .subspan(static_cast<size_t>(relative_offset),
                                      copy_size));
          bytes_copied += copy_size;
          relative_offset = 0;
          if (bytes_copied == static_cast<size_t>(buf_len)) {
            break;
          }
        } else {
          relative_offset -= chunk->size();
        }
      }
      return buf_len;
    }
    // If the request overlaps with the buffer, flush the buffer first.
    if (std::max(offset, write_buffer_.offset) <
        std::min(offset + buf_len, write_buffer_end)) {
      FlushBuffer(/*force_flush_for_creation=*/false);
    }
  }

  // When `db_handle_` is in the initial state, `write_buffer_.offset` must be 0
  // and `write_buffer_.size` must be `body_end_`.
  // In that case:
  // - If the request is beyond `write_buffer_`:
  //     0 is returned above (body_end_ <= offset).
  // - If the request is within `write_buffer_`:
  //     `buf_len` is returned above.
  // - If the request overlaps with `write_buffer_`:
  //     FlushBuffer is called above, transitioning to creating state in
  //     SqlBackendImpl::WriteEntryData.
  // Therefore, `db_handle_` cannot be in the initial state here.
  CHECK(!db_handle_->IsInitialState());

  auto read_callback = base::BindOnce(
      [](base::WeakPtr<SqlEntryImpl> self, CompletionOnceCallback callback,
         SqlPersistentStore::ReadResultOrError result) {
        if (!result.has_value()) {
          std::move(callback).Run(result.error() ==
                                          SqlPersistentStore::Error::kAborted
                                      ? net::ERR_ABORTED
                                      : net::ERR_FAILED);
          return;
        }
        if (self) {
          self->read_cache_buffer_ = std::move(result->cache_buffer);
          self->read_cache_buffer_offset_ = result->cache_buffer_offset;
        }
        std::move(callback).Run(result->read_bytes);
      },
      weak_factory_.GetWeakPtr(), std::move(callback));

  return backend_->ReadEntryData(key_, db_handle_, offset, buf, buf_len,
                                 body_end_, sparse_reading,
                                 std::move(read_callback));
}

int SqlEntryImpl::WriteData(int index,
                            int64_t offset,
                            IOBuffer* buf,
                            int buf_len,
                            CompletionOnceCallback callback,
                            bool truncate) {
  UpdateLastUsed();
  if ((index != 0 && index != 1) || (offset < 0) || (buf_len < 0) ||
      (!buf && buf_len > 0) || !base::CheckAdd(offset, buf_len).IsValid()) {
    return net::ERR_INVALID_ARGUMENT;
  }

  // TODO(crbug.com/422065015): To enable int64_t offset reads for stream 1 in
  // the SQL backend, the check should be moved to the index == 0 logic path, as
  // stream 1 of SQL backend is designed to handle offsets larger than int max.
  if (offset + buf_len > std::numeric_limits<int>::max()) {
    return net::ERR_INVALID_ARGUMENT;
  }

  if (index == 1) {
    return WriteDataInternal(offset, buf, buf_len, std::move(callback),
                             truncate, /*sparse_write=*/false);
  }
  CHECK_EQ(index, 0);

  // If this is the first write to the header, store its original size for later
  // persistence.
  if (!previous_header_size_in_storage_) {
    previous_header_size_in_storage_ = head_->size();
  }

  size_t u_offset = base::checked_cast<size_t>(offset);
  size_t u_buf_len = base::checked_cast<size_t>(buf_len);
  if (offset == 0 && truncate) {
    head_->SetCapacity(buf_len);
    if (buf_len) {
      head_->span().copy_from(buf->first(u_buf_len));
    }
  } else {
    const size_t original_size = head_->size();
    const size_t buffer_size =
        truncate ? u_offset + u_buf_len
                 : std::max(u_offset + u_buf_len, original_size);
    head_->SetCapacity(base::checked_cast<int>(buffer_size));

    // Fill any gap with zeros if writing beyond current size.
    const size_t fill_size =
        u_offset <= original_size ? 0 : u_offset - original_size;
    if (fill_size > 0) {
      std::ranges::fill(head_->span().subspan(original_size, fill_size), 0);
    }
    // Copy new data into the buffer.
    if (buf) {
      head_->span().subspan(u_offset).copy_prefix_from(buf->first(u_buf_len));
    }
  }
  return buf_len;
}

int SqlEntryImpl::WriteDataInternal(int64_t offset,
                                    IOBuffer* buf,
                                    int buf_len,
                                    CompletionOnceCallback callback,
                                    bool truncate,
                                    bool sparse_write) {
  if (!backend_) {
    return net::ERR_FAILED;
  }
  read_cache_buffer_.reset();
  read_cache_buffer_offset_ = -1;

  // Ignore zero-length writes that do not change the file size.
  if (buf_len == 0) {
    if (truncate ? (offset == body_end_) : (offset <= body_end_)) {
      return buf_len;
    }
  }

  // Try to buffer the write if it's a sequential append.
  if (offset == body_end_ && !sparse_write) {
    const int entry_limit =
        net::features::kSqlDiskCacheMaxWriteBufferSizePerEntry.Get();
    if (write_buffer_.size + buf_len > entry_limit) {
      FlushBuffer(/*force_flush_for_creation=*/false);
    }
    if (buf_len <= entry_limit && backend_->write_buffer_monitor().Allocate(
                                      buf_len, write_buffer_reservation_)) {
      if (write_buffer_.buffers.empty()) {
        write_buffer_.offset = offset;
      } else {
        // Ensure continuity.
        CHECK_EQ(
            write_buffer_.offset + static_cast<int64_t>(write_buffer_.size),
            offset);
      }
      write_buffer_.buffers.push_back(base::MakeRefCounted<net::VectorIOBuffer>(
          buf->first(static_cast<size_t>(buf_len))));
      write_buffer_.size += buf_len;

      body_end_ += buf_len;

      return buf_len;
    }
  }

  FlushBuffer(/*force_flush_for_creation=*/false);

  // The end of the current write must not cause an integer overflow. Callers
  // are responsible for validating this, so we CHECK it here.
  const int64_t end_offset = base::CheckAdd(offset, buf_len).ValueOrDie();
  // Calculate the new size of the body (stream 1). If `truncate` is true, the
  // new size is the end of the current write. Otherwise, the body is extended
  // if the write goes past the current end.
  const int64_t new_body_end =
      truncate ? end_offset : std::max(end_offset, body_end_);

  if (!sparse_write && new_body_end > backend_->MaxFileSize()) {
    return net::ERR_FAILED;
  }

  const auto old_body_end = body_end_;
  body_end_ = new_body_end;

  if (db_handle_->IsInitialState()) {
    // WriteEntryData creates an entry in the DB with `last_used_` set.
    // Unless `last_used_` is updated in the future, there is no need to write
    // to the DB again.
    last_used_modified_ = false;
  }

  return backend_->WriteEntryData(
      key_, db_handle_, old_body_end, body_end_,
      EntryWriteBuffer(buf, buf_len, offset), truncate, last_used_,
      /*copy_buffer_for_optimistic_write=*/true, std::move(callback));
}

void SqlEntryImpl::FlushBuffer(bool force_flush_for_creation) {
  CHECK(backend_);
  EntryWriteBuffer buffer;
  SqlWriteBufferMemoryMonitor::ScopedReservation reservation;

  if (!TakeWriteBuffer(buffer, reservation)) {
    if (force_flush_for_creation && db_handle_->IsInitialState()) {
      // Even if the write buffer is empty, if `force_flush_for_creation` is
      // true and `db_handle_` is in the initial state, create an entry in the
      // DB using WriteEntryDataAndMetadata.
      //
      // WriteEntryDataAndMetadata creates an entry in the DB with `last_used_`
      // set. Unless `last_used_` is updated in the future, there is no need to
      // write to the DB again.
      last_used_modified_ = false;
      backend_->WriteEntryDataAndMetadata(
          key_, db_handle_, /*old_body_end=*/std::nullopt, 0,
          EntryWriteBuffer(), last_used_,
          /*new_hints=*/std::nullopt, /*head_buffer=*/nullptr,
          /*header_size_delta*/ 0, base::DoNothing());
    }
    return;
  }
  if (db_handle_->IsInitialState()) {
    // WriteEntryData creates an entry in the DB with `last_used_` set.
    // Unless `last_used_` is updated in the future, there is no need to write
    // to the DB again.
    last_used_modified_ = false;
  }

  const int64_t offset = buffer.offset;

  // We pass copy_buffer_for_optimistic_write=false because we are passing
  // ownership of the write buffer to the backend.
  backend_->WriteEntryData(
      key_, db_handle_, /*old_body_end=*/offset,
      /*body_end=*/body_end_, std::move(buffer),
      /*truncate=*/false, last_used_,
      /*copy_buffer_for_optimistic_write=*/false,
      base::DoNothingWithBoundArgs(std::move(reservation)));
}

bool SqlEntryImpl::TakeWriteBuffer(
    EntryWriteBuffer& buffer,
    SqlWriteBufferMemoryMonitor::ScopedReservation& reservation) {
  if (write_buffer_.buffers.empty()) {
    return false;
  }

  buffer = std::move(write_buffer_);
  write_buffer_ = EntryWriteBuffer();
  reservation = std::move(write_buffer_reservation_);
  return true;
}

int SqlEntryImpl::ReadSparseData(int64_t offset,
                                 IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  UpdateLastUsed();
  if (buf_len == 0) {
    return net::OK;
  }
  if (!buf || buf_len < 0 || offset < 0) {
    return net::ERR_INVALID_ARGUMENT;
  }
  return ReadDataInternal(offset, buf, buf_len, std::move(callback),
                          /*sparse_reading=*/true);
}

int SqlEntryImpl::WriteSparseData(int64_t offset,
                                  IOBuffer* buf,
                                  int buf_len,
                                  CompletionOnceCallback callback) {
  UpdateLastUsed();
  if ((offset < 0) || (buf_len < 0) || (!buf && buf_len > 0) ||
      !base::CheckAdd(offset, buf_len).IsValid()) {
    return net::ERR_INVALID_ARGUMENT;
  }
  return WriteDataInternal(offset, buf, buf_len, std::move(callback),
                           /*truncate=*/false, /*sparse_write=*/true);
}

RangeResult SqlEntryImpl::GetAvailableRange(int64_t offset,
                                            int len,
                                            RangeResultCallback callback) {
  if (!backend_) {
    return RangeResult(net::ERR_FAILED);
  }
  if (offset < 0 || len < 0) {
    return RangeResult(net::ERR_INVALID_ARGUMENT);
  }

  // Since the processing of GetAvailableRange is implemented only on the DB,
  // flush all write buffers.
  FlushBuffer(/*force_flush_for_creation=*/true);

  return backend_->GetEntryAvailableRange(key_, db_handle_, offset, len,
                                          std::move(callback));
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

void SqlEntryImpl::SetEntryInMemoryData(uint8_t data) {
  const MemoryEntryDataHints hints(data);
  new_hints_ = hints;
  if (backend_) {
    backend_->SetEntryDataHints(key_, db_handle_, hints);
  }
}

void SqlEntryImpl::SetLastUsedTimeForTest(base::Time time) {
  last_used_ = time;
  last_used_modified_ = true;
}

void SqlEntryImpl::UpdateLastUsed() {
  last_used_ = base::Time::Now();
  last_used_modified_ = true;
}

bool SqlEntryImpl::doomed() const {
  return db_handle_->doomed();
}

}  // namespace disk_cache
