// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/segment_stream.h"

namespace media::hls {

SegmentStream::SegmentIndex::SegmentIndex(const MediaSegment& segment)
    : SegmentIndex(segment.GetMediaSequenceNumber(),
                   segment.GetDiscontinuitySequenceNumber()) {}

SegmentStream::SegmentIndex::SegmentIndex(types::DecimalInteger discontinuity,
                                          types::DecimalInteger media)
    : media_sequence_(media), discontinuity_sequence_(discontinuity) {}

bool SegmentStream::SegmentIndex::operator<(
    const SegmentStream::SegmentIndex& other) const {
  return (discontinuity_sequence_ < other.discontinuity_sequence_) ||
         (discontinuity_sequence_ == other.discontinuity_sequence_ &&
          media_sequence_ < other.media_sequence_);
}

bool SegmentStream::SegmentIndex::operator<=(
    const SegmentStream::SegmentIndex& other) const {
  return *this < other || *this == other;
}

bool SegmentStream::SegmentIndex::operator==(
    const SegmentStream::SegmentIndex& other) const {
  return discontinuity_sequence_ == other.discontinuity_sequence_ &&
         media_sequence_ == other.media_sequence_;
}

bool SegmentStream::SegmentIndex::operator>(
    const SegmentStream::SegmentIndex& other) const {
  return !(*this == other) && !(*this < other);
}

SegmentStream::SegmentIndex SegmentStream::SegmentIndex::MaxOf(
    const MediaSegment& other) const {
  SegmentIndex other_index(other);
  if (other_index < *this) {
    return *this;
  }
  return other_index;
}

SegmentStream::SegmentIndex SegmentStream::SegmentIndex::Next() const {
  return {discontinuity_sequence_, media_sequence_ + 1};
}

SegmentStream::~SegmentStream() = default;
SegmentStream::SegmentStream(scoped_refptr<MediaPlaylist> playlist,
                             bool seekable)
    : seekable_(seekable),
      next_segment_start_(base::Seconds(0)),
      active_playlist_(std::move(playlist)) {
  for (const auto& segment : active_playlist_->GetSegments()) {
    segments_.push(segment);
    highest_segment_index_ = highest_segment_index_.MaxOf(*segment);
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
    // TODO(crbug.com/40057824): Should this be an error? I do not know if this
    // ever happens in the wild. I can imagine that it does, hence not raising
    // an error here for now. The spec doesn't seem to clear it up.
    return;
  }

  bool must_keep_encrypted_ = false;
  if (!Exhausted()) {
    // If the head is a non-fresh encrypted segment, keep it.
    must_keep_encrypted_ = !!segments_.front()->GetEncryptionData() &&
                           !segments_.front()->HasNewEncryptionData();
  }

  SegmentIndex starting_segment_index = {0, 0};
  if (Exhausted()) {
    if (seekable_) {
      // If a VOD stream is exhausted, there is nothing to append. Seeking later
      // will use the new active playlist's queue.
      return;
    }
    starting_segment_index = highest_segment_index_.Next();
  } else {
    starting_segment_index = SegmentIndex(*segments_.front());
  }

  if (must_keep_encrypted_) {
    starting_segment_index = starting_segment_index.Next();
  }

  base::queue<scoped_refptr<MediaSegment>> new_queue;
  if (must_keep_encrypted_) {
    new_queue.push(std::move(segments_.front()));
  }

  for (const auto& segment : segments) {
    auto segment_sequence_index = SegmentIndex(*segment);
    if (starting_segment_index <= segment_sequence_index) {
      new_queue.push(segment);
    }
    if (segment_sequence_index > highest_segment_index_) {
      highest_segment_index_ = segment_sequence_index;
    }
  }

  segments_ = std::move(new_queue);
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
