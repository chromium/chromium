// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_tracks.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser.h"
#include "media/filters/source_buffer_parse_warnings.h"
#include "media/filters/source_buffer_state.h"
#include "media/filters/source_buffer_stream.h"
#include "media/filters/stream_parser_factory.h"

namespace media {

class SourceBufferStream;

class AudioDecoderConfig;
class VideoDecoderConfig;

class MEDIA_EXPORT ChunkDemuxerStream : public DemuxerStream {
 public:
  using BufferQueue = base::circular_deque<scoped_refptr<StreamParserBuffer>>;

  ChunkDemuxerStream() = delete;

  ChunkDemuxerStream(Type type, MediaTrack::Id media_track_id);

  ChunkDemuxerStream(const ChunkDemuxerStream&) = delete;
  ChunkDemuxerStream& operator=(const ChunkDemuxerStream&) = delete;

  ~ChunkDemuxerStream() override;

  // ChunkDemuxerStream control methods.
  void StartReturningData();
  void AbortReads();
  void CompletePendingReadIfPossible();
  void Shutdown();

  // SourceBufferStream manipulation methods.
  void Seek(base::TimeDelta time);
  bool IsSeekWaitingForData() const;

  // Add buffers to this stream. Buffers are stored in SourceBufferStreams,
  // which handle ordering and overlap resolution.
  // Returns true if buffers were successfully added.
  bool Append(const StreamParser::BufferQueue& buffers);

  // Removes buffers between |start| and |end| according to the steps
  // in the "Coded Frame Removal Algorithm" in the Media Source
  // Extensions Spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-coded-frame-removal
  //
  // |duration| is the current duration of the presentation. It is
  // required by the computation outlined in the spec.
  void Remove(base::TimeDelta start, base::TimeDelta end,
              base::TimeDelta duration);

  // If the buffer is full, attempts to try to free up space, as specified in
  // the "Coded Frame Eviction Algorithm" in the Media Source Extensions Spec.
  // Returns false iff buffer is still full after running eviction.
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-eviction
  bool EvictCodedFrames(base::TimeDelta media_time, size_t newDataSize);

  void OnMemoryPressure(
      base::TimeDelta media_time,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
      bool force_instant_gc);

  // Signal to the stream that duration has changed to |duration|.
  void OnSetDuration(base::TimeDelta duration);

  // Returns the range of buffered data in this stream, capped at |duration|.
  Ranges<base::TimeDelta> GetBufferedRanges(base::TimeDelta duration) const;

  // Returns the lowest PTS of the buffered data.
  // Returns base::TimeDelta() if the stream has no buffered data.
  base::TimeDelta GetLowestPresentationTimestamp() const;

  // Returns the highest PTS of the buffered data.
  // Returns base::TimeDelta() if the stream has no buffered data.
  base::TimeDelta GetHighestPresentationTimestamp() const;

  // Returns the duration of the buffered data.
  // Returns base::TimeDelta() if the stream has no buffered data.
  base::TimeDelta GetBufferedDuration() const;

  // Returns the memory usage of the buffered data in bytes.
  size_t GetMemoryUsage() const;

  // Signal to the stream that buffers handed in through subsequent calls to
  // Append() belong to a coded frame group that starts at |start_pts|.
  // |start_dts| is used only to help tests verify correctness of calls to this
  // method. If |group_start_observer_cb_| is set, first invokes this test-only
  // callback with |start_dts| and |start_pts| to assist test verification.
  void OnStartOfCodedFrameGroup(DecodeTimestamp start_dts,
                                base::TimeDelta start_pts);

  // Called when midstream config updates occur.
  // For audio and video, if the codec is allowed to change, the caller should
  // set |allow_codec_change| to true.
  // Returns true if the new config is accepted.
  // Returns false if the new config should trigger an error.
  bool UpdateAudioConfig(const AudioDecoderConfig& config,
                         bool allow_codec_change,
                         MediaLog* media_log);
  bool UpdateVideoConfig(const VideoDecoderConfig& config,
                         bool allow_codec_change,
                         MediaLog* media_log);

