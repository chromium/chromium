// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SourceBufferStream is a data structure that stores media Buffers in ranges.
// Buffers can be appended out of presentation order. Buffers are retrieved by
// seeking to the desired start point and calling GetNextBuffer(). Buffers are
// returned in sequential order to feed decoder, generally near presentation
// order though not necessarily the same as presentation order within GOPs of
// out-of-order codecs.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/source_buffer_range.h"

namespace media {

// Status returned by GetNextBuffer().
// kSuccess: Indicates that the next buffer was returned.
// kNeedBuffer: Indicates that we need more data before a buffer can be
//              returned.
// kConfigChange: Indicates that the next buffer requires a config change.
enum class SourceBufferStreamStatus {
  kSuccess,
  kNeedBuffer,
  kConfigChange,
  kEndOfStream
};

enum class SourceBufferStreamType { kAudio, kVideo, kText };

// See file-level comment for complete description.
class MEDIA_EXPORT SourceBufferStream {
 public:
  using BufferQueue = StreamParser::BufferQueue;
  using RangeList = std::list<std::unique_ptr<SourceBufferRange>>;

  // Helper for PrepareRangesForNextAppend and BufferQueueToLogString that
  // populates |start| and |end| with the presentation interval of |buffers|.
  static void GetTimestampInterval(const BufferQueue& buffers,
                                   base::TimeDelta* start,
                                   base::TimeDelta* end);

  SourceBufferStream(const AudioDecoderConfig& audio_config,
                     MediaLog* media_log);
  SourceBufferStream(const VideoDecoderConfig& video_config,
                     MediaLog* media_log);
  SourceBufferStream(const TextTrackConfig& text_config, MediaLog* media_log);

  ~SourceBufferStream();

  // Signals that the next buffers appended are part of a new coded frame group
  // starting at |coded_frame_group_start_pts|.
  void OnStartOfCodedFrameGroup(base::TimeDelta coded_frame_group_start_pts);

  // Add the |buffers| to the SourceBufferStream. Buffers within the queue are
  // expected to be in order, but multiple calls to Append() may add buffers out
  // of order or overlapping. Assumes all buffers within |buffers| are in
  // presentation order and are non-overlapping.
  void Append(const BufferQueue& buffers);

  // Removes buffers between |start| and |end| according to the steps
  // in the "Coded Frame Removal Algorithm" in the Media Source
  // Extensions Spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-coded-frame-removal
  //
  // |duration| is the current duration of the presentation. It is
  // required by the computation outlined in the spec.
  void Remove(base::TimeDelta start, base::TimeDelta end,
              base::TimeDelta duration);

  // Frees up space if the SourceBufferStream is taking up too much memory.
  // |media_time| is current playback position.
  bool GarbageCollectIfNeeded(base::TimeDelta media_time, size_t newDataSize);

  // Gets invoked when the system is experiencing memory pressure, i.e. there's
  // not enough free memory. The |media_time| is the media playback position at
  // the time of memory pressure notification (needed for accurate GC). The
  // |memory_pressure_level| indicates memory pressure severity. The
  // |force_instant_gc| is used to force the MSE garbage collection algorithm to
  // be run right away, without waiting for the next append.
  void OnMemoryPressure(
      base::TimeDelta media_time,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
      bool force_instant_gc);

  // Changes the SourceBufferStream's state so that it will start returning
  // buffers starting from the closest keyframe before |timestamp|.
  void Seek(base::TimeDelta timestamp);

  // Returns true if the SourceBufferStream has seeked to a time without
  // buffered data and is waiting for more data to be appended.
  bool IsSeekPending() const;

  // Notifies the SourceBufferStream that the media duration has been changed to
  // |duration| so it should drop any data past that point.
  void OnSetDuration(base::TimeDelta duration);

  // Fills |out_buffer| with a new buffer. Buffers are presented in order from
  // the last call to Seek(), or starting with the first buffer appended if
  // Seek() has not been called yet.
  // |out_buffer|'s timestamp may be earlier than the |timestamp| passed to
  // the last Seek() call.
  // Returns kSuccess if |out_buffer| is filled with a valid buffer, kNeedBuffer
  // if there is not enough data buffered to fulfill the request, and
  // kConfigChange if the next buffer requires a config change.
  SourceBufferStreamStatus GetNextBuffer(
      scoped_refptr<StreamParserBuffer>* out_buffer);

