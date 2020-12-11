// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_STATE_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_STATE_H_

#include <list>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "media/base/audio_codecs.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_codecs.h"
#include "media/filters/source_buffer_parse_warnings.h"

namespace media {

using base::TimeDelta;

class ChunkDemuxerStream;
class FrameProcessor;

// Contains state belonging to a source id.
class MEDIA_EXPORT SourceBufferState {
 public:
  // Callback signature used to create ChunkDemuxerStreams.
  using CreateDemuxerStreamCB =
      base::RepeatingCallback<ChunkDemuxerStream*(DemuxerStream::Type)>;

  using NewTextTrackCB = base::RepeatingCallback<void(ChunkDemuxerStream*,
                                                      const TextTrackConfig&)>;

  SourceBufferState(std::unique_ptr<StreamParser> stream_parser,
                    std::unique_ptr<FrameProcessor> frame_processor,
                    CreateDemuxerStreamCB create_demuxer_stream_cb,
                    MediaLog* media_log);

  ~SourceBufferState();

  void Init(StreamParser::InitCB init_cb,
            const std::string& expected_codecs,
            const StreamParser::EncryptedMediaInitDataCB&
                encrypted_media_init_data_cb,
            NewTextTrackCB new_text_track_cb);

  // Reconfigures this source buffer to use |new_stream_parser|. Caller must
  // first ensure that ResetParserState() was done to flush any pending frames
  // from the old stream parser.
  void ChangeType(std::unique_ptr<StreamParser> new_stream_parser,
                  const std::string& new_expected_codecs);

  // Appends new data to the StreamParser.
  // Returns true if the data was successfully appended. Returns false if an
  // error occurred. |*timestamp_offset| is used and possibly updated by the
  // append. |append_window_start| and |append_window_end| correspond to the MSE
  // spec's similarly named source buffer attributes that are used in coded
  // frame processing.
  bool Append(const uint8_t* data,
              size_t length,
              TimeDelta append_window_start,
              TimeDelta append_window_end,
              TimeDelta* timestamp_offset);

  // Aborts the current append sequence and resets the parser.
  void ResetParserState(TimeDelta append_window_start,
                        TimeDelta append_window_end,
                        TimeDelta* timestamp_offset);

  // Calls Remove(|start|, |end|, |duration|) on all
  // ChunkDemuxerStreams managed by this object.
  void Remove(TimeDelta start, TimeDelta end, TimeDelta duration);

  // If the buffer is full, attempts to try to free up space, as specified in
  // the "Coded Frame Eviction Algorithm" in the Media Source Extensions Spec.
  // Returns false iff buffer is still full after running eviction.
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-eviction
  bool EvictCodedFrames(base::TimeDelta media_time, size_t newDataSize);

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

  // Returns true if currently parsing a media segment, or false otherwise.
  bool parsing_media_segment() const { return parsing_media_segment_; }

  // Returns the 'Generate Timestamps Flag' for this SourceBuffer's byte stream
  // format parser as described in the MSE Byte Stream Format Registry.
  bool generate_timestamps_flag() const {
    return stream_parser_->GetGenerateTimestampsFlag();
  }

  // Sets |frame_processor_|'s sequence mode to |sequence_mode|.
  void SetSequenceMode(bool sequence_mode);

  // Signals the coded frame processor to update its group start timestamp to be
  // |timestamp_offset| if it is in sequence append mode.
  void SetGroupStartTimestampIfInSequenceMode(base::TimeDelta timestamp_offset);

  // Returns the range of buffered data in this source, capped at |duration|.
  // |ended| - Set to true if end of stream has been signaled and the special
  // end of stream range logic needs to be executed.
  Ranges<TimeDelta> GetBufferedRanges(TimeDelta duration, bool ended) const;

  // Returns the highest PTS of currently buffered frames in this source, or
  // base::TimeDelta() if none of the streams contain buffered data.
  TimeDelta GetHighestPresentationTimestamp() const;

  // Returns the highest buffered duration across all streams managed
  // by this object.
  // Returns TimeDelta() if none of the streams contain buffered data.
  TimeDelta GetMaxBufferedDuration() const;

  // Helper methods that call methods with similar names on all the
  // ChunkDemuxerStreams managed by this object.
  void StartReturningData();
  void AbortReads();
  void Seek(TimeDelta seek_time);
  void CompletePendingReadIfPossible();
  void OnSetDuration(TimeDelta duration);
  void MarkEndOfStream();
  void UnmarkEndOfStream();
  void Shutdown();
  // Sets the memory limit on each stream of a specific type.
  // |memory_limit| is the maximum number of bytes each stream of type |type|
  // is allowed to hold in its buffer.
  void SetMemoryLimits(DemuxerStream::Type type, size_t memory_limit);
  bool IsSeekWaitingForData() const;

  using RangesList = std::vector<Ranges<TimeDelta>>;
  static Ranges<TimeDelta> ComputeRangesIntersection(
      const RangesList& active_ranges,
      bool ended);

  void SetTracksWatcher(const Demuxer::MediaTracksUpdatedCB& tracks_updated_cb);

