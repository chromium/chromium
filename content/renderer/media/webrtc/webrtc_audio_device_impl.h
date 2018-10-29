// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_DEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_DEVICE_IMPL_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_audio_device_not_impl.h"
#include "ipc/ipc_platform_file.h"

// A WebRtcAudioDeviceImpl instance implements the abstract interface
// webrtc::AudioDeviceModule which makes it possible for a user (e.g. webrtc::
// VoiceEngine) to register this class as an external AudioDeviceModule (ADM).
//
// Implementation notes:
//
//  - This class must be created and destroyed on the main render thread and
//    most methods are called on the same thread. However, some methods are
//    also called on a Libjingle worker thread. RenderData is called on the
//    AudioOutputDevice thread and CaptureData on the AudioInputDevice thread.
//    To summarize: this class lives on four different threads, so it is
//    important to be careful with the order in which locks are acquired in
//    order to avoid potential deadlocks.
//
namespace media {
class AudioBus;
}

namespace content {

class ProcessedLocalAudioSource;
class WebRtcAudioRenderer;

// TODO(xians): Move the following two interfaces to webrtc so that
// libjingle can own references to the renderer and capturer.
class WebRtcAudioRendererSource {
 public:
  // Callback to get the rendered data.
  // |audio_bus| must have buffer size |sample_rate/100| and 1-2 channels.
  virtual void RenderData(media::AudioBus* audio_bus,
                          int sample_rate,
                          int audio_delay_milliseconds,
                          base::TimeDelta* current_time) = 0;

  // Callback to notify the client that the renderer is going away.
  virtual void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) = 0;

  // Callback to notify the client that the audio renderer thread stopped.
  // This function must be called only when that thread is actually stopped.
  // Otherwise a race may occur.
  virtual void AudioRendererThreadStopped() = 0;

  // Callback to notify the client of the output device the renderer is using.
  virtual void SetOutputDeviceForAec(const std::string& output_device_id) = 0;

  // Returns the UnguessableToken used to connect this stream to an input stream
  // for echo cancellation.
  virtual base::UnguessableToken GetAudioProcessingId() const = 0;

 protected:
  virtual ~WebRtcAudioRendererSource() {}
};

// TODO(xians): Merge this interface with WebRtcAudioRendererSource.
// The reason why we could not do it today is that WebRtcAudioRendererSource
// gets the data by pulling, while the data is pushed into
// WebRtcPlayoutDataSource::Sink.
class WebRtcPlayoutDataSource {
 public:
  class Sink {
   public:
    // Callback to get the playout data.
    // Called on the audio render thread.
    // |audio_bus| must have buffer size |sample_rate/100| and 1-2 channels.
    virtual void OnPlayoutData(media::AudioBus* audio_bus,
                               int sample_rate,
                               int audio_delay_milliseconds) = 0;

    // Callback to notify the sink that the source has changed.
    // Called on the main render thread.
    virtual void OnPlayoutDataSourceChanged() = 0;

    // Called to notify that the audio render thread has changed, and
    // OnPlayoutData() will from now on be called on the new thread.
    // Called on the new audio render thread.
    virtual void OnRenderThreadChanged() = 0;

   protected:
    virtual ~Sink() {}
  };

  // Adds/Removes the sink of WebRtcAudioRendererSource to the ADM.
  // These methods are used by the MediaStreamAudioProcesssor to get the
  // rendered data for AEC.
  virtual void AddPlayoutSink(Sink* sink) = 0;
  virtual void RemovePlayoutSink(Sink* sink) = 0;

 protected:
  virtual ~WebRtcPlayoutDataSource() {}
};

