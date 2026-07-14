// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/segment_stream.h"

#include <algorithm>

namespace media::hls {

namespace {
std::vector<scoped_refptr<MediaSegment>>::const_iterator FindSegmentIndexByPdt(
    const std::vector<scoped_refptr<MediaSegment>>& new_segments,
    base::Time target_pdt) {
  auto get_pdt_diff = [target_pdt](const scoped_refptr<MediaSegment>& segment) {
    auto pdt = segment->GetProgramDateTime();
    return pdt ? (*pdt - target_pdt).magnitude() : base::TimeDelta::Max();
  };

  auto it = std::ranges::min_element(new_segments, std::less<>{}, get_pdt_diff);
  if (it != new_segments.end() && get_pdt_diff(*it) < base::Seconds(1)) {
    return it;
  }
  return new_segments.end();
}
}  // namespace

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

  if (!seekable_) {
    SkipEarlySegmentsForLiveStream();
  }
}

void SegmentStream::SkipEarlySegmentsForLiveStream() {
  // This method can only be called on non-seekable (live) segment streams.
  CHECK(!seekable_);
  while (segments_.size() > 3) {
    segments_.pop();
  }
}

SegmentInfo SegmentStream::GetNextSegment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!segments_.empty());
  bool needs_init_segment = false;
  auto segment = std::move(segments_.front());
  segments_.pop();

  last_popped_segment_pdt_ = segment->GetProgramDateTime();
  last_popped_segment_duration_ = segment->GetDuration();

  if (auto init_segment = segment->GetInitializationSegment()) {
    if (segment->HasNewInitSegment()) {
      previous_segment_init_segment_ = init_segment->GetUri();
      needs_init_segment = true;
    } else if (previous_segment_init_segment_ != init_segment->GetUri()) {
      previous_segment_init_segment_ = init_segment->GetUri();
      needs_init_segment = true;
    }
  }
  base::TimeDelta previous_segment_start = next_segment_start_;
  next_segment_start_ += segment->GetDuration();
  return std::make_tuple(segment, previous_segment_start, next_segment_start_,
                         needs_init_segment);
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

  const auto& new_segments = active_playlist_->GetSegments();
  if (new_segments.empty()) {
    // No new segments.
    // TODO(crbug.com/40057824): Should this be an error? I do not know if this
    // ever happens in the wild. I can imagine that it does, hence not raising
    // an error here for now. The spec doesn't seem to clear it up.
    return;
  }

  // If a live stream's queue was completely exhausted when receiving an updated
  // playlist, we should skip early segments so playback starts near the live
  // edge per RFC 8216.
  bool should_flush_live_segment_queue = !seekable_ && Exhausted();

  bool must_keep_encrypted_ = false;
  if (!Exhausted()) {
    // If the head of the current queue is an encrypted segment that relies on
    // existing key/IV state (i.e. does not bring a new initialization
    // section/key), we must preserve it in the queue so decryption context is
    // not lost during rendition switching.
    must_keep_encrypted_ = !!segments_.front()->GetEncryptionData() &&
                           !segments_.front()->HasNewEncryptionData();
  }

  scoped_refptr<MediaSegment> front_segment;
  if (!Exhausted()) {
    front_segment = segments_.front();
  }

  // Target Program Date Time (PDT) used to align segments across playlist
  // updates. We prioritize the front segment's PDT (or the start time of the
  // next segment if keeping the front encrypted segment), or fall back to the
  // end timestamp of the most recently popped segment if the queue is
  // exhausted.
  std::optional<base::Time> target_pdt;
  if (front_segment) {
    if (must_keep_encrypted_) {
      if (front_segment->GetProgramDateTime().has_value()) {
        target_pdt = front_segment->GetProgramDateTime().value() +
                     front_segment->GetDuration();
      }
    } else {
      target_pdt = front_segment->GetProgramDateTime();
    }
  } else if (last_popped_segment_pdt_.has_value()) {
    target_pdt =
        last_popped_segment_pdt_.value() + last_popped_segment_duration_;
  }

  base::queue<scoped_refptr<MediaSegment>> new_queue;
  if (must_keep_encrypted_) {
    new_queue.push(std::move(segments_.front()));
  }

  // Attempt to align segments using Program Date Time (PDT) tags if available.
  bool aligned_by_pdt = false;
  if (target_pdt.has_value()) {
    auto new_start_it = FindSegmentIndexByPdt(new_segments, target_pdt.value());
    if (new_start_it != new_segments.end()) {
      aligned_by_pdt = true;
      for (; new_start_it != new_segments.end(); ++new_start_it) {
        new_queue.push(*new_start_it);
      }
      if (!new_segments.empty()) {
        highest_segment_index_ = SegmentIndex(*new_segments.back());
      }
    }
  }

  // If PDT tags are absent or alignment by PDT failed, fall back to sequence
  // number and discontinuity sequence index matching.
  if (!aligned_by_pdt) {
    SegmentIndex starting_segment_index = {0, 0};
    if (Exhausted()) {
      if (seekable_) {
        // If a VOD stream is exhausted, there is nothing to append. Seeking
        // later will use the new active playlist's queue.
        return;
      }
      starting_segment_index = highest_segment_index_.Next();
    } else {
      starting_segment_index = SegmentIndex(*front_segment);
    }

    if (must_keep_encrypted_) {
      starting_segment_index = starting_segment_index.Next();
    }

    for (const auto& segment : new_segments) {
      auto segment_sequence_index = SegmentIndex(*segment);
      if (starting_segment_index <= segment_sequence_index) {
        new_queue.push(segment);
      }
      if (segment_sequence_index > highest_segment_index_) {
        highest_segment_index_ = segment_sequence_index;
      }
    }
  }

  segments_ = std::move(new_queue);

  if (should_flush_live_segment_queue) {
    SkipEarlySegmentsForLiveStream();
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
  previous_segment_init_segment_.reset();
  last_popped_segment_pdt_.reset();
  last_popped_segment_duration_ = base::TimeDelta();
}

void SegmentStream::SetSeekable(bool seekable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seekable_ = seekable;
}

}  // namespace media::hls
