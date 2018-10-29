// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_range_by_dts.h"

#include <algorithm>
#include <memory>

#include "media/base/timestamp_constants.h"

namespace media {

// Comparison operators for std::upper_bound() and std::lower_bound().
static bool CompareDecodeTimestampToStreamParserBuffer(
    const DecodeTimestamp& decode_timestamp,
    const scoped_refptr<StreamParserBuffer>& buffer) {
  return decode_timestamp < buffer->GetDecodeTimestamp();
}
static bool CompareStreamParserBufferToDecodeTimestamp(
    const scoped_refptr<StreamParserBuffer>& buffer,
    const DecodeTimestamp& decode_timestamp) {
  return buffer->GetDecodeTimestamp() < decode_timestamp;
}

SourceBufferRangeByDts::SourceBufferRangeByDts(
    GapPolicy gap_policy,
    const BufferQueue& new_buffers,
    DecodeTimestamp range_start_decode_time,
    const InterbufferDistanceCB& interbuffer_distance_cb)
    : SourceBufferRange(gap_policy, interbuffer_distance_cb),
      range_start_decode_time_(range_start_decode_time),
      keyframe_map_index_base_(0) {
  DVLOG(3) << __func__;
  CHECK(!new_buffers.empty());
  DCHECK(new_buffers.front()->is_key_frame());
  AppendBuffersToEnd(new_buffers, range_start_decode_time_);
}

SourceBufferRangeByDts::~SourceBufferRangeByDts() = default;

DecodeTimestamp SourceBufferRangeByDts::NextRangeStartTimeForAppendRangeToEnd(
    const SourceBufferRangeByDts& range) const {
  DCHECK(!buffers_.empty());
  DCHECK(!range.buffers_.empty());

  DecodeTimestamp next_range_first_buffer_time =
      range.buffers_.front()->GetDecodeTimestamp();
  DecodeTimestamp this_range_end_time = GetEndTimestamp();
  if (next_range_first_buffer_time < this_range_end_time)
    return kNoDecodeTimestamp();

  DecodeTimestamp next_range_start_time = range.GetStartTimestamp();
  DCHECK(next_range_start_time <= next_range_first_buffer_time);

  if (next_range_start_time >= this_range_end_time)
    return next_range_start_time;

  return this_range_end_time;
}

void SourceBufferRangeByDts::AppendRangeToEnd(
    const SourceBufferRangeByDts& range,
    bool transfer_current_position) {
  DCHECK(CanAppendRangeToEnd(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position && range.next_buffer_index_ >= 0)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendBuffersToEnd(range.buffers_,
                     NextRangeStartTimeForAppendRangeToEnd(range));
}

void SourceBufferRangeByDts::DeleteAll(BufferQueue* deleted_buffers) {
  TruncateAt(buffers_.begin(), deleted_buffers);
}

bool SourceBufferRangeByDts::CanAppendRangeToEnd(
    const SourceBufferRangeByDts& range) const {
  return CanAppendBuffersToEnd(range.buffers_,
                               NextRangeStartTimeForAppendRangeToEnd(range));
}

void SourceBufferRangeByDts::AppendBuffersToEnd(
    const BufferQueue& new_buffers,
    DecodeTimestamp new_buffers_group_start_timestamp) {
  CHECK(buffers_.empty() ||
        CanAppendBuffersToEnd(new_buffers, new_buffers_group_start_timestamp));
  DCHECK(range_start_decode_time_ == kNoDecodeTimestamp() ||
         range_start_decode_time_ <= new_buffers.front()->GetDecodeTimestamp());

  AdjustEstimatedDurationForNewAppend(new_buffers);

  for (BufferQueue::const_iterator itr = new_buffers.begin();
       itr != new_buffers.end(); ++itr) {
    DCHECK((*itr)->GetDecodeTimestamp() != kNoDecodeTimestamp());

    buffers_.push_back(*itr);
    UpdateEndTime(*itr);
    size_in_bytes_ += (*itr)->data_size();

    if ((*itr)->is_key_frame()) {
      keyframe_map_.insert(
          std::make_pair((*itr)->GetDecodeTimestamp(),
                         buffers_.size() - 1 + keyframe_map_index_base_));
    }
  }
}

bool SourceBufferRangeByDts::AllowableAppendAfterEstimatedDuration(
    const BufferQueue& buffers,
    DecodeTimestamp new_buffers_group_start_timestamp) const {
  if (buffers_.empty() || !buffers_.back()->is_duration_estimated() ||
      buffers.empty() || !buffers.front()->is_key_frame()) {
    return false;
  }

  if (new_buffers_group_start_timestamp == kNoDecodeTimestamp()) {
    return GetBufferedEndTimestamp() == buffers.front()->GetDecodeTimestamp();
  }

  return GetBufferedEndTimestamp() == new_buffers_group_start_timestamp;
}

bool SourceBufferRangeByDts::CanAppendBuffersToEnd(
    const BufferQueue& buffers,
    DecodeTimestamp new_buffers_group_start_timestamp) const {
  DCHECK(!buffers_.empty());
  if (new_buffers_group_start_timestamp == kNoDecodeTimestamp()) {
    return IsNextInDecodeSequence(buffers.front()->GetDecodeTimestamp()) ||
           AllowableAppendAfterEstimatedDuration(
               buffers, new_buffers_group_start_timestamp);
  }
  DCHECK(new_buffers_group_start_timestamp >= GetEndTimestamp());
  DCHECK(buffers.front()->GetDecodeTimestamp() >=
         new_buffers_group_start_timestamp);
  return IsNextInDecodeSequence(new_buffers_group_start_timestamp) ||
         AllowableAppendAfterEstimatedDuration(
             buffers, new_buffers_group_start_timestamp);
}

void SourceBufferRangeByDts::Seek(DecodeTimestamp timestamp) {
  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  auto result = GetFirstKeyframeAtOrBefore(timestamp);
  next_buffer_index_ = result->second - keyframe_map_index_base_;
  CHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()))
      << next_buffer_index_ << ", size = " << buffers_.size();
}

