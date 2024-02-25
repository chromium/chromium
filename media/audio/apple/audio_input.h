// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLE_AUDIO_INPUT_H_
#define MEDIA_AUDIO_APPLE_AUDIO_INPUT_H_

#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioQueue.h>
#include <stdint.h>

#include <memory>

#include "base/atomicops.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class AudioManagerApple;

// Implementation of AudioInputStream for macOS using the Audio Queue service
// in Audio Toolbox. Design reflects PCMQueueOutAudioOutputStream.
class PCMQueueInAudioInputStream : public AudioInputStream {
 public:
  // Parameters as per AudioManager::MakeAudioInputStream.
  PCMQueueInAudioInputStream(AudioManagerApple* manager,
                             const AudioParameters& params);

  PCMQueueInAudioInputStream(const PCMQueueInAudioInputStream&) = delete;
  PCMQueueInAudioInputStream& operator=(const PCMQueueInAudioInputStream&) =
      delete;

  ~PCMQueueInAudioInputStream() override;

  // Implementation of AudioInputStream.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  // Issue the OnError to |callback_|;
  void HandleError(OSStatus err);

  // Allocates and prepares the memory that will be used for recording.
  bool SetupBuffers();

  // Sends a buffer to the audio driver for recording.
  OSStatus QueueNextBuffer(AudioQueueBufferRef audio_buffer);

  // Callback from OS, delegates to non-static version below.
  static void HandleInputBufferStatic(
      void* data,
      AudioQueueRef audio_queue,
      AudioQueueBufferRef audio_buffer,
      const AudioTimeStamp* start_time,
      UInt32 num_packets,
      const AudioStreamPacketDescription* desc);

  // Handles callback from OS. Will be called on OS internal thread.
  void HandleInputBuffer(AudioQueueRef audio_queue,
                         AudioQueueBufferRef audio_buffer,
                         const AudioTimeStamp* start_time,
                         UInt32 num_packets,
                         const AudioStreamPacketDescription* packet_desc);

  static const int kNumberBuffers = 3;

  // Helper methods to set and get atomic |input_callback_is_active_|.
  void SetInputCallbackIsActive(bool active);
  bool GetInputCallbackIsActive();

  // Checks if a stream was started successfully and the audio unit also starts
  // to call InputProc() as it should. This method is called once when a timer
  // expires, a few seconds after calling Start().
  void CheckInputStartupSuccess();

  // Manager that owns this stream, used for closing down.
  raw_ptr<AudioManagerApple> manager_;
  // We use the callback mostly to periodically supply the recorded audio data.
  raw_ptr<AudioInputCallback> callback_;
  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;
  // Handle to the OS audio queue object.
  AudioQueueRef audio_queue_;
  // Size of each of the buffers in |audio_buffers_|
  uint32_t buffer_size_bytes_;
  // True iff Start() has been called successfully.
  bool started_;
  // Used to determine if we need to slow down |callback_| calls.
  base::TimeTicks last_fill_;
  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableOnceClosure deferred_start_cb_;

  // Is set to true on the internal AUHAL IO thread in the first input callback
  // after Start() has bee called.
  base::subtle::Atomic32 input_callback_is_active_;

  // Timer which triggers CheckInputStartupSuccess() to verify that input
  // callbacks have started as intended after a successful call to Start().
  // This timer lives on the main browser thread.
  std::unique_ptr<base::OneShotTimer> input_callback_timer_;

  std::unique_ptr<media::AudioBus> audio_bus_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_APPLE_AUDIO_INPUT_H_