  void MarkEndOfStream();
  void UnmarkEndOfStream();

  // DemuxerStream methods.
  void Read(uint32_t count, ReadCB read_cb) override;
  Type type() const override;
  StreamLiveness liveness() const override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  bool SupportsConfigChanges() override;

  bool IsEnabled() const;
  void SetEnabled(bool enabled, base::TimeDelta timestamp);

  // Sets the memory limit, in bytes, on the SourceBufferStream.
  void SetStreamMemoryLimit(size_t memory_limit);

  void SetLiveness(StreamLiveness liveness);

  MediaTrack::Id media_track_id() const { return media_track_id_; }

  // Allows tests to verify invocations of Append().
  using AppendObserverCB = base::RepeatingCallback<void(const BufferQueue*)>;
  void set_append_observer_for_testing(AppendObserverCB append_observer_cb) {
    append_observer_cb_ = std::move(append_observer_cb);
  }

  // Allows tests to verify invocations of OnStartOfCodedFrameGroup().
  using GroupStartObserverCB =
      base::RepeatingCallback<void(DecodeTimestamp, base::TimeDelta)>;
  void set_group_start_observer_for_testing(
      GroupStartObserverCB group_start_observer_cb) {
    group_start_observer_cb_ = std::move(group_start_observer_cb);
  }

 private:
  enum State {
    UNINITIALIZED,
    RETURNING_DATA_FOR_READS,
    RETURNING_ABORT_FOR_READS,
    SHUTDOWN,
  };

  // Assigns |state_| to |state|
  void ChangeState_Locked(State state) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void CompletePendingReadIfPossible_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::pair<SourceBufferStreamStatus, DemuxerStream::DecoderBufferVector>
  GetPendingBuffers_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Specifies the type of the stream.
  const Type type_;

  StreamLiveness liveness_ GUARDED_BY(lock_);

  std::unique_ptr<SourceBufferStream> stream_ GUARDED_BY(lock_);

  const MediaTrack::Id media_track_id_;

  // Test-only callbacks to assist verification of Append() and
  // OnStartOfCodedFrameGroup() calls, respectively.
  AppendObserverCB append_observer_cb_;
  GroupStartObserverCB group_start_observer_cb_;

  // Requested buffer count. The actual returned buffer count could be less
  // according to DemuxerStream::Read() API.
  uint32_t requested_buffer_count_ = 0;

  mutable base::Lock lock_;
  State state_ GUARDED_BY(lock_);
  ReadCB read_cb_ GUARDED_BY(lock_);
  bool is_enabled_ GUARDED_BY(lock_);
};

// Demuxer implementation that allows chunks of media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer {
 public:
  enum Status {
    kOk,              // ID added w/o error.
    kNotSupported,    // Type specified is not supported.
    kReachedIdLimit,  // Reached ID limit. We can't handle any more IDs.
  };

  // |open_cb| Run when Initialize() is called to signal that the demuxer
  //   is ready to receive media data via AppendToParseBuffer()/AppendChunks().
  // |progress_cb| Run each time data is appended.
  // |encrypted_media_init_data_cb| Run when the demuxer determines that an
  //   encryption key is needed to decrypt the content.
  // |media_log| Used to report content and engine debug messages.
  ChunkDemuxer(base::OnceClosure open_cb,
               base::RepeatingClosure progress_cb,
               EncryptedMediaInitDataCB encrypted_media_init_data_cb,
               MediaLog* media_log);

  ChunkDemuxer(const ChunkDemuxer&) = delete;
  ChunkDemuxer& operator=(const ChunkDemuxer&) = delete;

  ~ChunkDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;

