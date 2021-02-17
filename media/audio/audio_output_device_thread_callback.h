// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_THREAD_CALLBACK_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_THREAD_CALLBACK_H_

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/optional.h"
#include "media/audio/audio_device_thread.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

// Takes care of invoking the render callback on the audio thread.
// An instance of this class is created for each capture stream on output device
// stream created.
class MEDIA_EXPORT AudioOutputDeviceThreadCallback
    : public media::AudioDeviceThread::Callback {
 public:
  AudioOutputDeviceThreadCallback(
      const media::AudioParameters& audio_parameters,
      base::UnsafeSharedMemoryRegion shared_memory_region,
      media::AudioRendererSink::RenderCallback* render_callback);
  ~AudioOutputDeviceThreadCallback() override;

  void MapSharedMemory() override;

  // Called whenever we receive notifications about pending data.
  void Process(uint32_t control_signal) override;

  // Returns whether the current thread is the audio device thread or not.
  // Will always return true if DCHECKs are not enabled.
  bool CurrentThreadIsAudioDeviceThread();

  // Sets |first_play_start_time_| to the current time unless it's already set,
  // in which case it's a no-op. The first call to this method MUST have
  // completed by the time we recieve our first Process() callback to avoid
  // data races.
  void InitializePlayStartTime();

 private:
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  media::AudioRendererSink::RenderCallback* render_callback_;
  std::unique_ptr<media::AudioBus> output_bus_;
  uint64_t callback_num_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceThreadCallback);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_THREAD_CALLBACK_H_