  // Returns a list of the buffered time ranges.
  Ranges<base::TimeDelta> GetBufferedTime() const;

  // Returns the highest buffered PTS or base::TimeDelta() if nothing is
  // buffered.
  base::TimeDelta GetHighestPresentationTimestamp() const;

  // Returns the duration of the buffered ranges, which is equivalent
  // to the end timestamp of the last buffered range. If no data is buffered
  // then base::TimeDelta() is returned.
  base::TimeDelta GetBufferedDuration() const;

  // Returns the size of the buffered data in bytes.
  size_t GetBufferedSize() const;

  // Notifies this object that end of stream has been signalled.
  void MarkEndOfStream();

  // Clear the end of stream state set by MarkEndOfStream().
  void UnmarkEndOfStream();

  const AudioDecoderConfig& GetCurrentAudioDecoderConfig();
  const VideoDecoderConfig& GetCurrentVideoDecoderConfig();
  const TextTrackConfig& GetCurrentTextTrackConfig();

  // Notifies this object that the audio config has changed and buffers in
  // future Append() calls should be associated with this new config.
  // If the codec is allowed to change, the caller should set
  // |allow_codec_change| to true.
  bool UpdateAudioConfig(const AudioDecoderConfig& config,
                         bool allow_codec_change);

  // Notifies this object that the video config has changed and buffers in
  // future Append() calls should be associated with this new config.
  // If the codec is allowed to change, the caller should set
  // |allow_codec_change| to true.
  bool UpdateVideoConfig(const VideoDecoderConfig& config,
                         bool allow_codec_change);

  // Returns the largest distance between two adjacent buffers in this stream,
  // or an estimate if no two adjacent buffers have been appended to the stream
  // yet.
  base::TimeDelta GetMaxInterbufferDistance() const;

  void set_memory_limit(size_t memory_limit) {
    memory_limit_ = memory_limit;
  }

 private:
  friend class SourceBufferStreamTest;

  // Attempts to delete approximately |total_bytes_to_free| amount of data
  // |ranges_|, starting at the front of |ranges_| and moving linearly forward
  // through the buffers. Deletes starting from the back if |reverse_direction|
  // is true. |media_time| is current playback position.
  // Returns the number of bytes freed.
  size_t FreeBuffers(size_t total_bytes_to_free,
                     base::TimeDelta media_time,
                     bool reverse_direction);

  // Attempts to delete approximately |total_bytes_to_free| amount of data from
  // |ranges_|, starting after the last appended media
  // (|highest_buffered_end_time_in_append_sequence_|) but before the current
  // playback position |media_time|.
  size_t FreeBuffersAfterLastAppended(size_t total_bytes_to_free,
                                      base::TimeDelta media_time);

  // Gets the removal range to secure |total_bytes_to_free| from
  // [|start_timestamp|, |end_timestamp|).
  // Returns the size of buffers to secure if future
  // Remove(|start_timestamp|, |removal_end_timestamp|, duration) is called.
  // Will not update |removal_end_timestamp| if the returned size is 0.
  size_t GetRemovalRange(base::TimeDelta start_timestamp,
                         base::TimeDelta end_timestamp,
                         size_t total_bytes_to_free,
                         base::TimeDelta* removal_end_timestamp);

  // Prepares |range_for_next_append_| so |new_buffers| can be appended.
  // This involves removing buffers between the end of the previous append
  // and any buffers covered by the time range in |new_buffers|.
  // |deleted_buffers| is an output parameter containing candidates for
  // |track_buffer_| if this method ends up removing the current playback
  // position from the range.
  void PrepareRangesForNextAppend(const BufferQueue& new_buffers,
                                  BufferQueue* deleted_buffers);

  // Removes buffers, from the |track_buffer_|, that come after |timestamp|.
  // Due to out-of-order decode versus presentation times for some kinds of
  // media, |timestamp| should be the time of a keyframe known by the caller.
  // |timestamp| must not be kNoTimestamp.
  void PruneTrackBuffer(const base::TimeDelta timestamp);

  // Checks to see if |range_with_new_buffers_itr| can be merged with the range
  // next to it, and merges them if so while preserving correctness of
  // |range_for_next_append_| and |selected_range_|.
  void MergeWithNextRangeIfNecessary(
      const RangeList::iterator& range_with_new_buffers_itr);

