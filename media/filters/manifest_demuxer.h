// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MANIFEST_DEMUXER_H_
#define MEDIA_FILTERS_MANIFEST_DEMUXER_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/stream_parser_factory.h"

namespace media {

// Declared and defined in manifest_demuxer.cc.
class ManifestDemuxerStream;

// This class provides an interface for ManifestDemuxer::Engine to talk to
// ManifestDemuxer about the internal ChunkDemuxer, without allowing it to call
// any of the methods that the pipeline uses, like Seek or Stop.
class MEDIA_EXPORT ManifestDemuxerEngineHost {
 public:
  virtual ~ManifestDemuxerEngineHost() {}

  // Adds a new role to the chunk demuxer, and returns true if it succeeded.
  virtual bool AddRole(std::string_view role,
                       RelaxedParserSupportedType mime) = 0;

  // Removes a role (on the media thread) to ensure that there are no
  // media-thread-bound weak references.
  virtual void RemoveRole(std::string_view role) = 0;

  // Sets the sequence mode flag for a |role| which has been created with
  // `AddRole`
  virtual void SetSequenceMode(std::string_view role, bool sequence_mode) = 0;

  // Sets the chunk demuxer duration.
  virtual void SetDuration(double duration) = 0;

  // Get the ranges that chunk demuxer has loaded, which allow seeking to avoid
  // fetching new data, if the seek is into a loaded range already.
  virtual Ranges<base::TimeDelta> GetBufferedRanges(std::string_view role) = 0;

  // Removes all data from the chunk demuxer between `start` and `end`.
  virtual void Remove(std::string_view role,
                      base::TimeDelta start,
                      base::TimeDelta end) = 0;

  // Removes all data from the chunk demuxer between |start| and |end| for a
  // given role, and resets the parser state while updating the parse offset.
  virtual void RemoveAndReset(std::string_view role,
                              base::TimeDelta start,
                              base::TimeDelta end,
                              base::TimeDelta* offset) = 0;

  // Checks to see if we're parsing a media segment and if it is the case, then
  // resets the group start timestamp.
  virtual void SetGroupStartIfParsingAndSequenceMode(std::string_view role,
                                                     base::TimeDelta start) = 0;

  // Evicts frames from chunk demuxer.
  virtual void EvictCodedFrames(std::string_view role,
                                base::TimeDelta time,
                                size_t data_size) = 0;

  // Appends data to the chunk demuxer, parses it, and returns true if the new
  // data was parsed successfully.
  virtual bool AppendAndParseData(std::string_view role,
                                  base::TimeDelta end,
                                  base::TimeDelta* offset,
                                  base::span<const uint8_t> data) = 0;

  // Reset the parser state in chunk demuxer.
  virtual void ResetParserState(std::string_view role,
                                base::TimeDelta end,
                                base::TimeDelta* offset);

  // Allow seeking from within an implementation.
  virtual void RequestSeek(base::TimeDelta time) = 0;

  // Handle errors.
  virtual void OnError(PipelineStatus error) = 0;

  virtual void SetGroupStartTimestamp(std::string_view role,
                                      base::TimeDelta time) = 0;

  virtual void SetEndOfStream() = 0;
  virtual void UnsetEndOfStream() = 0;
};

// A Demuxer designed to allow implementation of media demuxers which don't
// rely on raw media data alone, such as HLS or DASH. This demuxer owns an
// implementation of an engine and handles the seeking and event dispatching
// for the engine so that it can focus on keeping internal manifest states up
// to date.
class MEDIA_EXPORT ManifestDemuxer : public Demuxer, ManifestDemuxerEngineHost {
 public:
  using DelayCallback = base::OnceCallback<void(base::TimeDelta)>;

  // Seeks respond with either:
  //  - an error
  //  - kIsReady: buffers are full and chunk demuxer can seek normally.
  //  - kNeedsData: buffers are empty and need more data before chunk demuxer
  //                would otherwise finish seeking.
  enum class SeekState {
    kIsReady,
    kNeedsData,
  };
  using SeekResponse = PipelineStatus::Or<SeekState>;
  using SeekCallback = base::OnceCallback<void(SeekResponse)>;

  class Engine {
   public:
    virtual ~Engine() {}