  void Initialize(DemuxerHost* host, PipelineStatusCallback init_cb) override;
  void Stop() override;
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) override;
  bool IsSeekable() const override;
  base::Time GetTimelineOffset() const override;
  std::vector<DemuxerStream*> GetAllStreams() override;
  base::TimeDelta GetStartTime() const override;
  int64_t GetMemoryUsage() const override;
  std::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;
  void AbortPendingReads() override;

  // ChunkDemuxer reads are abortable. StartWaitingForSeek() and
  // CancelPendingSeek() always abort pending and future reads until the
  // expected seek occurs, so that ChunkDemuxer can stay synchronized with the
  // associated JS method calls.
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;

  // Registers a new `id` to use for AppendToParseBuffer(),
  // RunSegmentParserLoop(), AppendChunks(), etc calls. `content_type` indicates
  // the MIME type's ContentType and `codecs` indicates the MIME type's "codecs"
  // parameter string (if any) for the data that we intend to append for this
  // ID. kOk is returned if the demuxer has enough resources to support another
  // ID and supports the format indicated by `content_type` and `codecs`.
  // kReachedIdLimit is returned if the demuxer cannot handle another ID right
  // now. kNotSupported is returned if `content_type` and `codecs` is not a
  // supported format.
  // The `audio_config` and `video_config` overloads behave similarly, except
  // the caller must provide valid, supported decoder configs; those overloads'
  // usage indicates that we intend to append WebCodecs encoded audio or video
  // chunks for this ID.
  [[nodiscard]] Status AddId(const std::string& id,
                             const std::string& content_type,
                             const std::string& codecs);
  [[nodiscard]] Status AddId(const std::string& id,
                             std::unique_ptr<AudioDecoderConfig> audio_config);
  [[nodiscard]] Status AddId(const std::string& id,
                             std::unique_ptr<VideoDecoderConfig> video_config);

  // `AddAutoDetectedCodecsId` operates similarly to the `AddId` methods, except
  // that it creates parsers which are capable of auto-detecting the codecs
  // present. It is used internally by the HLS demuxer.
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  [[nodiscard]] Status AddAutoDetectedCodecsId(
      const std::string& id,
      RelaxedParserSupportedType mime_type);
