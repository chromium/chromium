// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_RANGE_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_RANGE_H_

#include <stddef.h>
#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser_buffer.h"

namespace media {

// Base class for representing a continuous range of buffered data in the
// presentation timeline. All buffers in a SourceBufferRange are ordered
// sequentially by GOP presentation interval, and within each GOP by decode
// order. Unless constructed with |ALLOW_GAPS|, the range contains no internal
// presentation gaps.
class MEDIA_EXPORT SourceBufferRange {
 public:
  // Returns the maximum distance in time between any buffer seen in the stream
  // of which this range is a part. Used to estimate the duration of a buffer if
  // its duration is not known, and in GetFudgeRoom() for determining whether a
  // time or coded frame is close enough to be considered part of this range.
  using InterbufferDistanceCB = base::Callback<base::TimeDelta()>;

  using BufferQueue = StreamParser::BufferQueue;

  // Policy for handling large gaps between buffers. Continuous media like
  // audio & video should use NO_GAPS_ALLOWED. Discontinuous media like
  // timed text should use ALLOW_GAPS because large differences in timestamps
  // are common and acceptable.
  enum GapPolicy {
    NO_GAPS_ALLOWED,
    ALLOW_GAPS
  };

  // Creates a range with |new_buffers|. |new_buffers| cannot be empty and the
  // front of |new_buffers| must be a keyframe.
  // |range_start_pts| refers to the starting timestamp for the coded
  // frame group to which these buffers belong.
  SourceBufferRange(GapPolicy gap_policy,
                    const BufferQueue& new_buffers,
                    base::TimeDelta range_start_pts,
                    const InterbufferDistanceCB& interbuffer_distance_cb);

  ~SourceBufferRange();

  // Deletes all buffers in range.
  void DeleteAll(BufferQueue* deleted_buffers);

  // Seeks to the beginning of the range.
  void SeekToStart();

  // Updates |out_buffer| with the next buffer in presentation order by GOP and
  // by decode order within each GOP (in general, in sequence to feed a
  // decoder). Seek() must be called before calls to GetNextBuffer(), and
  // buffers are returned in order from the last call to Seek(). Returns true if
  // |out_buffer| is filled with a valid buffer, false if there is not enough
  // data to fulfill the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);
  bool HasNextBuffer() const;

  // Returns the config ID for the buffer that will be returned by
  // GetNextBuffer().
  int GetNextConfigId() const;

  // Returns true if the range knows the position of the next buffer it should
  // return, i.e. it has been Seek()ed. This does not necessarily mean that it
  // has the next buffer yet.
  bool HasNextBufferPosition() const;

  // Resets this range to an "unseeked" state.
  void ResetNextBufferPosition();

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_index_|
  // is transferred to this SourceBufferRange.
  // Note: Use these only to merge existing ranges. |range|'s first buffer
  // timestamp must be adjacent to this range. No group start timestamp
  // adjacency is involved in these methods.
  // During append, |highest_frame_| is updated, if necessary.
  void AppendRangeToEnd(const SourceBufferRange& range,
                        bool transfer_current_position);
  bool CanAppendRangeToEnd(const SourceBufferRange& range) const;

  // Appends |buffers| to the end of the range and updates |keyframe_map_| as
  // it encounters new keyframes.
  // If |new_buffers_group_start_pts| is kNoTimestamp, then the
  // first buffer in |buffers| must come directly after the last buffer in this
  // range (within the fudge room) - specifically, if the first buffer in
  // |buffers| is not a keyframe, then it must be next in DTS order w.r.t. last
  // buffer in |buffers|. Otherwise, it's a keyframe that must be next in PTS
  // order w.r.t. |highest_frame_| or be immediately adjacent to the last buffer
  // in this range if that buffer has estimated duration (only allowed in WebM
  // streams).
  // If |new_buffers_group_start_pts| is set otherwise, then that time must come
  // directly after |highest_frame_| (within the fudge room), or directly after
  // the last buffered frame if it has estimated duration (only allowed in WebM
  // streams), and the first buffer in |buffers| must be a keyframe.
  // The latter scenario is required when a muxed coded frame group has such a
  // large jagged start across tracks that its first buffer is not within the
  // fudge room, yet its group start was.
  // The conditions around estimated duration are handled by
  // AllowableAppendAfterEstimatedDuration, and are intended to solve the edge
  // case in the SourceBufferStreamTest
  // MergeAllowedIfRangeEndTimeWithEstimatedDurationMatchesNextRangeStart.
  // During append, |highest_frame_| is updated, if necessary.
  void AppendBuffersToEnd(const BufferQueue& buffers,
                          base::TimeDelta new_buffers_group_start_timestamp);
  bool AllowableAppendAfterEstimatedDuration(
      const BufferQueue& buffers,
      base::TimeDelta new_buffers_group_start_pts) const;
  bool CanAppendBuffersToEnd(const BufferQueue& buffers,
                             base::TimeDelta new_buffers_group_start_pts) const;