    // Set for the engine, such as fetching manifests or content.
    virtual void Initialize(ManifestDemuxerEngineHost* demuxer,
                            PipelineStatusCallback status_cb) = 0;

    // Get the name of the engine impl.
    virtual std::string GetName() const = 0;

    // A tick signal indicating that the state of the engine should be
    // checked. `time` is the current player time. `playback_rate` is the
    // current playback rate. `loaded_ranges` is the current set of loaded
    // ranges in the chunk demuxer. `cb` should be called with the amount of
    // time to delay until the next event is requested.
    virtual void OnTimeUpdate(base::TimeDelta time,
                              double playback_rate,
                              DelayCallback cb) = 0;

    // A synchronous seek, mostly intended to reset parts of the chunk
    // demuxer. returns whether the chunk demuxer needs more data.
    virtual void Seek(base::TimeDelta time, SeekCallback cb) = 0;

    // Start waiting for seek, usually means canceling outstanding events
    // and network fetches.
    virtual void StartWaitingForSeek() = 0;

    // Abort any pending reads, parses, or network requests. calls CB when
    // finished.
    virtual void AbortPendingReads(base::OnceClosure cb) = 0;

    // Returns whether this engine supports seeking. Some live stream content
    // can't be seeked.
    virtual bool IsSeekable() const = 0;

    // Gets the memory usage of the engine.
    virtual int64_t GetMemoryUsage() const = 0;

    // Stop demuxing and clean up pending CBs.
    virtual void Stop() = 0;
  };

  // ManifestDemuxer takes and keeps ownership of `impl` for the lifetime of
  // both.
  ManifestDemuxer(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                  base::RepeatingCallback<void(base::TimeDelta)> request_seek,
                  std::unique_ptr<Engine> impl,
                  MediaLog* media_log);

  ~ManifestDemuxer() override;

  // `media::Demuxer` implementation
  std::vector<DemuxerStream*> GetAllStreams() override;
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  bool IsSeekable() const override;
  void Stop() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  void SetPlaybackRate(double rate) override;
  std::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;

  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  // `ManifestDemuxerEngineHost` implementation
  bool AddRole(std::string_view role, RelaxedParserSupportedType mime) override;
  void RemoveRole(std::string_view role) override;
  void SetSequenceMode(std::string_view role, bool sequence_mode) override;
  void SetDuration(double duration) override;
  Ranges<base::TimeDelta> GetBufferedRanges(std::string_view role) override;
  void Remove(std::string_view role,
              base::TimeDelta start,
              base::TimeDelta end) override;
  void RemoveAndReset(std::string_view role,
                      base::TimeDelta start,
                      base::TimeDelta end,
                      base::TimeDelta* offset) override;
  void SetGroupStartIfParsingAndSequenceMode(std::string_view role,
                                             base::TimeDelta start) override;
  void EvictCodedFrames(std::string_view role,
                        base::TimeDelta time,
                        size_t data_size) override;
  bool AppendAndParseData(std::string_view role,
                          base::TimeDelta end,
                          base::TimeDelta* offset,
                          base::span<const uint8_t> data) override;
  void ResetParserState(std::string_view role,
                        base::TimeDelta end,
                        base::TimeDelta* offset) override;
  void OnError(PipelineStatus status) override;
  void RequestSeek(base::TimeDelta time) override;
  void SetGroupStartTimestamp(std::string_view role,
                              base::TimeDelta time) override;
  void SetEndOfStream() override;
  void UnsetEndOfStream() override;

  // Allow unit tests to grab the chunk demuxer.
  ChunkDemuxer* GetChunkDemuxerForTesting();
  bool has_pending_seek_for_testing() const { return !pending_seek_.is_null(); }
  base::TimeDelta get_media_time_for_testing() const { return media_time_; }
  bool has_pending_event_for_testing() const { return has_pending_event_; }
  bool has_next_task_for_testing() const {
    return !cancelable_next_event_.IsCancelled();
  }

