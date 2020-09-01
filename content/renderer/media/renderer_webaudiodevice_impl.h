// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class SilentSinkSuspender;
}

namespace content {
class CONTENT_EXPORT RendererWebAudioDeviceImpl
    : public blink::WebAudioDevice,
      public media::AudioRendererSink::RenderCallback {
 public:
  ~RendererWebAudioDeviceImpl() override;

  static std::unique_ptr<RendererWebAudioDeviceImpl> Create(
      media::ChannelLayout layout,
      int channels,
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
             int prior_frames_skipped,
             media::AudioBus* dest) override;

  void OnRenderError() override;

  void SetSuspenderTaskRunnerForTesting(
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

  // Callback get render frame token for current context (for tests).
  using RenderFrameTokenCallback = base::OnceCallback<blink::LocalFrameToken()>;

  RendererWebAudioDeviceImpl(media::ChannelLayout layout,
                             int channels,
                             const blink::WebAudioLatencyHint& latency_hint,
                             blink::WebAudioDevice::RenderCallback* callback,
                             const base::UnguessableToken& session_id,
                             OutputDeviceParamsCallback device_params_cb,
                             RenderFrameTokenCallback render_frame_token_cb);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> GetSuspenderTaskRunner();

  media::AudioParameters sink_params_;

  const blink::WebAudioLatencyHint latency_hint_;

  // Weak reference to the callback into WebKit code.
  blink::WebAudioDevice::RenderCallback* const client_callback_;

  // To avoid the need for locking, ensure the control methods of the
  // blink::WebAudioDevice implementation are called on the same thread.
  base::ThreadChecker thread_checker_;

  // When non-NULL, we are started.  When NULL, we are stopped.
  scoped_refptr<media::AudioRendererSink> sink_;

  // ID to allow browser to select the correct input device for unified IO.
  base::UnguessableToken session_id_;

  // Used to suspend |sink_| usage when silence has been detected for too long.
  std::unique_ptr<media::SilentSinkSuspender> webaudio_suspender_;

  // Render frame token for the current context.
  blink::LocalFrameToken frame_token_;

  // Allow unit tests to set a custom TaskRunner for |webaudio_suspender_|.
  scoped_refptr<base::SingleThreadTaskRunner> suspender_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebAudioDeviceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBAUDIODEVICE_IMPL_H_
