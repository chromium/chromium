// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_range_by_pts.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include "base/logging.h"
#include "media/base/timestamp_constants.h"

namespace media {

SourceBufferRangeByPts::SourceBufferRangeByPts(
    GapPolicy gap_policy,
    const BufferQueue& new_buffers,
    base::TimeDelta range_start_pts,
    const InterbufferDistanceCB& interbuffer_distance_cb)
    : SourceBufferRange(gap_policy, interbuffer_distance_cb),
      range_start_pts_(range_start_pts),
      keyframe_map_index_base_(0) {
  DVLOG(3) << __func__;
  CHECK(!new_buffers.empty());
  DCHECK(new_buffers.front()->is_key_frame());
  AppendBuffersToEnd(new_buffers, range_start_pts_);
}

SourceBufferRangeByPts::~SourceBufferRangeByPts() = default;

void SourceBufferRangeByPts::DeleteAll(BufferQueue* deleted_buffers) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  TruncateAt(0u, deleted_buffers);
}

void SourceBufferRangeByPts::AppendRangeToEnd(
    const SourceBufferRangeByPts& range,
    bool transfer_current_position) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(CanAppendRangeToEnd(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position && range.next_buffer_index_ >= 0)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendBuffersToEnd(range.buffers_,
                     NextRangeStartTimeForAppendRangeToEnd(range));
}

bool SourceBufferRangeByPts::CanAppendRangeToEnd(
    const SourceBufferRangeByPts& range) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  return CanAppendBuffersToEnd(range.buffers_,
                               NextRangeStartTimeForAppendRangeToEnd(range));
}

bool SourceBufferRangeByPts::AllowableAppendAfterEstimatedDuration(
    const BufferQueue& buffers,
    base::TimeDelta new_buffers_group_start_pts) const {
  if (buffers_.empty() || !buffers_.back()->is_duration_estimated() ||
      buffers.empty() || !buffers.front()->is_key_frame()) {
    return false;
  }

  if (new_buffers_group_start_pts == kNoTimestamp) {
    return GetBufferedEndTimestamp() == buffers.front()->timestamp();
  }

  return GetBufferedEndTimestamp() == new_buffers_group_start_pts;
}

bool SourceBufferRangeByPts::CanAppendBuffersToEnd(
    const BufferQueue& buffers,
    base::TimeDelta new_buffers_group_start_pts) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!buffers_.empty());
  if (new_buffers_group_start_pts == kNoTimestamp) {
    return buffers.front()->is_key_frame()
               ? (IsNextInPresentationSequence(buffers.front()->timestamp()) ||
                  AllowableAppendAfterEstimatedDuration(
                      buffers, new_buffers_group_start_pts))
               : IsNextInDecodeSequence(buffers.front()->GetDecodeTimestamp());
  }
  CHECK(buffers.front()->is_key_frame());
  DCHECK(new_buffers_group_start_pts >= GetEndTimestamp());
  DCHECK(buffers.front()->timestamp() >= new_buffers_group_start_pts);
  return IsNextInPresentationSequence(new_buffers_group_start_pts) ||
         AllowableAppendAfterEstimatedDuration(buffers,
                                               new_buffers_group_start_pts);
}

