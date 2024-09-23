// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_THREAD_CALLBACK_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_THREAD_CALLBACK_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "media/audio/audio_device_stats_reporter.h"
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

  AudioOutputDeviceThreadCallback(const AudioOutputDeviceThreadCallback&) =
      delete;
  AudioOutputDeviceThreadCallback& operator=(
      const AudioOutputDeviceThreadCallback&) = delete;

  ~AudioOutputDeviceThreadCallback() override;

  void MapSharedMemory() override;

  // Called whenever we receive notifications about pending data.
  void Process(uint32_t control_signal) override;

  // Called when the AudioDeviceThread shuts down. Unexpected calls are treated
  // as errors.
  void OnSocketError() override;

  // Returns whether the current thread is the audio device thread or not.
  // Will always return true if DCHECKs are not enabled.
  bool CurrentThreadIsAudioDeviceThread();

  // Sets |first_play_start_time_| to the current time unless it's already set,
  // in which case it's a no-op. The first call to this method MUST have
  // completed by the time we receive our first Process() callback to avoid
  // data races.
  void InitializePlayStartTime();

 private:
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  raw_ptr<media::AudioRendererSink::RenderCallback, DanglingUntriaged>
      render_callback_;
  std::unique_ptr<media::AudioBus> output_bus_;
  uint64_t callback_num_ = 0;

  // Used to record a UMA stat for the audio output stream duration form the
  // moment it successfully started to the moment it stopped - as seen by the
  // renderer process (which equals to |this| lifetime duration).
  const base::TimeTicks create_time_;

  // If set, used to record the startup duration UMA stat.
  std::optional<base::TimeTicks> first_play_start_time_;

  AudioDeviceStatsReporter stats_reporter_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_THREAD_CALLBACK_H_
