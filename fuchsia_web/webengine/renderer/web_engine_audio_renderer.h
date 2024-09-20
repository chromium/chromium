// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_RENDERER_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_RENDERER_H_

#include <fuchsia/media/cpp/fidl.h>

#include <list>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "media/base/audio_renderer.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/time_source.h"
#include "media/fuchsia/common/sysmem_buffer_stream.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "media/fuchsia/common/vmo_buffer.h"

namespace media {

class MediaLog;
}

// AudioRenderer implementation that output audio to AudioConsumer interface on
// Fuchsia. Unlike the default AudioRendererImpl it doesn't decode audio and
// sends encoded stream directly to AudioConsumer provided by the platform.
class WEB_ENGINE_EXPORT WebEngineAudioRenderer final
    : public media::AudioRenderer,
      public media::TimeSource,
      public media::SysmemBufferStream::Sink {
 public:
  WebEngineAudioRenderer(media::MediaLog* media_log,
                         fidl::InterfaceHandle<fuchsia::media::AudioConsumer>
                             audio_consumer_handle);
  ~WebEngineAudioRenderer() override;

  // AudioRenderer implementation.
  void Initialize(media::DemuxerStream* stream,
                  media::CdmContext* cdm_context,
                  media::RendererClient* client,
                  media::PipelineStatusCallback init_cb) override;
  TimeSource* GetTimeSource() override;
  void Flush(base::OnceClosure callback) override;
  void StartPlaying() override;
  void SetVolume(float volume) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement) override;

  // TimeSource implementation.
  void StartTicking() override;
  void StopTicking() override;
  void SetPlaybackRate(double playback_rate) override;
  void SetMediaTime(base::TimeDelta time) override;
  base::TimeDelta CurrentMediaTime() override;
  bool GetWallClockTimes(
      const std::vector<base::TimeDelta>& media_timestamps,
      std::vector<base::TimeTicks>* wall_clock_times) override;

 private:
  enum class PlaybackState {
    kStopped,

    // StartTicking() was called, but sysmem buffers haven't been allocated yet.
    // AudioConsumer::Start() will be called after CreateStreamSink() once the
    // sysmem buffers are allocated.
    kStartPending,

    // We've called Start(), but haven't received updated state. |start_time_|
    // should not be used yet.
    kStarting,

    // Playback is active. When the stream reaches EOS it stays in the kPlaying
    // state.
    kPlaying,

    // AudioConsumer was started, but the rate is set to 0. This state is used
    // to avoid stopping AudioConsumer unnecessarily between StopTicking() and
    // StartTicking().
    kPaused,
  };

  void StartAudioConsumer();

  // Returns current PlaybackState. Should be used only on the main thread.
  PlaybackState GetPlaybackState() NO_THREAD_SAFETY_ANALYSIS;

  // Used to update |state_|. Can be called only in the main thread. This is
  // necessary to ensure that GetPlaybackState() without locks is safe. Caller
  // must acquire |timeline_lock_|.
  void SetPlaybackState(PlaybackState state)
      EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // Resets AudioConsumer and reports error to the |client_|.
  void OnError(media::PipelineStatus Status);

  // Connects |volume_control_|, if it hasn't been connected, and then sets
  // |volume_|.
  void UpdateVolume();

  void InitializeStream();

  // Callback for input_buffer_collection_.AcquireBuffers().
  void OnBuffersAcquired(
      std::vector<media::VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& buffer_settings);

  // Initializes |stream_sink_|. Called during initialization and every time
  // configuration changes.
  void InitializeStreamSink();

  // Calculates desirable playback rate based on `playback_rate_` and
  // `playback_state_` and sends it to the AudioConsumer.
  void UpdatePlaybackRate();

  // Helpers to receive AudioConsumerStatus from the |audio_consumer_|.
  void RequestAudioConsumerStatus();
  void OnAudioConsumerStatusChanged(fuchsia::media::AudioConsumerStatus status);

  // Calls `ScheduleReadDemuxerStream()` and `ScheduleOutOfBufferTimer()` to
  // schedule the corresponding timers.
  void ScheduleBufferTimers();

  // Helpers to pump data from |demuxer_stream_| to |stream_sink_|.
  void ScheduleReadDemuxerStream(bool is_time_moving,
                                 base::TimeTicks end_of_buffer_time);
  void ReadDemuxerStream();
  void OnDemuxerStreamReadDone(
      media::DemuxerStream::Status status,
      media::DemuxerStream::DecoderBufferVector buffers);

  // Schedules `out_of_buffer_timer_` timer, which transitions the renderer to
  // the `BUFFERING_HAVE_NOTHING` state.
  void ScheduleOutOfBufferTimer(bool is_time_moving,
                                base::TimeTicks end_of_buffer_time);

  // Sends the specified packet to |stream_sink_|.
  void SendInputPacket(media::StreamProcessorHelper::IoPacket packet);

  // Result handler for StreamSink::SendPacket().
  void OnStreamSendDone(
      std::unique_ptr<media::StreamProcessorHelper::IoPacket> packet);

  // Updates buffer state and notifies the |client_| if necessary.
  void SetBufferState(media::BufferingState buffer_state);

  // Discards all pending packets.
  void FlushInternal();

  // End-of-stream event handler for |audio_consumer_|.
  void OnEndOfStream();

  // Returns true if media clock is ticking and the rate is above 0.0.
  bool IsTimeMoving() EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // Calculates media position based on the TimelineFunction returned from
  // AudioConsumer. Must be called only when IsTimeMoving() is true.
  base::TimeDelta CurrentMediaTimeLocked()
      EXCLUSIVE_LOCKS_REQUIRED(timeline_lock_);

  // SysmemBufferStream::Sink implementation.
  void OnSysmemBufferStreamBufferCollectionToken(
      fuchsia::sysmem2::BufferCollectionTokenPtr token) override;
  void OnSysmemBufferStreamOutputPacket(
      media::StreamProcessorHelper::IoPacket packet) override;
  void OnSysmemBufferStreamEndOfStream() override;
  void OnSysmemBufferStreamError() override;
  void OnSysmemBufferStreamNoKey() override;

  // Handle for |audio_consumer_|. It's stored here until Initialize() is
  // called.
  fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle_;

  fuchsia::media::AudioConsumerPtr audio_consumer_;
  fuchsia::media::StreamSinkPtr stream_sink_;
  fuchsia::media::audio::VolumeControlPtr volume_control_;

  float volume_ = 1.0;

  double playback_rate_ = 1.0;

  media::CdmContext* cdm_context_ = nullptr;
  media::DemuxerStream* demuxer_stream_ = nullptr;
  bool is_demuxer_read_pending_ = false;
  bool drop_next_demuxer_read_result_ = false;

  media::RendererClient* client_ = nullptr;

  // Initialize() completion callback.
  media::PipelineStatusCallback init_cb_;

  // Indicates that StartPlaying() has been called. Note that playback doesn't
  // start until TimeSource::StartTicking() is called.
  bool renderer_started_ = false;

  media::BufferingState buffer_state_ = media::BUFFERING_HAVE_NOTHING;

  base::TimeDelta last_packet_timestamp_ = base::TimeDelta::Min();
  base::DeadlineTimer read_timer_;
  base::DeadlineTimer out_of_buffer_timer_;

  media::SysmemAllocatorClient sysmem_allocator_{"WebEngineAudioRenderer"};
  std::unique_ptr<media::SysmemCollectionClient> input_buffer_collection_;

  std::unique_ptr<media::SysmemBufferStream> sysmem_buffer_stream_;

  // VmoBuffers for the buffers |input_buffer_collection_|.
  std::vector<media::VmoBuffer> input_buffers_;

  // Packets and EndOfStream produced before the |stream_sink_| is connected.
  // They are sent as soon as input buffers are acquired and |stream_sink_| is
  // connected in OnBuffersAcquired().
  std::list<media::StreamProcessorHelper::IoPacket> delayed_packets_;
  bool has_delayed_end_of_stream_ = false;

  // Lead time range requested by the |audio_consumer_|. Initialized to  the
  // [100ms, 500ms] until the initial AudioConsumerStatus is received.
  base::TimeDelta min_lead_time_ = base::Milliseconds(100);
  base::TimeDelta max_lead_time_ = base::Milliseconds(500);

  // Set to true after we've received end-of-stream from the |demuxer_stream_|.
  // The renderer may be restarted after Flush().
  bool is_at_end_of_stream_ = false;

  // TimeSource interface is not single-threaded. The lock is used to guard
  // fields that are accessed in the TimeSource implementation. Note that these
  // fields are updated only on the main thread (which corresponds to the
  // |thread_checker_|), so on that thread it's safe to assume that the values
  // don't change even when not holding the lock.
  base::Lock timeline_lock_;

  // Should be changed by calling SetPlaybackState() on the main thread.
  PlaybackState state_ GUARDED_BY(timeline_lock_) = PlaybackState::kStopped;

  // Values from TimelineFunction returned by AudioConsumer.
  base::TimeTicks reference_time_ GUARDED_BY(timeline_lock_);
  base::TimeDelta media_pos_ GUARDED_BY(timeline_lock_);

  // Initialize `media_delta_` to 0 because media clock isn't ticking until
  // playback has started.
  int32_t media_delta_ GUARDED_BY(timeline_lock_) = 0;
  int32_t reference_delta_ GUARDED_BY(timeline_lock_) = 1;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<WebEngineAudioRenderer> weak_factory_{this};
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_RENDERER_H_
