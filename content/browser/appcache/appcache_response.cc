// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_response.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_disk_cache.h"
#include "content/browser/appcache/appcache_storage.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/common/storage_histograms.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

namespace {

using OnceCompletionCallback = base::OnceCallback<void(int)>;

// Disk cache entry data indices.
enum { kResponseInfoIndex, kResponseContentIndex, kResponseMetadataIndex };

// An IOBuffer that wraps a pickle's data.
class WrappedPickleIOBuffer : public net::WrappedIOBuffer {
 public:
  explicit WrappedPickleIOBuffer(std::unique_ptr<const base::Pickle> pickle)
      : net::WrappedIOBuffer(reinterpret_cast<const char*>(pickle->data())),
        pickle_(std::move(pickle)) {
    DCHECK(pickle_->data());
  }

 private:
  ~WrappedPickleIOBuffer() override = default;

  const std::unique_ptr<const base::Pickle> pickle_;
};

}  // anon namespace


// AppCacheResponseInfo ----------------------------------------------

AppCacheResponseInfo::AppCacheResponseInfo(
    base::WeakPtr<AppCacheStorage> storage,
    const GURL& manifest_url,
    int64_t response_id,
    std::unique_ptr<net::HttpResponseInfo> http_info,
    int64_t response_data_size)
    : manifest_url_(manifest_url),
      response_id_(response_id),
      http_response_info_(std::move(http_info)),
      response_data_size_(response_data_size),
      storage_(std::move(storage)) {
  DCHECK(http_response_info_);
  DCHECK(response_id != blink::mojom::kAppCacheNoResponseId);
  storage_->working_set()->AddResponseInfo(this);
}

AppCacheResponseInfo::~AppCacheResponseInfo() {
  if (storage_)
    storage_->working_set()->RemoveResponseInfo(this);
}

// HttpResponseInfoIOBuffer ------------------------------------------

HttpResponseInfoIOBuffer::HttpResponseInfoIOBuffer()
    : response_data_size(kUnknownResponseDataSize) {}

HttpResponseInfoIOBuffer::HttpResponseInfoIOBuffer(
    std::unique_ptr<net::HttpResponseInfo> info)
    : http_info(std::move(info)),
      response_data_size(kUnknownResponseDataSize) {}

HttpResponseInfoIOBuffer::~HttpResponseInfoIOBuffer() = default;

// AppCacheResponseIO ----------------------------------------------

AppCacheResponseIO::AppCacheResponseIO(
    int64_t response_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : response_id_(response_id),
      disk_cache_(std::move(disk_cache)),
      entry_(nullptr),
      buffer_len_(0) {}

AppCacheResponseIO::~AppCacheResponseIO() {
  if (entry_)
    entry_->Close();
}

void AppCacheResponseIO::ScheduleIOCompletionCallback(int result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AppCacheResponseIO::OnIOComplete, GetWeakPtr(), result));
}

void AppCacheResponseIO::InvokeUserCompletionCallback(int result) {
  // Clear the user callback and buffers prior to invoking the callback
  // so the caller can schedule additional operations in the callback.
  buffer_ = nullptr;
  info_buffer_ = nullptr;
  OnceCompletionCallback cb = std::move(callback_);
  callback_.Reset();
  std::move(cb).Run(result);
}

