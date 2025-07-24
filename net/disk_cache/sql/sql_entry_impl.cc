// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_entry_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/sql/sql_backend_impl.h"

namespace disk_cache {
namespace {

// Wraps a OnceCallback. If the returned callback is destroyed without being
// run, the original callback is run with `abort_result`.
// This ensures that the callback is always run, even if the operation is
// cancelled or the owner is destroyed.
template <typename ResultType>
base::OnceCallback<void(ResultType)> WrapCallbackWithAbortError(
    base::OnceCallback<void(ResultType)> callback,
    ResultType abort_result) {
  CHECK(callback);
  auto [success_cb, failure_cb] = base::SplitOnceCallback(std::move(callback));

  // The ScopedClosureRunner will run the `failure_cb` with `abort_result` if
  // it's destroyed before being released.
  auto runner = std::make_unique<base::ScopedClosureRunner>(
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(std::move(failure_cb), abort_result)));

  // The returned callback represents the "success" path.
  return base::BindOnce(
      [](std::unique_ptr<base::ScopedClosureRunner> runner,
         base::OnceCallback<void(ResultType)> cb, ResultType result) {
        // Release the runner to prevent the failure callback from running on
        // destruction.
        std::ignore = runner->Release();
        // Run the success callback with the provided result.
        std::move(cb).Run(std::move(result));
      },
      std::move(runner), std::move(success_cb));
}

}  // namespace

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
  backend_->ReadEntryData(
      key_, token_, offset, buf, buf_len, body_end_, sparse_reading,
      base::BindOnce(
          [](CompletionOnceCallback callback,
             SqlPersistentStore::IntOrError result) {
            std::move(callback).Run(result.value_or(net::ERR_FAILED));
          },
          WrapCallbackWithAbortError<int>(std::move(callback),
                                          net::ERR_ABORTED)));
  return net::ERR_IO_PENDING;
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
  // Ignore zero-length writes that do not change the file size.
  if (buf_len == 0) {
    if (truncate ? (offset == body_end_) : (offset <= body_end_)) {
      return buf_len;
    }
  }
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

  // TODO(crbug.com/422065015): Consider implementing optimistic writes, similar
  // to Simple Cache (see https://chromiumcodereview.appspot.com/13907009). This
  // would allow returning synchronously if no other operations are pending.
  backend_->WriteEntryData(
      key_, token_, old_body_end, body_end_, offset, buf, buf_len, truncate,
      base::BindOnce(
          [](CompletionOnceCallback callback, int buf_len,
             SqlPersistentStore::Error result) {
            std::move(callback).Run(result == SqlPersistentStore::Error::kOk
                                        ? buf_len
                                        : net::ERR_FAILED);
          },
          WrapCallbackWithAbortError<int>(std::move(callback),
                                          net::ERR_ABORTED),
          buf_len));
  return net::ERR_IO_PENDING;
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

  backend_->GetEntryAvailableRange(
      key_, token_, offset, len,
      WrapCallbackWithAbortError<const RangeResult&>(
          std::move(callback), RangeResult(net::ERR_ABORTED)));
  return RangeResult(net::ERR_IO_PENDING);
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
