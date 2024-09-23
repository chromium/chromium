// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "media/base/demuxer_memory_limit.h"
#include "media/base/media_switches.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// The minimum interbuffer decode timestamp delta (or buffer duration) for use
// in fudge room for range membership, adjacency and coalescing.
const int kMinimumInterbufferDistanceInMs = 1;

// Limit the number of MEDIA_LOG() logs for track buffer time gaps.
const int kMaxTrackBufferGapWarningLogs = 20;

// Limit the number of MEDIA_LOG() logs for MSE GC algorithm warnings.
const int kMaxGarbageCollectAlgorithmWarningLogs = 20;

// Limit the number of MEDIA_LOG() logs for splice overlap trimming.
const int kMaxAudioSpliceLogs = 20;

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
bool IsRangeListSorted(const SourceBufferStream::RangeList& ranges) {
  base::TimeDelta prev = kNoTimestamp;
  for (const auto& range_ptr : ranges) {
    if (prev != kNoTimestamp && prev >= range_ptr->GetStartTimestamp())
      return false;
    prev = range_ptr->GetBufferedEndTimestamp();
  }
  return true;
}

// Returns an estimate of how far from the beginning or end of a range a buffer
// can be to still be considered in the range, given the |approximate_duration|
// of a buffer in the stream.
// TODO(wolenetz): Once all stream parsers emit accurate frame durations, use
// logic like FrameProcessor (2*last_frame_duration + last_decode_timestamp)
// instead of an overall maximum interbuffer delta for range discontinuity
// detection.
// See http://crbug.com/351489 and http://crbug.com/351166.
base::TimeDelta ComputeFudgeRoom(base::TimeDelta approximate_duration) {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * approximate_duration;
}

// The amount of time the beginning of the buffered data can differ from the
// start time in order to still be considered the start of stream.
base::TimeDelta kSeekToStartFudgeRoom() {
  return base::Milliseconds(1000);
}

// Helper method for logging.
std::string StatusToString(const SourceBufferStreamStatus& status) {
  switch (status) {
    case SourceBufferStreamStatus::kSuccess:
      return "kSuccess";
    case SourceBufferStreamStatus::kNeedBuffer:
      return "kNeedBuffer";
    case SourceBufferStreamStatus::kConfigChange:
      return "kConfigChange";
    case SourceBufferStreamStatus::kEndOfStream:
      return "kEndOfStream";
  }
  NOTREACHED();
}

// Helper method for logging, converts a range into a readable string.
std::string RangeToString(const SourceBufferRange& range) {
  if (range.GetMemoryUsage() == 0) {
    return "[]";
  }
  std::stringstream ss;
  ss << "[" << range.GetStartTimestamp().InMicroseconds() << "us;"
     << range.GetEndTimestamp().InMicroseconds() << "us("
     << range.GetBufferedEndTimestamp().InMicroseconds() << "us)]";
  return ss.str();
}

// Helper method for logging, converts a set of ranges into a readable string.
std::string RangesToString(const SourceBufferStream::RangeList& ranges) {
  if (ranges.empty())
    return "<EMPTY>";

  std::stringstream ss;
  for (const auto& range_ptr : ranges) {
    if (range_ptr != ranges.front())
      ss << " ";
    ss << RangeToString(*range_ptr);
  }
  return ss.str();
}

std::string BufferQueueBuffersToLogString(
    const SourceBufferStream::BufferQueue& buffers) {
  std::stringstream result;

  result << "Buffers:\n";
  for (const auto& buf : buffers) {
    result << "\tdts=" << buf->GetDecodeTimestamp().InMicroseconds() << " "
           << buf->AsHumanReadableString()
           << ", is_duration_estimated=" << buf->is_duration_estimated()
           << "\n";
  }

  return result.str();
}

std::string BufferQueueMetadataToLogString(
    const SourceBufferStream::BufferQueue& buffers) {
  std::stringstream result;
  base::TimeDelta pts_interval_start;
  base::TimeDelta pts_interval_end;
  SourceBufferStream::GetTimestampInterval(buffers, &pts_interval_start,
                                           &pts_interval_end);

  result << "dts=[" << buffers.front()->GetDecodeTimestamp().InMicroseconds()
         << "us;" << buffers.back()->GetDecodeTimestamp().InMicroseconds()
         << "us(last frame dur=" << buffers.back()->duration().InMicroseconds()
         << "us)], pts interval=[" << pts_interval_start.InMicroseconds()
         << "us," << pts_interval_end.InMicroseconds() << "us)";
  return result.str();
}

}  // namespace

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config,
                                       MediaLog* media_log)
    : media_log_(media_log),
      seek_buffer_timestamp_(kNoTimestamp),
      coded_frame_group_start_pts_(kNoTimestamp),
      range_for_next_append_(ranges_.end()),
      highest_output_buffer_timestamp_(kNoTimestamp),
      max_interbuffer_distance_(
          base::Milliseconds(kMinimumInterbufferDistanceInMs)),
      memory_limit_(GetDemuxerStreamAudioMemoryLimit(&audio_config)) {
  DCHECK(audio_config.IsValidConfig());
  audio_configs_.push_back(audio_config);
  DVLOG(2) << __func__ << ": audio_buffer_size= " << memory_limit_;
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config,
                                       MediaLog* media_log)
    : media_log_(media_log),
      seek_buffer_timestamp_(kNoTimestamp),
      coded_frame_group_start_pts_(kNoTimestamp),
      range_for_next_append_(ranges_.end()),
      highest_output_buffer_timestamp_(kNoTimestamp),
      max_interbuffer_distance_(
          base::Milliseconds(kMinimumInterbufferDistanceInMs)),
      memory_limit_(
          GetDemuxerStreamVideoMemoryLimit(Demuxer::DemuxerTypes::kChunkDemuxer,
                                           &video_config)) {
  DCHECK(video_config.IsValidConfig());
  video_configs_.push_back(video_config);
  DVLOG(2) << __func__ << ": video_buffer_size= " << memory_limit_;
}

SourceBufferStream::~SourceBufferStream() = default;

void SourceBufferStream::OnStartOfCodedFrameGroup(
    base::TimeDelta coded_frame_group_start_pts) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " (pts "
           << coded_frame_group_start_pts.InMicroseconds() << "us)";
  DCHECK(!end_of_stream_);
  coded_frame_group_start_pts_ = coded_frame_group_start_pts;
  new_coded_frame_group_ = true;

  auto last_range = range_for_next_append_;
  range_for_next_append_ = FindExistingRangeFor(coded_frame_group_start_pts_);

  // Only reset |last_appended_buffer_timestamp_| if this new coded frame group
  // is not adjacent to the previous coded frame group appended to the stream.
  if (range_for_next_append_ == ranges_.end() ||
      !IsNextGopAdjacentToEndOfCurrentAppendSequence(
          coded_frame_group_start_pts_)) {
    ResetLastAppendedState();
    DVLOG(3) << __func__ << " next appended buffers will "
             << (range_for_next_append_ == ranges_.end()
                     ? "be in a new range"
                     : "overlap an existing range");

    if (range_for_next_append_ != ranges_.end()) {
      // If this new coded frame group overlaps an existing range, preserve
      // continuity from that range to the new group by moving the start time
      // earlier (but not at or beyond the most recent buffered frame's time
      // before |coded_frame_group_start_pts_| in the range, and not beyond the
      // range's start time. This update helps prevent discontinuity from being
      // introduced by the ::RemoveInternal processing during the next ::Append
      // call.
      base::TimeDelta adjusted_start_time =
          (*range_for_next_append_)
              ->FindHighestBufferedTimestampAtOrBefore(
                  coded_frame_group_start_pts_);
      if (adjusted_start_time < coded_frame_group_start_pts_) {
        // Exclude removal of that earlier frame during later Append
        // processing by adjusting the removal range slightly forward.
        coded_frame_group_start_pts_ =
            adjusted_start_time + base::Microseconds(1);
      }
    }
  } else if (last_range != ranges_.end()) {
    DCHECK(last_range == range_for_next_append_);
    DVLOG(3) << __func__ << " next appended buffers will continue range unless "
             << "intervening remove makes discontinuity";
  }
}