  // Updates |next_buffer_index_| to point to the keyframe with presentation
  // timestamp at or before |timestamp|. Assumes |timestamp| is valid and in
  // this range.
  void Seek(base::TimeDelta timestamp);

  // Returns true if the range has enough data to seek to the specified
  // |timestamp|, false otherwise.
  bool CanSeekTo(base::TimeDelta timestamp) const;

  // Return the config ID for the buffer at |timestamp|. Precondition: callers
  // must first verify CanSeekTo(timestamp) == true.
  int GetConfigIdAtTime(base::TimeDelta timestamp) const;

  // Return true if all buffers in range of [start, end] have the same config
  // ID. Precondition: callers must first verify that
  // CanSeekTo(start) ==  CanSeekTo(end) == true.
  bool SameConfigThruRange(base::TimeDelta start, base::TimeDelta end) const;

  // Finds the next keyframe from |buffers_| starting at or after |timestamp|
  // and creates and returns a new SourceBufferRange with the buffers from
  // that keyframe onward. The buffers in the new SourceBufferRange are
  // moved out of this range. The start time of the new SourceBufferRange
  // is set to the later of |timestamp| and this range's GetStartTimestamp().
  // Note that this may result in temporary overlap of the new range and this
  // range until the caller truncates any nonkeyframes out of this range with
  // time > |timestamp|.  If there is no keyframe at or after |timestamp|,
  // SplitRange() returns null and this range is unmodified. This range can
  // become empty if |timestamp| <= the PTS of the first buffer in this range.
  // |highest_frame_| is updated, if necessary.
  std::unique_ptr<SourceBufferRange> SplitRange(base::TimeDelta timestamp);

  // Deletes the buffers from this range starting at |timestamp|, exclusive if
  // |is_exclusive| is true, inclusive otherwise.
  // Resets |next_buffer_index_| if the buffer at |next_buffer_index_| was
  // deleted, and deletes the |keyframe_map_| entries for the buffers that
  // were removed.
  // |highest_frame_| is updated, if necessary.
  // |deleted_buffers| contains the buffers that were deleted from this range,
  // starting at the buffer that had been at |next_buffer_index_|.
  // Returns true if everything in the range was deleted. Otherwise
  // returns false.
  bool TruncateAt(base::TimeDelta timestamp,
                  BufferQueue* deleted_buffers,
                  bool is_exclusive);

  // Deletes a GOP from the front or back of the range and moves these
  // buffers into |deleted_buffers|. Returns the number of bytes deleted from
  // the range (i.e. the size in bytes of |deleted_buffers|).
  // |highest_frame_| is updated, if necessary.
  // This range must NOT be empty when these methods are called.
  // The GOP being deleted must NOT contain the next buffer position.
  size_t DeleteGOPFromFront(BufferQueue* deleted_buffers);
  size_t DeleteGOPFromBack(BufferQueue* deleted_buffers);

  // Gets the range of GOP to secure at least |bytes_to_free| from
  // [|start_timestamp|, |end_timestamp|).
  // Returns the size of the buffers to secure if the buffers of
  // [|start_timestamp|, |end_removal_timestamp|) is removed.
  // Will not update |end_removal_timestamp| if the returned size is 0.
  size_t GetRemovalGOP(base::TimeDelta start_timestamp,
                       base::TimeDelta end_timestamp,
                       size_t bytes_to_free,
                       base::TimeDelta* end_removal_timestamp) const;

  // Returns true iff the buffered end time of the first GOP in this range is
  // at or before |media_time|.
  bool FirstGOPEarlierThanMediaTime(base::TimeDelta media_time) const;

  // Indicates whether the GOP at the beginning or end of the range contains the
  // next buffer position.
  bool FirstGOPContainsNextBufferPosition() const;
  bool LastGOPContainsNextBufferPosition() const;

  // Returns the timestamp of the next buffer that will be returned from
  // GetNextBuffer(), or kNoTimestamp if the timestamp is unknown.
  base::TimeDelta GetNextTimestamp() const;

  // Returns the start timestamp of the range.
  base::TimeDelta GetStartTimestamp() const;

  // Returns the highest presentation timestamp of frames in the last GOP in the
  // range.
  base::TimeDelta GetEndTimestamp() const;

