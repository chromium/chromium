// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/chunked_upload_data_stream.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

ChunkedUploadDataStream::Writer::~Writer() = default;

bool ChunkedUploadDataStream::Writer::AppendData(base::span<const uint8_t> data,
                                                 bool is_done) {
  if (!upload_data_stream_)
    return false;
  upload_data_stream_->AppendData(data, is_done);
  return true;
}

ChunkedUploadDataStream::Writer::Writer(
    base::WeakPtr<ChunkedUploadDataStream> upload_data_stream)
    : upload_data_stream_(upload_data_stream) {}

ChunkedUploadDataStream::ChunkedUploadDataStream(int64_t identifier,
                                                 bool has_null_source)
    : UploadDataStream(/*is_chunked=*/true, has_null_source, identifier) {}

ChunkedUploadDataStream::~ChunkedUploadDataStream() = default;

std::unique_ptr<ChunkedUploadDataStream::Writer>
ChunkedUploadDataStream::CreateWriter() {
  return base::WrapUnique(new Writer(weak_factory_.GetWeakPtr()));
}

void ChunkedUploadDataStream::AppendData(base::span<const uint8_t> data,
                                         bool is_done) {
  DCHECK(!all_data_appended_);
  DCHECK(!data.empty() || is_done);
  if (!data.empty()) {
    upload_data_.push_back(
        std::make_unique<std::vector<uint8_t>>(data.begin(), data.end()));
  }
  all_data_appended_ = is_done;

  if (!read_buffer_.get())
    return;

  int result = ReadChunk(read_buffer_.get(), read_buffer_len_);
  // Shouldn't get an error or ERR_IO_PENDING.
  DCHECK_GE(result, 0);
  read_buffer_ = nullptr;
  read_buffer_len_ = 0;
  OnReadCompleted(result);
}

int ChunkedUploadDataStream::InitInternal(const NetLogWithSource& net_log) {
  // ResetInternal should already have been called.
  DCHECK(!read_buffer_.get());
  DCHECK_EQ(0u, read_index_);
  DCHECK_EQ(0u, read_offset_);
  return OK;
}

int ChunkedUploadDataStream::ReadInternal(IOBuffer* buf, int buf_len) {
  DCHECK_LT(0, buf_len);
  DCHECK(!read_buffer_.get());

  int result = ReadChunk(buf, buf_len);
  if (result == ERR_IO_PENDING) {
    read_buffer_ = buf;
    read_buffer_len_ = buf_len;
  }
  return result;
}

void ChunkedUploadDataStream::ResetInternal() {
  read_buffer_ = nullptr;
  read_buffer_len_ = 0;
  read_index_ = 0;
  read_offset_ = 0;
}

int ChunkedUploadDataStream::ReadChunk(IOBuffer* buf, int buf_len) {
  // Copy as much data as possible from |upload_data_| to |buf|.
  int bytes_read = 0;
  while (read_index_ < upload_data_.size() && bytes_read < buf_len) {
    base::span<const uint8_t> data(*upload_data_[read_index_].get());
    base::span<const uint8_t> bytes_to_read = data.subspan(
        read_offset_, std::min(static_cast<size_t>(buf_len - bytes_read),
                               data.size() - read_offset_));
    buf->span().subspan(bytes_read).copy_prefix_from(bytes_to_read);
    bytes_read += bytes_to_read.size();
    read_offset_ += bytes_to_read.size();
    if (read_offset_ == data.size()) {
      read_index_++;
      read_offset_ = 0;
    }
  }
  DCHECK_LE(bytes_read, buf_len);

  // If no data was written, and not all data has been appended, return
  // ERR_IO_PENDING. The read will be completed in the next call to AppendData.
  if (bytes_read == 0 && !all_data_appended_)
    return ERR_IO_PENDING;

  if (read_index_ == upload_data_.size() && all_data_appended_)
    SetIsFinalChunk();
  return bytes_read;
}

}  // namespace net
