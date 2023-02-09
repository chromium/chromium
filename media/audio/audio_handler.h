// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_HANDLER_H_
#define MEDIA_AUDIO_AUDIO_HANDLER_H_

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

// The implementations of the class are expected to return the decoded audio
// data in order starting from the beginning of the stream via `CopyTo()` until
// reaching the end of the stream.
class MEDIA_EXPORT AudioHandler {
 public:
  AudioHandler() = default;
  AudioHandler(const AudioHandler&) = delete;
  AudioHandler& operator=(const AudioHandler&) = delete;
  virtual ~AudioHandler() = default;

  // The number of frames of `bus` in `CopyTo()`.
  static constexpr int kDefaultFrameCount = 1024;

  // Returns true if the instance of this class can be initialized successfully.
  virtual bool Initialize() = 0;
  virtual int GetNumChannels() const = 0;
  virtual int GetSampleRate() const = 0;
  virtual base::TimeDelta GetDuration() const = 0;

  // Returns true once `CopyTo()` has reached end of stream.
  virtual bool AtEnd() const = 0;

  // Fills `bus` with available audio data. If bus can't be completely filled,
  // this will be reflected in `frames_written`.
  // Notes that this is a stateful function and each call to it will resume
  // where the previous `CopyTo()` stops.
  virtual bool CopyTo(AudioBus* bus, size_t* frames_written) = 0;

  // Restores initial state to the head of the stream.
  virtual void Reset() = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_HANDLER_H_