void SourceBufferRangeByPts::AppendBuffersToEnd(
    const BufferQueue& new_buffers,
    base::TimeDelta new_buffers_group_start_pts) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  CHECK(buffers_.empty() ||
        CanAppendBuffersToEnd(new_buffers, new_buffers_group_start_pts));

  DCHECK(new_buffers_group_start_pts == kNoTimestamp ||
         new_buffers.front()->is_key_frame())
      << range_start_pts_ << ", " << new_buffers.front()->is_key_frame();

  // TODO(wolenetz): Uncomment this DCHECK once SAP-Type-2 is more fully
  // supported. It is hit by NewByPts versions of
  // FrameProcessorTest.OOOKeyframePrecededByDependantNonKeyframeShouldWarn. See
  // https://crbug.com/718641.
  // DCHECK(range_start_pts_ == kNoTimestamp ||
  // range_start_pts_ <= new_buffers.front()->timestamp());

  AdjustEstimatedDurationForNewAppend(new_buffers);

  for (BufferQueue::const_iterator itr = new_buffers.begin();
       itr != new_buffers.end(); ++itr) {
    DCHECK((*itr)->timestamp() != kNoTimestamp);
    DCHECK((*itr)->GetDecodeTimestamp() != kNoDecodeTimestamp());

    buffers_.push_back(*itr);
    UpdateEndTime(*itr);
    size_in_bytes_ += (*itr)->data_size();

    if ((*itr)->is_key_frame()) {
      keyframe_map_.insert(std::make_pair(
          (*itr)->timestamp(), buffers_.size() - 1 + keyframe_map_index_base_));
    }
  }

  DVLOG(4) << __func__ << " Result: " << ToStringForDebugging();
}

void SourceBufferRangeByPts::Seek(base::TimeDelta timestamp) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  auto result = GetFirstKeyframeAtOrBefore(timestamp);
  next_buffer_index_ = result->second - keyframe_map_index_base_;
  CHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()))
      << next_buffer_index_ << ", size = " << buffers_.size();
}

bool SourceBufferRangeByPts::CanSeekTo(base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  base::TimeDelta start_timestamp =
      std::max(base::TimeDelta(), GetStartTimestamp() - GetFudgeRoom());
  return !keyframe_map_.empty() && start_timestamp <= timestamp &&
         timestamp < GetBufferedEndTimestamp();
}

int SourceBufferRangeByPts::GetConfigIdAtTime(base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  auto result = GetFirstKeyframeAtOrBefore(timestamp);
  CHECK(result != keyframe_map_.end());
  size_t buffer_index = result->second - keyframe_map_index_base_;
  CHECK_LT(buffer_index, buffers_.size())
      << buffer_index << ", size = " << buffers_.size();

  return buffers_[buffer_index]->GetConfigId();
}

bool SourceBufferRangeByPts::SameConfigThruRange(base::TimeDelta start,
                                                 base::TimeDelta end) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

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
         buffers_[buffer_index]->timestamp() <= end) {
    if (buffers_[buffer_index]->GetConfigId() != start_config)
      return false;
    buffer_index++;
  }

  return true;
}

std::unique_ptr<SourceBufferRangeByPts> SourceBufferRangeByPts::SplitRange(
    base::TimeDelta timestamp) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  CHECK(!buffers_.empty());

  // Find the first keyframe at or after |timestamp|.
  auto new_beginning_keyframe = GetFirstKeyframeAt(timestamp, false);

  // If there is no keyframe at or after |timestamp|, we can't split the range.
  if (new_beginning_keyframe == keyframe_map_.end())
    return nullptr;

  // Remove the data beginning at |keyframe_index| from |buffers_| and save it
  // into |removed_buffers|.
  int keyframe_index =
      new_beginning_keyframe->second - keyframe_map_index_base_;
  CHECK_LT(keyframe_index, static_cast<int>(buffers_.size()));
  BufferQueue::iterator starting_point = buffers_.begin() + keyframe_index;
  BufferQueue removed_buffers(starting_point, buffers_.end());

  base::TimeDelta new_range_start_pts =
      std::max(timestamp, GetStartTimestamp());
  DCHECK(new_range_start_pts <= removed_buffers.front()->timestamp());

  keyframe_map_.erase(new_beginning_keyframe, keyframe_map_.end());
  FreeBufferRange(starting_point, buffers_.end());
  UpdateEndTimeUsingLastGOP();

  // Create a new range with |removed_buffers|.
  std::unique_ptr<SourceBufferRangeByPts> split_range =
      std::make_unique<SourceBufferRangeByPts>(gap_policy_, removed_buffers,
                                               new_range_start_pts,
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

bool SourceBufferRangeByPts::TruncateAt(base::TimeDelta timestamp,
                                        BufferQueue* deleted_buffers,
                                        bool is_exclusive) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  // Find the place in |buffers_| where we will begin deleting data, then
  // truncate from there.
  return TruncateAt(GetBufferIndexAt(timestamp, is_exclusive), deleted_buffers);
}

