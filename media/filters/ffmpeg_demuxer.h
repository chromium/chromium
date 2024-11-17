// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Demuxer interface using FFmpeg's libavformat.  At this time
// will support demuxing any audio/video format thrown at it.  The streams
// output mime types audio/x-ffmpeg and video/x-ffmpeg and include an integer
// key FFmpegCodecID which contains the CodecID enumeration value.  The CodecIDs
// can be used to create and initialize the corresponding FFmpeg decoder.
//
// FFmpegDemuxer sets the duration of pipeline during initialization by using
// the duration of the longest audio/video stream.
//
// NOTE: since FFmpegDemuxer reads packets sequentially without seeking, media
// files with very large drift between audio/video streams may result in
// excessive memory consumption.
//
// When stopped, FFmpegDemuxer and FFmpegDemuxerStream release all callbacks
// and buffered packets.  Reads from a stopped FFmpegDemuxerStream will not be
// replied to.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer_queue.h"
#include "media/base/demuxer.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/blocking_url_protocol.h"
#include "media/media_buildflags.h"

// FFmpeg forward declarations.
struct AVFormatContext;
struct AVRational;
struct AVStream;

namespace media {

class MediaLog;
class FFmpegBitstreamConverter;
class FFmpegDemuxer;
class FFmpegGlue;

class MEDIA_EXPORT FFmpegDemuxerStream : public DemuxerStream {
 public:
  // Attempts to create FFmpegDemuxerStream form the given AVStream. Will return
  // null if the AVStream cannot be translated into a valid decoder config.
  //
  // FFmpegDemuxerStream keeps a copy of |demuxer| and initializes itself using
  // information inside |stream|. Both parameters must outlive |this|.
  static std::unique_ptr<FFmpegDemuxerStream> Create(FFmpegDemuxer* demuxer,
                                                     AVStream* stream,
                                                     MediaLog* media_log);

  FFmpegDemuxerStream(const FFmpegDemuxerStream&) = delete;
  FFmpegDemuxerStream& operator=(const FFmpegDemuxerStream&) = delete;

  ~FFmpegDemuxerStream() override;

  // Enqueues the given AVPacket. It is invalid to queue a |packet| after
  // SetEndOfStream() has been called.
  void EnqueuePacket(ScopedAVPacket packet);

  // Enters the end of stream state. After delivering remaining queued buffers
  // only end of stream buffers will be delivered.
  void SetEndOfStream();

  // Drops queued buffers and clears end of stream state.
  // Passing |preserve_packet_position| will prevent replay of already seen
  // packets.
  void FlushBuffers(bool preserve_packet_position);

  // Empties the queues and ignores any additional calls to Read().
  void Stop();

  // Aborts any pending reads.
  void Abort();

  base::TimeDelta duration() const { return duration_; }

  // Enables fixes for files with negative timestamps.  Normally all timestamps
  // are rebased against FFmpegDemuxer::start_time() whenever that value is
  // negative.  When this fix is enabled, only AUDIO stream packets will be
  // rebased to time zero, all other stream types will use the muxed timestamp.
  //
  // Further, when no codec delay is present, all AUDIO packets which originally
  // had negative timestamps will be marked for post-decode discard.  When codec
  // delay is present, it is assumed the decoder will handle discard and does
  // not need the AUDIO packets to be marked for discard; just rebased to zero.
  void enable_negative_timestamp_fixups() {
    fixup_negative_timestamps_ = true;
  }
  void enable_chained_ogg_fixups() { fixup_chained_ogg_ = true; }

  // DemuxerStream implementation.
  Type type() const override;
  StreamLiveness liveness() const override;
  void Read(uint32_t count, ReadCB read_cb) override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;

  bool IsEnabled() const;
  void SetEnabled(bool enabled, base::TimeDelta timestamp);

  void SetLiveness(StreamLiveness liveness);

  // Returns the range of buffered data in this stream.
  Ranges<base::TimeDelta> GetBufferedRanges() const;

  // Returns true if this stream has capacity for additional data.
  bool HasAvailableCapacity();

  // Returns the total memory usage of FFMpegDemuxerStream.
  size_t MemoryUsage() const;

  // Returns the value associated with |key| in the metadata for the avstream.
  // Returns an empty string if the key is not present.
  std::string GetMetadata(const char* key) const;

  AVStream* av_stream() const { return stream_; }

  base::TimeDelta start_time() const { return start_time_; }
  void set_start_time(base::TimeDelta time) { start_time_ = time; }

 private:
  friend class FFmpegDemuxerTest;

  // Use FFmpegDemuxerStream::Create to construct.
  // Audio/Video streams must include their respective DecoderConfig. At most
  // one DecoderConfig should be provided (leaving the other nullptr).
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer,
                      AVStream* stream,
                      std::unique_ptr<AudioDecoderConfig> audio_config,
                      std::unique_ptr<VideoDecoderConfig> video_config,
                      MediaLog* media_log);

