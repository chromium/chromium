// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/hls_data_source_provider.h"
#include "base/trace_event/trace_event.h"

namespace media {

HlsDataSourceProvider::~HlsDataSourceProvider() = default;

void HlsDataSourceProvider::ReadFromUrl(UrlDataSegment segment,
                                        ReadCb callback) {
  base::queue<UrlDataSegment> segments({segment});
  ReadFromCombinedUrlQueue(std::move(segments), std::move(callback));
}

HlsDataSourceStream::HlsDataSourceStream(
    StreamId stream_id,
    HlsDataSourceProvider::SegmentQueue segments,
    base::OnceClosure on_destructed_cb)
    : stream_id_(stream_id),
      segments_(std::move(segments)),
      requires_next_data_source_(true),
      on_destructed_cb_(std::move(on_destructed_cb)) {}

HlsDataSourceStream::~HlsDataSourceStream() {
  CHECK(!stream_locked_);
  std::move(on_destructed_cb_).Run();
}

std::string_view HlsDataSourceStream::AsString() const {
  return std::string_view(reinterpret_cast<const char*>(buffer_.data()),
                          buffer_.size());
}

bool HlsDataSourceStream::RequiresNextDataSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return requires_next_data_source_;
}

GURL HlsDataSourceStream::GetNextSegmentURI() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::get<0>(GetNextSegmentURIAndCacheStatus());
}

std::pair<GURL, bool> HlsDataSourceStream::GetNextSegmentURIAndCacheStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(requires_next_data_source_);
  CHECK(!segments_.empty());
  const auto& first = segments_.front();
  if (first.range) {
    read_position_ = first.range->GetOffset();
    max_read_position_ = first.range->GetEnd();
  } else {
    read_position_ = 0;
    max_read_position_ = std::nullopt;
  }
  GURL new_url = std::move(first.uri);
  bool bypass_cache = first.bypass_cache;
  segments_.pop();
  requires_next_data_source_ = false;
  return std::make_pair(new_url, bypass_cache);
}

bool HlsDataSourceStream::CanReadMore() const {
  if (requires_next_data_source_) {
    return true;
  }
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
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::Read", this, "minimum space",
                                    ensure_minimum_space);
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
  TRACE_EVENT_NESTABLE_ASYNC_END2("media", "HLS::Read", this, "bytes",
                                  read_size, "eos", end_of_stream);
  CHECK(stream_locked_);
  write_index_ += read_size;
  read_position_ += read_size;

  // When `max_read_position_` is present and `read_size` matches, the end of
  // stream flag will be incorrect.
  if (max_read_position_.has_value() && *max_read_position_ == read_position_) {
    end_of_stream = true;
  }

  if (end_of_stream) {
    reached_end_of_stream_ = segments_.empty();
    requires_next_data_source_ = !reached_end_of_stream_;
    buffer_.resize(write_index_);
  }

  stream_locked_ = false;
}

}  // namespace media