  // Merges any adjacent ranges while preserving correctness of
  // |range_for_next_append_| and |selected_range_|.
  void MergeAllAdjacentRanges();

  // Returns true if |next_gop_timestamp| follows
  // |highest_timestamp_in_append_sequence_| within fudge room.
  bool IsNextGopAdjacentToEndOfCurrentAppendSequence(
      base::TimeDelta next_gop_timestamp) const;

  // Helper method that returns the timestamp for the next buffer that
  // |selected_range_| will return from GetNextBuffer() call, or kNoTimestamp
  // if in between seeking (i.e. |selected_range_| is null).
  base::TimeDelta GetNextBufferTimestamp();

  // Finds the range that should contain a coded frame group that begins with
  // |start_timestamp| (presentation time) and returns the iterator pointing to
  // it. Returns |ranges_.end()| if there's no such existing range.
  RangeList::iterator FindExistingRangeFor(base::TimeDelta start_timestamp);

  // Inserts |new_range| into |ranges_| preserving sorted order. Returns an
  // iterator in |ranges_| that points to |new_range|. |new_range| becomes owned
  // by |ranges_|.
  RangeList::iterator AddToRanges(std::unique_ptr<SourceBufferRange> new_range);

  // Sets the |selected_range_| to |range| and resets the next buffer position
  // for the previous |selected_range_|.
  void SetSelectedRange(SourceBufferRange* range);

  // Seeks |range| to |seek_timestamp| and then calls SetSelectedRange() with
  // |range|.
  void SeekAndSetSelectedRange(SourceBufferRange* range,
                               base::TimeDelta seek_timestamp);

  // Resets this stream back to an unseeked state.
  void ResetSeekState();

  // Reset state tracking various metadata about the last appended buffer.
  void ResetLastAppendedState();

  // Returns true if |seek_timestamp| refers to the beginning of the first range
  // in |ranges_|, false otherwise or if |ranges_| is empty.
  bool ShouldSeekToStartOfBuffered(base::TimeDelta seek_timestamp) const;

  // Returns true if the decode timestamps of |buffers| are monotonically
  // increasing (within each GOP) since the previous append to the coded frame
  // group, false otherwise.
  bool IsDtsMonotonicallyIncreasing(const BufferQueue& buffers);

  // Returns true if |selected_range_| is the only range in |ranges_| that
  // HasNextBufferPosition().
  bool OnlySelectedRangeIsSeeked() const;

  // Measures the distances between buffer decode timestamps and tracks the max.
  // This enables a reasonable approximation of adjacency fudge room, even for
  // out-of-order PTS vs DTS sequences. Returns true if
  // |max_interbuffer_distance_| was changed.
  bool UpdateMaxInterbufferDtsDistance(const BufferQueue& buffers);

  // Sets the config ID for each buffer to |append_config_index_|.
  void SetConfigIds(const BufferQueue& buffers);

  // Called to complete a config change. Updates |current_config_index_| to
  // match the index of the next buffer. Calling this method causes
  // GetNextBuffer() to stop returning kConfigChange and start returning
  // kSuccess.
  void CompleteConfigChange();

  // Sets |selected_range_| and seeks to the nearest keyframe after
  // |timestamp| if necessary and possible. This method only attempts to
  // set |selected_range_| if |seleted_range_| is null and |track_buffer_|
  // is empty.
  void SetSelectedRangeIfNeeded(const base::TimeDelta timestamp);

  // Find a keyframe timestamp that is >= |start_timestamp| and can be used to
  // find a new selected range.
  // Returns kNoTimestamp if an appropriate keyframe timestamp could not be
  // found.
  base::TimeDelta FindNewSelectedRangeSeekTimestamp(
      const base::TimeDelta start_timestamp);

  // Searches |ranges_| for the first keyframe timestamp that is >= |timestamp|.
  // If |ranges_| doesn't contain a GOP that covers |timestamp| or doesn't
  // have a keyframe after |timestamp| then kNoTimestamp is returned.
  base::TimeDelta FindKeyframeAfterTimestamp(const base::TimeDelta timestamp);

  // Returns "VIDEO" for a video SourceBufferStream, "AUDIO" for an audio
  // stream, and "TEXT" for a text stream.
  std::string GetStreamTypeName() const;