  // Runs |read_cb_| if present with the front of |buffer_queue_|, calling
  // NotifyCapacityAvailable() if capacity is still available.
  void SatisfyPendingRead();

  // Converts an FFmpeg stream timestamp into a base::TimeDelta.
  static base::TimeDelta ConvertStreamTimestamp(const AVRational& time_base,
                                                int64_t timestamp);

  // Resets any currently active bitstream converter.
  void ResetBitstreamConverter();

  // Create new bitstream converter, destroying active converter if present.
  void InitBitstreamConverter();

  raw_ptr<FFmpegDemuxer> demuxer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<AVStream> stream_;
  base::TimeDelta start_time_;
  std::unique_ptr<AudioDecoderConfig> audio_config_;
  std::unique_ptr<VideoDecoderConfig> video_config_;
  raw_ptr<MediaLog> media_log_;
  Type type_ = UNKNOWN;
  StreamLiveness liveness_ = StreamLiveness::kUnknown;
  base::TimeDelta duration_;
  bool end_of_stream_;
  base::TimeDelta last_packet_timestamp_;
  base::TimeDelta last_packet_duration_;
  Ranges<base::TimeDelta> buffered_ranges_;
  bool is_enabled_;
  bool waiting_for_keyframe_;
  bool aborted_;

  DecoderBufferQueue buffer_queue_;
  ReadCB read_cb_;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<FFmpegBitstreamConverter> bitstream_converter_;
#endif

  std::string encryption_key_id_;
  bool fixup_negative_timestamps_;
  bool fixup_chained_ogg_;

  int num_discarded_packet_warnings_;
  int64_t last_packet_pos_;
  int64_t last_packet_dts_;
  // Requested buffer count. The actual returned buffer count could be less
  // according to DemuxerStream::Read() API.
  size_t requested_buffer_count_ = 0;
};

class MEDIA_EXPORT FFmpegDemuxer : public Demuxer {
 public:
  FFmpegDemuxer(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                DataSource* data_source,
                const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
                MediaTracksUpdatedCB media_tracks_updated_cb,
                MediaLog* media_log,
                bool is_local_file);

  FFmpegDemuxer(const FFmpegDemuxer&) = delete;
  FFmpegDemuxer& operator=(const FFmpegDemuxer&) = delete;

