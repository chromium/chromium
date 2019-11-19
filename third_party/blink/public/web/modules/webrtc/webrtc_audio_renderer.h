// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_WEBRTC_WEBRTC_AUDIO_RENDERER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_WEBRTC_WEBRTC_AUDIO_RENDERER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/channel_layout.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_renderer.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_source.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_stream.h"

namespace webrtc {
class AudioSourceInterface;
}  // namespace webrtc

namespace blink {

class WebLocalFrame;
class WebRtcAudioRendererSource;

// This renderer handles calls from the pipeline and WebRtc ADM. It is used
// for connecting WebRtc MediaStream with the audio pipeline.
class BLINK_MODULES_EXPORT WebRtcAudioRenderer
    : public media::AudioRendererSink::RenderCallback,
      public blink::WebMediaStreamAudioRenderer {
 public:
  // This is a little utility class that holds the configured state of an audio
  // stream.
  // It is used by both WebRtcAudioRenderer and SharedAudioRenderer (see cc
  // file) so a part of why it exists is to avoid code duplication and track
  // the state in the same way in WebRtcAudioRenderer and SharedAudioRenderer.
  class PlayingState {
   public:
    PlayingState() : playing_(false), volume_(1.0f) {}

    ~PlayingState() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

    bool playing() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return playing_;
    }

    void set_playing(bool playing) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      playing_ = playing;
    }

