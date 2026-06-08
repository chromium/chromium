// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider.h"

#include "base/strings/string_view_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace media {

namespace {
perfetto::NamedTrack GetTracingTrack(const HlsDataSourceStream* stream) {
  return perfetto::NamedTrack::FromPointer("media::HlsDataSourceStream",
                                           stream);
}
}  // namespace

HlsDataSourceProvider::~HlsDataSourceProvider() = default;

void HlsDataSourceProvider::ReadFromUrlForTesting(UrlDataSegment segment,
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
  return base::as_string_view(buffer_);
}

bool HlsDataSourceStream::RequiresNextDataSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return requires_next_data_source_;
}

GURL HlsDataSourceStream::GetNextSegmentURI() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::get<0>(GetNextSegmentURIAndCacheStatus());
}

std::tuple<GURL,
           DataSource::CacheMode,
           DataSource::RangeMode,
           DataSource::EncodingMode>
HlsDataSourceStream::GetNextSegmentURIAndCacheStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(requires_next_data_source_);
  CHECK(!segments_.empty());
  GURL new_url;
  DataSource::CacheMode cache_mode;
  DataSource::EncodingMode encoding_mode;
  auto range_mode = DataSource::RangeMode::kFullRequest;
  {
    const auto& first = segments_.front();
    if (first.range) {
      range_mode = DataSource::RangeMode::kRangeRequest;
      read_position_ = first.range->GetOffset();
      max_read_position_ = first.range->GetEnd();
    } else {
      read_position_ = 0;
      max_read_position_ = std::nullopt;
    }
    new_url = std::move(first.uri);
    cache_mode = first.cache_mode;
    encoding_mode = first.encoding_mode;
  }
  segments_.pop();
  requires_next_data_source_ = false;
  return std::make_tuple(std::move(new_url), cache_mode, range_mode,
                         encoding_mode);
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

base::span<uint8_t> HlsDataSourceStream::LockStreamForWriting(
    size_t ensure_minimum_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN("media", "HLS::Read", GetTracingTrack(this),
                    "minimum space", ensure_minimum_space);
  CHECK(!stream_locked_);
  stream_locked_ = true;
  CHECK_GE(buffer_.size(), write_index_);
  size_t remaining_bytes = buffer_.size() - write_index_;
  if (ensure_minimum_space > remaining_bytes) {
    buffer_.resize(write_index_ + ensure_minimum_space);
  }
  return base::span(buffer_).subspan(write_index_, ensure_minimum_space);
}

void HlsDataSourceStream::UnlockStreamPostWrite(size_t read_size,
                                                bool end_of_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_END("media", GetTracingTrack(this), "bytes", read_size, "eos",
                  end_of_stream);
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
