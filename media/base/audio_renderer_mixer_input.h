// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THREAD SAFETY
//
// This class is generally not thread safe. Callers should ensure thread safety.
// For instance, the |sink_lock_| in WebAudioSourceProvider synchronizes access
// to this object across the main thread (for WebAudio APIs) and the
// media thread (for HTMLMediaElement APIs).
//
// The one exception is protection for |volume_| via |volume_lock_|. This lock
// prevents races between SetVolume() (called on any thread) and ProvideInput
// (called on audio device thread). See http://crbug.com/588992.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

class AudioRendererMixerPool;
class AudioRendererMixer;

class MEDIA_EXPORT AudioRendererMixerInput
    : public SwitchableAudioRendererSink,
      public AudioConverter::InputCallback {
 public:
  AudioRendererMixerInput(AudioRendererMixerPool* mixer_pool,
                          int owner_id,
                          const std::string& device_id,
                          AudioLatency::LatencyType latency);

  // SwitchableAudioRendererSink implementation.
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  bool IsOptimizedForHardwareParameters() override;
  void Initialize(const AudioParameters& params,
                  AudioRendererSink::RenderCallback* renderer) override;
  void SwitchOutputDevice(const std::string& device_id,
                          OutputDeviceStatusCB callback) override;
  // This is expected to be called on the audio rendering thread. The caller
  // must ensure that this input has been added to a mixer before calling the
  // function, and that it is not removed from the mixer before this function
  // returns.
  bool CurrentThreadIsRenderingThread() override;

  // Called by AudioRendererMixer when an error occurs.
  void OnRenderError();

 protected:
  ~AudioRendererMixerInput() override;

 private:
  friend class AudioRendererMixerInputTest;

  // Pool to obtain mixers from / return them to.
  AudioRendererMixerPool* const mixer_pool_;

  // Protect |volume_|, accessed by separate threads in ProvideInput() and
  // SetVolume().
  base::Lock volume_lock_;

  bool started_;
  bool playing_;
  double volume_ GUARDED_BY(volume_lock_);

  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override;

  // AudioParameters received during Initialize().
  AudioParameters params_;

  const int owner_id_;
  std::string device_id_;  // ID of hardware device to use
  const AudioLatency::LatencyType latency_;

  // AudioRendererMixer obtained from mixer pool during Initialize(),
  // guaranteed to live (at least) until it is returned to the pool.
  AudioRendererMixer* mixer_;

  // Source of audio data which is provided to the mixer.
  AudioRendererSink::RenderCallback* callback_;

  // Error callback for handing to AudioRendererMixer.
  const base::Closure error_cb_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerInput);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_INPUT_H_
