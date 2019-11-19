// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Audio rendering unit utilizing an AudioRendererSink to output data.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. Render thread
//    Where the object is created.
// 2. Media thread (provided via constructor)
//    All AudioDecoder methods are called on this thread.
// 3. Audio thread created by the AudioRendererSink.
//    Render() is called here where audio data is decoded into raw PCM data.
//
// AudioRendererImpl talks to an AudioRendererAlgorithm that takes care of
// queueing audio data and stretching/shrinking audio data when playback rate !=
// 1.0 or 0.0.

#ifndef MEDIA_RENDERERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_RENDERERS_AUDIO_RENDERER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/decryptor.h"
#include "media/base/media_log.h"
#include "media/base/time_source.h"
#include "media/filters/audio_renderer_algorithm.h"
#include "media/filters/decoder_stream.h"
#include "media/renderers/default_renderer_factory.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}

namespace media {

class AudioBufferConverter;
class AudioBus;
class AudioClock;

class MEDIA_EXPORT AudioRendererImpl
    : public AudioRenderer,
      public TimeSource,
      public base::PowerObserver,
      public AudioRendererSink::RenderCallback {
 public:
  using PlayDelayCBForTesting = base::RepeatingCallback<void(base::TimeDelta)>;

  // |task_runner| is the thread on which AudioRendererImpl will execute.
  //
  // |sink| is used as the destination for the rendered audio.
  //
  // |decoders| contains the AudioDecoders to use when initializing.
  AudioRendererImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      AudioRendererSink* sink,
      const CreateAudioDecodersCB& create_audio_decoders_cb,
      MediaLog* media_log);
  ~AudioRendererImpl() override;

  // TimeSource implementation.
  void StartTicking() override;
  void StopTicking() override;
  void SetPlaybackRate(double rate) override;
  void SetMediaTime(base::TimeDelta time) override;
  base::TimeDelta CurrentMediaTime() override;
  bool GetWallClockTimes(
      const std::vector<base::TimeDelta>& media_timestamps,
      std::vector<base::TimeTicks>* wall_clock_times) override;

  // AudioRenderer implementation.
  void Initialize(DemuxerStream* stream,
                  CdmContext* cdm_context,
                  RendererClient* client,
                  const PipelineStatusCB& init_cb) override;
  TimeSource* GetTimeSource() override;
  void Flush(const base::Closure& callback) override;
  void StartPlaying() override;
  void SetVolume(float volume) override;

  // base::PowerObserver implementation.
  void OnSuspend() override;
  void OnResume() override;

  void SetPlayDelayCBForTesting(PlayDelayCBForTesting cb);

 private:
  friend class AudioRendererImplTest;

  // Important detail: being in kPlaying doesn't imply that audio is being
  // rendered. Rather, it means that the renderer is ready to go. The actual
  // rendering of audio is controlled via Start/StopRendering().
  // Audio renderer can be reinitialized completely by calling Initialize again
  // when it is in a kFlushed state.
  //
  //   kUninitialized
  //  +----> | Initialize()
  //  |      |
  //  |      V
  //  | kInitializing
  //  |      | Decoders initialized
  //  |      |
  //  |      V            Decoders reset
  //  +-  kFlushed <------------------ kFlushing
  //         | StartPlaying()             ^
  //         |                            |
  //         |                            | Flush()
  //         `---------> kPlaying --------'
  enum State {
    kUninitialized,
    kInitializing,
    kFlushing,
    kFlushed,
    kPlaying
  };

  // Called after hardware device information is available.
  void OnDeviceInfoReceived(DemuxerStream* stream,
                            CdmContext* cdm_context,
                            OutputDeviceInfo output_device_info);

  // Callback from the audio decoder delivering decoded audio samples.
  void DecodedAudioReady(AudioDecoderStream::Status status,
                         scoped_refptr<AudioBuffer> buffer);

  // Handles buffers that come out of decoder (MSE: after passing through
  // |buffer_converter_|).
  // Returns true if more buffers are needed.
  bool HandleDecodedBuffer_Locked(scoped_refptr<AudioBuffer> buffer);

  // Helper functions for DecodeStatus values passed to
  // DecodedAudioReady().
  void HandleAbortedReadOrDecodeError(PipelineStatus status);

  void StartRendering_Locked();
  void StopRendering_Locked();

  // AudioRendererSink::RenderCallback implementation.
  //
  // NOTE: These are called on the audio callback thread!
  //
  // Render() fills the given buffer with audio data by delegating to its
  // |algorithm_|. Render() also takes care of updating the clock.
  // Returns the number of frames copied into |audio_bus|, which may be less
  // than or equal to the initial number of frames in |audio_bus|
  //
  // If this method returns fewer frames than the initial number of frames in
  // |audio_bus|, it could be a sign that the pipeline is stalled or unable to
  // stream the data fast enough.  In such scenarios, the callee should zero out
  // unused portions of their buffer to play back silence.
  //
  // Render() updates the pipeline's playback timestamp. If Render() is
  // not called at the same rate as audio samples are played, then the reported
  // timestamp in the pipeline will be ahead of the actual audio playback. In
  // this case |delay| should be used to indicate when in the future
  // should the filled buffer be played.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             AudioBus* dest) override;
  void OnRenderError() override;

  // Helper methods that schedule an asynchronous read from the decoder as long
  // as there isn't a pending read.
  //
  // Must be called on |task_runner_|.
  void AttemptRead();
  void AttemptRead_Locked();
  bool CanRead_Locked();
  void ChangeState_Locked(State new_state);

