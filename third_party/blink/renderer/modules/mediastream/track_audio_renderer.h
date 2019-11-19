// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRACK_AUDIO_RENDERER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRACK_AUDIO_RENDERER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_renderer.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace media {
class AudioBus;
class AudioShifter;
class AudioParameters;
}  // namespace media

namespace blink {

class WebLocalFrame;
class MediaStreamInternalFrameWrapper;

// TrackAudioRenderer is a WebMediaStreamAudioRenderer for plumbing audio
// data generated from either local or remote (but not
// PeerConnection/WebRTC-sourced) MediaStreamAudioTracks to an audio output
// device, reconciling differences in the rates of production and consumption of
// the audio data.  Note that remote PeerConnection-sourced tracks are NOT
// rendered by this implementation (see WebMediaStreamRendererFactoryImpl).
//
// This class uses AudioDeviceFactory to create media::AudioRendererSink and
// owns/manages their lifecycles.  Output devices are automatically re-created
// in response to audio format changes, or use of the SwitchOutputDevice() API
// by client code.
//
// Audio data is feed-in from the source via calls to OnData().  The
// internally-owned media::AudioOutputDevice calls Render() to pull-out that
// audio data.  However, because of clock differences and other environmental
// factors, the audio will inevitably feed-in at a rate different from the rate
// it is being rendered-out.  media::AudioShifter is used to buffer, stretch
// and skip audio to maintain time synchronization between the producer and
// consumer.
class TrackAudioRenderer : public WebMediaStreamAudioRenderer,
                           public WebMediaStreamAudioSink,
                           public media::AudioRendererSink::RenderCallback {
 public:
  // Creates a renderer for the given |audio_track|.  |playout_render_frame_id|
  // refers to the RenderFrame that owns this instance (e.g., it contains the
  // DOM widget representing the player).  |session_id| and |device_id| are
  // optional, and are used to direct audio output to a pre-selected device;
  // otherwise, audio is output to the default device for the system.
  //
  // Called on the main thread.
  TrackAudioRenderer(const WebMediaStreamTrack& audio_track,
                     WebLocalFrame* playout_web_frame,
                     const base::UnguessableToken& session_id,
                     const String& device_id);

  // WebMediaStreamAudioRenderer implementation.
  // Called on the main thread.
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  void SetVolume(float volume) override;
  base::TimeDelta GetCurrentRenderTime() override;
  bool IsLocalRenderer() override;
  void SwitchOutputDevice(const std::string& device_id,
                          media::OutputDeviceStatusCB callback) override;

 protected:
  ~TrackAudioRenderer() override;

 private:
  // WebMediaStreamAudioSink implementation.

  // Called on the AudioInputDevice worker thread.
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks reference_time) override;

  // Called on the AudioInputDevice worker thread.
  void OnSetFormat(const media::AudioParameters& params) override;

  // media::AudioRendererSink::RenderCallback implementation.
  // Render() is called on the AudioOutputDevice thread and OnRenderError()
  // on the IO thread.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* audio_bus) override;
  void OnRenderError() override;

  // Initializes and starts the |sink_| if
  //  we have received valid |source_params_| &&
  //  |playing_| has been set to true.
  void MaybeStartSink();

  // Sets new |source_params_| and then re-initializes and restarts |sink_|.
  void ReconfigureSink(const media::AudioParameters& params);

  // Creates a new AudioShifter, destroying the old one (if any).  This is
  // called any time playback is started/stopped, or the sink changes.
  void CreateAudioShifter();

  // Called when either the source or sink has changed somehow, or audio has
  // been paused.  Drops the AudioShifter and updates
  // |prior_elapsed_render_time_|.  May be called from either the main thread or
  // the audio thread.  Assumption: |thread_lock_| is already acquired.
  void HaltAudioFlowWhileLockHeld();

  // The audio track which provides access to the source data to render.
  //
  // This class is calling WebMediaStreamAudioSink::AddToAudioTrack() and
  // WebMediaStreamAudioSink::RemoveFromAudioTrack() to connect and
  // disconnect with the audio track.
  WebMediaStreamTrack audio_track_;

  // The WebLocalFrame in which the audio is rendered into |sink_|.
  std::unique_ptr<MediaStreamInternalFrameWrapper> internal_playout_frame_;
  const base::UnguessableToken session_id_;

  // MessageLoop associated with the single thread that performs all control
  // tasks.  Set to the MessageLoop that invoked the ctor.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioRendererSink> sink_;

  // This does all the synchronization/resampling/smoothing.
  std::unique_ptr<media::AudioShifter> audio_shifter_;

  // These track the time duration of all the audio rendered so far by this
  // instance.  |prior_elapsed_render_time_| tracks the time duration of all
  // audio rendered before the last format change.  |num_samples_rendered_|
  // tracks the number of audio samples rendered since the last format change.
  base::TimeDelta prior_elapsed_render_time_;
  int64_t num_samples_rendered_;

  // The audio parameters of the track's source.
  // Must only be touched on the main thread.
  media::AudioParameters source_params_;

  // Set when playing, cleared when paused.
  bool playing_;

  // Protects |audio_shifter_|, |prior_elapsed_render_time_|, and
  // |num_samples_rendered_|.
  mutable base::Lock thread_lock_;

  // The preferred device id of the output device or empty for the default
  // output device.
  String output_device_id_;

  // Cache value for the volume.  Whenever |sink_| is re-created, its volume
  // should be set to this.
  float volume_;

  // Flag to indicate whether |sink_| has been started yet.
  bool sink_started_;

  DISALLOW_COPY_AND_ASSIGN(TrackAudioRenderer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRACK_AUDIO_RENDERER_H_
