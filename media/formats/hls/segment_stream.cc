// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/segment_stream.h"

namespace media::hls {

SegmentStream::~SegmentStream() = default;
SegmentStream::SegmentStream(scoped_refptr<MediaPlaylist> playlist,
                             bool seekable)
    : seekable_(seekable),
      next_segment_start_(base::Seconds(0)),
      active_playlist_(std::move(playlist)) {
  for (const auto& segment : active_playlist_->GetSegments()) {
    segments_.push(segment);
    highest_sequence_number_ =
        std::max(highest_sequence_number_, segment->GetMediaSequenceNumber());
  }
}

SegmentInfo SegmentStream::GetNextSegment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!segments_.empty());
  auto segment = std::move(segments_.front());
  segments_.pop();
  base::TimeDelta previous_segment_start = next_segment_start_;
  next_segment_start_ += segment->GetDuration();
  return std::make_tuple(segment, previous_segment_start, next_segment_start_);
}

bool SegmentStream::Seek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!seekable_) {
    return false;
  }

  segments_ = {};
  base::TimeDelta start_time = base::Seconds(0);
  for (const auto& segment : active_playlist_->GetSegments()) {
    base::TimeDelta end_time = start_time + segment->GetDuration();
    if (seek_time < end_time) {
      segments_.push(segment);
    }
    if (segments_.size() == 1) {
      // Set the end time for the virtually-popped-sequence to be the current
      // start time.
      next_segment_start_ = start_time;
    }
    start_time = end_time;
  }

  return !segments_.empty();
}

void SegmentStream::SetNewPlaylist(scoped_refptr<MediaPlaylist> playlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_playlist_ = std::move(playlist);

  const auto& segments = active_playlist_->GetSegments();
  if (segments.empty()) {
    // No new segments.
    // TODO(crbug/1266991): Should this be an error? I do not know if this ever
    // happens in the wild. I can imagine that it does, hence not raising an
    // error here for now. The spec doesn't seem to clear it up.
    return;
  }

  // VOD playlists are very easy. because we know all the sequence numbers are
  // the same, we can just use the seek functionality to reset the queue.
  if (seekable_) {
    Seek(next_segment_start_);
    return;
  }

  // We need to get the sequence number for what should be the first new thing
  // in the queue. If it was exhausted, that thing is the highest seen so far.
  // if there is still data, we should use the first instead.
  types::DecimalInteger next_sequence_number = 0;
  if (Exhausted()) {
    next_sequence_number = highest_sequence_number_;
  } else {
    next_sequence_number = segments_.front()->GetMediaSequenceNumber() - 1;
  }

  // Then we clear the queue and add everything that came after the segment
  // which was most recently served.
  segments_ = {};
  for (const auto& segment : segments) {
    auto seg_seq_num = segment->GetMediaSequenceNumber();
    if (seg_seq_num > next_sequence_number) {
      segments_.push(segment);
    }
    if (seg_seq_num > highest_sequence_number_) {
      highest_sequence_number_ = seg_seq_num;
    }
  }
}

base::TimeDelta SegmentStream::GetMaxDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_playlist_->GetTargetDuration();
}

bool SegmentStream::Exhausted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return segments_.empty();
}

base::TimeDelta SegmentStream::NextSegmentStartTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return next_segment_start_;
}

bool SegmentStream::PlaylistHasSegments() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !active_playlist_->GetSegments().empty();
}

size_t SegmentStream::QueueSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return segments_.size();
}

void SegmentStream::ResetExpectingFutureManifest(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  segments_ = {};
  next_segment_start_ = time;
}

}  // namespace media::hls