  // Returns the timestamp for the end of the buffered region in this range.
  // This is an approximation if the duration for the buffer with highest PTS in
  // the last GOP in the range is unset.
  base::TimeDelta GetBufferedEndTimestamp() const;

  // Returns whether a buffer with a starting timestamp of |timestamp| would
  // belong in this range. This includes a buffer that would be appended to
  // the end of the range.
  bool BelongsToRange(base::TimeDelta timestamp) const;

  // Returns the highest time from among GetStartTimestamp() and frame timestamp
  // (in order in |buffers_| beginning at the first keyframe at or before
  // |timestamp|) for buffers in this range up to and including |timestamp|.
  // Note that |timestamp| must belong to this range.
  base::TimeDelta FindHighestBufferedTimestampAtOrBefore(
      base::TimeDelta timestamp) const;

  // Gets the timestamp for the keyframe that is at or after |timestamp|. If
  // there isn't such a keyframe in the range then kNoTimestamp is returned.
  // If |timestamp| is in the "gap" between the value returned by
  // GetStartTimestamp() and the timestamp on the first buffer in |buffers_|,
  // then |timestamp| is returned.
  base::TimeDelta NextKeyframeTimestamp(base::TimeDelta timestamp) const;

  // Gets the timestamp for the closest keyframe that is <= |timestamp|. If
  // there isn't a keyframe before |timestamp| or |timestamp| is outside
  // this range, then kNoTimestamp is returned.
  base::TimeDelta KeyframeBeforeTimestamp(base::TimeDelta timestamp) const;

  // Adds all buffers which overlap [start, end) to the end of |buffers|.  If
  // no buffers exist in the range returns false, true otherwise.
  // This method is used for finding audio splice overlap buffers, so all
  // buffers are expected to be keyframes here (so DTS doesn't matter at all).
  bool GetBuffersInRange(base::TimeDelta start,
                         base::TimeDelta end,
                         BufferQueue* buffers) const;

  size_t size_in_bytes() const { return size_in_bytes_; }

 private:
  // Friend of private is only for IsNextInPresentationSequence testing.
  friend class SourceBufferStreamTest;

  using KeyframeMap = std::map<base::TimeDelta, int>;

  // Called during AppendBuffersToEnd to adjust estimated duration at the
  // end of the last append to match the delta in timestamps between
  // the last append and the upcoming append. This is a workaround for
  // WebM media where a duration is not always specified. Caller should take
  // care of updating |highest_frame_|.
  void AdjustEstimatedDurationForNewAppend(const BufferQueue& new_buffers);

  // Frees the buffers in |buffers_| from [|start_point|,|ending_point|) and
  // updates the |size_in_bytes_| accordingly. Note, this does not update
  // |keyframe_map_|.
  void FreeBufferRange(const BufferQueue::const_iterator& starting_point,
                       const BufferQueue::const_iterator& ending_point);

  // Returns the distance in time estimating how far from the beginning or end
  // of this range a buffer can be to be considered in the range.
  base::TimeDelta GetFudgeRoom() const;

  // Returns the approximate duration of a buffer in this range.
  base::TimeDelta GetApproximateDuration() const;

  // Updates |highest_frame_| if |new_buffer| has a higher PTS than
  // |highest_frame_|, |new_buffer| has the same PTS as |highest_frame_| and
  // duration at least as long as |highest_frame_|, or if the range was
  // previously empty.
  void UpdateEndTime(scoped_refptr<StreamParserBuffer> new_buffer);

  // Returns true if |timestamp| is allowed in this range as the timestamp of
  // the next buffer in presentation sequence at or after |highest_frame_|.
  // |buffers_| must not be empty, and |highest_frame_| must not be nullptr.
  // Uses |gap_policy_| to potentially allow gaps.
  //
  // Due to potential for out-of-order decode vs presentation time, this method
  // should only be used to determine adjacency of keyframes with the end of
  // |buffers_|.
  bool IsNextInPresentationSequence(base::TimeDelta timestamp) const;

  // Returns true if |decode_timestamp| is allowed in this range as the decode
  // timestamp of the next buffer in decode sequence at or after the last buffer
  // in |buffers_|'s decode timestamp.  |buffers_| must not be empty. Uses
  // |gap_policy_| to potentially allow gaps.
  //
  // Due to potential for out-of-order decode vs presentation time, this method
  // should only be used to determine adjacency of non-keyframes with the end of
  // |buffers_|, when determining if a non-keyframe with |decode_timestamp|
  // continues the decode sequence of the coded frame group at the end of
  // |buffers_|.
  bool IsNextInDecodeSequence(DecodeTimestamp decode_timestamp) const;

