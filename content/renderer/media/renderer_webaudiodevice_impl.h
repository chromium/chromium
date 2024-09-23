// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class SilentSinkSuspender;
class SpeechRecognitionClient;
}

namespace content {

// The actual implementation of Blink "WebAudioDevice" that handles the
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
      media::AudioRendererSink::RenderCallback* webaudio_callback);

  // blink::WebAudioDevice implementation.
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Resume() override;
  double SampleRate() override;
  int FramesPerBuffer() override;
  int MaxChannelCount() override;

  // Sets the detect silence flag for SilentSinkSuspender. Invoked by Blink Web
  // Audio.
  void SetDetectSilence(bool enable_silence_detection) override;

  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;

  // This callback method may be called in two different scenarios:
  // 1) When the constructor's audio device activation fails. (main thread)
  // 2) When the audio infra reports an device/render error. (audio thread)
  void OnRenderError() override;

  // Notifies the client (e.g. Blink WebAudio) of device/renderer-related
  // errors. Intended to be executed via a task runner asynchronously.
  void NotifyRenderError();

  void SetSilentSinkTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  const media::AudioParameters& get_sink_params_for_testing() {
    return current_sink_params_;
  }

  // Creates a new sink if one hasn't been created yet, and returns the sink
  // status.
  media::OutputDeviceStatus MaybeCreateSinkAndGetStatus() override;

 protected:
  // Callback to get output device params (for tests).
  using OutputDeviceParamsCallback = base::OnceCallback<media::AudioParameters(
      const blink::LocalFrameToken& frame_token,
      const std::string& device_id)>;

  using CreateSilentSinkCallback =
      base::RepeatingCallback<scoped_refptr<media::AudioRendererSink>(
          const scoped_refptr<base::SequencedTaskRunner>& task_runner)>;

  RendererWebAudioDeviceImpl(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      media::ChannelLayoutConfig layout_config,
      const blink::WebAudioLatencyHint& latency_hint,
      media::AudioRendererSink::RenderCallback* webaudio_callback,
      OutputDeviceParamsCallback device_params_cb,
      CreateSilentSinkCallback create_silent_sink_cb);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetSilentSinkTaskRunner();

  void SendLogMessage(const std::string& message);

  // Create and initialize an instance of AudioRendererSink. Should only be
  // called when `sink_` is nullptr.
  void CreateAudioRendererSink();

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

  // Used to suspend |sink_| usage when silence has been detected for too long.
  std::unique_ptr<media::SilentSinkSuspender> silent_sink_suspender_;

  // Render frame token for the current context.
  blink::LocalFrameToken frame_token_;

  // An alternative task runner for `silent_sink_suspender_` or a silent audio
  // sink.
  scoped_refptr<base::SingleThreadTaskRunner> silent_sink_task_runner_;

  // Mainly to bubble up the OnRenderError to the Blink WebAudio module.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Used to trigger one single textlog indicating that rendering started as
  // intended. Set to true once in the first call to the Render callback.
  bool is_rendering_ = false;

  CreateSilentSinkCallback create_silent_sink_cb_;

  // Used to indicate if device is stopped.
  bool is_stopped_ = true;

  std::unique_ptr<media::SpeechRecognitionClient> speech_recognition_client_;

  base::WeakPtrFactory<RendererWebAudioDeviceImpl> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(RendererWebAudioDeviceImplTest,
                           CreateSinkAndGetDeviceStatus_HealthyDevice);
  FRIEND_TEST_ALL_PREFIXES(RendererWebAudioDeviceImplTest,
                           CreateSinkAndGetDeviceStatus_ErrorDevice);
  FRIEND_TEST_ALL_PREFIXES(RendererWebAudioDeviceImplTest,
                           CreateSinkAndGetDeviceStatus_SilentSink);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
