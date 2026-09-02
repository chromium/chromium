// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace media {
class SilentSinkSuspender;
class SpeechRecognitionClient;
}

namespace content {

// The actual implementation of `blink::WebAudioDevice` that handles the
// connection between Blink Web Audio API and the media renderer.
class CONTENT_EXPORT RendererWebAudioDeviceImpl
    : public blink::WebAudioDevice,
      public media::AudioRendererSink::RenderCallback {
 public:
  RendererWebAudioDeviceImpl(const RendererWebAudioDeviceImpl&) = delete;
  RendererWebAudioDeviceImpl& operator=(const RendererWebAudioDeviceImpl&) =
      delete;

  ~RendererWebAudioDeviceImpl() override;

  static std::unique_ptr<RendererWebAudioDeviceImpl> Create(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      int number_of_output_channels,
      const blink::WebAudioLatencyHint& latency_hint,
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback* webaudio_callback);
  static int GetOutputBufferSize(const blink::WebAudioLatencyHint& latency_hint,
                                 int resolved_context_sample_rate,
                                 const media::AudioParameters& hardware_params);

  // blink::WebAudioDevice implementation.
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Resume() override;
  double SampleRate() override;
  int FramesPerBuffer() override;
  int MaxChannelCount() override;

  // Sets the detect silence flag for `media::SilentSinkSuspender`. Invoked by
  // Blink Web Audio.
  void SetDetectSilence(bool enable_silence_detection) override;

  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;

  // This callback method may be called in two different scenarios:
  // 1) When the constructor's audio device activation fails. (main thread)
  // 2) When the audio infra reports a device/render error. (audio thread)
  void OnRenderError() override;

  // Notifies the client (e.g. Blink WebAudio) of device/renderer-related
  // errors. Intended to be executed via a task runner asynchronously.
  void NotifyRenderError();

  const media::AudioParameters& GetSinkParamsForTesting() const {
    return current_sink_params_;
  }

  scoped_refptr<media::AudioRendererSink> GetSinkForTesting() const {
    return sink_;
  }

  // Creates a new sink if one hasn't been created yet, and returns the sink
  // status.
  media::OutputDeviceStatus MaybeCreateSinkAndGetStatus() override;

  const media::AudioParameters& GetOriginalSinkParamsForTesting() const {
    return original_sink_params_;
  }

 protected:
  using CreateSilentSinkCallback =
      base::RepeatingCallback<scoped_refptr<media::AudioRendererSink>(
          const scoped_refptr<base::SequencedTaskRunner>& task_runner)>;

  RendererWebAudioDeviceImpl(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      media::ChannelLayoutConfig layout_config,
      const blink::WebAudioLatencyHint& latency_hint,
      std::optional<float> context_sample_rate,
      media::AudioRendererSink::RenderCallback* webaudio_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      CreateSilentSinkCallback create_silent_sink_cb,
      scoped_refptr<base::SingleThreadTaskRunner> silent_sink_task_runner =
          nullptr);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetSilentSinkTaskRunner();

  void SendLogMessage(const std::string& message);

  // Queries output device information for either silent or physical sink.
  media::OutputDeviceInfo GetSinkOutputDeviceInfo();

  // Create and initialize an instance of AudioRendererSink. Should only be
  // called when `sink_` is nullptr.
  void CreateAudioRendererSink();

  // Evaluates device status, computes buffer sizes and sample rates,
  // initializes the sink, and resets sink on error.
  void HandleDeviceStatus(media::OutputDeviceInfo device_info);

  // Initializes the underlying AudioRendererSink and configures the
  // SilentSinkSuspender (for audible sinks) or direct callback routing.
  void InitializeSink();

  // This is queried from the underlying sink device and then modified according
  // to the WebAudio renderer's needs.
  media::AudioParameters current_sink_params_;
  // This is the unmodified parameters obtained from the underlying sink device.
  // Used to provide the original hardware capacity.
  media::AudioParameters original_sink_params_;

  // To cache the device identifier for sink creation.
  const blink::WebAudioSinkDescriptor sink_descriptor_;

  const blink::WebAudioLatencyHint latency_hint_;

  // The WebAudio renderer's callback; directs to `AudioDestination::Render()`.
  const raw_ptr<media::AudioRendererSink::RenderCallback> webaudio_callback_;

  // To avoid the need for locking, ensure the control methods of the
  // blink::WebAudioDevice implementation are called on the same thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<media::AudioRendererSink> sink_;

  // Used to suspend `sink_` usage when silence has been detected for too long.
  std::unique_ptr<media::SilentSinkSuspender> silent_sink_suspender_;

  // An alternative task runner for `silent_sink_suspender_` or a silent audio
  // sink.
  scoped_refptr<base::SingleThreadTaskRunner> silent_sink_task_runner_;

  // Mainly to bubble up the OnRenderError to the Blink WebAudio module.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Used to trigger one single textlog indicating that rendering started as
  // intended. Set to true once in the first call to the Render callback.
  bool is_rendering_ = false;

  CreateSilentSinkCallback create_silent_sink_cb_;

  bool is_stopped_ = true;

  std::unique_ptr<media::SpeechRecognitionClient> speech_recognition_client_;

  const media::ChannelLayoutConfig layout_config_;
  const std::optional<float> context_sample_rate_;
  bool is_sink_initialized_ = false;
  bool is_detecting_silence_ = true;
  base::RepeatingClosure render_error_callback_;

  base::WeakPtrFactory<RendererWebAudioDeviceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
