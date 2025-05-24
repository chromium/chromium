// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_CATAP_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_MAC_CATAP_AUDIO_INPUT_STREAM_H_

#include <CoreAudio/CATapDescription.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_parameters.h"

@class NSError;

namespace media {

class CatapApi;

// Implementation of AudioInputStream using the CoreAudio API for macOS 14.2
// and later. The current implementation supports mono and stereo capture system
// audio loopback capture.
//
// Overview of operation:
// - An instance of CatapAudioInputStream is created by AudioManagerMac.
// - Open() is called, creating the underlying audio tap and aggregate device.
// - Start(sink) is called, causing the stream to start delivering samples.
// - Audio samples are being received by OnCatapSample() and forwarded to the
//   sink. The audio tap is setup to forward audio from all audio output devices
//   unless kMacCatapCaptureDefaultDevice is enabled, where we only capture the
//   default output device.
// - Stop() is called, causing the stream to stop.
// - Close() is called, causing the stream output to be removed and the stream
//   to be destroyed.
class MEDIA_EXPORT API_AVAILABLE(macos(14.2)) CatapAudioInputStream
    : public AgcAudioStream<AudioInputStream> {
  using NotifyOnCloseCallback = base::OnceCallback<void(AudioInputStream*)>;

 public:
  // The supplied parameters must be compatible with the loopback device. In
  // particular:
  // - Number of channels must be either mono or stereo.
  // - Sample rate must be the same as the sample rate of the current default
  //   output device.
  // - Number of frames per buffer must be 512, which is the default buffer size
  //   for CoreAudio Taps.
  // The parameters that are obtained by calling
  // AudioManagerMac::GetInputStreamParameters() for a loopback device fulfills
  // these requirements.
  // TODO(crbug.com/417799564): Add an intermediate buffer that handles
  // differences in buffer sizes and sample rates.
  CatapAudioInputStream(std::unique_ptr<CatapApi> catap_api,
                        const AudioParameters& params,
                        const std::string& device_id,
                        const AudioManager::LogCallback log_callback,
                        const NotifyOnCloseCallback close_callback,
                        const std::string& default_output_device_id);

  CatapAudioInputStream(const CatapAudioInputStream&) = delete;
  CatapAudioInputStream(CatapAudioInputStream&&) = delete;
  CatapAudioInputStream(const CatapAudioInputStream&&) = delete;
  CatapAudioInputStream& operator=(const CatapAudioInputStream&) = delete;
  CatapAudioInputStream& operator=(CatapAudioInputStream&&) = delete;
  CatapAudioInputStream& operator=(const CatapAudioInputStream&&) = delete;

  ~CatapAudioInputStream() override;

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

  void OnCatapSample(const base::span<const AudioBuffer> input_buffers,
                     const AudioTimeStamp* input_time);

 private:
  // Returns all CoreAudio process audio device IDs that belong to the specified
  // process ID.
  NSArray<NSNumber*>* GetProcessAudioDeviceIds(pid_t chrome_process_id);

  // Probe audio tap permission by getting and setting
  // AudioTapPropertyDescription. If either of these operations fail, this
  // function returns false which is an indication that we don't have system
  // audio capture permission.
  bool ProbeAudioTapPermissions();

  // Send log messages to the stream creator.
  void SendLogMessage(const char* format, ...);

  // Interface used to access the CoreAudio framework.
  const std::unique_ptr<CatapApi> catap_api_;

  // Audio parameters passed to the constructor.
  const AudioParameters params_;

  // The length of time covered by the audio data in a single audio buffer.
  const base::TimeDelta buffer_frames_duration_;

  // One of AudioDeviceDescription::kLoopback*.
  const std::string device_id_;

  // Audio bus used to pass audio samples to sink_.
  const std::unique_ptr<AudioBus> audio_bus_;

  // Receives the processed audio data and errors. sink_ is set in the call to
  // Start() and must not be modified until Stop() is called where the audio
  // capture is stopped. While the capture is running, sink_ is accessed on a
  // thread that is associated with the capturer.
  raw_ptr<AudioInputCallback> sink_;

  // Callback to send log messages to the client.
  AudioManager::LogCallback log_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Called when the stream is closed and can be safely deleted.
  NotifyOnCloseCallback close_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  const std::string default_output_device_id_
      GUARDED_BY_CONTEXT(sequence_checker_);

  AudioObjectID aggregate_device_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kAudioObjectUnknown;
  AudioDeviceIOProcID tap_io_proc_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  AudioObjectID tap_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kAudioObjectUnknown;
  CATapDescription* __strong tap_description_
      GUARDED_BY_CONTEXT(sequence_checker_) = nil;
  bool is_device_open_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

API_AVAILABLE(macos(14.2))
AudioInputStream* MEDIA_EXPORT CreateCatapAudioInputStreamForTesting(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    base::OnceCallback<void(AudioInputStream*)> close_callback,
    const std::string& default_output_device_id,
    std::unique_ptr<CatapApi> catap_api);

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_CATAP_AUDIO_INPUT_STREAM_H_