void AppCacheResponseIO::ReadRaw(int index, int offset,
                                 net::IOBuffer* buf, int buf_len) {
  DCHECK(entry_);
  int rv = entry_->Read(
      index, offset, buf, buf_len,
      base::BindOnce(&AppCacheResponseIO::OnRawIOComplete, GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    ScheduleIOCompletionCallback(rv);
}

void AppCacheResponseIO::WriteRaw(int index, int offset,
                                 net::IOBuffer* buf, int buf_len) {
  DCHECK(entry_);
  int rv = entry_->Write(
      index, offset, buf, buf_len,
      base::BindOnce(&AppCacheResponseIO::OnRawIOComplete, GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    ScheduleIOCompletionCallback(rv);
}

void AppCacheResponseIO::OnRawIOComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  OnIOComplete(result);
}

void AppCacheResponseIO::OpenEntryIfNeeded() {
  int rv;
  AppCacheDiskCacheEntry** entry_ptr = nullptr;
  if (entry_) {
    rv = net::OK;
  } else if (!disk_cache_) {
    rv = net::ERR_FAILED;
  } else {
    entry_ptr = new AppCacheDiskCacheEntry*;
    rv = disk_cache_->OpenEntry(
        response_id_, entry_ptr,
        base::BindOnce(&AppCacheResponseIO::OpenEntryCallback, GetWeakPtr(),
                       entry_ptr));
  }

  if (rv != net::ERR_IO_PENDING)
    OpenEntryCallback(GetWeakPtr(), entry_ptr, rv);
}

// static
void AppCacheResponseIO::OpenEntryCallback(
    base::WeakPtr<AppCacheResponseIO> response,
    AppCacheDiskCacheEntry** entry,
    int rv) {
  if (!response) {
    delete entry;
    return;
  }

  DCHECK(response->info_buffer_.get() || response->buffer_.get());

  if (!response->entry_ && rv == net::OK) {
    DCHECK(entry);
    response->entry_ = *entry;
  }
  delete entry;
  response->OnOpenEntryComplete();
}


// AppCacheResponseReader ----------------------------------------------

AppCacheResponseReader::AppCacheResponseReader(
    int64_t response_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseIO(response_id, std::move(disk_cache)),
      range_offset_(0),
      range_length_(std::numeric_limits<int32_t>::max()),
      read_position_(0),
      reading_metadata_size_(0) {}

AppCacheResponseReader::~AppCacheResponseReader() = default;

void AppCacheResponseReader::ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                                      OnceCompletionCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsReadPending());
  DCHECK(info_buf);
  DCHECK(!info_buf->http_info.get());
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());

  info_buffer_ = info_buf;
  callback_ = std::move(callback);  // cleared on completion
  OpenEntryIfNeeded();
}

void AppCacheResponseReader::ContinueReadInfo() {
  int size = entry_->GetSize(kResponseInfoIndex);
  if (size <= 0) {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  buffer_ = base::MakeRefCounted<net::IOBuffer>(size);
  ReadRaw(kResponseInfoIndex, 0, buffer_.get(), size);
}

void AppCacheResponseReader::ReadData(net::IOBuffer* buf,
                                      int buf_len,
                                      OnceCompletionCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsReadPending());
  DCHECK(buf);
  DCHECK(buf_len >= 0);
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());

  buffer_ = buf;
  buffer_len_ = buf_len;
  callback_ = std::move(callback);  // cleared on completion
  OpenEntryIfNeeded();
}

void AppCacheResponseReader::ContinueReadData() {
  // Since every read reads at most (range_length_ - read_position_) bytes,
  // read_position_ can never become larger than range_length_.
  DCHECK_GE(range_length_, read_position_);
  if (range_length_ - read_position_ < buffer_len_)
    buffer_len_ = range_length_ - read_position_;
  ReadRaw(kResponseContentIndex,
          range_offset_ + read_position_,
          buffer_.get(),
          buffer_len_);
}

void AppCacheResponseReader::SetReadRange(int offset, int length) {
  DCHECK(!IsReadPending() && !read_position_);
  range_offset_ = offset;
  range_length_ = length;
}

