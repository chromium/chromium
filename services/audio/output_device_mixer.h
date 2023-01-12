// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_H_
#define SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_parameters.h"
#include "services/audio/reference_output.h"

namespace media {
class AudioOutputStream;
}

namespace audio {

// Manages mixing and rendering of all audio outputs going to a specific output
// device. As ReferenceOutput provides to Listeners an ability to listen to the
// audio mix being rendered. If there is at least one listener connected, the
// mixer is mixing all the audio output and rendering the mix as a single audio
// output stream. Otherwise the streams are rendered independently.
class OutputDeviceMixer : public ReferenceOutput {
 public:
  // Callback to be used by OutputDeviceMixer to create actual output streams
  // playing audio.
  using CreateStreamCallback =
      base::RepeatingCallback<media::AudioOutputStream*(
          const std::string& device_id,
          const media::AudioParameters& params)>;

  // A helper class for the clients to pass OutputDeviceMixer::Create around as
  // a callback.
  using CreateCallback =
      base::RepeatingCallback<std::unique_ptr<OutputDeviceMixer>(
          const std::string& device_id,
          const media::AudioParameters& output_params,
          CreateStreamCallback create_stream_callback,
          scoped_refptr<base::SingleThreadTaskRunner> task_runner)>;

  // Creates OutputDeviceMixer which manages playback to the device identified
  // by |device_id|. |output_params| - parameters for the audio mix playback;
  // |create_stream_callback| will be used by OutputDeviceMixer to create output
  // streams playing audio; |task_runner| is the main task runner of
  // OutputDeviceMixer.
  static std::unique_ptr<OutputDeviceMixer> Create(
      const std::string& device_id,
      const media::AudioParameters& output_params,
      CreateStreamCallback create_stream_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // |device_id| is the id of the output device to manage the playback to.
  explicit OutputDeviceMixer(const std::string& device_id);

  OutputDeviceMixer(const OutputDeviceMixer&) = delete;
  OutputDeviceMixer& operator=(const OutputDeviceMixer&) = delete;

  // Id of the device audio output to which is managed by OutputDeviceMixer.
  const std::string& device_id() const { return device_id_; }

  // Creates an audio output stream managed by the given OutputDeviceMixer.
  // |params| - output stream parameters; |on_device_change_callback| - callback
  // to notify the AudioOutputStream client about device change events observed
  // by OutputDeviceMixer.
  virtual media::AudioOutputStream* MakeMixableStream(
      const media::AudioParameters& params,
      base::OnceCallback<void()> on_device_change_callback) = 0;

  // Notify OutputDeviceMixer about the device change event.
  virtual void ProcessDeviceChange() = 0;

 private:
  // Id of the device output to which is managed by the mixer.
  const std::string device_id_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_H_