  // Returns true if the data in the buffer is all before |start_timestamp_|.
  // This can only return true while in the kPlaying state.
  bool IsBeforeStartTime(const AudioBuffer& buffer);

  // Called upon AudioDecoderStream initialization, or failure thereof
  // (indicated by the value of |success|).
  void OnAudioDecoderStreamInitialized(bool succes);

  void FinishInitialization(PipelineStatus status);
  void FinishFlush();

  // Callback functions to be called on |client_|.
  void OnPlaybackError(PipelineStatus error);
  void OnPlaybackEnded();
  void OnStatisticsUpdate(const PipelineStatistics& stats);
  void OnBufferingStateChange(BufferingState state);
  void OnWaiting(WaitingReason reason);

  // Generally called by the AudioDecoderStream when a config change occurs. May
  // also be called internally with an empty config to reset config-based state.
  // Will notify RenderClient when called with a valid config.
  void OnConfigChange(const AudioDecoderConfig& config);

  // Used to initiate the flush operation once all pending reads have
  // completed.
  void DoFlush_Locked();

  // Called when the |decoder_|.Reset() has completed.
  void ResetDecoderDone();

  // Updates |buffering_state_| and fires |buffering_state_cb_|.
  void SetBufferingState_Locked(BufferingState buffering_state);

  // Configure's the channel mask for |algorithm_|. Must be called if the layout
  // changes. Expect the layout in |last_decoded_channel_layout_|.
  void ConfigureChannelMask();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<AudioBufferConverter> buffer_converter_;

  // Whether or not we expect to handle config changes.
  bool expecting_config_changes_;

  // Stores the last decoder config that was passed to
  // RendererClient::OnAudioConfigChange. Used to prevent signaling config
  // to the upper layers when when the new config is the same.
  AudioDecoderConfig current_decoder_config_;

  // The sink (destination) for rendered audio. |sink_| must only be accessed
  // on |task_runner_|. |sink_| must never be called under |lock_| or else we
  // may deadlock between |task_runner_| and the audio callback thread.
  scoped_refptr<media::AudioRendererSink> sink_;

  std::unique_ptr<AudioDecoderStream> audio_decoder_stream_;

  MediaLog* media_log_;

  // Cached copy of audio params that the renderer is initialized with.
  AudioParameters audio_parameters_;

  // Passed in during Initialize().
  DemuxerStream* demuxer_stream_;

  RendererClient* client_;

  // Callback provided during Initialize().
  PipelineStatusCB init_cb_;

  // Callback provided to Flush().
  base::Closure flush_cb_;

  // Overridable tick clock for testing.
  const base::TickClock* tick_clock_;

  // Memory usage of |algorithm_| recorded during the last
  // HandleDecodedBuffer_Locked() call.
  int64_t last_audio_memory_usage_;

  // Sample rate of the last decoded audio buffer. Allows for detection of
  // sample rate changes due to implicit AAC configuration change.
  int last_decoded_sample_rate_;

  // Similar to |last_decoded_sample_rate_|, used to configure the channel mask
  // given to the |algorithm_| for efficient playback rate changes.
  ChannelLayout last_decoded_channel_layout_;

  // Whether the stream is possibly encrypted.
  bool is_encrypted_;

  // Similar to |last_decoded_channel_layout_|, used to configure the channel
  // mask given to the |algorithm_| for efficient playback rate changes.
  int last_decoded_channels_;

  // After Initialize() has completed, all variables below must be accessed
  // under |lock_|. ------------------------------------------------------------
  base::Lock lock_;

  // Algorithm for scaling audio.
  double playback_rate_;
  std::unique_ptr<AudioRendererAlgorithm> algorithm_;

  // Simple state tracking variable.
  State state_;

  // TODO(servolk): Consider using DecoderFactory here instead of the
  // CreateAudioDecodersCB.
  CreateAudioDecodersCB create_audio_decoders_cb_;

  BufferingState buffering_state_;

  // Keep track of whether or not the sink is playing and whether we should be
  // rendering.
  bool rendering_;
  bool sink_playing_;

  // Keep track of our outstanding read to |decoder_|.
  bool pending_read_;

  // Keeps track of whether we received and rendered the end of stream buffer.
  bool received_end_of_stream_;
  bool rendered_end_of_stream_;

  std::unique_ptr<AudioClock> audio_clock_;

  // The media timestamp to begin playback at after seeking. Set via
  // SetMediaTime().
  base::TimeDelta start_timestamp_;

  // The media timestamp to signal end of audio playback. Determined during
  // Render() when writing the final frames of decoded audio data.
  base::TimeDelta ended_timestamp_;

  // Set every Render() and used to provide an interpolated time value to
  // CurrentMediaTimeForSyncingVideo().
  base::TimeTicks last_render_time_;

  // Set to the value of |last_render_time_| when StopRendering_Locked() is
  // called for any reason.  Cleared by the next successful Render() call after
  // being used to adjust for lost time between the last call.
  base::TimeTicks stop_rendering_time_;

  // Set upon receipt of the first decoded buffer after a StartPlayingFrom().
  // Used to determine how long to delay playback.
  base::TimeDelta first_packet_timestamp_;

  // Set by OnSuspend() and OnResume() to indicate when the system is about to
  // suspend/is suspended and when it resumes.
  bool is_suspending_;

  // Whether to pass compressed audio bitstream to audio sink directly.
  bool is_passthrough_;

  // Set and used only in tests to report positive play_delay values in
  // Render().
  PlayDelayCBForTesting play_delay_cb_for_testing_;

  // End variables which must be accessed under |lock_|. ----------------------

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AudioRendererImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_AUDIO_RENDERER_IMPL_H_