int SourceBufferRangeByDts::GetConfigIdAtTime(DecodeTimestamp timestamp) const {
  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  auto result = GetFirstKeyframeAtOrBefore(timestamp);
  CHECK(result != keyframe_map_.end());
  size_t buffer_index = result->second - keyframe_map_index_base_;
  CHECK_LT(buffer_index, buffers_.size())
      << buffer_index << ", size = " << buffers_.size();

  return buffers_[buffer_index]->GetConfigId();
}

bool SourceBufferRangeByDts::SameConfigThruRange(DecodeTimestamp start,
                                                 DecodeTimestamp end) const {
  DCHECK(CanSeekTo(start));
  DCHECK(CanSeekTo(end));
  DCHECK(start <= end);
  DCHECK(!keyframe_map_.empty());

  if (start == end)
    return true;

  auto result = GetFirstKeyframeAtOrBefore(start);
  CHECK(result != keyframe_map_.end());
  size_t buffer_index = result->second - keyframe_map_index_base_;
  CHECK_LT(buffer_index, buffers_.size())
      << buffer_index << ", size = " << buffers_.size();

  int start_config = buffers_[buffer_index]->GetConfigId();
  buffer_index++;
  while (buffer_index < buffers_.size() &&
         buffers_[buffer_index]->GetDecodeTimestamp() <= end) {
    if (buffers_[buffer_index]->GetConfigId() != start_config)
      return false;
    buffer_index++;
  }

  return true;
}

std::unique_ptr<SourceBufferRangeByDts> SourceBufferRangeByDts::SplitRange(
    DecodeTimestamp timestamp) {
  CHECK(!buffers_.empty());

  // Find the first keyframe at or after |timestamp|.
  auto new_beginning_keyframe = GetFirstKeyframeAt(timestamp, false);

  // If there is no keyframe after |timestamp|, we can't split the range.
  if (new_beginning_keyframe == keyframe_map_.end()) {
    return NULL;
  }

  // Remove the data beginning at |keyframe_index| from |buffers_| and save it
  // into |removed_buffers|.
  int keyframe_index =
      new_beginning_keyframe->second - keyframe_map_index_base_;
  DCHECK_LT(keyframe_index, static_cast<int>(buffers_.size()));
  BufferQueue::iterator starting_point = buffers_.begin() + keyframe_index;
  BufferQueue removed_buffers(starting_point, buffers_.end());

  DecodeTimestamp new_range_start_decode_timestamp =
      std::max(timestamp, GetStartTimestamp());
  DCHECK(new_range_start_decode_timestamp <=
         removed_buffers.front()->GetDecodeTimestamp());

  keyframe_map_.erase(new_beginning_keyframe, keyframe_map_.end());
  FreeBufferRange(starting_point, buffers_.end());
  UpdateEndTimeUsingLastGOP();

  // Create a new range with |removed_buffers|.
  std::unique_ptr<SourceBufferRangeByDts> split_range =
      std::make_unique<SourceBufferRangeByDts>(gap_policy_, removed_buffers,
                                               new_range_start_decode_timestamp,
                                               interbuffer_distance_cb_);

  // If the next buffer position is now in |split_range|, update the state of
  // this range and |split_range| accordingly.
  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    split_range->next_buffer_index_ = next_buffer_index_ - keyframe_index;

    int split_range_next_buffer_index = split_range->next_buffer_index_;
    CHECK_GE(split_range_next_buffer_index, 0);
    // Note that a SourceBufferRange's |next_buffer_index_| can be the index
    // of a buffer one beyond what is currently in |buffers_|.
    CHECK_LE(split_range_next_buffer_index,
             static_cast<int>(split_range->buffers_.size()));

    ResetNextBufferPosition();
  }

  return split_range;
}

