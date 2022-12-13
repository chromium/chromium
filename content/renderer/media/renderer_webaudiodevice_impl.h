// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class SilentSinkSuspender;
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
      media::ChannelLayout layout,
      int number_of_output_channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      const base::UnguessableToken& session_id);

  // blink::WebAudioDevice implementation.
  void Start() override;
  void Stop() override;
  void Pause() override;
  void Resume() override;
  double SampleRate() override;
  int FramesPerBuffer() override;

  // Sets the detect silence flag for SilentSinkSuspender. Invoked by Blink Web
  // Audio.
  void SetDetectSilence(bool enable_silence_detection) override;

  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;

  void OnRenderError() override;

  void SetSilentSinkTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  const media::AudioParameters& get_sink_params_for_testing() {
    return sink_params_;
  }

 protected:
  // Callback to get output device params (for tests).
  using OutputDeviceParamsCallback = base::OnceCallback<media::AudioParameters(
      const blink::LocalFrameToken& frame_token,
      const base::UnguessableToken& session_id,
      const std::string& device_id)>;

  using CreateSilentSinkCallback =
      base::RepeatingCallback<scoped_refptr<media::AudioRendererSink>(
          const scoped_refptr<base::SequencedTaskRunner>& task_runner)>;

  RendererWebAudioDeviceImpl(
      const blink::WebAudioSinkDescriptor& sink_descriptor,
      media::ChannelLayout layout,
      int number_of_output_channels,
      const blink::WebAudioLatencyHint& latency_hint,
      blink::WebAudioDevice::RenderCallback* callback,
      const base::UnguessableToken& session_id,
      OutputDeviceParamsCallback device_params_cb,
      CreateSilentSinkCallback create_silent_sink_cb);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetSilentSinkTaskRunner();

  void SendLogMessage(const std::string& message);

  media::AudioParameters sink_params_;

  // To cache the device identifier for sink creation.
  const blink::WebAudioSinkDescriptor sink_descriptor_;

  const blink::WebAudioLatencyHint latency_hint_;

  // Weak reference to the callback into WebKit code.
  blink::WebAudioDevice::RenderCallback* const client_callback_;

  // Used to wrap AudioBus to be passed into |client_callback_|.
  blink::WebVector<float*> web_audio_dest_data_;

  // To avoid the need for locking, ensure the control methods of the
  // blink::WebAudioDevice implementation are called on the same thread.
  base::ThreadChecker thread_checker_;

  // When non-NULL, we are started.  When NULL, we are stopped.
  scoped_refptr<media::AudioRendererSink> sink_;

  // ID to allow browser to select the correct input device for unified IO.
  const base::UnguessableToken session_id_;

  // Used to suspend |sink_| usage when silence has been detected for too long.
  std::unique_ptr<media::SilentSinkSuspender> silent_sink_suspender_;

  // Render frame token for the current context.
  blink::LocalFrameToken frame_token_;

  // An alternative task runner for `silent_sink_suspender_` or a silent audio
  // sink.
  scoped_refptr<base::SingleThreadTaskRunner> silent_sink_task_runner_;

  // Used to trigger one single textlog indicating that rendering started as
  // intended. Set to true once in the first call to the Render callback.
  bool is_rendering_ = false;

  CreateSilentSinkCallback create_silent_sink_cb_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