void SourceBufferStream::Append(const BufferQueue& buffers) {
  TRACE_EVENT2("media", "SourceBufferStream::Append",
               "stream type", GetStreamTypeName(),
               "buffers to append", buffers.size());

  DCHECK(!buffers.empty());
  DCHECK(coded_frame_group_start_pts_ != kNoTimestamp);
  DCHECK(!end_of_stream_);

  DVLOG(1) << __func__ << " " << GetStreamTypeName() << ": buffers "
           << BufferQueueMetadataToLogString(buffers);
  DVLOG(4) << BufferQueueBuffersToLogString(buffers);

  DCHECK(!buffers.front()->is_key_frame() ||
         coded_frame_group_start_pts_ <= buffers.front()->timestamp());
  DVLOG_IF(2, coded_frame_group_start_pts_ > buffers.front()->timestamp())
      << __func__
      << " Suspected SAP-Type-2 occurrence: coded_frame_group_start_pts_="
      << coded_frame_group_start_pts_.InMicroseconds()
      << "us, first new buffer has timestamp="
      << buffers.front()->timestamp().InMicroseconds() << "us";

  // New coded frame groups emitted by the coded frame processor must begin with
  // a keyframe. Avoid propagating with escalating impact if this assumption is
  // broken.
  CHECK(!new_coded_frame_group_ || buffers.front()->is_key_frame());

  // Buffers within each GOP in a coded frame group must be monotonically
  // increasing in DTS order.
  DCHECK(IsDtsMonotonicallyIncreasing(buffers));

  // Both of these checks enforce what should be guaranteed by how
  // FrameProcessor signals OnStartOfCodedFrameGroup and the buffers it tells us
  // to Append.
  DCHECK(coded_frame_group_start_pts_ >= base::TimeDelta());
  DCHECK(buffers.front()->timestamp() >= base::TimeDelta());

  if (UpdateMaxInterbufferDtsDistance(buffers)) {
    // Coalesce |ranges_| using the new fudge room. This helps keep |ranges_|
    // sorted in complex scenarios.  See https://crbug.com/793247.
    MergeAllAdjacentRanges();
  }

  SetConfigIds(buffers);

  // Save a snapshot of stream state before range modifications are made.
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();
  BufferQueue deleted_buffers;

  PrepareRangesForNextAppend(buffers, &deleted_buffers);

  // If there's a range for |buffers|, insert |buffers| accordingly. Otherwise,
  // create a new range with |buffers|.
  if (range_for_next_append_ != ranges_.end()) {
    if (new_coded_frame_group_) {
      // If the first append to this stream in a new coded frame group continues
      // a previous range, use the new group's start time instead of the first
      // new buffer's timestamp as the proof of adjacency to the existing range.
      // A large gap (larger than our normal buffer adjacency test) can occur in
      // a muxed set of streams (which share a common coded frame group start
      // time) with a significantly jagged start across the streams.
      (*range_for_next_append_)
          ->AppendBuffersToEnd(buffers, coded_frame_group_start_pts_);
    } else {
      // Otherwise, use the first new buffer as proof of adjacency.
      (*range_for_next_append_)->AppendBuffersToEnd(buffers, kNoTimestamp);
    }

    last_appended_buffer_timestamp_ = buffers.back()->timestamp();
    last_appended_buffer_duration_ = buffers.back()->duration();
    last_appended_buffer_is_keyframe_ = buffers.back()->is_key_frame();
    last_appended_buffer_decode_timestamp_ =
        buffers.back()->GetDecodeTimestamp();
    highest_timestamp_in_append_sequence_ =
        (*range_for_next_append_)->GetEndTimestamp();
    highest_buffered_end_time_in_append_sequence_ =
        (*range_for_next_append_)->GetBufferedEndTimestamp();
  } else {
    base::TimeDelta new_range_start_time =
        std::min(coded_frame_group_start_pts_, buffers.front()->timestamp());

    const BufferQueue* buffers_for_new_range = &buffers;
    BufferQueue trimmed_buffers;

    // If the new range is not being created because of a new coded frame group,
    // then we must make sure that we start with a key frame.  This can happen
    // if the GOP in the previous append gets destroyed by a Remove() call.
    if (!new_coded_frame_group_) {
      BufferQueue::const_iterator itr = buffers.begin();

      // Scan past all the non-key-frames.
      while (itr != buffers.end() && !(*itr)->is_key_frame()) {
        ++itr;
      }

      // If we didn't find a key frame, then update the last appended
      // buffer state and return.
      if (itr == buffers.end()) {
        last_appended_buffer_timestamp_ = buffers.back()->timestamp();
        last_appended_buffer_duration_ = buffers.back()->duration();
        last_appended_buffer_is_keyframe_ = buffers.back()->is_key_frame();
        last_appended_buffer_decode_timestamp_ =
            buffers.back()->GetDecodeTimestamp();
        // Since we didn't buffer anything, don't update
        // |highest_timestamp_in_append_sequence_|.
        DVLOG(1) << __func__ << " " << GetStreamTypeName()
                 << ": new buffers in the middle of coded frame group depend on"
                    " keyframe that has been removed, and contain no keyframes."
                    " Skipping further processing.";
        DVLOG(1) << __func__ << " " << GetStreamTypeName()
                 << ": done. ranges_=" << RangesToString(ranges_);
        return;
      } else if (itr != buffers.begin()) {
        // Copy the first key frame and everything after it into
        // |trimmed_buffers|.
        trimmed_buffers.assign(itr, buffers.end());
        buffers_for_new_range = &trimmed_buffers;
      }

      new_range_start_time = buffers_for_new_range->front()->timestamp();
    }

    range_for_next_append_ = AddToRanges(std::make_unique<SourceBufferRange>(
        SourceBufferRange::NO_GAPS_ALLOWED, *buffers_for_new_range,
        new_range_start_time,
        base::BindRepeating(&SourceBufferStream::GetMaxInterbufferDistance,
                            base::Unretained(this))));

    last_appended_buffer_timestamp_ =
        buffers_for_new_range->back()->timestamp();
    last_appended_buffer_duration_ = buffers_for_new_range->back()->duration();
    last_appended_buffer_is_keyframe_ =
        buffers_for_new_range->back()->is_key_frame();
    last_appended_buffer_decode_timestamp_ =
        buffers_for_new_range->back()->GetDecodeTimestamp();
    highest_timestamp_in_append_sequence_ =
        (*range_for_next_append_)->GetEndTimestamp();
    highest_buffered_end_time_in_append_sequence_ =
        (*range_for_next_append_)->GetBufferedEndTimestamp();
  }

  new_coded_frame_group_ = false;

  MergeWithNextRangeIfNecessary(range_for_next_append_);

  // Some SAP-Type-2 append sequences require that we coalesce
  // |range_for_next_append_| with the range that is *before* it.
  if (range_for_next_append_ != ranges_.begin()) {
    auto prior_range = range_for_next_append_;
    prior_range--;
    MergeWithNextRangeIfNecessary(prior_range);
  }

  // Seek to try to fulfill a previous call to Seek().
  if (seek_pending_) {
    DCHECK(!selected_range_);
    DCHECK(deleted_buffers.empty());
    Seek(seek_buffer_timestamp_);
  }

  if (!deleted_buffers.empty()) {
    track_buffer_.insert(track_buffer_.end(), deleted_buffers.begin(),
                         deleted_buffers.end());
    DVLOG(3) << __func__ << " " << GetStreamTypeName() << " Added "
             << deleted_buffers.size()
             << " buffers to track buffer. TB size is now "
             << track_buffer_.size();
  } else {
    DVLOG(3) << __func__ << " " << GetStreamTypeName()
             << " No deleted buffers for track buffer";
  }

  // Prune any extra buffers in |track_buffer_| if new keyframes
  // are appended to the range covered by |track_buffer_|.
  if (!track_buffer_.empty()) {
    base::TimeDelta keyframe_timestamp =
        FindKeyframeAfterTimestamp(track_buffer_.front()->timestamp());
    if (keyframe_timestamp != kNoTimestamp)
      PruneTrackBuffer(keyframe_timestamp);
  }

  SetSelectedRangeIfNeeded(next_buffer_timestamp);

  DVLOG(1) << __func__ << " " << GetStreamTypeName()
           << ": done. ranges_=" << RangesToString(ranges_);
  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
}

void SourceBufferStream::Remove(base::TimeDelta start,
                                base::TimeDelta end,
                                base::TimeDelta duration) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << start.InMicroseconds() << "us, " << end.InMicroseconds() << "us, "
           << duration.InMicroseconds() << "us)";
  DCHECK(start >= base::TimeDelta()) << start.InMicroseconds() << "us";
  DCHECK(start < end) << "start " << start.InMicroseconds() << "us, end "
                      << end.InMicroseconds() << "us";
  DCHECK(duration != kNoTimestamp);

  base::TimeDelta remove_end_timestamp = duration;
  base::TimeDelta keyframe_timestamp = FindKeyframeAfterTimestamp(end);
  if (keyframe_timestamp != kNoTimestamp) {
    remove_end_timestamp = keyframe_timestamp;
  } else if (end < remove_end_timestamp) {
    remove_end_timestamp = end;
  }

  BufferQueue deleted_buffers;
  RemoveInternal(start, remove_end_timestamp, false, &deleted_buffers);

  if (!deleted_buffers.empty()) {
    // Buffers for the current position have been removed.
    SetSelectedRangeIfNeeded(deleted_buffers.front()->timestamp());
    if (highest_output_buffer_timestamp_ == kNoTimestamp) {
      // We just removed buffers for the current playback position for this
      // stream, yet we also had output no buffer since the last Seek.
      // Re-seek to prevent stall.
      DVLOG(1) << __func__ << " " << GetStreamTypeName() << ": re-seeking to "
               << seek_buffer_timestamp_
               << " to prevent stall if this time becomes buffered again";
      Seek(seek_buffer_timestamp_);
    }
  }

  DCHECK(OnlySelectedRangeIsSeeked());
  DCHECK(IsRangeListSorted(ranges_));
}

base::TimeDelta SourceBufferStream::PotentialNextAppendTimestamp() const {
  // The next potential append will either be in a GOP adjacent to
  // |highest_timestamp_in_append_sequence_| (if known), or if unknown and we
  // are still at the beginning of a new coded frame group, then will be into
  // the range (if any) to which |coded_frame_group_start_pts_| belongs.
  if (highest_timestamp_in_append_sequence_ != kNoTimestamp)
    return highest_timestamp_in_append_sequence_;

  if (new_coded_frame_group_)
    return coded_frame_group_start_pts_;

  // If we still don't know a potential next append timestamp, then we have
  // removed the range to which it previously belonged and have not completed a
  // subsequent append or received a subsequent OnStartOfCodedFrameGroup()
  // signal.
  return kNoTimestamp;
}