  void SetParseWarningCallback(SourceBufferParseWarningCB parse_warning_cb);

 private:
  // State advances through this list to PARSER_INITIALIZED.
  // The intent is to ensure at least one config is received prior to parser
  // calling initialization callback, and that such initialization callback
  // occurs at most once per parser.
  // PENDING_PARSER_RECONFIG occurs if State had reached PARSER_INITIALIZED
  // before changing to a new StreamParser in ChangeType(). In such case, State
  // would then advance to PENDING_PARSER_REINIT, then PARSER_INITIALIZED upon
  // the next initialization segment parsed, but would not run the
  // initialization callback in this case (since such would already have
  // occurred on the initial transition from PENDING_PARSER_INIT to
  // PARSER_INITIALIZED.)
  enum State {
    UNINITIALIZED = 0,
    PENDING_PARSER_CONFIG,
    PENDING_PARSER_INIT,
    PARSER_INITIALIZED,
    PENDING_PARSER_RECONFIG,
    PENDING_PARSER_REINIT
  };

  // Initializes |stream_parser_|. Also, updates |expected_audio_codecs| and
  // |expected_video_codecs|.
  void InitializeParser(const std::string& expected_codecs);

  // Called by the |stream_parser_| when a new initialization segment is
  // encountered.
  // Returns true on a successful call. Returns false if an error occurred while
  // processing decoder configurations.
  bool OnNewConfigs(std::string expected_codecs,
                    std::unique_ptr<MediaTracks> tracks,
                    const StreamParser::TextTrackConfigMap& text_configs);

  // Called by the |stream_parser_| at the beginning of a new media segment.
  void OnNewMediaSegment();

  // Called by the |stream_parser_| at the end of a media segment.
  void OnEndOfMediaSegment();

  // Called by the |stream_parser_| when new buffers have been parsed.
  // It processes the new buffers using |frame_processor_|, which includes
  // appending the processed frames to associated demuxer streams for each
  // frame's track.
  // Returns true on a successful call. Returns false if an error occurred while
  // processing the buffers.
  bool OnNewBuffers(const StreamParser::BufferQueueMap& buffer_queue_map);

  // Called when StreamParser encounters encrypted media init data.
  void OnEncryptedMediaInitData(EmeInitDataType type,
                                const std::vector<uint8_t>& init_data);

  void OnSourceInitDone(const StreamParser::InitParameters& params);

  // Sets memory limits for all demuxer streams.
  void SetStreamMemoryLimits();

  // Tracks the number of MEDIA_LOGs emitted for segments missing expected audio
  // or video blocks. Useful to prevent log spam.
  int num_missing_track_logs_ = 0;

  // During Append(), if OnNewBuffers() coded frame processing updates the
  // timestamp offset then |*timestamp_offset_during_append_| is also updated
  // so Append()'s caller can know the new offset. This pointer is only non-NULL
  // during the lifetime of an Append() call.
  TimeDelta* timestamp_offset_during_append_;

  // During Append(), coded frame processing triggered by OnNewBuffers()
  // requires these two attributes. These are only valid during the lifetime of
  // an Append() call.
  TimeDelta append_window_start_during_append_;
  TimeDelta append_window_end_during_append_;

  // Keeps track of whether a media segment is being parsed.
  bool parsing_media_segment_;

  // Valid only while |parsing_media_segment_| is true. These flags enable
  // warning when the parsed media segment doesn't have frames for some track.
  std::map<StreamParser::TrackId, bool> media_segment_has_data_for_track_;

  // The object used to parse appended data.
  std::unique_ptr<StreamParser> stream_parser_;

  // Note that ChunkDemuxerStreams are created and owned by the parent
  // ChunkDemuxer. They are not owned by |this|.
  using DemuxerStreamMap = std::map<StreamParser::TrackId, ChunkDemuxerStream*>;
  DemuxerStreamMap audio_streams_;
  DemuxerStreamMap video_streams_;
  DemuxerStreamMap text_streams_;

  std::unique_ptr<FrameProcessor> frame_processor_;
  const CreateDemuxerStreamCB create_demuxer_stream_cb_;
  MediaLog* media_log_;

  StreamParser::InitCB init_cb_;
  StreamParser::EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  NewTextTrackCB new_text_track_cb_;

  State state_;

  // During Append(), OnNewConfigs() will trigger the initialization segment
  // received algorithm. Note, the MSE spec explicitly disallows this algorithm
  // during an Abort(), since Abort() is allowed only to emit coded frames, and
  // only if the parser is PARSING_MEDIA_SEGMENT (not an INIT segment). So we
  // also have a flag here that indicates if Append is in progress and we can
  // invoke this callback.
  Demuxer::MediaTracksUpdatedCB init_segment_received_cb_;
  bool append_in_progress_ = false;
  bool first_init_segment_received_ = false;
  bool encrypted_media_init_data_reported_ = false;

  std::vector<AudioCodec> expected_audio_codecs_;
  std::vector<VideoCodec> expected_video_codecs_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferState);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_STATE_H_