bool SourceBufferRangeByDts::TruncateAt(DecodeTimestamp timestamp,
                                        BufferQueue* deleted_buffers,
                                        bool is_exclusive) {
  // Find the place in |buffers_| where we will begin deleting data.
  BufferQueue::const_iterator starting_point =
      GetBufferItrAt(timestamp, is_exclusive);
  return TruncateAt(starting_point, deleted_buffers);
}

size_t SourceBufferRangeByDts::DeleteGOPFromFront(
    BufferQueue* deleted_buffers) {
  DCHECK(!buffers_.empty());
  DCHECK(!FirstGOPContainsNextBufferPosition());
  DCHECK(deleted_buffers);

  int buffers_deleted = 0;
  size_t total_bytes_deleted = 0;

  KeyframeMap::const_iterator front = keyframe_map_.begin();
  DCHECK(front != keyframe_map_.end());

  // Delete the keyframe at the start of |keyframe_map_|.
  keyframe_map_.erase(front);

  // Now we need to delete all the buffers that depend on the keyframe we've
  // just deleted.
  int end_index = keyframe_map_.size() > 0
                      ? keyframe_map_.begin()->second - keyframe_map_index_base_
                      : buffers_.size();

  // Delete buffers from the beginning of the buffered range up until (but not
  // including) the next keyframe.
  for (int i = 0; i < end_index; i++) {
    size_t bytes_deleted = buffers_.front()->data_size();
    DCHECK_GE(size_in_bytes_, bytes_deleted);
    size_in_bytes_ -= bytes_deleted;
    total_bytes_deleted += bytes_deleted;
    deleted_buffers->push_back(buffers_.front());
    buffers_.pop_front();
    ++buffers_deleted;
  }

  // Update |keyframe_map_index_base_| to account for the deleted buffers.
  keyframe_map_index_base_ += buffers_deleted;

  if (next_buffer_index_ > -1) {
    next_buffer_index_ -= buffers_deleted;
    CHECK_GE(next_buffer_index_, 0)
        << next_buffer_index_ << ", deleted " << buffers_deleted;
  }

  // Invalidate range start time if we've deleted the first buffer of the range.
  if (buffers_deleted > 0) {
    range_start_decode_time_ = kNoDecodeTimestamp();
    // Reset the range end time tracking if there are no more buffers in the
    // range.
    if (buffers_.empty())
      highest_frame_ = nullptr;
  }

  return total_bytes_deleted;
}

size_t SourceBufferRangeByDts::DeleteGOPFromBack(BufferQueue* deleted_buffers) {
  DCHECK(!buffers_.empty());
  DCHECK(!LastGOPContainsNextBufferPosition());
  DCHECK(deleted_buffers);

  // Remove the last GOP's keyframe from the |keyframe_map_|.
  KeyframeMap::const_iterator back = keyframe_map_.end();
  DCHECK_GT(keyframe_map_.size(), 0u);
  --back;

  // The index of the first buffer in the last GOP is equal to the new size of
  // |buffers_| after that GOP is deleted.
  size_t goal_size = back->second - keyframe_map_index_base_;
  keyframe_map_.erase(back);

  size_t total_bytes_deleted = 0;
  while (buffers_.size() != goal_size) {
    size_t bytes_deleted = buffers_.back()->data_size();
    DCHECK_GE(size_in_bytes_, bytes_deleted);
    size_in_bytes_ -= bytes_deleted;
    total_bytes_deleted += bytes_deleted;
    // We're removing buffers from the back, so push each removed buffer to the
    // front of |deleted_buffers| so that |deleted_buffers| are in nondecreasing
    // order.
    deleted_buffers->push_front(buffers_.back());
    buffers_.pop_back();
  }

  UpdateEndTimeUsingLastGOP();

  return total_bytes_deleted;
}