  ~FFmpegDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback init_cb) override;
  void AbortPendingReads() override;
  void Stop() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) override;
  bool IsSeekable() const override;
  base::Time GetTimelineOffset() const override;
  std::vector<DemuxerStream*> GetAllStreams() override;
  base::TimeDelta GetStartTime() const override;
  int64_t GetMemoryUsage() const override;
  std::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;

  // Calls |encrypted_media_init_data_cb_| with the initialization data
  // encountered in the file.
  void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                const std::string& encryption_key_id);

  // Allow FFmpegDemuxerStream to notify us when there is updated information
  // about capacity and what buffered data is available.
  void NotifyCapacityAvailable();
  void NotifyBufferingChanged();

  // Allow FFmpegDemxuerStream to notify us about an error.
  void NotifyDemuxerError(PipelineStatus error);

  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void SetPlaybackRate(double rate) override {}

  // The lowest demuxed timestamp.  If negative, DemuxerStreams must use this to
  // adjust packet timestamps such that external clients see a zero-based
  // timeline.
  base::TimeDelta start_time() const { return start_time_; }

  // Task runner used to execute blocking FFmpeg operations.
  scoped_refptr<base::SequencedTaskRunner> ffmpeg_task_runner() {
    return blocking_task_runner_;
  }

  container_names::MediaContainerName container() const {
    return glue_ ? glue_->container()
                 : container_names::MediaContainerName::kContainerUnknown;
  }

 private:
  // To allow tests access to privates.
  friend class FFmpegDemuxerTest;

  // Helper for video and audio track changing. For the `track_type`, enables
  // tracks associated with `track_ids` and disables the rest. Fires
  // `change_completed_cb` when the operation is completed.
  void FindAndEnableProperTracks(const std::vector<MediaTrack::Id>& track_ids,
                                 base::TimeDelta curr_time,
                                 DemuxerStream::Type track_type,
                                 TrackChangeCB change_completed_cb);

  // FFmpeg callbacks during initialization.
  void OnOpenContextDone(bool result);
  void OnFindStreamInfoDone(int result);

  void LogMetadata(AVFormatContext* avctx, base::TimeDelta max_duration);

  // Finds the stream with the lowest known start time (i.e. not kNoTimestamp
  // start time) with enabled status matching |enabled|.
  FFmpegDemuxerStream* FindStreamWithLowestStartTimestamp(bool enabled);

  // Finds a preferred stream for seeking to |seek_time|. Preference is
  // typically given to video streams, unless the |seek_time| is earlier than
  // the start time of the video stream. In that case a stream with the earliest
  // start time is preferred. Disabled streams are considered only as the last
  // fallback option.
  FFmpegDemuxerStream* FindPreferredStreamForSeeking(base::TimeDelta seek_time);

  // FFmpeg callbacks during seeking.
  void OnSeekFrameDone(int result);

  // FFmpeg callbacks during reading + helper method to initiate reads.
  void ReadFrameIfNeeded();
  void OnReadFrameDone(ScopedAVPacket packet, int result);

  // Returns true iff any stream has additional capacity. Note that streams can
  // go over capacity depending on how the file is muxed.
  bool StreamsHaveAvailableCapacity();

  // Returns true if the maximum allowed memory usage has been reached.
  bool IsMaxMemoryUsageReached() const;

  // Signal all FFmpegDemuxerStreams that the stream has ended.
  void StreamHasEnded();

  // Called by |url_protocol_| whenever |data_source_| returns a read error.
  void OnDataSourceError();

  // Returns the first stream from |streams_| that matches |type| as an
  // FFmpegDemuxerStream and is enabled.
  FFmpegDemuxerStream* GetFirstEnabledFFmpegStream(
      DemuxerStream::Type type) const;

  void SetLiveness(StreamLiveness liveness);

  void SeekInternal(base::TimeDelta time,
                    base::OnceCallback<void(int)> seek_cb);
  void OnVideoSeekedForTrackChange(DemuxerStream* video_stream,
                                   base::OnceClosure seek_completed_cb,
                                   int result);
  void SeekOnVideoTrackChange(base::TimeDelta seek_to_time,
                              TrackChangeCB seek_completed_cb,
                              const std::vector<DemuxerStream*>& streams);

  // Executes |init_cb_| with |status| and closes out the async trace.
  void RunInitCB(PipelineStatus status);

  // Executes |pending_seek_cb_| with |status| and closes out the async trace.
  void RunPendingSeekCB(PipelineStatus status);

  raw_ptr<DemuxerHost, AcrossTasksDanglingUntriaged> host_ = nullptr;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Task runner on which all blocking FFmpeg operations are executed; retrieved
  // from base::ThreadPoolInstance.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  PipelineStatusCallback init_cb_;

  // Indicates if Stop() has been called.
  bool stopped_ = false;

  // Tracks if there's an outstanding av_read_frame() operation.
  //
  // TODO(scherkus): Allow more than one read in flight for higher read
  // throughput using demuxer_bench to verify improvements.
  bool pending_read_ = false;

  // Tracks if there's an outstanding av_seek_frame() operation. Used to discard
  // results of pre-seek av_read_frame() operations.
  PipelineStatusCallback pending_seek_cb_;

  // |streams_| mirrors the AVStream array in AVFormatContext. It contains
  // FFmpegDemuxerStreams encapsluating AVStream objects at the same index.
  //
  // Since we only support a single audio and video stream, |streams_| will
  // contain NULL entries for additional audio/video streams as well as for
  // stream types that we do not currently support.
  //
  // Once initialized, operations on FFmpegDemuxerStreams should be carried out
  // on the demuxer thread.
  using StreamVector = std::vector<std::unique_ptr<FFmpegDemuxerStream>>;
  StreamVector streams_;

  // Provides asynchronous IO to this demuxer. Consumed by |url_protocol_| to
  // integrate with libavformat.
  raw_ptr<DataSource> data_source_;

  raw_ptr<MediaLog> media_log_;

  // Derived bitrate after initialization has completed.
  int bitrate_ = 0;

  // The first timestamp of the audio or video stream, whichever is lower.  This
  // is used to adjust timestamps so that external consumers always see a zero
  // based timeline.
  base::TimeDelta start_time_ = kNoTimestamp;

  // The Time associated with timestamp 0. Set to a null
  // time if the file doesn't have an association to Time.
  base::Time timeline_offset_;

  // Set if we know duration of the audio stream. Used when processing end of
  // stream -- at this moment we definitely know duration.
  bool duration_known_ = false;
  base::TimeDelta duration_;

  // FFmpegURLProtocol implementation and corresponding glue bits.
  std::unique_ptr<BlockingUrlProtocol> url_protocol_;
  std::unique_ptr<FFmpegGlue> glue_;

  const EncryptedMediaInitDataCB encrypted_media_init_data_cb_;

  const MediaTracksUpdatedCB media_tracks_updated_cb_;

  base::flat_map<MediaTrack::Id, raw_ptr<FFmpegDemuxerStream, CtnExperimental>>
      track_id_to_demux_stream_map_;

  const bool is_local_file_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtr<FFmpegDemuxer> weak_this_;
  base::WeakPtrFactory<FFmpegDemuxer> cancel_pending_seek_factory_{this};
  base::WeakPtrFactory<FFmpegDemuxer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_
