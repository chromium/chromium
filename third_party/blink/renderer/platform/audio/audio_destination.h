/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_io_callback.h"
#include "third_party/blink/renderer/platform/audio/media_multi_channel_resampler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class PushPullFIFO;
class WebAudioLatencyHint;
class WebAudioSinkDescriptor;

// The AudioDestination class is an audio sink interface between the media
// renderer and the Blink's WebAudio module. It has a FIFO to adapt the
// different processing block sizes of WebAudio renderer and actual hardware
// audio callback.
//
// Currently AudioDestination supports two types of threading models:
//  - Single-thread (default): process the entire WebAudio render call chain by
//    AudioDeviceThread.
//  - Dual-thread (experimental): Use WebThread for the WebAudio rendering with
//    AudioWorkletThread.
class PLATFORM_EXPORT AudioDestination
    : public ThreadSafeRefCounted<AudioDestination>,
      public WebAudioDevice::RenderCallback {
  USING_FAST_MALLOC(AudioDestination);

 public:
  // Represents the current state of the underlying |WebAudioDevice| object
  // (RendererWebAudioDeviceImpl).
  enum DeviceState {
    kRunning,
    kPaused,
    kStopped,
  };

  AudioDestination(AudioIOCallback&,
                   const WebAudioSinkDescriptor& sink_descriptor,
                   unsigned number_of_output_channels,
                   const WebAudioLatencyHint&,
                   absl::optional<float> context_sample_rate,
                   unsigned render_quantum_frames);
  AudioDestination(const AudioDestination&) = delete;
  AudioDestination& operator=(const AudioDestination&) = delete;
  ~AudioDestination() override;

  static scoped_refptr<AudioDestination> Create(
      AudioIOCallback&,
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint&,
      absl::optional<float> context_sample_rate,
      unsigned render_quantum_frames);

  // The actual render function (WebAudioDevice::RenderCallback) isochronously
  // invoked by the media renderer. This is never called after Stop() is called.
  void Render(const WebVector<float*>& destination_data,
              uint32_t number_of_frames,
              double delay,
              double delay_timestamp) override;

  // The actual render request to the WebAudio destination node. This method
  // can be invoked on both AudioDeviceThread (single-thread rendering) and
  // AudioWorkletThread (dual-thread rendering).
  void RequestRender(size_t frames_requested,
                     size_t frames_to_render,
                     double delay,
                     double delay_timestamp);

  virtual void Start();
  virtual void Stop();
  virtual void Pause();
  virtual void Resume();

  // Starts the destination with the AudioWorklet support.
  void StartWithWorkletTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> worklet_task_runner);

  // Getters must be accessed from the main thread.
  uint32_t CallbackBufferSize() const;
  bool IsPlaying();

  // This is the context sample rate, not the device one.
  double SampleRate() const { return context_sample_rate_; }

  // Returns the audio buffer size in frames used by the underlying audio
  // hardware.
  int FramesPerBuffer() const;

  // The information from the actual audio hardware. (via Platform::Current)
  static size_t HardwareBufferSize();
  static float HardwareSampleRate();
  static uint32_t MaxChannelCount();

  // Sets the detect silence flag for |web_audio_device_|.
  void SetDetectSilence(bool detect_silence);

  // This should only be called from the audio thread.
  unsigned RenderQuantumFrames() const { return render_quantum_frames_; }

 private:
  void SetDeviceState(DeviceState);

  // Provide input to the resampler (if used).
  void ProvideResamplerInput(int resampler_frame_delay, AudioBus* dest);

  // Check if the buffer size chosen by the WebAudioDevice is too large.
  bool CheckBufferSize(unsigned render_quantum_frames);

  void SendLogMessage(const String& message);

  unsigned render_quantum_frames_;

  // Accessed by the main thread.
  std::unique_ptr<WebAudioDevice> web_audio_device_;
  const unsigned number_of_output_channels_;
  uint32_t callback_buffer_size_;

  // The task runner for AudioWorklet operation. This is only valid when
  // the AudioWorklet is activated.
  scoped_refptr<base::SingleThreadTaskRunner> worklet_task_runner_;

  // Can be accessed by both threads: resolves the buffer size mismatch between
  // the WebAudio engine and the callback function from the actual audio device.
  std::unique_ptr<PushPullFIFO> fifo_;

  // Accessed by device thread: to pass the data from FIFO to the device.
  scoped_refptr<AudioBus> output_bus_;

  // Accessed by rendering thread: to push the rendered result from WebAudio
  // graph into the FIFO.
  scoped_refptr<AudioBus> render_bus_;

  // Accessed by rendering thread: the render callback function of WebAudio
  // engine. (i.e. DestinationNode)
  AudioIOCallback& callback_;

  // Accessed by rendering thread.
  size_t frames_elapsed_;

  // The sample rate used for rendering the Web Audio graph.
  float context_sample_rate_;

  // Used for resampling if the Web Audio sample rate differs from the platform
  // one.
  std::unique_ptr<MediaMultiChannelResampler> resampler_;
  std::unique_ptr<media::AudioBus> resampler_bus_;

  // Required for RequestRender and also in the resampling callback (if used).
  AudioIOPosition output_position_;

  AudioCallbackMetricReporter metric_reporter_;

  // This protects |device_state_| below.
  mutable base::Lock state_change_lock_;

  // Modified only on the main thread, so it can be read without holding a lock
  // there.
  DeviceState device_state_;

  // Collect the device latency matric only from the initial callback.
  bool is_latency_metric_collected_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DESTINATION_H_
