// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_MIXER_H_
#define SERVICES_AUDIO_OUTPUT_MIXER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_parameters.h"
#include "services/audio/snoopable.h"

namespace media {
class AudioOutputStream;
}

namespace audio {

// Manages mixing and rendering of all audio outputs going to a specific output
// device. As Snoopable provides to Snoopers an ability to snoop on the audio
// mix being rendered. If there is at least one snooper listening, the mixer
// is mixing all the audio output and rendering the mix as a single audio output
// stream. Otherwise the streams are rendered independently.
class OutputMixer : public Snoopable {
 public:
  // Callback to be used by OutputMixer to create actual output streams playing
  // audio.
  using CreateStreamCallback =
      base::RepeatingCallback<media::AudioOutputStream*(
          const std::string& device_id,
          const media::AudioParameters& params)>;

  // A helper class for the clients to pass OutputMixer::Create around as a
  // callback.
  using CreateCallback = base::RepeatingCallback<std::unique_ptr<OutputMixer>(
      const std::string& device_id,
      const media::AudioParameters& output_params,
      CreateStreamCallback create_stream_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)>;

  // Creates OutputMixer which manages playback to the device identified by
  // |device_id|. |output_params| - parameters for the audio mix playback;
  // |create_stream_callback| will be used by OutputMixer to create output
  // streams playing audio; |task_runner| is the main task runner of
  // OutputMixer.
  static std::unique_ptr<OutputMixer> Create(
      const std::string& device_id,
      const media::AudioParameters& output_params,
      CreateStreamCallback create_stream_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // |device_id| is the id of the output device to manage the playback to.
  explicit OutputMixer(const std::string& device_id);

  OutputMixer(const OutputMixer&) = delete;
  OutputMixer& operator=(const OutputMixer&) = delete;

  // Id of the device audio output to which is managed by OutputMixer.
  const std::string& device_id() const { return device_id_; }

  // Creates an audio output stream managed by the given OutputMixer.
  // |params| - output stream parameters; |on_device_change_callback| - callback
  // to notify the AudioOutputStream client about device change events observed
  // by OutputMixer.
  virtual media::AudioOutputStream* MakeMixableStream(
      const media::AudioParameters& params,
      base::OnceCallback<void()> on_device_change_callback) = 0;

  // Notify OutputMixer about the device change event.
  virtual void ProcessDeviceChange() = 0;

 private:
  // Id of the device output to which is managed by the mixer.
  const std::string device_id_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_MIXER_H_