#endif

  // Notifies a caller via `tracks_updated_cb` that the set of media tracks
  // for a given `id` has changed. This callback must be set before any calls to
  // AppendToParseBuffer() for this `id`.
  void SetTracksWatcher(const std::string& id,
                        MediaTracksUpdatedCB tracks_updated_cb);

  // Notifies a caller via `parse_warning_cb` of a parse warning. This callback
  // must be set before any calls to AppendToParseBuffer() for this `id`.
  void SetParseWarningCallback(const std::string& id,
                               SourceBufferParseWarningCB parse_warning_cb);

  // Removed an ID & associated resources that were previously added with
  // AddId().
  void RemoveId(const std::string& id);

  // Gets the currently buffered ranges for the specified ID.
  Ranges<base::TimeDelta> GetBufferedRanges(const std::string& id) const;

  // Gets the lowest buffered PTS for the specified |id|. If there is nothing
  // buffered, returns base::TimeDelta().
  base::TimeDelta GetLowestPresentationTimestamp(const std::string& id) const;

  // Gets the highest buffered PTS for the specified |id|. If there is nothing
  // buffered, returns base::TimeDelta().
  base::TimeDelta GetHighestPresentationTimestamp(const std::string& id) const;

  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  void SetPlaybackRate(double rate) override {}

  void DisableCanChangeType() override;

  // Appends media data to the source buffer's stream parser associated with
  // `id`. No parsing is done, just buffering the media data for future parsing
  // via RunSegmentParserLoop calls. Returns true on success. Returns false if
  // the parser was unable to allocate resources; content in `data` is not
  // copied as a result, and this failure is reported (through various layers)
  // up to the SourceBuffer's implementation of appendBuffer(), which should
  // then notify the app of append failure using a `QuotaExceededErr` exception
  // per the MSE specification. App could use a back-off and retry strategy or
  // otherwise alter their behavior to attempt to buffer media for further
  // playback.
  [[nodiscard]] bool AppendToParseBuffer(const std::string& id,
                                         base::span<const uint8_t> data);

  // Tells the stream parser for the source buffer associated with `id` to parse
  // more of the data previously sent to it from this object's
  // AppendToParseBuffer(). This operation applies and possibly updates
  // `*timestamp_offset` during coded frame processing. `append_window_start`
  // and `append_window_end` correspond to the MSE spec's similarly named source
  // buffer attributes that are used in coded frame processing.
  // Returns kSuccess if the segment parser loop iteration succeeded and all
  // previously provided data from AppendToParseBuffer() has been inspected.
  // Returns kSuccessHasMoreData if the segment parser loop iteration succeeded,
  // yet there remains uninspected data remaining from AppendToParseBuffer();
  // more call(s) to this method are necessary for the parser to attempt
  // inspection of that data.
  // Returns kFailed if the segment parser loop iteration hit error and the
  // caller needs to run the append error algorithm with decode error parameter
  // set to true.
  [[nodiscard]] StreamParser::ParseStatus RunSegmentParserLoop(
      const std::string& id,
      base::TimeDelta append_window_start,
      base::TimeDelta append_window_end,
      base::TimeDelta* timestamp_offset);

  // Appends webcodecs encoded chunks (already converted by caller into a
  // BufferQueue of StreamParserBuffers) to the source buffer associated with
  // |id|, with same semantic for other parameters and return value as
  // RunSegmentParserLoop().
  [[nodiscard]] bool AppendChunks(
      const std::string& id,
      std::unique_ptr<StreamParser::BufferQueue> buffer_queue,
      base::TimeDelta append_window_start,
      base::TimeDelta append_window_end,
      base::TimeDelta* timestamp_offset);

  // Aborts parsing the current segment and reset the parser to a state where
  // it can accept a new segment.
  // Some pending frames can be emitted during that process. These frames are
  // applied |timestamp_offset|.
  void ResetParserState(const std::string& id,
                        base::TimeDelta append_window_start,
                        base::TimeDelta append_window_end,
                        base::TimeDelta* timestamp_offset);

  // Remove buffers between |start| and |end| for the source buffer
  // associated with |id|.
  void Remove(const std::string& id, base::TimeDelta start,
              base::TimeDelta end);

  // Returns whether or not the source buffer associated with |id| can change
  // its parser type to one which parses |content_type| and |codecs|.
  // |content_type| indicates the ContentType of the MIME type for the data that
  // we intend to append for this |id|; |codecs| similarly indicates the MIME
  // type's "codecs" parameter, if any.
  bool CanChangeType(const std::string& id,
                     const std::string& content_type,
                     const std::string& codecs);

  // For the source buffer associated with |id|, changes its parser type to one
  // which parses |content_type| and |codecs|.  |content_type| indicates the
  // ContentType of the MIME type for the data that we intend to append for this
  // |id|; |codecs| similarly indicates the MIME type's "codecs" parameter, if
  // any.  Caller must first ensure CanChangeType() returns true for the same
  // parameters.  Caller must also ensure that ResetParserState() is done before
  // calling this, to flush any pending frames.
  void ChangeType(const std::string& id,
                  const std::string& content_type,
                  const std::string& codecs);

  // If the buffer is full, attempts to try to free up space, as specified in
  // the "Coded Frame Eviction Algorithm" in the Media Source Extensions Spec.
  // Returns false iff buffer is still full after running eviction.
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-eviction
  [[nodiscard]] bool EvictCodedFrames(const std::string& id,
                                      base::TimeDelta currentMediaTime,
                                      size_t newDataSize);

  void OnMemoryPressure(
      base::TimeDelta currentMediaTime,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
      bool force_instant_gc);

  // Returns the current presentation duration.
  double GetDuration();
  double GetDuration_Locked();

  // Notifies the demuxer that the duration of the media has changed to
  // |duration|.
  void SetDuration(double duration);

  // Returns true if the source buffer associated with |id| is currently parsing
  // a media segment, or false otherwise.
  bool IsParsingMediaSegment(const std::string& id);

  // Returns the 'Generate Timestamps Flag', as described in the MSE Byte Stream
  // Format Registry, for the source buffer associated with |id|.
  bool GetGenerateTimestampsFlag(const std::string& id);

  // Set the append mode to be applied to subsequent buffers appended to the
  // source buffer associated with |id|. If |sequence_mode| is true, caller
  // is requesting "sequence" mode. Otherwise, caller is requesting "segments"
  // mode.
  void SetSequenceMode(const std::string& id, bool sequence_mode);

  // Signals the coded frame processor for the source buffer associated with
  // |id| to update its group start timestamp to be |timestamp_offset| if it is
  // in sequence append mode.
  void SetGroupStartTimestampIfInSequenceMode(const std::string& id,
                                              base::TimeDelta timestamp_offset);

  // Called to signal changes in the "end of stream"
  // state. UnmarkEndOfStream() must not be called if a matching
  // MarkEndOfStream() has not come before it.
  void MarkEndOfStream(PipelineStatus status);
  void UnmarkEndOfStream();

  void Shutdown();

  // Sets the memory limit on each stream of a specific type.
  // |memory_limit| is the maximum number of bytes each stream of type |type|
  // is allowed to hold in its buffer.
  void SetMemoryLimitsForTest(DemuxerStream::Type type, size_t memory_limit);

  // Returns the ranges representing the buffered data in the demuxer.
  // TODO(wolenetz): Remove this method once MediaSourceDelegate no longer
  // requires it for doing hack browser seeks to I-frame on Android. See
  // http://crbug.com/304234.
  Ranges<base::TimeDelta> GetBufferedRanges() const;

 private:
  enum State {
    WAITING_FOR_INIT = 0,
    INITIALIZING,
    INITIALIZED,
    ENDED,

    // Any State at or beyond PARSE_ERROR cannot be changed to a state before
    // this. See ChangeState_Locked.
    PARSE_ERROR,
    SHUTDOWN,
  };

  // Helper for AddId's creation of FrameProcessor, and
  // SourceBufferState creation, initialization and tracking in
  // source_state_map_.
  ChunkDemuxer::Status AddIdInternal(
      const std::string& id,
      std::unique_ptr<media::StreamParser> stream_parser,
      std::optional<std::string_view> expected_codecs);

  // Helper for video and audio track changing. For the `track_type`, enables
  // tracks associated with `track_ids` and disables the rest. Fires
  // `change_completed_cb` when the operation is completed.
  void FindAndEnableProperTracks(const std::vector<MediaTrack::Id>& track_ids,
                                 base::TimeDelta curr_time,
                                 DemuxerStream::Type track_type,
                                 TrackChangeCB change_completed_cb);

  void ChangeState_Locked(State new_state);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  // Returns true if any stream has seeked to a time without buffered data.
  bool IsSeekWaitingForData_Locked() const;

  // Returns true if all streams can successfully call EndOfStream,
  // false if any can not.
  bool CanEndOfStream_Locked() const;

  // SourceBufferState callbacks.
  void OnSourceInitDone(const std::string& source_id,
                        const StreamParser::InitParameters& params);

  // Creates a DemuxerStream of the specified |type| for the SourceBufferState
  // with the given |source_id|.
  // Returns a pointer to a new ChunkDemuxerStream instance, which is owned by
  // ChunkDemuxer.
  ChunkDemuxerStream* CreateDemuxerStream(const std::string& source_id,
                                          DemuxerStream::Type type);

  // Returns true if |source_id| is valid, false otherwise.
  bool IsValidId_Locked(const std::string& source_id) const;

  // Increases |duration_| to |new_duration|, if |new_duration| is higher.
  void IncreaseDurationIfNecessary(base::TimeDelta new_duration);

  // Decreases |duration_| if the buffered region is less than |duration_| when
  // EndOfStream() is called.
  void DecreaseDurationIfNecessary();

  // Sets |duration_| to |new_duration|, sets |user_specified_duration_| to -1
  // and notifies |host_|.
  void UpdateDuration(base::TimeDelta new_duration);

  // Returns the ranges representing the buffered data in the demuxer.
  Ranges<base::TimeDelta> GetBufferedRanges_Locked() const;

  // Start returning data on all DemuxerStreams.
  void StartReturningData();

  void AbortPendingReads_Locked();

  // Completes any pending reads if it is possible to do so.
  void CompletePendingReadsIfPossible();

  // Seeks all SourceBufferStreams to |seek_time|.
  void SeekAllSources(base::TimeDelta seek_time);

  // Generates and returns a unique media track id.
  static MediaTrack::Id GenerateMediaTrackId();

  // Shuts down all DemuxerStreams by calling Shutdown() on
  // all objects in |source_state_map_|.
  void ShutdownAllStreams();

  // Executes |init_cb_| with |status| and closes out the async trace.
  void RunInitCB_Locked(PipelineStatus status);

  // Executes |seek_cb_| with |status| and closes out the async trace.
  void RunSeekCB_Locked(PipelineStatus status);

  mutable base::Lock lock_;
  State state_ = WAITING_FOR_INIT;
  bool cancel_next_seek_ = false;

  // Found dangling on `linux-rel` in
  // `benchmarks.system_health_smoke_test.SystemHealthBenchmarkSmokeTest.
  // system_health.memory_desktop/browse:media:youtubetv:2019`.
  raw_ptr<DemuxerHost, DanglingUntriaged> host_ = nullptr;
  base::OnceClosure open_cb_;
  const base::RepeatingClosure progress_cb_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;

  // MediaLog for reporting messages and properties to debug content and engine.
  raw_ptr<MediaLog> media_log_;

  PipelineStatusCallback init_cb_;
  // Callback to execute upon seek completion.
  // TODO(wolenetz/acolwell): Protect against possible double-locking by first
  // releasing |lock_| before executing this callback. See
  // http://crbug.com/308226
  PipelineStatusCallback seek_cb_;

  using OwnedChunkDemuxerStreamVector =
      std::vector<std::unique_ptr<ChunkDemuxerStream>>;
  OwnedChunkDemuxerStreamVector audio_streams_;
  OwnedChunkDemuxerStreamVector video_streams_;

  // Keep track of which ids still remain uninitialized so that we transition
  // into the INITIALIZED only after all ids/SourceBuffers got init segment.
  std::set<std::string> pending_source_init_ids_;

  base::TimeDelta duration_ = kNoTimestamp;

  // The duration passed to the last SetDuration(). If SetDuration() is never
  // called or a RunSegmentParserLoop()/AppendChunks() call or a EndOfStream()
  // call changes `duration_`, then this variable is set to < 0 to indicate that
  // the `duration_` represents the actual duration instead of a user specified
  // value.
  double user_specified_duration_ = -1;

  base::Time timeline_offset_;
  StreamLiveness liveness_ = StreamLiveness::kUnknown;

  std::map<std::string, std::unique_ptr<SourceBufferState>> source_state_map_;

  std::map<std::string, std::vector<ChunkDemuxerStream*>> id_to_streams_map_;
  // Used to hold alive the demuxer streams that were created for removed /
  // released SourceBufferState objects. Demuxer clients might still have
  // references to these streams, so we need to keep them alive. But they'll be
  // in a shut down state, so reading from them will return EOS.
  std::vector<std::unique_ptr<ChunkDemuxerStream>> removed_streams_;

  std::map<MediaTrack::Id, raw_ptr<ChunkDemuxerStream, CtnExperimental>>
      track_id_to_demux_stream_map_;

  bool supports_change_type_ = true;
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