 private:
  // This wrapper class allows us to capture the results of Read() and use
  // DecoderBuffer timestamps to update the current media time within the
  // loaded buffer, without having to make modifications to ChunkDemuxer.
  class ManifestDemuxerStream : public DemuxerStream {
   public:
    ~ManifestDemuxerStream() override;
    using WrapperReadCb =
        base::RepeatingCallback<void(DemuxerStream::ReadCB,
                                     DemuxerStream::Status,
                                     DemuxerStream::DecoderBufferVector)>;
    ManifestDemuxerStream(DemuxerStream* stream, WrapperReadCb cb);
    void Read(uint32_t count, DemuxerStream::ReadCB cb) override;
    AudioDecoderConfig audio_decoder_config() override;
    VideoDecoderConfig video_decoder_config() override;
    DemuxerStream::Type type() const override;
    StreamLiveness liveness() const override;
    void EnableBitstreamConverter() override;
    bool SupportsConfigChanges() override;

   private:
    WrapperReadCb read_cb_;
    raw_ptr<DemuxerStream> stream_;
  };

  void OnChunkDemuxerInitialized(PipelineStatus init_status);
  void OnChunkDemuxerOpened();
  void OnProgress();
  void OnEncryptedMediaData(EmeInitDataType type,
                            const std::vector<uint8_t>& data);
  void OnChunkDemuxerParseWarning(std::string role,
                                  SourceBufferParseWarning warning);
  void OnChunkDemuxerTracksChanged(std::string role,
                                   std::unique_ptr<MediaTracks> tracks);

  void OnDemuxerStreamRead(DemuxerStream::ReadCB wrapped_read_cb,
                           DemuxerStream::Status status,
                           DemuxerStream::DecoderBufferVector buffers);

  // Maps ChunkDemuxerStream instances to our internal ones for track changes.
  void MapDemuxerStreams(TrackChangeCB cb,
                         const std::vector<DemuxerStream*>&);

  std::vector<MediaTrack::Id> MapTrackIds(
      const std::vector<MediaTrack::Id>& track_ids);

  // Helper for the `Seek` call, so that returning from an event when a seek
  // is pending can continue the seek process.
  void SeekInternal();
  void OnEngineSeeked(SeekResponse seek_status);
  void OnChunkDemuxerSeeked(PipelineStatus seek_status);
  void OnSeekBuffered(base::TimeDelta delay_time);

  // Allows for both the chunk demuxer and the engine to be required for
  // initialization.
  void OnEngineInitialized(PipelineStatus status);
  void MaybeCompleteInitialize();

  // Trigger the next event, and based on it's expected delay, post a
  // cancellable callback to TriggerEvent again.
  void TriggerEvent();
  void TriggerEventWithTime(DelayCallback cb, base::TimeDelta current_time);
  void OnEngineEventFinished(base::TimeDelta delay_time);

  base::RepeatingCallback<void(base::TimeDelta)> request_seek_;

  std::unique_ptr<MediaLog> media_log_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Pending callbacks.
  PipelineStatusCallback pending_seek_;
  PipelineStatusCallback pending_init_;

  // Engine implementation
  std::unique_ptr<Engine> impl_;

  // Wrapped chunk demuxer that actually does the parsing and demuxing of the
  // raw data we feed it.
  std::unique_ptr<ChunkDemuxer> chunk_demuxer_;
  raw_ptr<DemuxerHost> host_;

  // Updated by seek, and by updates from outgoing frames.
  base::TimeDelta media_time_ = base::Seconds(0);

  // Playback rate helps calculate how often we should check for new data.
  double current_playback_rate_ = 0.0;

  // Keeps a map of demuxer streams to their wrapper implementations which
  // can be used to set the current media time. ChunkDemuxer's streams live
  // forever due to the use of raw pointers in the pipeline, so these must
  // also live for the duration of `this` lifetime.
  base::flat_map<DemuxerStream*, std::unique_ptr<ManifestDemuxerStream>>
      streams_;

  // Flags for the two part asynchronous initialization process.
  bool demuxer_opened_ = false;
  bool engine_impl_ready_ = false;

  bool can_complete_seek_ = true;

  // Pending an event. Don't trigger a new event chain while one is in
  // progress.
  bool has_pending_event_ = false;

  std::optional<MediaTrack::Id> internal_video_track_id_;
  std::optional<MediaTrack::Id> internal_audio_track_id_;

  // A pending "next event" callback, which can be canceled in the case of a
  // seek or a playback rate change.
  base::CancelableOnceClosure cancelable_next_event_;

  base::WeakPtrFactory<ManifestDemuxer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_MANIFEST_DEMUXER_H_