  // (Audio only) If |new_buffers| overlap existing buffers, trims end of
  // existing buffers to remove overlap. |new_buffers| are not modified.
  void TrimSpliceOverlap(const BufferQueue& new_buffers);

  // Returns true if end of stream has been reached, i.e. the
  // following conditions are met:
  // 1. end of stream is marked and there is nothing in the track_buffer.
  // 2. We don't have any ranges, or the last or no range is selected,
  //    or there is a pending seek beyond any existing ranges.
  bool IsEndOfStreamReached() const;

  // Deletes the range pointed to by |*itr| and removes it from |ranges_|.
  // If |*itr| points to |selected_range_|, then |selected_range_| is set to
  // NULL. After the range is removed, |*itr| is to the range after the one that
  // was removed or to |ranges_.end()| if the last range was removed.
  void DeleteAndRemoveRange(RangeList::iterator* itr);

  // Helper function used when updating |range_for_next_append_|. Returns a
  // guess of what the next append timestamp will be based on
  // |last_appended_buffer_timestamp_|, |new_coded_frame_group_| and
  // |coded_frame_group_start_pts_|. Returns kNoTimestamp if unable to guess,
  // which can occur prior to first OnStartOfCodedFrameGroup(), or when the most
  // recent GOP appended to since the last OnStartOfCodedFrameGroup() is
  // removed.
  base::TimeDelta PotentialNextAppendTimestamp() const;

  // Helper function used by Remove() and PrepareRangesForNextAppend() to
  // remove buffers and ranges between |start| and |end|.
  // |exclude_start| - If set to true, buffers with timestamps that
  // match |start| are not removed. If set to false, buffers with
  // timestamps that match |start| will be removed.
  // |*deleted_buffers| - Filled with buffers for the current playback position
  // if the removal range included the current playback position. These buffers
  // can be used as candidates for placing in the |track_buffer_|.
  void RemoveInternal(base::TimeDelta start,
                      base::TimeDelta end,
                      bool exclude_start,
                      BufferQueue* deleted_buffers);

  // Helper function used by RemoveInternal() to evaluate whether remove will
  // disrupt the last appended GOP. If disruption is expected, reset state
  // tracking the last append. This will trigger frame filtering in Append()
  // until a new key frame is provided.
  void UpdateLastAppendStateForRemove(base::TimeDelta remove_start,
                                      base::TimeDelta remove_end,
                                      bool exclude_start);

  SourceBufferStreamType GetType() const;

  // See GetNextBuffer() for additional details.  This method handles preroll
  // frame processing.
  SourceBufferStreamStatus HandleNextBufferWithPreroll(
      scoped_refptr<StreamParserBuffer>* out_buffer);

  // See GetNextBuffer() for additional details.  The internal method hands out
  // single buffers from the |track_buffer_| and |selected_range_| without
  // additional processing for preroll buffers.
  SourceBufferStreamStatus GetNextBufferInternal(
      scoped_refptr<StreamParserBuffer>* out_buffer);

  // If the next buffer's timestamp is significantly beyond the last output
  // buffer, and if we just exhausted |track_buffer_| on the previous read, this
  // method logs a warning to |media_log_| that there could be perceivable
  // delay. Apps can avoid this behavior by not overlap-appending buffers near
  // current playback position.
  void WarnIfTrackBufferExhaustionSkipsForward(
      scoped_refptr<StreamParserBuffer> next_buffer);

  // If |out_buffer| has preroll, sets |pending_buffer_| to feed out preroll and
  // returns true.  Otherwise returns false.
  bool SetPendingBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);

  // Used to report log messages that can help the web developer figure out what
  // is wrong with the content.
  MediaLog* media_log_;

  // List of disjoint buffered ranges, ordered by start time.
  RangeList ranges_;

  // Indicates which decoder config is being used by the decoder.
  // GetNextBuffer() is only allows to return buffers that have a
  // config ID that matches this index. If there is a mismatch then
  // it must signal that a config change is needed.
  int current_config_index_ = 0;

  // Indicates which decoder config to associate with new buffers
  // being appended. Each new buffer appended has its config ID set
  // to the value of this field.
  int append_config_index_ = 0;