size_t SourceBufferRangeByDts::GetRemovalGOP(
    DecodeTimestamp start_timestamp,
    DecodeTimestamp end_timestamp,
    size_t total_bytes_to_free,
    DecodeTimestamp* removal_end_timestamp) const {
  size_t bytes_removed = 0;

  auto gop_itr = GetFirstKeyframeAt(start_timestamp, false);
  if (gop_itr == keyframe_map_.end())
    return 0;
  int keyframe_index = gop_itr->second - keyframe_map_index_base_;
  BufferQueue::const_iterator buffer_itr = buffers_.begin() + keyframe_index;
  auto gop_end = keyframe_map_.end();
  if (end_timestamp < GetBufferedEndTimestamp())
    gop_end = GetFirstKeyframeAtOrBefore(end_timestamp);

  // Check if the removal range is within a GOP and skip the loop if so.
  // [keyframe]...[start_timestamp]...[end_timestamp]...[keyframe]
  auto gop_itr_prev = gop_itr;
  if (gop_itr_prev != keyframe_map_.begin() && --gop_itr_prev == gop_end)
    gop_end = gop_itr;

  while (gop_itr != gop_end && bytes_removed < total_bytes_to_free) {
    ++gop_itr;

    size_t gop_size = 0;
    int next_gop_index = gop_itr == keyframe_map_.end()
                             ? buffers_.size()
                             : gop_itr->second - keyframe_map_index_base_;
    BufferQueue::const_iterator next_gop_start =
        buffers_.begin() + next_gop_index;
    for (; buffer_itr != next_gop_start; ++buffer_itr) {
      gop_size += (*buffer_itr)->data_size();
    }

    bytes_removed += gop_size;
  }
  if (bytes_removed > 0) {
    *removal_end_timestamp = gop_itr == keyframe_map_.end()
                                 ? GetBufferedEndTimestamp()
                                 : gop_itr->first;
  }
  return bytes_removed;
}

bool SourceBufferRangeByDts::FirstGOPEarlierThanMediaTime(
    DecodeTimestamp media_time) const {
  if (keyframe_map_.size() == 1u)
    return (GetBufferedEndTimestamp() <= media_time);

  auto second_gop = keyframe_map_.begin();
  ++second_gop;
  return second_gop->first <= media_time;
}

bool SourceBufferRangeByDts::FirstGOPContainsNextBufferPosition() const {
  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  auto second_gop = keyframe_map_.begin();
  ++second_gop;
  return next_buffer_index_ < second_gop->second - keyframe_map_index_base_;
}

bool SourceBufferRangeByDts::LastGOPContainsNextBufferPosition() const {
  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  auto last_gop = keyframe_map_.end();
  --last_gop;
  return last_gop->second - keyframe_map_index_base_ <= next_buffer_index_;
}

DecodeTimestamp SourceBufferRangeByDts::GetNextTimestamp() const {
  CHECK(!buffers_.empty()) << next_buffer_index_;
  CHECK(HasNextBufferPosition())
      << next_buffer_index_ << ", size=" << buffers_.size();

  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    return kNoDecodeTimestamp();
  }

  return buffers_[next_buffer_index_]->GetDecodeTimestamp();
}

DecodeTimestamp SourceBufferRangeByDts::GetStartTimestamp() const {
  DCHECK(!buffers_.empty());
  DecodeTimestamp start_timestamp = range_start_decode_time_;
  if (start_timestamp == kNoDecodeTimestamp())
    start_timestamp = buffers_.front()->GetDecodeTimestamp();
  return start_timestamp;
}

DecodeTimestamp SourceBufferRangeByDts::GetEndTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.back()->GetDecodeTimestamp();
}