size_t SourceBufferRangeByPts::DeleteGOPFromFront(
    BufferQueue* deleted_buffers) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

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
    range_start_pts_ = kNoTimestamp;
    // Reset the range end time tracking if there are no more buffers in the
    // range.
    if (buffers_.empty())
      highest_frame_ = nullptr;
  }

  return total_bytes_deleted;
}

size_t SourceBufferRangeByPts::DeleteGOPFromBack(BufferQueue* deleted_buffers) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

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

size_t SourceBufferRangeByPts::GetRemovalGOP(
    base::TimeDelta start_timestamp,
    base::TimeDelta end_timestamp,
    size_t total_bytes_to_free,
    base::TimeDelta* removal_end_timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

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

bool SourceBufferRangeByPts::FirstGOPEarlierThanMediaTime(
    base::TimeDelta media_time) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  if (keyframe_map_.size() == 1u)
    return (GetBufferedEndTimestamp() <= media_time);

  auto second_gop = keyframe_map_.begin();
  ++second_gop;
  return second_gop->first <= media_time;
}

bool SourceBufferRangeByPts::FirstGOPContainsNextBufferPosition() const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  auto second_gop = keyframe_map_.begin();
  ++second_gop;
  return next_buffer_index_ < second_gop->second - keyframe_map_index_base_;
}

bool SourceBufferRangeByPts::LastGOPContainsNextBufferPosition() const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  if (!HasNextBufferPosition())
    return false;

  // If there is only one GOP, it must contain the next buffer position.
  if (keyframe_map_.size() == 1u)
    return true;

  auto last_gop = keyframe_map_.end();
  --last_gop;
  return last_gop->second - keyframe_map_index_base_ <= next_buffer_index_;
}

base::TimeDelta SourceBufferRangeByPts::GetNextTimestamp() const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  CHECK(!buffers_.empty()) << next_buffer_index_;
  CHECK(HasNextBufferPosition())
      << next_buffer_index_ << ", size=" << buffers_.size();

  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    return kNoTimestamp;
  }

  return buffers_[next_buffer_index_]->timestamp();
}

base::TimeDelta SourceBufferRangeByPts::GetStartTimestamp() const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!buffers_.empty());
  base::TimeDelta start_timestamp = range_start_pts_;
  if (start_timestamp == kNoTimestamp)
    start_timestamp = buffers_.front()->timestamp();
  return start_timestamp;
}

base::TimeDelta SourceBufferRangeByPts::GetEndTimestamp() const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!buffers_.empty());
  return highest_frame_->timestamp();
}

base::TimeDelta SourceBufferRangeByPts::GetBufferedEndTimestamp() const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!buffers_.empty());
  base::TimeDelta duration = highest_frame_->duration();

  // FrameProcessor should protect against unknown buffer durations.
  DCHECK_NE(duration, kNoTimestamp);

  // Because media::Ranges<base::TimeDelta>::Add() ignores 0 duration ranges,
  // report 1 microsecond for the last buffer's duration if it is a 0 duration
  // buffer.
  if (duration.is_zero())
    duration = base::TimeDelta::FromMicroseconds(1);

  return GetEndTimestamp() + duration;
}

bool SourceBufferRangeByPts::BelongsToRange(base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!buffers_.empty());

  return (IsNextInPresentationSequence(timestamp) ||
          (GetStartTimestamp() <= timestamp && timestamp <= GetEndTimestamp()));
}