void AppCacheResponseReader::OnIOComplete(int result) {
  if (result >= 0) {
    if (reading_metadata_size_) {
      DCHECK(reading_metadata_size_ == result);
      DCHECK(info_buffer_->http_info->metadata);
      reading_metadata_size_ = 0;
    } else if (info_buffer_.get()) {
      // Deserialize the http info structure, ensuring we got headers.
      base::Pickle pickle(buffer_->data(), result);
      auto info = std::make_unique<net::HttpResponseInfo>();
      bool response_truncated = false;
      if (!info->InitFromPickle(pickle, &response_truncated) ||
          !info->headers.get()) {
        InvokeUserCompletionCallback(net::ERR_FAILED);
        return;
      }
      DCHECK(!response_truncated);
      info_buffer_->http_info.reset(info.release());

      // Also return the size of the response body
      DCHECK(entry_);
      info_buffer_->response_data_size =
          entry_->GetSize(kResponseContentIndex);

      int64_t metadata_size = entry_->GetSize(kResponseMetadataIndex);
      if (metadata_size > 0) {
        reading_metadata_size_ = metadata_size;
        info_buffer_->http_info->metadata =
            base::MakeRefCounted<net::IOBufferWithSize>(
                base::checked_cast<size_t>(metadata_size));
        ReadRaw(kResponseMetadataIndex, 0,
                info_buffer_->http_info->metadata.get(), metadata_size);
        return;
      }
    } else {
      read_position_ += result;
    }
  }
  if (result > 0 && disk_cache_)
    storage::RecordBytesRead(disk_cache_->uma_name(), result);
  InvokeUserCompletionCallback(result);
  // Note: |this| may have been deleted by the completion callback.
}

void AppCacheResponseReader::OnOpenEntryComplete() {
  if (!entry_)  {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }
  if (info_buffer_.get())
    ContinueReadInfo();
  else
    ContinueReadData();
}

base::WeakPtr<AppCacheResponseIO> AppCacheResponseReader::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// AppCacheResponseWriter ----------------------------------------------

AppCacheResponseWriter::AppCacheResponseWriter(
    int64_t response_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseIO(response_id, std::move(disk_cache)),
      info_size_(0),
      write_position_(0),
      write_amount_(0),
      creation_phase_(INITIAL_ATTEMPT) {}

AppCacheResponseWriter::~AppCacheResponseWriter() = default;

void AppCacheResponseWriter::WriteInfo(HttpResponseInfoIOBuffer* info_buf,
                                       OnceCompletionCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsWritePending());
  DCHECK(info_buf);
  DCHECK(info_buf->http_info.get());
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());
  DCHECK(info_buf->http_info->headers.get());

  info_buffer_ = info_buf;
  callback_ = std::move(callback);  // cleared on completion
  CreateEntryIfNeededAndContinue();
}

void AppCacheResponseWriter::ContinueWriteInfo() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }

  const bool kSkipTransientHeaders = true;
  const bool kTruncated = false;
  std::unique_ptr<base::Pickle> pickle = std::make_unique<base::Pickle>();
  info_buffer_->http_info->Persist(pickle.get(), kSkipTransientHeaders,
                                   kTruncated);
  write_amount_ = static_cast<int>(pickle->size());
  buffer_ = base::MakeRefCounted<WrappedPickleIOBuffer>(std::move(pickle));
  WriteRaw(kResponseInfoIndex, 0, buffer_.get(), write_amount_);
}

void AppCacheResponseWriter::WriteData(net::IOBuffer* buf,
                                       int buf_len,
                                       OnceCompletionCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsWritePending());
  DCHECK(buf);
  DCHECK(buf_len >= 0);
  DCHECK(!buffer_.get());
  DCHECK(!info_buffer_.get());

  buffer_ = buf;
  write_amount_ = buf_len;
  callback_ = std::move(callback);  // cleared on completion
  CreateEntryIfNeededAndContinue();
}

void AppCacheResponseWriter::ContinueWriteData() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }
  WriteRaw(
      kResponseContentIndex, write_position_, buffer_.get(), write_amount_);
}

void AppCacheResponseWriter::OnIOComplete(int result) {
  if (result >= 0) {
    DCHECK(write_amount_ == result);
    if (!info_buffer_.get())
      write_position_ += result;
    else
      info_size_ = result;
  }
  if (result > 0 && disk_cache_)
    storage::RecordBytesWritten(disk_cache_->uma_name(), result);
  InvokeUserCompletionCallback(result);
  // Note: |this| may have been deleted by the completion callback.
}

