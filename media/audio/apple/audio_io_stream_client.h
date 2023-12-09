// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLE_AUDIO_IO_STREAM_CLIENT_H_
#define MEDIA_AUDIO_APPLE_AUDIO_IO_STREAM_CLIENT_H_

#include <AudioUnit/AudioUnit.h>

#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_io.h"

#if BUILDFLAG(IS_MAC)
#include <CoreAudio/CoreAudio.h>
#else
#include "media/audio/ios/audio_private_api.h"
#endif

namespace media {

// This class serves as an interface for callback implementation on Apple
// platforms, catering to both iOS and macOS. AudioManagerIOS and
// AppleManagerMac are responsible for providing concrete implementations for
// this interface. Notably, AUHALStream or PCMQueueInAudioInputStream holds an
// instance of this class and notify the platform-specific implementation for
// iOS or macOS to close down and release the stream.
class AudioIOStreamClient {
 public:
  virtual void ReleaseOutputStreamUsingRealDevice(AudioOutputStream* stream,
                                                  AudioDeviceID device_id) = 0;
  virtual void ReleaseInputStreamUsingRealDevice(AudioInputStream* stream) = 0;
  virtual bool MaybeChangeBufferSize(AudioDeviceID device_id,
                                     AudioUnit audio_unit,
                                     AudioUnitElement element,
                                     size_t desired_buffer_size) = 0;
#if BUILDFLAG(IS_MAC)
  virtual base::TimeDelta GetDeferStreamStartTimeout() const = 0;
  virtual base::SingleThreadTaskRunner* GetTaskRunner() const = 0;
  virtual void StopAmplitudePeakTrace() = 0;
#endif
};

}  // namespace media

#endif  // MEDIA_AUDIO_APPLE_AUDIO_IO_STREAM_CLIENT_H_