void SourceBufferStream::UpdateLastAppendStateForRemove(
    base::TimeDelta remove_start,
    base::TimeDelta remove_end,
    bool exclude_start) {
  // TODO(chcunningham): change exclude_start to include_start in this class and
  // SourceBufferRange. Negatives are hard to reason about.
  bool include_start = !exclude_start;

  // No need to check previous append's GOP if starting a new CFG. New CFG is
  // already required to begin with a key frame.
  if (new_coded_frame_group_)
    return;

  if (range_for_next_append_ != ranges_.end()) {
    if (last_appended_buffer_timestamp_ != kNoTimestamp) {
      // Note start and end of last appended GOP.
      base::TimeDelta gop_end = highest_timestamp_in_append_sequence_;
      base::TimeDelta gop_start =
          (*range_for_next_append_)->KeyframeBeforeTimestamp(gop_end);

      // If last append is about to be disrupted, reset associated state so we
      // know to create a new range for future appends and require an initial
      // key frame.
      if (((include_start && remove_start == gop_end) ||
           remove_start < gop_end) &&
          remove_end > gop_start) {
        DVLOG(2) << __func__ << " " << GetStreamTypeName()
                 << " Resetting next append state for remove ("
                 << remove_start.InMicroseconds() << "us, "
                 << remove_end.InMicroseconds() << "us, " << exclude_start
                 << ")";
        range_for_next_append_ = ranges_.end();
        ResetLastAppendedState();
      }
    } else {
      NOTREACHED_IN_MIGRATION()
          << __func__ << " " << GetStreamTypeName()
          << " range_for_next_append_ set, but not tracking last"
          << " append nor new coded frame group.";
    }
  }
}

void SourceBufferStream::RemoveInternal(base::TimeDelta start,
                                        base::TimeDelta end,
                                        bool exclude_start,
                                        BufferQueue* deleted_buffers) {
  DVLOG(2) << __func__ << " " << GetStreamTypeName() << " ("
           << start.InMicroseconds() << "us, " << end.InMicroseconds() << "us, "
           << exclude_start << ")";
  DVLOG(3) << __func__ << " " << GetStreamTypeName()
           << ": before remove ranges_=" << RangesToString(ranges_);

  DCHECK(start >= base::TimeDelta());
  DCHECK(start < end) << "start " << start.InMicroseconds() << "us, end "
                      << end.InMicroseconds() << "us";
  DCHECK(deleted_buffers);

  // Doing this up-front simplifies decisions about |range_for_next_append_|
  // below.
  UpdateLastAppendStateForRemove(start, end, exclude_start);

  auto itr = ranges_.begin();
  while (itr != ranges_.end()) {
    SourceBufferRange* range = itr->get();
    if (range->GetStartTimestamp() >= end)
      break;

    // Split off any remaining GOPs starting at or after |end| and add it to
    // |ranges_|.
    std::unique_ptr<SourceBufferRange> new_range = range->SplitRange(end);
    if (new_range) {
      itr = ranges_.insert(++itr, std::move(new_range));

      // Update |range_for_next_append_| if it was previously |range| and should
      // be the new range (that |itr| is at) now.
      if (range_for_next_append_ != ranges_.end() &&
          range_for_next_append_->get() == range) {
        base::TimeDelta potential_next_append_timestamp =
            PotentialNextAppendTimestamp();
        if (potential_next_append_timestamp != kNoTimestamp &&
            (*itr)->BelongsToRange(potential_next_append_timestamp)) {
          range_for_next_append_ = itr;
        }
      }

      // Update the selected range if the next buffer position was transferred
      // to the newly inserted range (that |itr| is at now).
      if ((*itr)->HasNextBufferPosition())
        SetSelectedRange(itr->get());

      --itr;
    }

    // Truncate the current range so that it only contains data before
    // the removal range.
    BufferQueue saved_buffers;
    bool delete_range = range->TruncateAt(start, &saved_buffers, exclude_start);

    // Check to see if the current playback position was removed and update the
    // selected range appropriately.
    if (!saved_buffers.empty()) {
      DCHECK(!range->HasNextBufferPosition());
      DCHECK(deleted_buffers->empty());

      *deleted_buffers = saved_buffers;
    }

    if (range == selected_range_ && !range->HasNextBufferPosition())
      SetSelectedRange(NULL);

    // If the current range now is completely covered by the removal
    // range then delete it and move on.
    if (delete_range) {
      DeleteAndRemoveRange(&itr);
      continue;
    }

    // Clear |range_for_next_append_| if we determine that the removal
    // operation makes it impossible for the next append to be added
    // to the current range.
    if (range_for_next_append_ != ranges_.end() &&
        range_for_next_append_->get() == range) {
      base::TimeDelta potential_next_append_timestamp =
          PotentialNextAppendTimestamp();

      if (!range->BelongsToRange(potential_next_append_timestamp)) {
        DVLOG(1) << "Resetting range_for_next_append_ since the next append"
                 <<  " can't add to the current range.";
        range_for_next_append_ =
            FindExistingRangeFor(potential_next_append_timestamp);
      }
    }

    // Move on to the next range.
    ++itr;
  }

  DVLOG(3) << __func__ << " " << GetStreamTypeName()
           << ": after remove ranges_=" << RangesToString(ranges_);

  DCHECK(OnlySelectedRangeIsSeeked());
}

void SourceBufferStream::ResetSeekState() {
  SetSelectedRange(NULL);
  track_buffer_.clear();
  config_change_pending_ = false;
  highest_output_buffer_timestamp_ = kNoTimestamp;
  just_exhausted_track_buffer_ = false;
  pending_buffer_.reset();
  pending_buffers_complete_ = false;
}

void SourceBufferStream::ResetLastAppendedState() {
  last_appended_buffer_timestamp_ = kNoTimestamp;
  last_appended_buffer_duration_ = kNoTimestamp;
  last_appended_buffer_is_keyframe_ = false;
  last_appended_buffer_decode_timestamp_ = kNoDecodeTimestamp;
  highest_timestamp_in_append_sequence_ = kNoTimestamp;
  highest_buffered_end_time_in_append_sequence_ = kNoTimestamp;
}

bool SourceBufferStream::ShouldSeekToStartOfBuffered(
    base::TimeDelta seek_timestamp) const {
  if (ranges_.empty())
    return false;
  base::TimeDelta beginning_of_buffered = ranges_.front()->GetStartTimestamp();
  return (seek_timestamp <= beginning_of_buffered &&
          beginning_of_buffered < kSeekToStartFudgeRoom());
}

bool SourceBufferStream::IsDtsMonotonicallyIncreasing(
    const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  DecodeTimestamp prev_dts = last_appended_buffer_decode_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    DecodeTimestamp current_dts = (*itr)->GetDecodeTimestamp();
    bool current_is_keyframe = (*itr)->is_key_frame();
    DCHECK(current_dts != kNoDecodeTimestamp);
    DCHECK((*itr)->duration() >= base::TimeDelta())
        << "Packet with invalid duration."
        << " pts " << (*itr)->timestamp().InMicroseconds() << "us dts "
        << (*itr)->GetDecodeTimestamp().InMicroseconds() << "us dur "
        << (*itr)->duration().InMicroseconds() << "us";

    // Only verify DTS monotonicity within the current GOP (since the last
    // keyframe). FrameProcessor should have enforced that all audio frames are
    // keyframes already, or are nonkeyframes with monotonically increasing PTS
    // since the last keyframe for those types of audio for which nonkeyframes
    // may be involved, e.g. xHE-AAC. Video nonkeyframes are not restricted to
    // being in-order by PTS, but both audio and video nonkeyframes must be in
    // decode sequence since the last keyframe.
    if (current_is_keyframe) {
      // Reset prev_dts tracking since a new GOP is starting.
      prev_dts = kNoDecodeTimestamp;
    }

    if (prev_dts != kNoDecodeTimestamp) {
      if (current_dts < prev_dts) {
        MEDIA_LOG(ERROR, media_log_)
            << "Buffers did not monotonically increase.";
        return false;
      }
    }

    prev_dts = current_dts;
  }
  return true;
}

bool SourceBufferStream::OnlySelectedRangeIsSeeked() const {
  for (auto itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->HasNextBufferPosition() && itr->get() != selected_range_)
      return false;
  }
  return !selected_range_ || selected_range_->HasNextBufferPosition();
}

bool SourceBufferStream::UpdateMaxInterbufferDtsDistance(
    const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  base::TimeDelta old_distance = max_interbuffer_distance_;
  DecodeTimestamp prev_dts = last_appended_buffer_decode_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    DecodeTimestamp current_dts = (*itr)->GetDecodeTimestamp();
    DCHECK(current_dts != kNoDecodeTimestamp);

    base::TimeDelta interbuffer_distance = (*itr)->duration();
    DCHECK(interbuffer_distance >= base::TimeDelta());

    if (prev_dts != kNoDecodeTimestamp) {
      interbuffer_distance =
          std::max(current_dts - prev_dts, interbuffer_distance);
    }

    DCHECK(max_interbuffer_distance_ >=
           base::Milliseconds(kMinimumInterbufferDistanceInMs));
    max_interbuffer_distance_ =
        std::max(max_interbuffer_distance_, interbuffer_distance);
    prev_dts = current_dts;
  }
  bool changed_max = max_interbuffer_distance_ != old_distance;
  DVLOG_IF(2, changed_max) << __func__ << " " << GetStreamTypeName()
                           << " Changed max interbuffer DTS distance from "
                           << old_distance.InMicroseconds() << "us to "
                           << max_interbuffer_distance_.InMicroseconds()
                           << "us";
  return changed_max;
}

void SourceBufferStream::SetConfigIds(const BufferQueue& buffers) {
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetConfigId(append_config_index_);
  }
}

void SourceBufferStream::OnMemoryPressure(
    base::TimeDelta media_time,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
    bool force_instant_gc) {
  DVLOG(4) << __func__ << " level=" << memory_pressure_level;
  // TODO(sebmarchand): Check if MEMORY_PRESSURE_LEVEL_MODERATE should also be
  // ignored.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  memory_pressure_level_ = memory_pressure_level;

  if (force_instant_gc)
    GarbageCollectIfNeeded(media_time, 0);
}

