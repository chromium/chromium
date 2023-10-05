// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider.h"

namespace media {

HlsDataSourceProvider::~HlsDataSourceProvider() = default;

HlsDataSourceStream::HlsDataSourceStream(
    StreamId stream_id,
    base::OnceClosure on_destructed_cb,
    absl::optional<hls::types::ByteRange> range)
    : stream_id_(stream_id), on_destructed_cb_(std::move(on_destructed_cb)) {
  if (range) {
    read_position_ = range->GetOffset();
    max_read_position_ = range->GetEnd();
  }
}

HlsDataSourceStream::~HlsDataSourceStream() {
  CHECK(!stream_locked_);
  std::move(on_destructed_cb_).Run();
}

std::string_view HlsDataSourceStream::AsString() const {
  return std::string_view(reinterpret_cast<const char*>(buffer_.data()),
                          buffer_.size());
}

bool HlsDataSourceStream::CanReadMore() const {
  if (reached_end_of_stream_) {
    return false;
  }
  if (!max_read_position_.has_value()) {
    return true;
  }
  return *max_read_position_ > read_position_;
}

void HlsDataSourceStream::Clear() {
  CHECK(!stream_locked_);
  buffer_.resize(0);
  write_index_ = 0;
}

uint8_t* HlsDataSourceStream::LockStreamForWriting(int ensure_minimum_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!stream_locked_);
  stream_locked_ = true;
  CHECK_GE(buffer_.size(), write_index_);
  int remaining_bytes = buffer_.size() - write_index_;
  if (ensure_minimum_space > remaining_bytes) {
    buffer_.resize(write_index_ + ensure_minimum_space);
  }
  return buffer_.data() + write_index_;
}

void HlsDataSourceStream::UnlockStreamPostWrite(int read_size,
                                                bool end_of_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream_locked_);
  write_index_ += read_size;
  read_position_ += read_size;
  if (end_of_stream) {
    reached_end_of_stream_ = true;
    buffer_.resize(write_index_);
  }
  stream_locked_ = false;
}

}  // namespace media
