// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/memory_webm_muxer_delegate.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/logging.h"

namespace media {

MemoryWebmMuxerDelegate::MemoryWebmMuxerDelegate(
    Muxer::WriteDataCB write_data_callback,
    base::OnceClosure started_callback)
    : write_data_callback_(std::move(write_data_callback)),
      started_callback_(std::move(started_callback)) {
  DCHECK(write_data_callback_);
}

MemoryWebmMuxerDelegate::~MemoryWebmMuxerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(write_data_callback_).Run(buffer_);
}

void MemoryWebmMuxerDelegate::InitSegment(mkvmuxer::Segment* segment) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  segment->Init(this);
  segment->set_mode(mkvmuxer::Segment::kFile);
  // According to the Matroska specs [1], it is possible to seek without the
  // Cues elements, but it would be much more difficult because a video player
  // would have to "hunt and peck through the file looking for the correct
  // timestamp". So the use of Cues are recommended, because they allow for
  // optimized seeking to absolute timestamps within the Segment.
  //
  // [1]: https://www.matroska.org/technical/cues.html.
  segment->OutputCues(true);
}

mkvmuxer::int64 MemoryWebmMuxerDelegate::Position() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return position_.ValueOrDie();
}

mkvmuxer::int32 MemoryWebmMuxerDelegate::Position(mkvmuxer::int64 position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!seeking_allowed_) {
    return -1;
  }
  position_ = position;
  return 0;
}

bool MemoryWebmMuxerDelegate::Seekable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seeking_allowed_;
}

void MemoryWebmMuxerDelegate::ElementStartNotify(mkvmuxer::uint64 element_id,
                                                 mkvmuxer::int64 position) {}

void MemoryWebmMuxerDelegate::FlushAndDisableSeeking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  seeking_allowed_ = false;

  // If we haven't received any data yet, there's nothing to flush.
  if (started_callback_) {
    return;
  }

  write_data_callback_.Run(buffer_);
  buffer_.clear();
}

mkvmuxer::int32 MemoryWebmMuxerDelegate::DoWrite(
    base::span<const uint8_t> buf) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (started_callback_) {
    std::move(started_callback_).Run();
  }

  // Once seeking has been disabled, we must always insert at the end.
  if (!seeking_allowed_) {
    buffer_.insert(buffer_.end(), std::begin(buf), std::end(buf));
  } else {
    size_t pos = position_.ValueOrDie<size_t>();
    if (pos == buffer_.size()) {
      buffer_.insert(buffer_.end(), std::begin(buf), std::end(buf));
    } else {
      auto dest_span = base::span<uint8_t>(buffer_).subspan(pos, buf.size());
      dest_span.copy_from_nonoverlapping(buf);
    }
  }
  return 0;
}

}  // namespace media