bool SourceBufferStream::GarbageCollectIfNeeded(base::TimeDelta media_time,
                                                size_t newDataSize) {
  DCHECK(media_time != kNoTimestamp);
  // Garbage collection should only happen before/during appending new data,
  // which should not happen in end-of-stream state. Unless we also allow GC to
  // happen on memory pressure notifications, which might happen even in EOS
  // state.
  if (!base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC))
    DCHECK(!end_of_stream_);
  // Compute size of |ranges_|.
  size_t ranges_size = GetMemoryUsage();

  // Sanity and overflow checks
  if ((newDataSize > memory_limit_) ||
      (ranges_size + newDataSize < ranges_size)) {
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_garbage_collect_algorithm_logs_,
                      kMaxGarbageCollectAlgorithmWarningLogs)
        << GetStreamTypeName() << " stream: "
        << "new append of newDataSize=" << newDataSize
        << " bytes exceeds memory_limit_=" << memory_limit_
        << ", currently buffered ranges_size=" << ranges_size;
    return false;
  }

  size_t effective_memory_limit = memory_limit_;
  if (base::FeatureList::IsEnabled(kMemoryPressureBasedSourceBufferGC)) {
    switch (memory_pressure_level_) {
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
        effective_memory_limit = memory_limit_ / 2;
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
        effective_memory_limit = 0;
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
        break;
    }
  }

  // Return if we're under or at the memory limit.
  if (ranges_size + newDataSize <= effective_memory_limit)
    return true;

  size_t bytes_over_hard_memory_limit = 0;
  if (ranges_size + newDataSize > memory_limit_)
    bytes_over_hard_memory_limit = ranges_size + newDataSize - memory_limit_;

  size_t bytes_to_free = ranges_size + newDataSize - effective_memory_limit;

  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << ": Before GC media_time=" << media_time.InMicroseconds()
           << "us ranges_=" << RangesToString(ranges_)
           << " seek_pending_=" << seek_pending_
           << " ranges_size=" << ranges_size << " newDataSize=" << newDataSize
           << " memory_limit_=" << memory_limit_
           << " effective_memory_limit=" << effective_memory_limit
           << " last_appended_buffer_timestamp_="
           << last_appended_buffer_timestamp_.InMicroseconds()
           << "us highest_timestamp_in_append_sequence_="
           << highest_timestamp_in_append_sequence_.InMicroseconds()
           << "us highest_buffered_end_time_in_append_sequence_="
           << highest_buffered_end_time_in_append_sequence_.InMicroseconds()
           << "us";

  if (selected_range_ && !seek_pending_ &&
      media_time > selected_range_->GetBufferedEndTimestamp()) {
    // Strictly speaking |media_time| (taken from HTMLMediaElement::currentTime)
    // should always be in the buffered ranges, but media::Pipeline uses audio
    // stream as the main time source, when audio is present.
    // In cases when audio and video streams have different buffered ranges, the
    // |media_time| value might be slightly outside of the video stream buffered
    // range. In those cases we need to clamp |media_time| value to the current
    // stream buffered ranges, to ensure the MSE garbage collection algorithm
    // works correctly (see crbug.com/563292 for details).
    base::TimeDelta selected_buffered_end =
        selected_range_->GetBufferedEndTimestamp();

    DVLOG(2) << __func__ << " media_time " << media_time.InMicroseconds()
             << "us is outside of selected_range_=["
             << selected_range_->GetStartTimestamp().InMicroseconds() << "us;"
             << selected_buffered_end.InMicroseconds()
             << "us] clamping media_time to be "
             << selected_buffered_end.InMicroseconds() << "us";
    media_time = selected_buffered_end;
  }

  size_t bytes_freed = 0;

  // If last appended buffer position was earlier than the current playback time
  // then try deleting data between last append and current media_time.
  if (last_appended_buffer_timestamp_ != kNoTimestamp &&
      last_appended_buffer_duration_ != kNoTimestamp &&
      highest_buffered_end_time_in_append_sequence_ != kNoTimestamp &&
      media_time > highest_buffered_end_time_in_append_sequence_) {
    size_t between = FreeBuffersAfterLastAppended(bytes_to_free, media_time);
    DVLOG(3) << __func__ << " FreeBuffersAfterLastAppended "
             << " released " << between << " bytes"
             << " ranges_=" << RangesToString(ranges_);
    bytes_freed += between;

    // Some players start appending data at the new seek target position before
    // actually initiating the seek operation (i.e. they try to improve seek
    // performance by prebuffering some data at the seek target position and
    // initiating seek once enough data is pre-buffered. In those cases we'll
    // see that data is being appended at some new position, but there is no
    // pending seek reported yet. In this situation we need to try preserving
    // the most recently appended data, i.e. data belonging to the same buffered
    // range as the most recent append.
    if (range_for_next_append_ != ranges_.end()) {
      DCHECK((*range_for_next_append_)->GetStartTimestamp() <= media_time);
      media_time = (*range_for_next_append_)->GetStartTimestamp();
      DVLOG(3) << __func__ << " media_time adjusted to "
               << media_time.InMicroseconds() << "us";
    }
  }

  // If there is an unsatisfied pending seek, we can safely remove all data that
  // is earlier than seek target, then remove from the back until we reach the
  // most recently appended GOP and then remove from the front if we still don't
  // have enough space for the upcoming append.
  if (bytes_freed < bytes_to_free && seek_pending_) {
    DCHECK(!ranges_.empty());
    // All data earlier than the seek target |media_time| can be removed safely
    size_t front = FreeBuffers(bytes_to_free - bytes_freed, media_time, false);
    DVLOG(3) << __func__ << " Removed " << front
             << " bytes from the front. ranges_=" << RangesToString(ranges_);
    bytes_freed += front;

    // If removing data earlier than |media_time| didn't free up enough space,
    // then try deleting from the back until we reach most recently appended GOP
    if (bytes_freed < bytes_to_free) {
      size_t back = FreeBuffers(bytes_to_free - bytes_freed, media_time, true);
      DVLOG(3) << __func__ << " Removed " << back
               << " bytes from the back. ranges_=" << RangesToString(ranges_);
      bytes_freed += back;
    }

    // If even that wasn't enough, then try greedily deleting from the front,
    // that should allow us to remove as much data as necessary to succeed.
    if (bytes_freed < bytes_to_free) {
      size_t front2 = FreeBuffers(bytes_to_free - bytes_freed,
                                  ranges_.back()->GetEndTimestamp(), false);
      DVLOG(3) << __func__ << " Removed " << front2
               << " bytes from the front. ranges_=" << RangesToString(ranges_);
      bytes_freed += front2;
    }
    DCHECK(bytes_freed >= bytes_to_free);
  }

  // Try removing data from the front of the SourceBuffer up to |media_time|
  // position.
  if (bytes_freed < bytes_to_free) {
    size_t front = FreeBuffers(bytes_to_free - bytes_freed, media_time, false);
    DVLOG(3) << __func__ << " Removed " << front
             << " bytes from the front. ranges_=" << RangesToString(ranges_);
    bytes_freed += front;
  }

  // Try removing data from the back of the SourceBuffer, until we reach the
  // most recent append position.
  if (bytes_freed < bytes_to_free) {
    size_t back = FreeBuffers(bytes_to_free - bytes_freed, media_time, true);
    DVLOG(3) << __func__ << " Removed " << back
             << " bytes from the back. ranges_=" << RangesToString(ranges_);
    bytes_freed += back;
  }

  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << ": After GC bytes_to_free=" << bytes_to_free
           << " bytes_freed=" << bytes_freed
           << " bytes_over_hard_memory_limit=" << bytes_over_hard_memory_limit
           << " ranges_=" << RangesToString(ranges_);

  return bytes_freed >= bytes_over_hard_memory_limit;
}

size_t SourceBufferStream::FreeBuffersAfterLastAppended(
    size_t total_bytes_to_free,
    base::TimeDelta media_time) {
  DVLOG(4) << __func__ << " highest_buffered_end_time_in_append_sequence_="
           << highest_buffered_end_time_in_append_sequence_.InMicroseconds()
           << "us media_time=" << media_time.InMicroseconds() << "us";

  base::TimeDelta remove_range_start =
      highest_buffered_end_time_in_append_sequence_;
  if (last_appended_buffer_is_keyframe_)
    remove_range_start += GetMaxInterbufferDistance();

  base::TimeDelta remove_range_start_keyframe =
      FindKeyframeAfterTimestamp(remove_range_start);
  if (remove_range_start_keyframe != kNoTimestamp)
    remove_range_start = remove_range_start_keyframe;
  if (remove_range_start >= media_time)
    return 0;

  base::TimeDelta remove_range_end;
  size_t bytes_freed = GetRemovalRange(remove_range_start,
                                       media_time,
                                       total_bytes_to_free,
                                       &remove_range_end);
  if (bytes_freed > 0) {
    DVLOG(4) << __func__ << " removing [" << remove_range_start.InMicroseconds()
             << "us;" << remove_range_end.InMicroseconds() << "us]";
    Remove(remove_range_start, remove_range_end, media_time);
  }

  return bytes_freed;
}

size_t SourceBufferStream::GetRemovalRange(
    base::TimeDelta start_timestamp,
    base::TimeDelta end_timestamp,
    size_t total_bytes_to_free,
    base::TimeDelta* removal_end_timestamp) {
  DCHECK(start_timestamp >= base::TimeDelta())
      << start_timestamp.InMicroseconds() << "us";
  DCHECK(start_timestamp < end_timestamp)
      << "start " << start_timestamp.InMicroseconds() << "us, end "
      << end_timestamp.InMicroseconds() << "us";

  size_t bytes_freed = 0;

  for (auto itr = ranges_.begin();
       itr != ranges_.end() && bytes_freed < total_bytes_to_free; ++itr) {
    SourceBufferRange* range = itr->get();
    if (range->GetStartTimestamp() >= end_timestamp)
      break;
    if (range->GetEndTimestamp() < start_timestamp)
      continue;

    size_t bytes_to_free = total_bytes_to_free - bytes_freed;
    size_t bytes_removed = range->GetRemovalGOP(
        start_timestamp, end_timestamp, bytes_to_free, removal_end_timestamp);
    bytes_freed += bytes_removed;
  }
  return bytes_freed;
}