    float volume() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return volume_;
    }

    void set_volume(float volume) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      volume_ = volume;
    }

   private:
    bool playing_;
    float volume_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  WebRtcAudioRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread,
      const blink::WebMediaStream& media_stream,
      WebLocalFrame* web_frame,
      const base::UnguessableToken& session_id,
      const std::string& device_id);

  // Initialize function called by clients like WebRtcAudioDeviceImpl.
  // Stop() has to be called before |source| is deleted.
  bool Initialize(WebRtcAudioRendererSource* source);

  // When sharing a single instance of WebRtcAudioRenderer between multiple
  // users (e.g. WebMediaPlayerMS), call this method to create a proxy object
  // that maintains the Play and Stop states per caller.
  // The wrapper ensures that Play() won't be called when the caller's state
  // is "playing", Pause() won't be called when the state already is "paused"
  // etc and similarly maintains the same state for Stop().
  // When Stop() is called or when the proxy goes out of scope, the proxy
  // will ensure that Pause() is called followed by a call to Stop(), which
  // is the usage pattern that WebRtcAudioRenderer requires.
  scoped_refptr<blink::WebMediaStreamAudioRenderer>
  CreateSharedAudioRendererProxy(const blink::WebMediaStream& media_stream);

  // Used to DCHECK on the expected state.
  bool IsStarted() const;

  // Accessors to the sink audio parameters.
  int channels() const { return sink_params_.channels(); }
  int sample_rate() const { return sink_params_.sample_rate(); }
  int frames_per_buffer() const { return sink_params_.frames_per_buffer(); }

  // Returns true if called on rendering thread, otherwise false.
  bool CurrentThreadIsRenderingThread();

 private:
  // blink::WebMediaStreamAudioRenderer implementation.  This is private since
  // we want callers to use proxy objects.
  // TODO(tommi): Make the blink::WebMediaStreamAudioRenderer implementation a
  // pimpl?
  void Start() override;
  void Play() override;
  void Pause() override;
  void Stop() override;
  void SetVolume(float volume) override;
  base::TimeDelta GetCurrentRenderTime() override;
  bool IsLocalRenderer() override;
  void SwitchOutputDevice(const std::string& device_id,
                          media::OutputDeviceStatusCB callback) override;

  // Called when an audio renderer, either the main or a proxy, starts playing.
  // Here we maintain a reference count of how many renderers are currently
  // playing so that the shared play state of all the streams can be reflected
  // correctly.
  void EnterPlayState();

  // Called when an audio renderer, either the main or a proxy, is paused.
  // See EnterPlayState for more details.
  void EnterPauseState();

 protected:
  ~WebRtcAudioRenderer() override;

 private:
  enum State {
    UNINITIALIZED,
    PLAYING,
    PAUSED,
  };

  // Holds raw pointers to PlaingState objects.  Ownership is managed outside
  // of this type.
  typedef std::vector<PlayingState*> PlayingStates;
  // Maps an audio source to a list of playing states that collectively hold
  // volume information for that source.
  typedef std::map<webrtc::AudioSourceInterface*, PlayingStates>
      SourcePlayingStates;

  // Used to DCHECK that we are called on the correct thread.
  THREAD_CHECKER(thread_checker_);

  // Flag to keep track the state of the renderer.
  State state_;

  // media::AudioRendererSink::RenderCallback implementation.
  // These two methods are called on the AudioOutputDevice worker thread.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* audio_bus) override;
  void OnRenderError() override;

  // Called by AudioPullFifo when more data is necessary.
  // This method is called on the AudioOutputDevice worker thread.
  void SourceCallback(int fifo_frame_delay, media::AudioBus* audio_bus);

  // Goes through all renderers for the |source| and applies the proper
  // volume scaling for the source based on the volume(s) of the renderer(s).
  void UpdateSourceVolume(webrtc::AudioSourceInterface* source);

  // Tracks a playing state.  The state must be playing when this method
  // is called.
  // Returns true if the state was added, false if it was already being tracked.
  bool AddPlayingState(webrtc::AudioSourceInterface* source,
                       PlayingState* state);
  // Removes a playing state for an audio source.
  // Returns true if the state was removed from the internal map, false if
  // it had already been removed or if the source isn't being rendered.
  bool RemovePlayingState(webrtc::AudioSourceInterface* source,
                          PlayingState* state);

  // Called whenever the Play/Pause state changes of any of the renderers
  // or if the volume of any of them is changed.
  // Here we update the shared Play state and apply volume scaling to all audio
  // sources associated with the |media_stream| based on the collective volume
  // of playing renderers.
  void OnPlayStateChanged(const blink::WebMediaStream& media_stream,
                          PlayingState* state);

  // Called when |state| is about to be destructed.
  void OnPlayStateRemoved(PlayingState* state);

  // Updates |sink_params_| and |audio_fifo_| based on |sink_|, and initializes
  // |sink_|.
  void PrepareSink();

  // The WebLocalFrame in which the audio is rendered into |sink_|.
  //
  // TODO(crbug.com/704136): Replace |source_internal_frame_| with regular
  // fields once this header file moves to blink/renderer.
  class InternalFrame;
  std::unique_ptr<InternalFrame> source_internal_frame_;

  const base::UnguessableToken session_id_;

  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioRendererSink> sink_;

  // The media stream that holds the audio tracks that this renderer renders.
  const blink::WebMediaStream media_stream_;

  // Audio data source from the browser process.
  //
  // TODO(crbug.com/704136): Make it a Member.
  WebRtcAudioRendererSource* source_;

  // Protects access to |state_|, |source_|, |audio_fifo_|,
  // |audio_delay_milliseconds_|, |fifo_delay_milliseconds_|, |current_time_|,
  // |sink_params_|, |render_callback_count_| and |max_render_time_|.
  mutable base::Lock lock_;

  // Ref count for the MediaPlayers which are playing audio.
  int play_ref_count_;

  // Ref count for the MediaPlayers which have called Start() but not Stop().
  int start_ref_count_;

  // Used to buffer data between the client and the output device in cases where
  // the client buffer size is not the same as the output device buffer size.
  std::unique_ptr<media::AudioPullFifo> audio_fifo_;

  // Contains the accumulated delay estimate which is provided to the WebRTC
  // AEC.
  base::TimeDelta audio_delay_;

  base::TimeDelta current_time_;

  // Saved volume and playing state of the root renderer.
  PlayingState playing_state_;

  // Audio params used by the sink of the renderer.
  media::AudioParameters sink_params_;

  // The preferred device id of the output device or empty for the default
  // output device. Can change as a result of a SetSinkId() call.
  std::string output_device_id_;

  // Maps audio sources to a list of active audio renderers.
  // Pointers to PlayingState objects are only kept in this map while the
  // associated renderer is actually playing the stream.  Ownership of the
  // state objects lies with the renderers and they must leave the playing state
  // before being destructed (PlayingState object goes out of scope).
  SourcePlayingStates source_playing_states_;

  // Stores the maximum time spent waiting for render data from the source. Used
  // for logging UMA data. Logged and reset when Stop() is called.
  base::TimeDelta max_render_time_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioRenderer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_WEBRTC_WEBRTC_AUDIO_RENDERER_H_
