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
// - Reading packets, including gapless trimming represented by discard padding.
// - Seeking, aborting reads, and enabling/disabling tracks.
//
// What does not work yet:
// - Video tracks - ignored; TODO(crbug.com/583934381).
// - Encrypted media - `encrypted_media_init_data_cb` is never invoked;
//   TODO(crbug.com/583935700).
// - Mid-stream config changes (TODO(crbug.com/583935452),
//   TODO(crbug.com/583934912)) and bitstream conversions
//   (TODO(crbug.com/583935590)).
// - Packet side data, excluding discard padding; TODO(crbug.com/583935113).
// - Non-zero start time (TODO(crbug.com/583934943)) and timeline offsets
//   (TODO(crbug.com/583936230)).
// - Re-enabling tracks at the current playback time - currently resumes at the
//   current demuxing position; TODO(crbug.com/583935964).
class MEDIA_EXPORT SymphoniaDemuxer : public Demuxer {
 public:
  SymphoniaDemuxer(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   DataSource* data_source,
                   const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
                   MediaTracksUpdatedCB media_tracks_updated_cb,
                   std::unique_ptr<MediaLog> media_log);

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

  // The probe result, or Symphonia's error message if probing failed.
  using InitResultOrError = base::expected<InitResult, std::string>;

  // Runs on `blocking_task_runner`. Asks Symphonia to probe the container
  // read through `source_bridge`.
  static InitResultOrError InitializeOnBlockingSequence(
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      SymphoniaDataSourceBridge* source_bridge);

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

  // Ends every stream, and sets the duration from the packets read so far if
  // the container did not report one.
  void OnEndOfStream();

  // Reports a fatal error to the `DemuxerHost`.
  void OnDemuxerError(PipelineStatus status);

  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  const MediaTracksUpdatedCB media_tracks_updated_cb_;
  const std::unique_ptr<MediaLog> media_log_;

  raw_ptr<DataSource> data_source_ = nullptr;
  raw_ptr<DemuxerHost> host_ = nullptr;

  std::vector<std::unique_ptr<SymphoniaDemuxerStream>> streams_;
  // `kInfiniteDuration` until end of stream if the container does not report
  // a duration.
  base::TimeDelta duration_;
  // End time of the latest packet read so far, used to set the duration at end
  // of stream.
  base::TimeDelta max_packet_end_time_;
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