size_t SourceBufferStream::FreeBuffers(size_t total_bytes_to_free,
                                       base::TimeDelta media_time,
                                       bool reverse_direction) {
  TRACE_EVENT2("media", "SourceBufferStream::FreeBuffers",
               "total bytes to free", total_bytes_to_free,
               "reverse direction", reverse_direction);

  DCHECK_GT(total_bytes_to_free, 0u);
  size_t bytes_freed = 0;

  // This range will save the last GOP appended to |range_for_next_append_|
  // if the buffers surrounding it get deleted during garbage collection.
  std::unique_ptr<SourceBufferRange> new_range_for_append;

  while (!ranges_.empty() && bytes_freed < total_bytes_to_free) {
    SourceBufferRange* current_range = NULL;
    BufferQueue buffers;
    size_t bytes_deleted = 0;

    if (reverse_direction) {
      current_range = ranges_.back().get();
      DVLOG(5) << "current_range=" << RangeToString(*current_range);
      if (current_range->LastGOPContainsNextBufferPosition()) {
        DCHECK_EQ(current_range, selected_range_);
        DVLOG(5) << "current_range contains next read position, stopping GC";
        break;
      }
      DVLOG(5) << "Deleting GOP from back: " << RangeToString(*current_range);
      bytes_deleted = current_range->DeleteGOPFromBack(&buffers);
    } else {
      current_range = ranges_.front().get();
      DVLOG(5) << "current_range=" << RangeToString(*current_range);

      // FirstGOPEarlierThanMediaTime() is useful here especially if
      // |seek_pending_| (such that no range contains next buffer
      // position).
      // FirstGOPContainsNextBufferPosition() is useful here especially if
      // |!seek_pending_| to protect against DeleteGOPFromFront() if
      // FirstGOPEarlierThanMediaTime() was insufficient alone.
      if (!current_range->FirstGOPEarlierThanMediaTime(media_time) ||
          current_range->FirstGOPContainsNextBufferPosition()) {
        // We have removed all data up to the GOP that contains current playback
        // position, we can't delete any further.
        DVLOG(5) << "current_range contains playback position, stopping GC";
        break;
      }
      DVLOG(4) << "Deleting GOP from front: " << RangeToString(*current_range)
               << ", media_time: " << media_time.InMicroseconds()
               << ", current_range->HasNextBufferPosition(): "
               << current_range->HasNextBufferPosition();
      bytes_deleted = current_range->DeleteGOPFromFront(&buffers);
    }

    // Check to see if we've just deleted the GOP that was last appended.
    base::TimeDelta end_timestamp = buffers.back()->timestamp();
    if (end_timestamp == last_appended_buffer_timestamp_) {
      DCHECK(last_appended_buffer_timestamp_ != kNoTimestamp);
      DCHECK(!new_range_for_append);

      // Create a new range containing these buffers.
      new_range_for_append = std::make_unique<SourceBufferRange>(
          SourceBufferRange::NO_GAPS_ALLOWED, buffers, kNoTimestamp,
          base::BindRepeating(&SourceBufferStream::GetMaxInterbufferDistance,
                              base::Unretained(this)));

      range_for_next_append_ = ranges_.end();
    } else {
      bytes_freed += bytes_deleted;
    }

    if (current_range->GetMemoryUsage() == 0) {
      DCHECK_NE(current_range, selected_range_);
      DCHECK(range_for_next_append_ == ranges_.end() ||
             range_for_next_append_->get() != current_range);

      // Delete |current_range| by popping it out of |ranges_|.
      reverse_direction ? ranges_.pop_back() : ranges_.pop_front();
    }

    if (reverse_direction && new_range_for_append) {
      // We don't want to delete any further, or we'll be creating gaps
      break;
    }
  }

  // Insert |new_range_for_append| into |ranges_|, if applicable.
  if (new_range_for_append) {
    range_for_next_append_ = AddToRanges(std::move(new_range_for_append));
    DCHECK(range_for_next_append_ != ranges_.end());

    // Check to see if we need to merge the just added range that was in
    // |new_range_for_append| with the range before or after it. That added
    // range is created whenever the last GOP appended is encountered,
    // regardless of whether any buffers after it are ultimately deleted.
    // Merging is necessary if there were no buffers (or very few buffers)
    // deleted after creating that added range.
    if (range_for_next_append_ != ranges_.begin()) {
      auto range_before_next = range_for_next_append_;
      --range_before_next;
      MergeWithNextRangeIfNecessary(range_before_next);
    }
    MergeWithNextRangeIfNecessary(range_for_next_append_);
  }
  return bytes_freed;
}

void SourceBufferStream::TrimSpliceOverlap(const BufferQueue& new_buffers) {
  DCHECK(!new_buffers.empty());
  DCHECK_EQ(SourceBufferStreamType::kAudio, GetType());

  const base::TimeDelta splice_timestamp = new_buffers.front()->timestamp();

  // Since some audio formats may have nonkeyframes (in PTS order since last
  // keyframe), if the front of the new buffers is one of those, it cannot be
  // used to begin a decode following an overlap. Here, we bail in this case,
  // since such a splice could not be coherently decoded.
  if (!new_buffers.front()->is_key_frame()) {
    DVLOG(3) << __func__
             << " No splice trimming. Front of |new_buffers| is not a "
                "keyframe, at time "
             << splice_timestamp.InMicroseconds();
    return;
  }

  // Find the overlapped range (if any).
  auto range_itr = FindExistingRangeFor(splice_timestamp);
  if (range_itr == ranges_.end()) {
    DVLOG(3) << __func__ << " No splice trimming. No range overlap at time "
             << splice_timestamp.InMicroseconds();
    return;
  }

  // Search for overlapped buffer needs exclusive end value. Choosing smallest
  // possible value.
  const base::TimeDelta end_pts = splice_timestamp + base::Microseconds(1);

  // Find if new buffer's start would overlap an existing buffer. Note that
  // overlapped audio buffers might be nonkeyframes, but if so, FrameProcessor
  // ensures they are in PTS order since the previous keyframe.
  BufferQueue overlapped_buffers;
  if (!(*range_itr)
           ->GetBuffersInRange(splice_timestamp, end_pts,
                               &overlapped_buffers)) {
    // Bail if no overlapped buffers found.
    DVLOG(3) << __func__ << " No splice trimming. No buffer overlap at time "
             << splice_timestamp.InMicroseconds();
    return;
  }

  // At most one buffer should exist containing the time of the newly appended
  // buffer's start. It may happen that bad content appends buffers with
  // durations that cause nonsensical overlap. Trimming should not be performed
  // in these cases, as the content is already in a bad state.
  if (overlapped_buffers.size() != 1U) {
    DVLOG(3) << __func__
             << " No splice trimming. Found more than one overlapped buffer"
                " (bad content) at time "
             << splice_timestamp.InMicroseconds();

    MEDIA_LOG(WARNING, media_log_)
        << "Media is badly muxed. Detected " << overlapped_buffers.size()
        << " overlapping audio buffers at time "
        << splice_timestamp.InMicroseconds();
    return;
  }
  StreamParserBuffer* overlapped_buffer = overlapped_buffers.front().get();

  if (overlapped_buffer->timestamp() == splice_timestamp) {
    // Ignore buffers with the same start time. They will be completely removed
    // in PrepareRangesForNextAppend().
    DVLOG(3) << __func__ << " No splice trimming at time "
             << splice_timestamp.InMicroseconds()
             << ". Overlapped buffer will be completely removed.";
    return;
  }

  // Trimming a buffer with estimated duration is too risky. Estimates are rough
  // and what appears to be overlap may really just be a bad estimate. Imprecise
  // trimming may lead to loss of AV sync.
  if (overlapped_buffer->is_duration_estimated()) {
    DVLOG(3) << __func__ << " Skipping audio splice trimming at PTS="
             << splice_timestamp.InMicroseconds() << ". Overlapped buffer has "
             << "estimated duration.";
    return;
  }

  // Determine the duration of overlap.
  base::TimeDelta overlapped_end_time =
      overlapped_buffer->timestamp() + overlapped_buffer->duration();
  base::TimeDelta overlap_duration = overlapped_end_time - splice_timestamp;

  // At this point overlap should be non-empty (ruled out same-timestamp above).
  DCHECK_GT(overlap_duration, base::TimeDelta());

  // Don't trim for overlaps of less than one millisecond (which is frequently
  // the extent of timestamp resolution for poorly encoded media).
  if (overlap_duration < base::Milliseconds(1)) {
    std::stringstream log_string;
    log_string << "Skipping audio splice trimming at PTS="
               << splice_timestamp.InMicroseconds() << "us. Found only "
               << overlap_duration.InMicroseconds()
               << "us of overlap, need at least 1000us. Multiple occurrences "
               << "may result in loss of A/V sync.";
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_splice_logs_, kMaxAudioSpliceLogs)
        << log_string.str();
    DVLOG(1) << __func__ << log_string.str();
    return;
  }

  // Trim overlap from the existing buffer.
  DecoderBuffer::DiscardPadding discard_padding =
      overlapped_buffer->discard_padding();
  discard_padding.second += overlap_duration;
  overlapped_buffer->set_discard_padding(discard_padding);
  overlapped_buffer->set_duration(overlapped_buffer->duration() -
                                  overlap_duration);

  // Note that the range's end time tracking shouldn't need explicit updating
  // here due to the overlapped buffer's truncation because the range tracks
  // that end time using a pointer to the buffer (which should be
  // |overlapped_buffer| if the overlap occurred at the end of the range).
  // Every audio frame is either a keyframe, or if a nonkeyframe is in PTS order
  // since the last keyframe, so there is no out-of-order PTS vs DTS sequencing
  // to overcome. If the overlap occurs in the middle of the range, the caller
  // invokes methods on the range which internally update the end time(s) of the
  // resulting range(s) involved in the append.

  std::stringstream log_string;
  log_string << "Audio buffer splice at PTS="
             << splice_timestamp.InMicroseconds()
             << "us. Trimmed tail of overlapped buffer (PTS="
             << overlapped_buffer->timestamp().InMicroseconds() << "us) by "
             << overlap_duration.InMicroseconds() << "us.";
  LIMITED_MEDIA_LOG(DEBUG, media_log_, num_splice_logs_, kMaxAudioSpliceLogs)
      << log_string.str();
  DVLOG(1) << __func__ << log_string.str();
}

