// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/live_webm_muxer_delegate.h"

#include <string_view>

#include "base/numerics/ostream_operators.h"

namespace media {

LiveWebmMuxerDelegate::LiveWebmMuxerDelegate(
    Muxer::WriteDataCB write_data_callback)
    : write_data_callback_(std::move(write_data_callback)) {
  DCHECK(!write_data_callback_.is_null());
}

LiveWebmMuxerDelegate::~LiveWebmMuxerDelegate() = default;

void LiveWebmMuxerDelegate::InitSegment(mkvmuxer::Segment* segment) {
  segment->Init(this);
  segment->set_mode(mkvmuxer::Segment::kLive);
  segment->OutputCues(false);
}

mkvmuxer::int64 LiveWebmMuxerDelegate::Position() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return position_.ValueOrDie();
}

mkvmuxer::int32 LiveWebmMuxerDelegate::Position(mkvmuxer::int64 position) {
  // The stream is not Seekable() so indicate we cannot set the position.
  return -1;
}

bool LiveWebmMuxerDelegate::Seekable() const {
  return false;
}

void LiveWebmMuxerDelegate::ElementStartNotify(mkvmuxer::uint64 element_id,
                                               mkvmuxer::int64 position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This method gets pinged before items are sent to
  // |WebmMuxerDelegate::Write()|.
  DCHECK_GE(position, position_.ValueOrDefault(0))
      << "Can't go back in a live WebM stream.";
}

mkvmuxer::int32 LiveWebmMuxerDelegate::DoWrite(const void* buf,
                                               mkvmuxer::uint32 len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  write_data_callback_.Run(
      // SAFETY: buf is a pointer that points to exactly len length.
      UNSAFE_BUFFERS(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(buf), len)));
  return 0;
}

}  // namespace media
