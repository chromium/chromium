// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_range.h"

#include "media/base/timestamp_constants.h"

namespace media {

// static
bool SourceBufferRange::IsUncommonSameTimestampSequence(
    bool prev_is_keyframe,
    bool current_is_keyframe) {
  return current_is_keyframe && !prev_is_keyframe;
}

SourceBufferRange::SourceBufferRange(
    GapPolicy gap_policy,
    const InterbufferDistanceCB& interbuffer_distance_cb)
    : gap_policy_(gap_policy),
      next_buffer_index_(-1),
      interbuffer_distance_cb_(interbuffer_distance_cb),
      size_in_bytes_(0) {
  DCHECK(interbuffer_distance_cb);
}

SourceBufferRange::~SourceBufferRange() = default;

void SourceBufferRange::SeekToStart() {
  CHECK(!buffers_.empty());
  next_buffer_index_ = 0;
}

bool SourceBufferRange::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (!HasNextBuffer())
    return false;

  *out_buffer = buffers_[next_buffer_index_];
  next_buffer_index_++;
  return true;
}

bool SourceBufferRange::HasNextBuffer() const {
  return next_buffer_index_ >= 0 &&
      next_buffer_index_ < static_cast<int>(buffers_.size());
}

int SourceBufferRange::GetNextConfigId() const {
  CHECK(HasNextBuffer()) << next_buffer_index_;
  // If the next buffer is an audio splice frame, the next effective config id
  // comes from the first fade out preroll buffer.
  return buffers_[next_buffer_index_]->GetConfigId();
}

bool SourceBufferRange::HasNextBufferPosition() const {
  return next_buffer_index_ >= 0;
}

void SourceBufferRange::ResetNextBufferPosition() {
  next_buffer_index_ = -1;
}

void SourceBufferRange::GetRangeEndTimesForTesting(
    base::TimeDelta* highest_pts,
    base::TimeDelta* end_time) const {
  if (highest_frame_) {
    *highest_pts = highest_frame_->timestamp();
    *end_time = *highest_pts + highest_frame_->duration();
    DCHECK_NE(*highest_pts, kNoTimestamp);
    DCHECK_NE(*end_time, kNoTimestamp);
    return;
  }

  *highest_pts = *end_time = kNoTimestamp;
}

void SourceBufferRange::AdjustEstimatedDurationForNewAppend(
    const BufferQueue& new_buffers) {
  if (buffers_.empty() || new_buffers.empty()) {
    return;
  }

  // Do not adjust estimate for Audio buffers to avoid competing with
  // SourceBufferStream::TrimSpliceOverlap()
  if (buffers_.front()->type() == StreamParserBuffer::Type::AUDIO) {
    return;
  }

  // If the last of the previously appended buffers contains estimated duration,
  // we now refine that estimate by taking the PTS delta from the first new
  // buffer being appended.
  const auto& last_appended_buffer = buffers_.back();
  if (last_appended_buffer->is_duration_estimated()) {
    base::TimeDelta timestamp_delta =
        new_buffers.front()->timestamp() - last_appended_buffer->timestamp();
    DCHECK_GE(timestamp_delta, base::TimeDelta());
    if (last_appended_buffer->duration() != timestamp_delta) {
      DVLOG(1) << "Replacing estimated duration ("
               << last_appended_buffer->duration()
               << ") from previous range-end with derived duration ("
               << timestamp_delta << ").";
      last_appended_buffer->set_duration(timestamp_delta);
    }
  }
}

void SourceBufferRange::FreeBufferRange(
    const BufferQueue::const_iterator& starting_point,
    const BufferQueue::const_iterator& ending_point) {
  for (BufferQueue::const_iterator itr = starting_point; itr != ending_point;
       ++itr) {
    size_t itr_data_size = static_cast<size_t>((*itr)->data_size());
    DCHECK_GE(size_in_bytes_, itr_data_size);
    size_in_bytes_ -= itr_data_size;
  }
  buffers_.erase(starting_point, ending_point);
}

base::TimeDelta SourceBufferRange::GetFudgeRoom() const {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * GetApproximateDuration();
}

base::TimeDelta SourceBufferRange::GetApproximateDuration() const {
  base::TimeDelta max_interbuffer_distance = interbuffer_distance_cb_.Run();
  DCHECK(max_interbuffer_distance != kNoTimestamp);
  return max_interbuffer_distance;
}

void SourceBufferRange::UpdateEndTime(
    scoped_refptr<StreamParserBuffer> new_buffer) {
  base::TimeDelta timestamp = new_buffer->timestamp();
  base::TimeDelta duration = new_buffer->duration();
  DVLOG(1) << __func__ << " timestamp=" << timestamp
           << ", duration=" << duration;
  DCHECK_NE(timestamp, kNoTimestamp);
  DCHECK_GE(timestamp, base::TimeDelta());
  DCHECK_GE(duration, base::TimeDelta());

  if (!highest_frame_) {
    DVLOG(1) << "Updating range end time from <empty> to "
             << timestamp.InMicroseconds() << "us, "
             << (timestamp + duration).InMicroseconds() << "us";
    highest_frame_ = std::move(new_buffer);
    return;
  }

  if (highest_frame_->timestamp() < timestamp ||
      (highest_frame_->timestamp() == timestamp &&
       highest_frame_->duration() <= duration)) {
    DVLOG(1) << "Updating range end time from "
             << highest_frame_->timestamp().InMicroseconds() << "us, "
             << (highest_frame_->timestamp() + highest_frame_->duration())
                    .InMicroseconds()
             << "us to " << timestamp.InMicroseconds() << "us, "
             << (timestamp + duration).InMicroseconds();
    highest_frame_ = std::move(new_buffer);
  }
}

bool SourceBufferRange::IsNextInPresentationSequence(
    base::TimeDelta timestamp) const {
  DCHECK_NE(timestamp, kNoTimestamp);
  CHECK(!buffers_.empty());
  base::TimeDelta highest_timestamp = highest_frame_->timestamp();
  DCHECK_NE(highest_timestamp, kNoTimestamp);
  return (highest_timestamp == timestamp ||
          (highest_timestamp < timestamp &&
           (gap_policy_ == ALLOW_GAPS ||
            timestamp <= highest_timestamp + GetFudgeRoom())));
}

bool SourceBufferRange::IsNextInDecodeSequence(
    DecodeTimestamp decode_timestamp) const {
  CHECK(!buffers_.empty());
  DecodeTimestamp end = buffers_.back()->GetDecodeTimestamp();
  return (
      end == decode_timestamp ||
      (end < decode_timestamp && (gap_policy_ == ALLOW_GAPS ||
                                  decode_timestamp <= end + GetFudgeRoom())));
}

}  // namespace media