void SourceBufferStream::PrepareRangesForNextAppend(
    const BufferQueue& new_buffers,
    BufferQueue* deleted_buffers) {
  DCHECK(deleted_buffers);

  if (GetType() == SourceBufferStreamType::kAudio)
    TrimSpliceOverlap(new_buffers);

  base::TimeDelta buffers_start_timestamp = kNoTimestamp;
  base::TimeDelta buffers_end_timestamp = kNoTimestamp;
  GetTimestampInterval(new_buffers, &buffers_start_timestamp,
                       &buffers_end_timestamp);
  DCHECK(buffers_start_timestamp != kNoTimestamp);
  DCHECK(buffers_end_timestamp != kNoTimestamp);

  // 1. Clean up the old buffers between the last appended buffers and the
  //    beginning of |new_buffers|.
  if (highest_timestamp_in_append_sequence_ != kNoTimestamp &&
      highest_timestamp_in_append_sequence_ < buffers_start_timestamp) {
    RemoveInternal(highest_timestamp_in_append_sequence_,
                   buffers_start_timestamp, true, deleted_buffers);
  }

  // 2. Delete the buffers that |new_buffers| overlaps.
  // There may be buffers in |new_buffers| with timestamp before
  // |highest_timestamp_in_append_sequence_| that shouldn't trigger removal of
  // stuff before |highest_timestamp_in_append_sequence_|.
  if (highest_timestamp_in_append_sequence_ != kNoTimestamp &&
      buffers_start_timestamp < highest_timestamp_in_append_sequence_) {
    DCHECK(highest_timestamp_in_append_sequence_ <=
           highest_buffered_end_time_in_append_sequence_);
    buffers_start_timestamp = highest_buffered_end_time_in_append_sequence_;
  }

  if (new_coded_frame_group_) {
    // Extend the deletion range earlier to the coded frame group start time if
    // this is the first append in a new coded frame group.
    DCHECK(coded_frame_group_start_pts_ != kNoTimestamp);
    buffers_start_timestamp =
        std::min(coded_frame_group_start_pts_, buffers_start_timestamp);
  }

  // Return early if no further overlap removal is needed. First check if
  // |buffers_start_timestamp| is in the middle of the range; we could be
  // overlap-appending the middle of a previous coded frame sequence's range
  // with non-keyframes prior to |highest_timestamp_in_append_sequence_|, so we
  // need to split that range appropriately here and then return early. If we
  // don't return early here, overlap removal (including any necessary range
  // splitting) will occur.
  if (buffers_start_timestamp >= buffers_end_timestamp) {
    DCHECK(highest_timestamp_in_append_sequence_ != kNoTimestamp);
    DCHECK(range_for_next_append_ != ranges_.end());
    DCHECK((*range_for_next_append_)->BelongsToRange(buffers_start_timestamp));

    // Split the range at |buffers_start_timestamp|, if necessary, then return
    // early.
    std::unique_ptr<SourceBufferRange> new_range =
        (*range_for_next_append_)->SplitRange(buffers_start_timestamp);
    if (!new_range)
      return;

    range_for_next_append_ =
        ranges_.insert(++range_for_next_append_, std::move(new_range));

    // Update the selected range if the next buffer position was transferred
    // to the newly inserted range.
    if ((*range_for_next_append_)->HasNextBufferPosition())
      SetSelectedRange(range_for_next_append_->get());

    --range_for_next_append_;
    return;
  }

  // Exclude the start from removal to avoid deleting the highest appended
  // buffer in cases where the first buffer in |new_buffers| has same timestamp
  // as the highest appended buffer (even in out-of-order DTS vs PTS sequence).
  // Only do this when :
  //   A. Type is video. This may occur in cases of VP9 alt-ref frames or frames
  //      with incorrect timestamps. Removing a frame may break decode
  //      dependencies and there are no downsides to just keeping it (other than
  //      some throw-away decoder work).
  //   B. Type is audio and overlapped duration is 0. We've encountered Vorbis
  //      streams containing zero-duration buffers (i.e. no real overlap). For
  //      non-zero duration removing overlapped frames is important to preserve
  //      A/V sync (see AudioClock).
  const bool exclude_start =
      highest_timestamp_in_append_sequence_ ==
          new_buffers.front()->timestamp() &&
      (GetType() == SourceBufferStreamType::kVideo ||
       last_appended_buffer_duration_ == base::TimeDelta());

  // Finally do the deletion of overlap.
  RemoveInternal(buffers_start_timestamp, buffers_end_timestamp, exclude_start,
                 deleted_buffers);
}

// static
void SourceBufferStream::GetTimestampInterval(const BufferQueue& buffers,
                                              base::TimeDelta* start,
                                              base::TimeDelta* end) {
  base::TimeDelta start_pts = buffers.front()->timestamp();
  base::TimeDelta end_pts = start_pts;

  for (const auto& buffer : buffers) {
    base::TimeDelta timestamp = buffer->timestamp();
    start_pts = std::min(timestamp, start_pts);
    base::TimeDelta duration = buffer->duration();

    // FrameProcessor should protect against unknown buffer durations.
    DCHECK_NE(duration, kNoTimestamp);

    if (duration.is_positive() && !buffer->is_duration_estimated()) {
      timestamp += duration;
    } else {
      // TODO(chcunningham): Emit warning when 0ms durations are not expected.
      // http://crbug.com/312836
      timestamp += base::Microseconds(1);
    }
    end_pts = std::max(timestamp, end_pts);
  }
  *start = start_pts;
  *end = end_pts;
}

bool SourceBufferStream::IsNextGopAdjacentToEndOfCurrentAppendSequence(
    base::TimeDelta next_gop_timestamp) const {
  base::TimeDelta upper_bound = highest_timestamp_in_append_sequence_ +
                                ComputeFudgeRoom(GetMaxInterbufferDistance());
  DVLOG(4) << __func__ << " " << GetStreamTypeName()
           << " next_gop_timestamp=" << next_gop_timestamp.InMicroseconds()
           << "us, highest_timestamp_in_append_sequence_="
           << highest_timestamp_in_append_sequence_.InMicroseconds()
           << "us, upper_bound=" << upper_bound.InMicroseconds() << "us";
  return highest_timestamp_in_append_sequence_ < next_gop_timestamp &&
         next_gop_timestamp <= upper_bound;
}

void SourceBufferStream::PruneTrackBuffer(const base::TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp);

  // Scan forward until we find a buffer with timestamp at or beyond the limit.
  // Then remove all those at and beyond that point.
  size_t goal_size = 0;  // The number of buffers we will keep in the result.
  for (const auto& buf : track_buffer_) {
    if (buf->timestamp() >= timestamp)
      break;
    goal_size++;
  }

  while (track_buffer_.size() > goal_size) {
    track_buffer_.pop_back();
  }

  DVLOG(3) << __func__ << " " << GetStreamTypeName()
           << " Removed all buffers in track buffer sequence starting with the "
              "first at timestamp >= "
           << timestamp.InMicroseconds()
           << "us. New track buffer size:" << track_buffer_.size();
}

void SourceBufferStream::MergeWithNextRangeIfNecessary(
    const RangeList::iterator& range_with_new_buffers_itr) {
  DCHECK(range_with_new_buffers_itr != ranges_.end());

  SourceBufferRange* range_with_new_buffers = range_with_new_buffers_itr->get();
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr == ranges_.end() ||
      !range_with_new_buffers->CanAppendRangeToEnd(**next_range_itr)) {
    return;
  }

  bool transfer_current_position = selected_range_ == next_range_itr->get();
  DVLOG(3) << __func__ << " " << GetStreamTypeName() << " merging "
           << RangeToString(*range_with_new_buffers) << " into "
           << RangeToString(**next_range_itr);
  range_with_new_buffers->AppendRangeToEnd(**next_range_itr,
                                           transfer_current_position);
  // Update |selected_range_| pointer if |range| has become selected after
  // merges.
  if (transfer_current_position)
    SetSelectedRange(range_with_new_buffers);

  if (next_range_itr == range_for_next_append_)
    range_for_next_append_ = range_with_new_buffers_itr;

  DeleteAndRemoveRange(&next_range_itr);
}

void SourceBufferStream::MergeAllAdjacentRanges() {
  DVLOG(1) << __func__ << " " << GetStreamTypeName()
           << ": Before: ranges_=" << RangesToString(ranges_);

  auto range_itr = ranges_.begin();

  while (range_itr != ranges_.end()) {
    const size_t old_ranges_size = ranges_.size();
    MergeWithNextRangeIfNecessary(range_itr);

    // Only proceed to the next range if the current range didn't merge with it.
    if (old_ranges_size == ranges_.size())
      range_itr++;
  }

  DVLOG(1) << __func__ << " " << GetStreamTypeName()
           << ": After: ranges_=" << RangesToString(ranges_);
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  DCHECK(timestamp >= base::TimeDelta());
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << timestamp.InMicroseconds() << "us)";
  ResetSeekState();

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  if (ShouldSeekToStartOfBuffered(timestamp)) {
    ranges_.front()->SeekToStart();
    SetSelectedRange(ranges_.front().get());
    seek_pending_ = false;
    return;
  }

  auto itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->CanSeekTo(timestamp))
      break;
  }

  if (itr == ranges_.end())
    return;

  if (!audio_configs_.empty()) {
    // Adjust |timestamp| for an Opus stream backward up to the config's seek
    // preroll, but not further than the range start time, and not at all if
    // there is a config change in the middle of that preroll interval. If
    // |timestamp| is already before the range start time, as can happen due to
    // fudge room, do not adjust it.
    const auto& config = audio_configs_[(*itr)->GetConfigIdAtTime(timestamp)];
    if (config.codec() == AudioCodec::kOpus &&
        timestamp > (*itr)->GetStartTimestamp()) {
      base::TimeDelta preroll_timestamp = std::max(
          timestamp - config.seek_preroll(), (*itr)->GetStartTimestamp());
      if ((*itr)->CanSeekTo(preroll_timestamp) &&
          (*itr)->SameConfigThruRange(preroll_timestamp, timestamp)) {
        timestamp = preroll_timestamp;
      }
    }
  }

  SeekAndSetSelectedRange(itr->get(), timestamp);
  seek_pending_ = false;
}