// Note that this class inherits from webrtc::AudioDeviceModule but due to
// the high number of non-implemented methods, we move the cruft over to the
// WebRtcAudioDeviceNotImpl.
class CONTENT_EXPORT WebRtcAudioDeviceImpl : public WebRtcAudioDeviceNotImpl,
                                             public WebRtcAudioRendererSource,
                                             public WebRtcPlayoutDataSource {
 public:
  // The maximum volume value WebRtc uses.
  static const int kMaxVolumeLevel = 255;

  // Instances of this object are created on the main render thread.
  WebRtcAudioDeviceImpl();

 protected:
  // Make destructor protected, we should only be deleted by Release().
  ~WebRtcAudioDeviceImpl() override;

 private:
  // webrtc::AudioDeviceModule implementation.
  // All implemented methods are called on the main render thread unless
  // anything else is stated.

  int32_t RegisterAudioCallback(
      webrtc::AudioTransport* audio_callback) override;

  int32_t Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int32_t PlayoutIsAvailable(bool* available) override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool* available) override;
  bool RecordingIsInitialized() const override;

  // All Start/Stop methods are called on a libJingle worker thread.
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  // Called on the AudioInputDevice worker thread.
  int32_t SetMicrophoneVolume(uint32_t volume) override;

  // TODO(henrika): sort out calling thread once we start using this API.
  int32_t MicrophoneVolume(uint32_t* volume) const override;

  int32_t MaxMicrophoneVolume(uint32_t* max_volume) const override;
  int32_t MinMicrophoneVolume(uint32_t* min_volume) const override;
  int32_t PlayoutDelay(uint16_t* delay_ms) const override;

 public:
  // Sets the |renderer_|, returns false if |renderer_| already exists.
  // Called on the main renderer thread.
  bool SetAudioRenderer(WebRtcAudioRenderer* renderer);

  // Adds/Removes the |capturer| to the ADM.  Does NOT take ownership.
  // Capturers must remain valid until RemoveAudioCapturer() is called.
  // TODO(xians): Remove these two methods once the ADM does not need to pass
  // hardware information up to WebRtc.
  void AddAudioCapturer(ProcessedLocalAudioSource* capturer);
  void RemoveAudioCapturer(ProcessedLocalAudioSource* capturer);

  // Returns the session id of the capture device if it has a paired output
  // device, otherwise 0. The session id is passed on to a webrtc audio renderer
  // (either local or remote), so that audio will be rendered to a matching
  // output device. Note that if there are more than one open capture device the
  // function will not be able to pick an appropriate device and return 0.
  int GetAuthorizedDeviceSessionIdForAudioRenderer();

  const scoped_refptr<WebRtcAudioRenderer>& renderer() const {
    return renderer_;
  }

  // WebRtcAudioRendererSource implementation.

  // Called on the AudioOutputDevice worker thread.
  void RenderData(media::AudioBus* audio_bus,
                  int sample_rate,
                  int audio_delay_milliseconds,
                  base::TimeDelta* current_time) override;

  // Called on the main render thread.
  void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) override;
  void AudioRendererThreadStopped() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;
  base::UnguessableToken GetAudioProcessingId() const override;

  // WebRtcPlayoutDataSource implementation.
  void AddPlayoutSink(WebRtcPlayoutDataSource::Sink* sink) override;
  void RemovePlayoutSink(WebRtcPlayoutDataSource::Sink* sink) override;

 private:
  using CapturerList = std::list<ProcessedLocalAudioSource*>;
  using PlayoutDataSinkList = std::list<WebRtcPlayoutDataSource::Sink*>;

  class RenderBuffer;

  // Used to check methods that run on the main render thread.
  THREAD_CHECKER(main_thread_checker_);
  // Used to check methods that are called on libjingle's signaling thread.
  THREAD_CHECKER(signaling_thread_checker_);
  THREAD_CHECKER(worker_thread_checker_);
  THREAD_CHECKER(audio_renderer_thread_checker_);

  const base::UnguessableToken audio_processing_id_;

  // List of captures which provides access to the native audio input layer
  // in the browser process.  The last capturer in this list is considered the
  // "default capturer" by the methods implementing the
  // webrtc::AudioDeviceModule interface.
  CapturerList capturers_;

  // Provides access to the audio renderer in the browser process.
  scoped_refptr<WebRtcAudioRenderer> renderer_;

  // A list of raw pointer of WebRtcPlayoutDataSource::Sink objects which want
  // to get the playout data, the sink need to call RemovePlayoutSink()
  // before it goes away.
  PlayoutDataSinkList playout_sinks_;

  // Weak reference to the audio callback.
  // The webrtc client defines |audio_transport_callback_| by calling
  // RegisterAudioCallback().
  webrtc::AudioTransport* audio_transport_callback_;

  // Cached value of the current audio delay on the output/renderer side.
  int output_delay_ms_;

  // Protects |recording_|, |output_delay_ms_|, |input_delay_ms_|, |renderer_|
  // |recording_|, |microphone_volume_| and |playout_sinks_|.
  mutable base::Lock lock_;

  bool initialized_;
  bool playing_;
  bool recording_;

  // Buffer used for temporary storage during render callback.
  // It is only accessed by the audio render thread.
  std::vector<int16_t> render_buffer_;

  // The output device used for echo cancellation
  std::string output_device_id_for_aec_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_DEVICE_IMPL_H_