DecodeTimestamp SourceBufferRangeByDts::GetBufferedEndTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta duration = buffers_.back()->duration();

  // FrameProcessor should protect against unknown buffer durations.
  DCHECK_NE(duration, kNoTimestamp);

  // Because media::Ranges<base::TimeDelta>::Add() ignores 0 duration ranges,
  // report 1 microsecond for the last buffer's duration if it is a 0 duration
  // buffer.
  if (duration.is_zero())
    duration = base::TimeDelta::FromMicroseconds(1);

  return GetEndTimestamp() + duration;
}

bool SourceBufferRangeByDts::BelongsToRange(DecodeTimestamp timestamp) const {
  DCHECK(!buffers_.empty());

  return (IsNextInDecodeSequence(timestamp) ||
          (GetStartTimestamp() <= timestamp && timestamp <= GetEndTimestamp()));
}

DecodeTimestamp SourceBufferRangeByDts::FindHighestBufferedTimestampAtOrBefore(
    DecodeTimestamp timestamp) const {
  DCHECK(!buffers_.empty());
  DCHECK(BelongsToRange(timestamp));

  if (keyframe_map_.begin()->first > timestamp) {
    // If the first keyframe in the range starts after |timestamp|, then return
    // the range start time (which could be earlier due to coded frame group
    // signalling.)
    DecodeTimestamp range_start = GetStartTimestamp();

    DCHECK(timestamp >= range_start) << "BelongsToRange() semantics failed.";
    return range_start;
  }

  if (keyframe_map_.begin()->first == timestamp) {
    return timestamp;
  }

  auto key_iter = GetFirstKeyframeAtOrBefore(timestamp);
  DCHECK(key_iter != keyframe_map_.end())
      << "BelongsToRange() semantics failed.";
  DCHECK(key_iter->first <= timestamp);

  // Scan forward in |buffers_| to find the highest frame decode timestamp <=
  // |timestamp|.
  size_t key_index = key_iter->second - keyframe_map_index_base_;
  SourceBufferRange::BufferQueue::const_iterator search_iter =
      buffers_.begin() + key_index;
  CHECK(search_iter != buffers_.end());
  DecodeTimestamp result = (*search_iter)->GetDecodeTimestamp();
  while (true) {
    search_iter++;
    if (search_iter == buffers_.end())
      return result;
    DecodeTimestamp cur_frame_time = (*search_iter)->GetDecodeTimestamp();
    if (cur_frame_time > timestamp)
      return result;
    result = cur_frame_time;
  }

  NOTREACHED();
  return DecodeTimestamp();
}

DecodeTimestamp SourceBufferRangeByDts::NextKeyframeTimestamp(
    DecodeTimestamp timestamp) const {
  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoDecodeTimestamp();

  auto itr = GetFirstKeyframeAt(timestamp, false);
  if (itr == keyframe_map_.end())
    return kNoDecodeTimestamp();

  // If the timestamp is inside the gap between the start of the coded frame
  // group and the first buffer, then just pretend there is a keyframe at the
  // specified timestamp.
  if (itr == keyframe_map_.begin() && timestamp > range_start_decode_time_ &&
      timestamp < itr->first) {
    return timestamp;
  }

  return itr->first;
}

DecodeTimestamp SourceBufferRangeByDts::KeyframeBeforeTimestamp(
    DecodeTimestamp timestamp) const {
  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoDecodeTimestamp();

  return GetFirstKeyframeAtOrBefore(timestamp)->first;
}

bool SourceBufferRangeByDts::CanSeekTo(DecodeTimestamp timestamp) const {
  DecodeTimestamp start_timestamp =
      std::max(DecodeTimestamp(), GetStartTimestamp() - GetFudgeRoom());
  return !keyframe_map_.empty() && start_timestamp <= timestamp &&
         timestamp < GetBufferedEndTimestamp();
}

bool SourceBufferRangeByDts::GetBuffersInRange(DecodeTimestamp start,
                                               DecodeTimestamp end,
                                               BufferQueue* buffers) const {
  // Find the nearest buffer with a decode timestamp <= start.
  const DecodeTimestamp first_timestamp = KeyframeBeforeTimestamp(start);
  if (first_timestamp == kNoDecodeTimestamp())
    return false;

  // Find all buffers involved in the range.
  const size_t previous_size = buffers->size();
  for (BufferQueue::const_iterator it = GetBufferItrAt(first_timestamp, false);
       it != buffers_.end(); ++it) {
    scoped_refptr<StreamParserBuffer> buffer = *it;
    // Buffers without duration are not supported, so bail if we encounter any.
    if (buffer->duration() == kNoTimestamp ||
        buffer->duration() <= base::TimeDelta()) {
      return false;
    }
    if (buffer->timestamp() >= end.ToPresentationTime())
      break;

    if (buffer->timestamp() + buffer->duration() <= start.ToPresentationTime())
      continue;
    buffers->emplace_back(std::move(buffer));
  }
  return previous_size < buffers->size();
}

