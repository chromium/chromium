// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_MAC_H_
#define REMOTING_HOST_AUDIO_CAPTURER_MAC_H_

#include <AudioToolbox/AudioToolbox.h>

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_silence_detector.h"

namespace remoting {

// An AudioCapturer implementation for the Mac which uses an audio loopback
// device. The user will need to manually install and set up an audio loopback
// device such that it takes the system's audio output as its input, and routes
// its input to its output, then this capturer will be able to capture system
// audio on Mac through the loopback device.
class AudioCapturerMac : public AudioCapturer {
 public:
  struct AudioDeviceInfo {
    // Human readable name for the device. Might not be unique.
    std::string device_name;

    // A unique ID for the device.
    std::string device_uid;
  };

  // Gets information about all available audio devices.
  static std::vector<AudioDeviceInfo> GetAudioDevices();

  explicit AudioCapturerMac(const std::string& audio_device_uid);
  ~AudioCapturerMac() override;

  // AudioCapturer interface.
  bool Start(const PacketCapturedCallback& callback) override;

  AudioCapturerMac(const AudioCapturerMac&) = delete;
  AudioCapturerMac& operator=(const AudioCapturerMac&) = delete;

 private:
  static void HandleInputBufferOnAQThread(
      void* user_data,
      AudioQueueRef aq,
      AudioQueueBufferRef buffer,
      const AudioTimeStamp* start_time,
      UInt32 num_packets,
      const AudioStreamPacketDescription* packet_descs);
  void HandleInputBuffer(AudioQueueRef aq, AudioQueueBufferRef buffer);

  bool StartInputQueue();
  void DisposeInputQueue();

  // If an error occurs (err != noErr), stops capturing and disposes the input
  // queue, otherwise no-op.
  // Returns true if error occurs and the input queue has been disposed.
  bool HandleError(OSStatus err, const char* function_name);

  SEQUENCE_CHECKER(sequence_checker_);

  std::string audio_device_uid_;

  AudioStreamBasicDescription stream_description_;
  AudioSilenceDetector silence_detector_;
  PacketCapturedCallback callback_;
  AudioQueueRef input_queue_ = nullptr;
  bool is_started_ = false;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;

  base::WeakPtrFactory<AudioCapturerMac> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_MAC_H_
