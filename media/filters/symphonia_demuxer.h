// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SYMPHONIA_DEMUXER_H_
#define MEDIA_FILTERS_SYMPHONIA_DEMUXER_H_

#include <stdint.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "media/base/container_names.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/symphonia_data_source_bridge.h"
#include "media/filters/symphonia_demuxer_stream.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace media {

class DataSource;
class MediaTracks;
struct SymphoniaDemuxerBridge;
struct SymphoniaReadPacketResult;
struct SymphoniaSeekResult;
struct SymphoniaTrackInfo;

// A `Demuxer` backed by the Symphonia Rust media library, accessed through the
// `symphonia_demuxer_bridge` CXX bridge.
//
// Threading: all public methods (with the exception of CancelPendingSeek())
// should be invoked on the media sequence, and Stop() should be called on that
// sequence before destruction. Construction and destruction may be completed on
// any thread.
//
// Since Symphonia performs I/O synchronously, actions such as probing, packet
// reads, and seeks are posted to a dedicated blocking sequenced task runner,
// and SymphoniaDataSourceBridge acts as an adapter between the asynchronous
// DataSource interface and the synchronous reads expected by Symphonia.
//
// NOTE: this demuxer is currently undergoing active development, and is only
// enabled in a select set of experimental build configurations. See
// crbug.com/550619039 for more information.
//
// What works now:
// - Probing all container formats and starting the first supported audio track.
// - Audio decoder config metadata: codec delay (including Opus pre-skip), seek
//   preroll (80 ms for Opus), and channel layout configurations (including
//   discrete multichannel layouts).
// - Reading packets, including gapless trimming represented by discard padding.
// - Reporting buffered time ranges to `DemuxerHost`.
// - Seeking, aborting reads, and enabling/disabling tracks.
//
// What does not work yet:
// - Video tracks - files with video fall back (see below), or are played audio
//   only if no fallback is available; TODO(crbug.com/583934381).
// - Encrypted media - `encrypted_media_init_data_cb` is never invoked;
//   TODO(crbug.com/583935700).
// - Mid-stream config changes other than a new chained Ogg stream
//   (TODO(crbug.com/583934912)) and bitstream conversions
//   (TODO(crbug.com/583935590)).
// - Packet side data, excluding discard padding; TODO(crbug.com/583935113).
// - Non-zero start time (TODO(crbug.com/583934943)) and timeline offsets
//   (TODO(crbug.com/583936230)).
class MEDIA_EXPORT SymphoniaDemuxer : public Demuxer {
 public:
  // Outcome of `Initialize()`, recorded to the
  // Media.Audio.Symphonia.DemuxerInitResult histogram. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  // LINT.IfChange(SymphoniaDemuxerInitResult)
  enum class InitOutcome {
    kSuccess = 0,
    kFallbackSniffedContainerDisabled = 1,
    kFallbackProbedContainerDisabled = 2,
    kFallbackVideoTrack = 3,
    kError = 4,
    kMaxValue = kError,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:SymphoniaDemuxerInitResult)

  // If `allow_fallback` is true, initialization reports
  // `DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED` for media that Symphonia demuxing is
  // not enabled for (see `IsSymphoniaDemuxingEnabledForContainer()`) or that
  // has a video track, after releasing `data_source` without stopping it, so
  // the owner can play it with another demuxer. If false, Symphonia demuxes
  // whatever it can, and other media fails with a regular error.
  SymphoniaDemuxer(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   DataSource* data_source,
                   const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
                   MediaTracksUpdatedCB media_tracks_updated_cb,
                   std::unique_ptr<MediaLog> media_log,
                   bool allow_fallback);

  SymphoniaDemuxer(const SymphoniaDemuxer&) = delete;
  SymphoniaDemuxer& operator=(const SymphoniaDemuxer&) = delete;

  ~SymphoniaDemuxer() override;

  // Demuxer implementation.
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
  std::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;
  void OnTracksChanged(DemuxerStream::Type track_type,
                       std::optional<MediaTrack::Id> track_id,
                       base::TimeDelta curr_time,
                       TrackChangeCB change_completed_cb) override;
  void SetPlaybackRate(double rate) override;
  std::vector<raw_ptr<DemuxerStream>> GetAllStreams() override;

 private:
  // Output of probing the container on the blocking task runner.
  struct InitResult {
    int64_t duration_us = 0;
    std::string container_name;
    rust::Vec<SymphoniaTrackInfo> tracks;

    // Normally moved into `demuxer_bridge_`. If `Stop()` or
    // `AbortPendingReads()` drops the init reply, it is freed on the media
    // sequence instead, which is safe because the Rust demuxer does not keep a
    // pointer to the source bridge between calls.
    rust::Box<SymphoniaDemuxerBridge> demuxer_bridge;
  };

  // Why initialization failed on the blocking task runner.
  struct InitFailure {
    // `DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED` means Symphonia demuxing is not
    // enabled for `sniffed_container`.
    PipelineStatus status;
    // The container found at the start of the stream, or `kContainerUnknown`
    // if the stream could not be read.
    container_names::MediaContainerName sniffed_container =
        container_names::MediaContainerName::kContainerUnknown;
  };

