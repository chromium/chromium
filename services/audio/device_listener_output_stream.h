// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_DEVICE_LISTENER_OUTPUT_STREAM_H_
#define SERVICES_AUDIO_DEVICE_LISTENER_OUTPUT_STREAM_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"

namespace audio {

// Simple wrapper class, which forwards all AudioOutputStream calls to the
// wrapped |stream_|. It also listens for device change events (via
// the AudioDeviceListener interface, or via errors reporting device changes),
// and forwards them via the |on_device_change_callback_|, on the owning
// (AudioManager) thread. |on_device_change_callback_| must synchronously close
// the DeviceListenerOutputStream.
class DeviceListenerOutputStream final
    : public media::AudioOutputStream,
      public media::AudioOutputStream::AudioSourceCallback,
      public media::AudioManager::AudioDeviceListener {
 public:
  // Note: |on_device_change_callback| must synchronously call close(), which
  // will delete |this|.
  DeviceListenerOutputStream(media::AudioManager* audio_manager,
                             media::AudioOutputStream* wrapped_stream,
                             base::OnceClosure on_device_change_callback);

  DeviceListenerOutputStream(const DeviceListenerOutputStream&) = delete;
  DeviceListenerOutputStream& operator=(const DeviceListenerOutputStream&) =
      delete;

  // AudioOutputStream implementation
  bool Open() final;
  void Start(
      media::AudioOutputStream::AudioSourceCallback* source_callback) final;
  void Stop() final;
  void SetVolume(double volume) final;
  void GetVolume(double* volume) final;
  void Close() final;
  void Flush() final;

 private:
  ~DeviceListenerOutputStream() final;

  // AudioManager::AudioDeviceListener implementation.
  void OnDeviceChange() final;

  // AudioOutputStream::AudioSourceCallback implementation.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final;
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest,
                 bool is_mixing) final;
  void OnError(ErrorType type) final;

  void ReportError(ErrorType type);

  const raw_ptr<media::AudioManager> audio_manager_;

  raw_ptr<media::AudioOutputStream> stream_;

  // Callback to process the device change.
  base::OnceClosure on_device_change_callback_;

  // Actual producer of the audio.
  raw_ptr<media::AudioOutputStream::AudioSourceCallback> source_callback_ =
      nullptr;

  // The task runner for the audio manager. The main task runner for the object.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // For posting cancelable tasks.
  base::WeakPtrFactory<DeviceListenerOutputStream> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_DEVICE_LISTENER_OUTPUT_STREAM_H_
