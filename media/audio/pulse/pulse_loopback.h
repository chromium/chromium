// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_LOOPBACK_H_
#define MEDIA_AUDIO_PULSE_PULSE_LOOPBACK_H_

#include <pulse/pulseaudio.h>

#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_parameters.h"

namespace media {

class PulseAudioInputStream;
class PulseLoopbackManager;

// An AudioInputStream managed by PulseLoopbackManager, intended for system
// audio loopback capture. The underlying PulseAudioInputStream is managed by
// this stream.
class PulseLoopbackAudioStream : public AgcAudioStream<AudioInputStream> {
 public:
  using ReleaseStreamCallback = base::OnceCallback<void(AudioInputStream*)>;

  PulseLoopbackAudioStream(ReleaseStreamCallback release_stream_callback,
                           const std::string& source_name,
                           const AudioParameters& params,
                           pa_threaded_mainloop* mainloop,
                           pa_context* context,
                           AudioManager::LogCallback log_callback,
                           bool mute_system_audio);

  PulseLoopbackAudioStream(const PulseLoopbackAudioStream&) = delete;
  PulseLoopbackAudioStream(const PulseLoopbackAudioStream&&) = delete;
  PulseLoopbackAudioStream& operator=(const PulseLoopbackAudioStream&) = delete;
  PulseLoopbackAudioStream& operator=(const PulseLoopbackAudioStream&&) =
      delete;

  ~PulseLoopbackAudioStream() override;

  // AudioInputStream:: implementation.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // Closes the currently open `stream_` and opens a new one. The new source
  // name is provided by PulseLoopbackManager when the default sink changes.
  void ChangeStreamSource(const std::string& source_name);

 private:
  void CloseWrappedStream();

  // Used to notify the owner when the stream closes.
  ReleaseStreamCallback release_stream_callback_;

  const AudioParameters params_;
  const raw_ptr<pa_threaded_mainloop> mainloop_;
  const raw_ptr<pa_context> context_;
  AudioManager::LogCallback log_callback_;
  const bool mute_system_audio_;

  // Passed to each underlying PulseAudioInputStream.
  raw_ptr<AudioInputCallback> sink_;

  // Currently open stream.
  raw_ptr<PulseAudioInputStream> stream_;

  // Indicates if the underlying stream is open.
  bool stream_opened_{};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_LOOPBACK_H_