base::TimeDelta SourceBufferRangeByPts::FindHighestBufferedTimestampAtOrBefore(
    base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!buffers_.empty());
  DCHECK(BelongsToRange(timestamp));

  if (keyframe_map_.begin()->first > timestamp) {
    // If the first keyframe in the range starts after |timestamp|, then
    // return the range start time (which could be earlier due to coded frame
    // group signalling.)
    base::TimeDelta range_start = GetStartTimestamp();
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

  // Scan forward in |buffers_| to find the highest frame with timestamp <=
  // |timestamp|. Stop once a frame with timestamp > |timestamp| is encountered.
  size_t key_index = key_iter->second - keyframe_map_index_base_;
  SourceBufferRange::BufferQueue::const_iterator search_iter =
      buffers_.begin() + key_index;
  CHECK(search_iter != buffers_.end());
  base::TimeDelta cur_frame_time = (*search_iter)->timestamp();
  base::TimeDelta result = cur_frame_time;
  while (true) {
    result = std::max(result, cur_frame_time);
    search_iter++;
    if (search_iter == buffers_.end())
      return result;
    cur_frame_time = (*search_iter)->timestamp();
    if (cur_frame_time > timestamp)
      return result;
  }

  NOTREACHED();
  return base::TimeDelta();
}

base::TimeDelta SourceBufferRangeByPts::NextKeyframeTimestamp(
    base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoTimestamp;

  auto itr = GetFirstKeyframeAt(timestamp, false);
  if (itr == keyframe_map_.end())
    return kNoTimestamp;

  // If the timestamp is inside the gap between the start of the coded frame
  // group and the first buffer, then just pretend there is a keyframe at the
  // specified timestamp.
  if (itr == keyframe_map_.begin() && timestamp > range_start_pts_ &&
      timestamp < itr->first) {
    return timestamp;
  }

  return itr->first;
}

base::TimeDelta SourceBufferRangeByPts::KeyframeBeforeTimestamp(
    base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  DCHECK(!keyframe_map_.empty());

  if (timestamp < GetStartTimestamp() || timestamp >= GetBufferedEndTimestamp())
    return kNoTimestamp;

  return GetFirstKeyframeAtOrBefore(timestamp)->first;
}

bool SourceBufferRangeByPts::GetBuffersInRange(base::TimeDelta start,
                                               base::TimeDelta end,
                                               BufferQueue* buffers) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  // Find the nearest buffer with a timestamp <= start.
  const base::TimeDelta first_timestamp = KeyframeBeforeTimestamp(start);
  if (first_timestamp == kNoTimestamp)
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
    if (buffer->timestamp() >= end)
      break;

    if (buffer->timestamp() + buffer->duration() <= start)
      continue;

    DCHECK(buffer->is_key_frame());
    buffers->emplace_back(std::move(buffer));
  }
  return previous_size < buffers->size();
}

base::TimeDelta SourceBufferRangeByPts::NextRangeStartTimeForAppendRangeToEnd(
    const SourceBufferRangeByPts& range) const {
  DCHECK(!buffers_.empty());
  DCHECK(!range.buffers_.empty());

  base::TimeDelta next_range_first_buffer_time =
      range.buffers_.front()->timestamp();
  base::TimeDelta this_range_end_time = GetEndTimestamp();
  if (next_range_first_buffer_time < this_range_end_time)
    return kNoTimestamp;

  base::TimeDelta next_range_start_time = range.GetStartTimestamp();
  DCHECK(next_range_start_time <= next_range_first_buffer_time);

  if (next_range_start_time >= this_range_end_time)
    return next_range_start_time;

  return this_range_end_time;
}