void AppCacheResponseWriter::CreateEntryIfNeededAndContinue() {
  int rv;
  AppCacheDiskCacheEntry** entry_ptr = nullptr;
  if (entry_) {
    creation_phase_ = NO_ATTEMPT;
    rv = net::OK;
  } else if (!disk_cache_) {
    creation_phase_ = NO_ATTEMPT;
    rv = net::ERR_FAILED;
  } else {
    creation_phase_ = INITIAL_ATTEMPT;
    entry_ptr = new AppCacheDiskCacheEntry*;
    rv = disk_cache_->CreateEntry(
        response_id_, entry_ptr,
        base::BindOnce(&AppCacheResponseWriter::OnCreateEntryComplete,
                       weak_factory_.GetWeakPtr(), entry_ptr));
  }
  if (rv != net::ERR_IO_PENDING)
    OnCreateEntryComplete(weak_factory_.GetWeakPtr(), entry_ptr, rv);
}

// static
void AppCacheResponseWriter::OnCreateEntryComplete(
    base::WeakPtr<AppCacheResponseWriter> writer,
    AppCacheDiskCacheEntry** entry,
    int rv) {
  if (!writer) {
    if (entry) {
      delete entry;
    }
    return;
  }

  DCHECK(writer->info_buffer_.get() || writer->buffer_.get());

  if (!writer->disk_cache_) {
    if (entry) {
      delete entry;
    }
    writer->ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  } else if (writer->creation_phase_ == INITIAL_ATTEMPT) {
    if (rv != net::OK) {
      // We may try to overwrite existing entries.
      delete entry;
      writer->creation_phase_ = DOOM_EXISTING;
      rv = writer->disk_cache_->DoomEntry(
          writer->response_id_,
          base::BindOnce(&AppCacheResponseWriter::OnCreateEntryComplete, writer,
                         nullptr));
      if (rv != net::ERR_IO_PENDING)
        OnCreateEntryComplete(writer, nullptr, rv);
      return;
    }
  } else if (writer->creation_phase_ == DOOM_EXISTING) {
    DCHECK_EQ(nullptr, entry);
    writer->creation_phase_ = SECOND_ATTEMPT;
    AppCacheDiskCacheEntry** entry_ptr = new AppCacheDiskCacheEntry*;
    rv = writer->disk_cache_->CreateEntry(
        writer->response_id_, entry_ptr,
        base::BindOnce(&AppCacheResponseWriter::OnCreateEntryComplete, writer,
                       entry_ptr));
    if (rv != net::ERR_IO_PENDING)
      OnCreateEntryComplete(writer, entry_ptr, rv);
    return;
  }

  if (!writer->entry_ && rv == net::OK) {
    DCHECK(entry);
    writer->entry_ = *entry;
  }

  delete entry;

  if (writer->info_buffer_.get())
    writer->ContinueWriteInfo();
  else
    writer->ContinueWriteData();
}

base::WeakPtr<AppCacheResponseIO> AppCacheResponseWriter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// AppCacheResponseMetadataWriter ----------------------------------------------

AppCacheResponseMetadataWriter::AppCacheResponseMetadataWriter(
    int64_t response_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseIO(response_id, std::move(disk_cache)),
      write_amount_(0) {}

AppCacheResponseMetadataWriter::~AppCacheResponseMetadataWriter() = default;

void AppCacheResponseMetadataWriter::WriteMetadata(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(!IsIOPending());
  DCHECK(buf);
  DCHECK(buf_len >= 0);
  DCHECK(!buffer_.get());

  buffer_ = buf;
  write_amount_ = buf_len;
  callback_ = std::move(callback);  // cleared on completion
  OpenEntryIfNeeded();
}

void AppCacheResponseMetadataWriter::OnOpenEntryComplete() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }
  WriteRaw(kResponseMetadataIndex, 0, buffer_.get(), write_amount_);
}

void AppCacheResponseMetadataWriter::OnIOComplete(int result) {
  DCHECK(result < 0 || write_amount_ == result);
  if (result > 0 && disk_cache_)
    storage::RecordBytesWritten(disk_cache_->uma_name(), result);
  InvokeUserCompletionCallback(result);
  // Note: |this| may have been deleted by the completion callback.
}

base::WeakPtr<AppCacheResponseIO> AppCacheResponseMetadataWriter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