bool SourceBufferStream::IsSeekPending() const {
  return seek_pending_ && !IsEndOfStreamReached();
}

// TODO(wolenetz): Disallow duration changes that truncate buffered media. See
// https://crbug.com/623729.
void SourceBufferStream::OnSetDuration(base::TimeDelta duration) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << " ("
           << duration.InMicroseconds() << "us)";
  DCHECK(!end_of_stream_);

  if (ranges_.empty())
    return;

  base::TimeDelta start = duration;
  base::TimeDelta end = ranges_.back()->GetBufferedEndTimestamp();

  // Trim the end if it exceeds the new duration.
  if (start < end) {
    BufferQueue deleted_buffers;
    RemoveInternal(start, end, false, &deleted_buffers);

    if (!deleted_buffers.empty()) {
      // Truncation removed current position. Clear selected range.
      SetSelectedRange(NULL);
    }
  }
}

SourceBufferStreamStatus SourceBufferStream::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DVLOG(2) << __func__ << " " << GetStreamTypeName();
  if (!pending_buffer_.get()) {
    const SourceBufferStreamStatus status = GetNextBufferInternal(out_buffer);
    if (status != SourceBufferStreamStatus::kSuccess ||
        !SetPendingBuffer(out_buffer)) {
      DVLOG(2) << __func__ << " " << GetStreamTypeName()
               << ": no pending buffer, returning status "
               << StatusToString(status);
      return status;
    }
  }

  DCHECK(pending_buffer_->preroll_buffer().get());

  const SourceBufferStreamStatus status =
      HandleNextBufferWithPreroll(out_buffer);
  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << ": handled next buffer with preroll, returning status "
           << StatusToString(status);
  return status;
}

SourceBufferStreamStatus SourceBufferStream::HandleNextBufferWithPreroll(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  // Any config change should have already been handled.
  DCHECK_EQ(current_config_index_, pending_buffer_->GetConfigId());

  // Check if the preroll buffer has already been handed out.
  if (!pending_buffers_complete_) {
    pending_buffers_complete_ = true;
    *out_buffer = pending_buffer_->preroll_buffer();
    return SourceBufferStreamStatus::kSuccess;
  }

  // Preroll complete, hand out the final buffer.
  *out_buffer = std::move(pending_buffer_);
  return SourceBufferStreamStatus::kSuccess;
}

SourceBufferStreamStatus SourceBufferStream::GetNextBufferInternal(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  CHECK(!config_change_pending_);

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);

    if (track_buffer_.front()->GetConfigId() != current_config_index_) {
      config_change_pending_ = true;
      DVLOG(1) << "Config change (track buffer config ID does not match).";
      return SourceBufferStreamStatus::kConfigChange;
    }

    DVLOG(3) << __func__ << " Next buffer coming from track_buffer_";
    *out_buffer = std::move(track_buffer_.front());
    track_buffer_.pop_front();
    WarnIfTrackBufferExhaustionSkipsForward(*out_buffer);
    highest_output_buffer_timestamp_ =
        std::max(highest_output_buffer_timestamp_, (*out_buffer)->timestamp());

    // If the track buffer becomes empty, then try to set the selected range
    // based on the timestamp of this buffer being returned.
    if (track_buffer_.empty()) {
      just_exhausted_track_buffer_ = true;
      SetSelectedRangeIfNeeded(highest_output_buffer_timestamp_);
    }

    return SourceBufferStreamStatus::kSuccess;
  }

  DCHECK(track_buffer_.empty());
  if (!selected_range_ || !selected_range_->HasNextBuffer()) {
    if (IsEndOfStreamReached()) {
      return SourceBufferStreamStatus::kEndOfStream;
    }
    DVLOG(3) << __func__ << " " << GetStreamTypeName()
             << ": returning kNeedBuffer "
             << (selected_range_ ? "(selected range has no next buffer)"
                                 : "(no selected range)");
    return SourceBufferStreamStatus::kNeedBuffer;
  }

  if (selected_range_->GetNextConfigId() != current_config_index_) {
    config_change_pending_ = true;
    DVLOG(1) << "Config change (selected range config ID does not match).";
    return SourceBufferStreamStatus::kConfigChange;
  }

  CHECK(selected_range_->GetNextBuffer(out_buffer));
  WarnIfTrackBufferExhaustionSkipsForward(*out_buffer);
  highest_output_buffer_timestamp_ =
      std::max(highest_output_buffer_timestamp_, (*out_buffer)->timestamp());
  return SourceBufferStreamStatus::kSuccess;
}

void SourceBufferStream::WarnIfTrackBufferExhaustionSkipsForward(
    scoped_refptr<StreamParserBuffer> next_buffer) {
  if (!just_exhausted_track_buffer_)
    return;

  just_exhausted_track_buffer_ = false;
  DCHECK(next_buffer->is_key_frame());
  base::TimeDelta next_output_buffer_timestamp = next_buffer->timestamp();
  base::TimeDelta delta =
      next_output_buffer_timestamp - highest_output_buffer_timestamp_;
  if (delta > GetMaxInterbufferDistance()) {
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_track_buffer_gap_warning_logs_,
                      kMaxTrackBufferGapWarningLogs)
        << "Media append that overlapped current playback position may cause "
           "time gap in playing "
        << GetStreamTypeName() << " stream because the next keyframe is "
        << delta.InMilliseconds()
        << "ms beyond last overlapped frame. Media may appear temporarily "
           "frozen.";
  }
}

bool SourceBufferStream::IsNextBufferConfigChanged() {
  if (!track_buffer_.empty())
    return track_buffer_.front()->GetConfigId() != current_config_index_;

  if (!selected_range_ || !selected_range_->HasNextBuffer())
    return false;

  return selected_range_->GetNextConfigId() != current_config_index_;
}

base::TimeDelta SourceBufferStream::GetNextBufferTimestamp() {
  if (!track_buffer_.empty())
    return track_buffer_.front()->timestamp();

  if (!selected_range_)
    return kNoTimestamp;

  DCHECK(selected_range_->HasNextBufferPosition());
  return selected_range_->GetNextTimestamp();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::FindExistingRangeFor(base::TimeDelta start_timestamp) {
  for (auto itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->BelongsToRange(start_timestamp))
      return itr;
  }
  return ranges_.end();
}

SourceBufferStream::RangeList::iterator SourceBufferStream::AddToRanges(
    std::unique_ptr<SourceBufferRange> new_range) {
  base::TimeDelta start_timestamp = new_range->GetStartTimestamp();
  auto itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetStartTimestamp() > start_timestamp)
      break;
  }
  return ranges_.insert(itr, std::move(new_range));
}

void SourceBufferStream::SeekAndSetSelectedRange(
    SourceBufferRange* range,
    base::TimeDelta seek_timestamp) {
  if (range)
    range->Seek(seek_timestamp);
  SetSelectedRange(range);
}

void SourceBufferStream::SetSelectedRange(SourceBufferRange* range) {
  DVLOG(1) << __func__ << " " << GetStreamTypeName() << ": " << selected_range_
           << " " << (selected_range_ ? RangeToString(*selected_range_) : "")
           << " -> " << range << " " << (range ? RangeToString(*range) : "");
  if (selected_range_)
    selected_range_->ResetNextBufferPosition();
  DCHECK(!range || range->HasNextBufferPosition());
  selected_range_ = range;
}

Ranges<base::TimeDelta> SourceBufferStream::GetBufferedTime() const {
  Ranges<base::TimeDelta> ranges;
  for (auto itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    ranges.Add((*itr)->GetStartTimestamp(), (*itr)->GetBufferedEndTimestamp());
  }
  return ranges;
}

base::TimeDelta SourceBufferStream::GetLowestPresentationTimestamp() const {
  if (ranges_.empty()) {
    return base::TimeDelta();
  }

  return ranges_.front()->GetStartTimestamp();
}

base::TimeDelta SourceBufferStream::GetHighestPresentationTimestamp() const {
  if (ranges_.empty())
    return base::TimeDelta();

  return ranges_.back()->GetEndTimestamp();
}

base::TimeDelta SourceBufferStream::GetBufferedDuration() const {
  if (ranges_.empty())
    return base::TimeDelta();

  return ranges_.back()->GetBufferedEndTimestamp();
}

size_t SourceBufferStream::GetMemoryUsage() const {
  size_t memory_usage = 0;
  for (const auto& range_ptr : ranges_)
    memory_usage += range_ptr->GetMemoryUsage();
  return memory_usage;
}

void SourceBufferStream::MarkEndOfStream() {
  DCHECK(!end_of_stream_);
  end_of_stream_ = true;
}

void SourceBufferStream::UnmarkEndOfStream() {
  DCHECK(end_of_stream_);
  end_of_stream_ = false;
}