  // Helper method for Appending |range| to the end of this range.  If |range|'s
  // first buffer time is before the time of the last buffer in this range,
  // returns kNoTimestamp.  Otherwise, returns the closest time within
  // [|range|'s start time, |range|'s first buffer time] that is at or after the
  // this range's GetEndTimestamp(). This allows |range| to potentially be
  // determined to be adjacent within fudge room for appending to the end of
  // this range, especially if |range| has a start time that is before its first
  // buffer's time.
  base::TimeDelta NextRangeStartTimeForAppendRangeToEnd(
      const SourceBufferRange& range) const;

  // Returns an index (or iterator) into |buffers_| pointing to the first buffer
  // at or after |timestamp|.  If |skip_given_timestamp| is true, this returns
  // the first buffer with timestamp strictly greater than |timestamp|. If
  // |buffers_| has no such buffer, returns |buffers_.size()| (or
  // |buffers_.end()|).
  size_t GetBufferIndexAt(base::TimeDelta timestamp,
                          bool skip_given_timestamp) const;
  BufferQueue::const_iterator GetBufferItrAt(base::TimeDelta timestamp,
                                             bool skip_given_timestamp) const;

  // Returns an iterator in |keyframe_map_| pointing to the next keyframe after
  // |timestamp|. If |skip_given_timestamp| is true, this returns the first
  // keyframe with a timestamp strictly greater than |timestamp|.
  KeyframeMap::const_iterator GetFirstKeyframeAt(
      base::TimeDelta timestamp,
      bool skip_given_timestamp) const;

  // Returns an iterator in |keyframe_map_| pointing to the first keyframe
  // before or at |timestamp|.
  KeyframeMap::const_iterator GetFirstKeyframeAtOrBefore(
      base::TimeDelta timestamp) const;

  // Helper method to delete buffers in |buffers_| starting at
  // |starting_point|, an index in |buffers_|.
  // Returns true if everything in the range was removed. Returns
  // false if the range still contains buffers.
  bool TruncateAt(const size_t starting_point, BufferQueue* deleted_buffers);

  // Updates |highest_frame_| to be the frame with highest PTS in the last GOP
  // in this range.  If there are no buffers in this range, resets
  // |highest_frame_|.
  // Normally, incremental additions to this range should just use
  // UpdateEndTime(). When removing buffers from this range (which could be out
  // of order presentation vs decode order), inspecting the last buffer in
  // decode order of this range can be insufficient to determine the correct
  // presentation end time of this range. Hence this helper method.
  void UpdateEndTimeUsingLastGOP();

  // Helper for debugging state.
  std::string ToStringForDebugging() const;

  // Keeps track of whether gaps are allowed.
  const GapPolicy gap_policy_;

  // The ordered list of buffers in this range.
  BufferQueue buffers_;

  // Index into |buffers_| for the next buffer to be returned by
  // GetNextBuffer(), set to -1 by ResetNextBufferPosition().
  int next_buffer_index_;

  // Caches the buffer, if any, with the highest PTS currently in |buffers_|.
  // This is nullptr if this range is empty.  This is useful in determining
  // range membership and adjacency.
  scoped_refptr<StreamParserBuffer> highest_frame_;

  // Called to get the largest interbuffer distance seen so far in the stream.
  InterbufferDistanceCB interbuffer_distance_cb_;

  // Stores the amount of memory taken up by the data in |buffers_|.
  size_t size_in_bytes_;

  // If the first buffer in this range is the beginning of a coded frame group,
  // |range_start_pts_| is the presentation time when the coded frame group
  // begins. This is especially important in muxed media where the first coded
  // frames for each track do not necessarily begin at the same time.
  // |range_start_pts_| may be <= the timestamp of the first buffer in
  // |buffers_|. |range_start_pts_| is kNoTimestamp if this range does not start
  // at the beginning of a coded frame group, which can happen by range removal
  // or split when we don't have a way of knowing, across potentially multiple
  // muxed streams, the coded frame group start timestamp for the new range.
  base::TimeDelta range_start_pts_;

  // Index base of all positions in |keyframe_map_|. In other words, the
  // real position of entry |k| of |keyframe_map_| in the range is:
  //   keyframe_map_[k] - keyframe_map_index_base_
  int keyframe_map_index_base_;

  // Maps keyframe presentation timestamps to GOP start index of |buffers_|
  // (with index adjusted by |keyframe_map_index_base_|);
  KeyframeMap keyframe_map_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferRange);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_RANGE_H_
