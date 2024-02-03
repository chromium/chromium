// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_PLAYBACK_EVENTS_RECORDER_H_
#define MEDIA_MOJO_SERVICES_PLAYBACK_EVENTS_RECORDER_H_

#include <optional>

#include "base/time/time.h"
#include "media/mojo/mojom/playback_events_recorder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MEDIA_MOJO_EXPORT PlaybackEventsRecorder final
    : public mojom::PlaybackEventsRecorder {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::PlaybackEventsRecorder> receiver);

  PlaybackEventsRecorder();
  ~PlaybackEventsRecorder() final;

  PlaybackEventsRecorder(const PlaybackEventsRecorder&) = delete;
  PlaybackEventsRecorder& operator=(const PlaybackEventsRecorder&) =
      delete;

  // mojom::PlaybackEventsRecorder implementation.
  void OnPlaying() final;
  void OnPaused() final;
  void OnSeeking() final;
  void OnEnded() final;
  void OnBuffering() final;
  void OnBufferingComplete() final;
  void OnError(const PipelineStatus& status) final;
  void OnNaturalSizeChanged(const gfx::Size& size) final;
  void OnPipelineStatistics(const PipelineStatistics& stats) final;

 private:
  class BitrateEstimator {
   public:
    BitrateEstimator();
    ~BitrateEstimator();

    void Update(const PipelineStatistics& stats);
    void OnPause();

   private:
    base::TimeDelta time_elapsed_;
    size_t audio_bytes_ = 0;
    size_t video_bytes_ = 0;

    std::optional<PipelineStatistics> last_stats_;
    base::TimeTicks last_stats_time_;
  };

  enum class BufferingState {
    kInitialBuffering,
    kBuffering,
    kBuffered,
  };

  BufferingState buffering_state_ = BufferingState::kInitialBuffering;
  base::TimeTicks buffering_start_time_;
  base::TimeTicks last_buffering_end_time_;

  BitrateEstimator bitrate_estimator_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_PLAYBACK_EVENTS_RECORDER_H_