  // The probe result, or why initialization failed.
  using InitResultOrError = base::expected<InitResult, InitFailure>;

  // Runs on `blocking_task_runner`. Asks Symphonia to probe the container
  // read through `source_bridge`. If `allow_fallback` is true, first checks
  // that Symphonia demuxing is enabled for the container at the start of the
  // stream.
  static InitResultOrError InitializeOnBlockingSequence(
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      SymphoniaDataSourceBridge* source_bridge,
      bool allow_fallback);

  // Creates a stream for an audio `track`, adding it to `media_tracks`. Tracks
  // with an unsupported codec or an invalid config are skipped.
  void AddAudioStream(const SymphoniaTrackInfo& track,
                      MediaTracks& media_tracks);

  // Posts a packet read to the blocking task runner if no read or seek is in
  // flight and at least one enabled stream wants more data.
  void ReadFrameIfNeeded();

  // Returns true if any enabled stream wants more data.
  bool StreamsHaveAvailableCapacity() const;

  // Replies from the blocking task runner, run on the media sequence.
  void OnInitializeDone(InitResultOrError result);
  void OnReadPacketDone(SymphoniaReadPacketResult packet_result);
  void OnSeekDone(SymphoniaSeekResult seek_result);
  void OnTrackChangeSeekDone(SymphoniaSeekResult seek_result);

  // Ends every stream, and sets the duration from the packets read so far if
  // the container did not report one.
  void OnEndOfStream();

  // Handles a new stream starting mid-playback (e.g. chained Ogg) with the
  // container's new `tracks`.
  void OnStreamResetRequired(const rust::Vec<SymphoniaTrackInfo>& tracks);

  // Reports a fatal error to the `DemuxerHost`.
  void OnDemuxerError(PipelineStatus status);

  // Completes initialization with `DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED` so that
  // `DemuxerManager` falls back to FFmpegDemuxer, and records `outcome` and
  // `container` to UMA. Releases `data_source_` without stopping it, so the
  // fallback demuxer can read it.
  void FailInitializationForFallback(
      InitOutcome outcome,
      container_names::MediaContainerName container,
      std::string_view reason);

  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  const MediaTracksUpdatedCB media_tracks_updated_cb_;
  const std::unique_ptr<MediaLog> media_log_;
  const bool allow_fallback_;

  raw_ptr<DataSource> data_source_ = nullptr;
  raw_ptr<DemuxerHost> host_ = nullptr;

  std::vector<std::unique_ptr<SymphoniaDemuxerStream>> streams_;
  // When a chained stream (e.g. chained Ogg) starts with a new track ID,
  // packets with this ID are routed to `streams_[0]`.
  std::optional<uint32_t> chained_track_id_;

  // `kInfiniteDuration` until end of stream if the container does not report
  // a duration.
  base::TimeDelta duration_;
  // True after a chained stream reset (`OnStreamResetRequired()`). Symphonia's
  // Ogg reader probes only the first stream's duration, so chained streams
  // raise `duration_` as later links run past it.
  bool has_chained_stream_ = false;
  // End time of the latest packet read so far, used to set the duration at end
  // of stream.
  base::TimeDelta max_packet_end_time_;
  // End time of the latest packet added to `buffered_ranges_` in the current
  // contiguous read run, or `kNoTimestamp` before any packet (and after a
  // `Seek()`).
  base::TimeDelta last_buffered_timestamp_ = kNoTimestamp;
  Ranges<base::TimeDelta> buffered_ranges_;
  // Atomic because `OnInitializeDone()` sets it on the media sequence, while
  // `DemuxerManager::GetContainerForMetrics()` reads it from the main thread.
  std::atomic<container_names::MediaContainerName> container_name_{
      container_names::MediaContainerName::kContainerUnknown};

  // Created and reset on the media sequence; used and destroyed on
  // `blocking_task_runner_`.
  SymphoniaDataSourceBridge::UniquePtr source_bridge_;
  std::optional<rust::Box<SymphoniaDemuxerBridge>> demuxer_bridge_;

  PipelineStatusCallback pending_init_cb_;
  PipelineStatusCallback pending_seek_cb_;
  // Set while seeking to the current time after a track is re-enabled; runs
  // the `TrackChangeCB` for that change.
  base::OnceClosure pending_track_change_cb_;
  bool is_read_pending_ = false;
  bool is_eos_ = false;
  bool is_stopped_ = false;

  // Detached in the constructor so that it binds to the media sequence on the
  // first call there.
  SEQUENCE_CHECKER(sequence_checker_);

  // Set in `Initialize()` on the media sequence so that `CancelPendingSeek()`
  // may copy this WeakPtr from the main thread without racing on
  // `WeakPtrFactory`.
  base::WeakPtr<SymphoniaDemuxer> persistent_weak_this_;

  // Never invalidated before destruction, unlike `weak_factory_`. Used for
  // WeakPtrs that must survive `AbortPendingReads()`: DataSource error reports,
  // and `CancelPendingSeek()`.
  base::WeakPtrFactory<SymphoniaDemuxer> persistent_weak_factory_{this};

  // Invalidated on abort and stop to drop replies from in-flight blocking
  // tasks.
  base::WeakPtrFactory<SymphoniaDemuxer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_SYMPHONIA_DEMUXER_H_