SourceBufferRange::BufferQueue::const_iterator
SourceBufferRangeByDts::GetBufferItrAt(DecodeTimestamp timestamp,
                                       bool skip_given_timestamp) const {
  return skip_given_timestamp
             ? std::upper_bound(buffers_.begin(), buffers_.end(), timestamp,
                                CompareDecodeTimestampToStreamParserBuffer)
             : std::lower_bound(buffers_.begin(), buffers_.end(), timestamp,
                                CompareStreamParserBufferToDecodeTimestamp);
}

SourceBufferRangeByDts::KeyframeMap::const_iterator
SourceBufferRangeByDts::GetFirstKeyframeAt(DecodeTimestamp timestamp,
                                           bool skip_given_timestamp) const {
  return skip_given_timestamp ? keyframe_map_.upper_bound(timestamp)
                              : keyframe_map_.lower_bound(timestamp);
}

SourceBufferRangeByDts::KeyframeMap::const_iterator
SourceBufferRangeByDts::GetFirstKeyframeAtOrBefore(
    DecodeTimestamp timestamp) const {
  auto result = keyframe_map_.lower_bound(timestamp);
  // lower_bound() returns the first element >= |timestamp|, so we want the
  // previous element if it did not return the element exactly equal to
  // |timestamp|.
  if (result != keyframe_map_.begin() &&
      (result == keyframe_map_.end() || result->first != timestamp)) {
    --result;
  }
  return result;
}

bool SourceBufferRangeByDts::TruncateAt(
    const BufferQueue::const_iterator& starting_point,
    BufferQueue* deleted_buffers) {
  DCHECK(!deleted_buffers || deleted_buffers->empty());

  // Return if we're not deleting anything.
  if (starting_point == buffers_.end())
    return buffers_.empty();

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  if (HasNextBufferPosition()) {
    DecodeTimestamp next_buffer_timestamp = GetNextTimestamp();
    if (next_buffer_timestamp == kNoDecodeTimestamp() ||
        next_buffer_timestamp >= (*starting_point)->GetDecodeTimestamp()) {
      if (HasNextBuffer() && deleted_buffers) {
        int starting_offset = starting_point - buffers_.begin();
        int next_buffer_offset = next_buffer_index_ - starting_offset;
        DCHECK_GE(next_buffer_offset, 0);
        int deleted_begin_offset = starting_offset + next_buffer_offset;
        CHECK_GE(deleted_begin_offset, 0);

        BufferQueue saved(buffers_.begin() + deleted_begin_offset,
                          buffers_.end());
        deleted_buffers->swap(saved);
      }
      ResetNextBufferPosition();
    }
  }

  // Remove keyframes from |starting_point| onward.
  KeyframeMap::const_iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point)->GetDecodeTimestamp());
  keyframe_map_.erase(starting_point_keyframe, keyframe_map_.end());

  // Remove everything from |starting_point| onward.
  FreeBufferRange(starting_point, buffers_.end());

  UpdateEndTimeUsingLastGOP();
  return buffers_.empty();
}

void SourceBufferRangeByDts::UpdateEndTimeUsingLastGOP() {
  if (buffers_.empty()) {
    DVLOG(1) << __func__ << " Empty range, resetting range end";
    highest_frame_ = nullptr;
    return;
  }

  highest_frame_ = nullptr;

  KeyframeMap::const_iterator last_gop = keyframe_map_.end();
  CHECK_GT(keyframe_map_.size(), 0u);
  --last_gop;

  // Iterate through the frames of the last GOP in this range, finding the
  // frame with the highest PTS.
  for (BufferQueue::const_iterator buffer_itr =
           buffers_.begin() + (last_gop->second - keyframe_map_index_base_);
       buffer_itr != buffers_.end(); ++buffer_itr) {
    UpdateEndTime(*buffer_itr);
  }

  DVLOG(1) << __func__ << " Updated range end time to "
           << highest_frame_->timestamp() << ", "
           << highest_frame_->timestamp() + highest_frame_->duration();
}

}  // namespace media