bool SourceBufferStream::IsEndOfStreamReached() const {
  if (!end_of_stream_ || !track_buffer_.empty())
    return false;

  if (ranges_.empty())
    return true;

  if (seek_pending_) {
    base::TimeDelta last_range_end_time =
        ranges_.back()->GetBufferedEndTimestamp();
    return seek_buffer_timestamp_ >= last_range_end_time;
  }

  if (!selected_range_)
    return true;

  return selected_range_ == ranges_.back().get();
}

const AudioDecoderConfig& SourceBufferStream::GetCurrentAudioDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  // Trying to track down crash. http://crbug.com/715761
  CHECK(current_config_index_ >= 0 &&
        static_cast<size_t>(current_config_index_) < audio_configs_.size());
  return audio_configs_[current_config_index_];
}

const VideoDecoderConfig& SourceBufferStream::GetCurrentVideoDecoderConfig() {
  if (config_change_pending_)
    CompleteConfigChange();
  // Trying to track down crash. http://crbug.com/715761
  CHECK(current_config_index_ >= 0 &&
        static_cast<size_t>(current_config_index_) < video_configs_.size());
  return video_configs_[current_config_index_];
}

base::TimeDelta SourceBufferStream::GetMaxInterbufferDistance() const {
  return max_interbuffer_distance_;
}

bool SourceBufferStream::UpdateAudioConfig(const AudioDecoderConfig& config,
                                           bool allow_codec_change) {
  DCHECK(!audio_configs_.empty());
  DCHECK(video_configs_.empty());
  DVLOG(3) << "UpdateAudioConfig.";

  if (!allow_codec_change &&
      audio_configs_[append_config_index_].codec() != config.codec()) {
    // TODO(wolenetz): When we relax addSourceBuffer() and changeType() codec
    // strictness, codec changes should be allowed even without changing the
    // bytestream.
    // TODO(wolenetz): Remove "experimental" from this error message when
    // changeType() ships without needing experimental blink flag.
    MEDIA_LOG(ERROR, media_log_) << "Audio codec changes not allowed unless "
                                    "using experimental changeType().";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < audio_configs_.size(); ++i) {
    if (config.Matches(audio_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = audio_configs_.size();
  DVLOG(2) << "New audio config - index: " << append_config_index_;
  audio_configs_.resize(audio_configs_.size() + 1);
  audio_configs_[append_config_index_] = config;

  if (memory_limit_overridden_) {
    DVLOG(2)
        << __func__
        << ": Skipping updating memory limit as memory limit was overridden.";
  } else {
    // Dynamically increase |memory_limit_| on audio config changes.
    size_t new_memory_limit = GetDemuxerStreamAudioMemoryLimit(&config);
    if (new_memory_limit > memory_limit_) {
      DVLOG(2) << __func__ << ": Increase memory limit from " << memory_limit_
               << " to " << new_memory_limit << ".";
      memory_limit_ = new_memory_limit;
    }
  }

  return true;
}

bool SourceBufferStream::UpdateVideoConfig(const VideoDecoderConfig& config,
                                           bool allow_codec_change) {
  DCHECK(!video_configs_.empty());
  DCHECK(audio_configs_.empty());
  DVLOG(3) << "UpdateVideoConfig.";

  if (!allow_codec_change &&
      video_configs_[append_config_index_].codec() != config.codec()) {
    // TODO(wolenetz): When we relax addSourceBuffer() and changeType() codec
    // strictness, codec changes should be allowed even without changing the
    // bytestream.
    // TODO(wolenetz): Remove "experimental" from this error message when
    // changeType() ships without needing experimental blink flag.
    MEDIA_LOG(ERROR, media_log_) << "Video codec changes not allowed unless "
                                    "using experimental changeType()";
    return false;
  }

  // Check to see if the new config matches an existing one.
  for (size_t i = 0; i < video_configs_.size(); ++i) {
    if (config.Matches(video_configs_[i])) {
      append_config_index_ = i;
      return true;
    }
  }

  // No matches found so let's add this one to the list.
  append_config_index_ = video_configs_.size();
  DVLOG(2) << "New video config - index: " << append_config_index_;
  video_configs_.resize(video_configs_.size() + 1);
  video_configs_[append_config_index_] = config;

  if (memory_limit_overridden_) {
    DVLOG(2)
        << __func__
        << ": Skipping updating memory limit as memory limit was overridden.";
  } else {
    // Dynamically increase |memory_limit_| on video config changes.
    size_t new_memory_limit = GetDemuxerStreamVideoMemoryLimit(
        Demuxer::DemuxerTypes::kChunkDemuxer, &config);
    if (new_memory_limit > memory_limit_) {
      DVLOG(2) << __func__ << ": Increase memory limit from " << memory_limit_
               << " to " << new_memory_limit << ".";
      memory_limit_ = new_memory_limit;
    }
  }

  return true;
}

void SourceBufferStream::CompleteConfigChange() {
  config_change_pending_ = false;

  if (!track_buffer_.empty()) {
    current_config_index_ = track_buffer_.front()->GetConfigId();
    return;
  }

  if (selected_range_ && selected_range_->HasNextBuffer())
    current_config_index_ = selected_range_->GetNextConfigId();
}

void SourceBufferStream::SetSelectedRangeIfNeeded(
    const base::TimeDelta timestamp) {
  DVLOG(2) << __func__ << " " << GetStreamTypeName() << "("
           << timestamp.InMicroseconds() << "us)";

  if (selected_range_) {
    DCHECK(track_buffer_.empty());
    return;
  }

  if (!track_buffer_.empty()) {
    DCHECK(!selected_range_);
    return;
  }

  base::TimeDelta start_timestamp = timestamp;

  // If the next buffer timestamp is not known then use a timestamp just after
  // the timestamp on the last buffer returned by GetNextBuffer().
  if (start_timestamp == kNoTimestamp) {
    if (highest_output_buffer_timestamp_ == kNoTimestamp) {
      DVLOG(2) << __func__ << " " << GetStreamTypeName()
               << " no previous output timestamp";
      return;
    }

    start_timestamp = highest_output_buffer_timestamp_ + base::Microseconds(1);
  }

  base::TimeDelta seek_timestamp =
      FindNewSelectedRangeSeekTimestamp(start_timestamp);

  // If we don't have buffered data to seek to, then return.
  if (seek_timestamp == kNoTimestamp) {
    DVLOG(2) << __func__ << " " << GetStreamTypeName()
             << " couldn't find new selected range seek timestamp";
    return;
  }

  DCHECK(track_buffer_.empty());
  SeekAndSetSelectedRange(FindExistingRangeFor(seek_timestamp)->get(),
                          seek_timestamp);
}

base::TimeDelta SourceBufferStream::FindNewSelectedRangeSeekTimestamp(
    const base::TimeDelta start_timestamp) {
  DCHECK(start_timestamp != kNoTimestamp);
  DCHECK(start_timestamp >= base::TimeDelta());

  auto itr = ranges_.begin();

  // When checking a range to see if it has or begins soon enough after
  // |start_timestamp|, use the fudge room to determine "soon enough".
  base::TimeDelta start_timestamp_plus_fudge =
      start_timestamp + ComputeFudgeRoom(GetMaxInterbufferDistance());

  // Multiple ranges could be within the fudge room, because the fudge room is
  // dynamic based on max inter-buffer distance seen so far. Optimistically
  // check the earliest ones first.
  for (; itr != ranges_.end(); ++itr) {
    base::TimeDelta range_start = (*itr)->GetStartTimestamp();
    if (range_start >= start_timestamp_plus_fudge)
      break;
    if ((*itr)->GetEndTimestamp() < start_timestamp)
      continue;
    base::TimeDelta search_timestamp = start_timestamp;
    if (start_timestamp < range_start &&
        start_timestamp_plus_fudge >= range_start) {
      search_timestamp = range_start;
    }
    base::TimeDelta keyframe_timestamp =
        (*itr)->NextKeyframeTimestamp(search_timestamp);
    if (keyframe_timestamp != kNoTimestamp)
      return keyframe_timestamp;
  }

  DVLOG(2) << __func__ << " " << GetStreamTypeName()
           << " no buffered data for pts=" << start_timestamp.InMicroseconds()
           << "us";
  return kNoTimestamp;
}

base::TimeDelta SourceBufferStream::FindKeyframeAfterTimestamp(
    const base::TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp);

  auto itr = FindExistingRangeFor(timestamp);

  if (itr == ranges_.end())
    return kNoTimestamp;

  // First check for a keyframe timestamp >= |timestamp|
  // in the current range.
  return (*itr)->NextKeyframeTimestamp(timestamp);
}

std::string SourceBufferStream::GetStreamTypeName() const {
  switch (GetType()) {
    case SourceBufferStreamType::kAudio:
      return "AUDIO";
    case SourceBufferStreamType::kVideo:
      return "VIDEO";
  }
  NOTREACHED();
}

SourceBufferStreamType SourceBufferStream::GetType() const {
  if (!audio_configs_.empty())
    return SourceBufferStreamType::kAudio;
  DCHECK(!video_configs_.empty());
  return SourceBufferStreamType::kVideo;
}

void SourceBufferStream::DeleteAndRemoveRange(RangeList::iterator* itr) {
  DVLOG(1) << __func__;

  DCHECK(*itr != ranges_.end());
  if ((*itr)->get() == selected_range_) {
    DVLOG(1) << __func__ << " deleting selected range.";
    SetSelectedRange(NULL);
  }

  if (*itr == range_for_next_append_) {
    DVLOG(1) << __func__ << " deleting range_for_next_append_.";
    range_for_next_append_ = ranges_.end();
    ResetLastAppendedState();
  }

  *itr = ranges_.erase(*itr);
}

bool SourceBufferStream::SetPendingBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DCHECK(out_buffer->get());
  DCHECK(!pending_buffer_.get());

  const bool have_preroll_buffer = !!(*out_buffer)->preroll_buffer().get();

  if (!have_preroll_buffer)
    return false;

  pending_buffer_.swap(*out_buffer);
  pending_buffers_complete_ = false;
  return true;
}

}  // namespace media