size_t SourceBufferRangeByPts::GetBufferIndexAt(
    base::TimeDelta timestamp,
    bool skip_given_timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  // Find the GOP containing |timestamp| (or trivial buffers_.size() if none
  // contain |timestamp|).
  auto gop_iter = GetFirstKeyframeAtOrBefore(timestamp);
  if (gop_iter == keyframe_map_.end())
    return buffers_.size();

  // Then scan forward in this GOP in decode sequence for the first frame with
  // PTS >= |timestamp| (or strictly > if |skip_given_timestamp| is true). If
  // this GOP doesn't contain such a frame, returns the index of the keyframe of
  // the next GOP (which could be the index of end() of |buffers_| if this was
  // the last GOP in |buffers_|). We do linear scan of the GOP here because we
  // don't know the DTS for the searched-for frame, and the PTS sequence within
  // a GOP may not match the DTS-sorted sequence of frames within the GOP.
  DCHECK_GT(buffers_.size(), 0u);
  size_t search_index = gop_iter->second - keyframe_map_index_base_;
  SourceBufferRange::BufferQueue::const_iterator search_iter =
      buffers_.begin() + search_index;
  gop_iter++;

  SourceBufferRange::BufferQueue::const_iterator next_gop_start =
      gop_iter == keyframe_map_.end()
          ? buffers_.end()
          : buffers_.begin() + (gop_iter->second - keyframe_map_index_base_);

  while (search_iter != next_gop_start) {
    if (((*search_iter)->timestamp() > timestamp) ||
        (!skip_given_timestamp && (*search_iter)->timestamp() == timestamp)) {
      break;
    }
    search_index++;
    search_iter++;
  }

  return search_index;
}

SourceBufferRange::BufferQueue::const_iterator
SourceBufferRangeByPts::GetBufferItrAt(base::TimeDelta timestamp,
                                       bool skip_given_timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  return buffers_.begin() + GetBufferIndexAt(timestamp, skip_given_timestamp);
}

SourceBufferRangeByPts::KeyframeMap::const_iterator
SourceBufferRangeByPts::GetFirstKeyframeAt(base::TimeDelta timestamp,
                                           bool skip_given_timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  return skip_given_timestamp ? keyframe_map_.upper_bound(timestamp)
                              : keyframe_map_.lower_bound(timestamp);
}

SourceBufferRangeByPts::KeyframeMap::const_iterator
SourceBufferRangeByPts::GetFirstKeyframeAtOrBefore(
    base::TimeDelta timestamp) const {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

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

bool SourceBufferRangeByPts::TruncateAt(const size_t starting_point,
                                        BufferQueue* deleted_buffers) {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

  CHECK_LE(starting_point, buffers_.size());
  DCHECK(!deleted_buffers || deleted_buffers->empty());

  // Return if we're not deleting anything.
  if (starting_point == buffers_.size())
    return buffers_.empty();

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  if (HasNextBufferPosition()) {
    if (static_cast<size_t>(next_buffer_index_) >= starting_point) {
      if (HasNextBuffer() && deleted_buffers) {
        BufferQueue saved(buffers_.begin() + next_buffer_index_,
                          buffers_.end());
        deleted_buffers->swap(saved);
      }
      ResetNextBufferPosition();
    }
  }

  const BufferQueue::const_iterator starting_point_iter =
      buffers_.begin() + starting_point;

  // Remove keyframes from |starting_point| onward.
  KeyframeMap::const_iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point_iter)->timestamp());
  keyframe_map_.erase(starting_point_keyframe, keyframe_map_.end());

  // Remove everything from |starting_point| onward.
  FreeBufferRange(starting_point_iter, buffers_.end());

  UpdateEndTimeUsingLastGOP();
  return buffers_.empty();
}

void SourceBufferRangeByPts::UpdateEndTimeUsingLastGOP() {
  DVLOG(1) << __func__;
  DVLOG(4) << ToStringForDebugging();

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

std::string SourceBufferRangeByPts::ToStringForDebugging() const {
  std::stringstream result;

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  result << "keyframe_map_index_base_=" << keyframe_map_index_base_
         << ", buffers.size()=" << buffers_.size()
         << ", keyframe_map_.size()=" << keyframe_map_.size()
         << ", keyframe_map_:\n";
  for (const auto& entry : keyframe_map_) {
    result << "\t pts " << entry.first.InMicroseconds()
           << ", unadjusted idx = " << entry.second << "\n";
  }
#endif  // !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)

  return result.str();
}

}  // namespace media