  // Holds the audio/video configs for this stream. |current_config_index_|
  // and |append_config_index_| represent indexes into one of these vectors.
  std::vector<AudioDecoderConfig> audio_configs_;
  std::vector<VideoDecoderConfig> video_configs_;

  // Holds the text config for this stream.
  TextTrackConfig text_track_config_;

  // True if more data needs to be appended before the Seek() can complete,
  // false if no Seek() has been requested or the Seek() is completed.
  bool seek_pending_ = false;

  // True if the end of the stream has been signalled.
  bool end_of_stream_ = false;

  // Timestamp of the last request to Seek().
  base::TimeDelta seek_buffer_timestamp_;

  // Pointer to the seeked-to Range. This is the range from which
  // GetNextBuffer() calls are fulfilled after the |track_buffer_| has been
  // emptied.
  SourceBufferRange* selected_range_ = nullptr;

  // Queue of the next buffers to be returned from calls to GetNextBuffer(). If
  // |track_buffer_| is empty, return buffers from |selected_range_|.
  BufferQueue track_buffer_;

  // If there has been no intervening Seek, this will be true if the last
  // emitted buffer emptied |track_buffer_|.
  bool just_exhausted_track_buffer_ = false;

  // The start presentation time of the current coded frame group being
  // appended.
  base::TimeDelta coded_frame_group_start_pts_;

  // Points to the range containing the current coded frame group being
  // appended.
  RangeList::iterator range_for_next_append_;

  // True when the next call to Append() begins a new coded frame group.
  // TODO(wolenetz): Simplify by passing this flag into Append().
  bool new_coded_frame_group_ = false;

  // The timestamp of the last buffer appended to the coded frame group, set to
  // kNoTimestamp if the beginning of the group.
  base::TimeDelta last_appended_buffer_timestamp_ = kNoTimestamp;
  base::TimeDelta last_appended_buffer_duration_ = kNoTimestamp;
  bool last_appended_buffer_is_keyframe_ = false;

  // When buffering GOPs by keyframe PTS and intra-gop by nonkeyframe DTS, to
  // verify monotonically increasing intra-GOP DTS sequence and to update max
  // interbuffer distance also by DTS deltas within a coded frame group, the
  // following is needed.
  DecodeTimestamp last_appended_buffer_decode_timestamp_ = kNoDecodeTimestamp();

  // The following is the highest presentation timestamp appended so far in this
  // coded frame group. Due to potentially out-of-order decode versus
  // presentation time sequence, this isn't necessarily the most recently
  // appended frame. This is used as the lower bound of removing previously
  // buffered media when processing new appends.
  base::TimeDelta highest_timestamp_in_append_sequence_ = kNoTimestamp;

  // The following is used in determining if FreeBuffersAfterLastAppended() is
  // allowed during garbage collection. Due to out-of-order decode versus
  // presentation sequence, this isn't necessarily the end time of the most
  // recently appended frame.
  base::TimeDelta highest_buffered_end_time_in_append_sequence_ = kNoTimestamp;

  // The highest presentation timestamp for buffers returned by recent
  // GetNextBuffer() calls. Set to kNoTimestamp if GetNextBuffer() hasn't been
  // called yet or a seek has happened since the last GetNextBuffer() call.
  base::TimeDelta highest_output_buffer_timestamp_;

  // Stores the largest distance between two adjacent buffers in this stream.
  base::TimeDelta max_interbuffer_distance_;

  base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level_ =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  // The maximum amount of data in bytes the stream will keep in memory.
  size_t memory_limit_;

  // Indicates that a kConfigChanged status has been reported by GetNextBuffer()
  // and GetCurrentXXXDecoderConfig() must be called to update the current
  // config. GetNextBuffer() must not be called again until
  // GetCurrentXXXDecoderConfig() has been called.
  bool config_change_pending_ = false;

  // Used by HandleNextBufferWithPreroll() when a buffer with preroll is
  // returned from GetNextBufferInternal().
  scoped_refptr<StreamParserBuffer> pending_buffer_;

  // Indicates that all buffers before |pending_buffer_| have been handed out.
  bool pending_buffers_complete_ = false;

  // To prevent log spam, count the number of logs for different log scenarios.
  int num_splice_logs_ = 0;
  int num_track_buffer_gap_warning_logs_ = 0;
  int num_garbage_collect_algorithm_logs_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
