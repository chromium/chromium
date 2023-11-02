// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_

#include "media/base/media_export.h"

namespace media {

// Enables/disables audio debug recording on construction/destruction. Objects
// are created using audio::CreateAudioDebugRecordingSession.
class MEDIA_EXPORT AudioDebugRecordingSession {
 public:
  AudioDebugRecordingSession(const AudioDebugRecordingSession&) = delete;
  AudioDebugRecordingSession& operator=(const AudioDebugRecordingSession&) =
      delete;

  virtual ~AudioDebugRecordingSession() = default;

 protected:
  AudioDebugRecordingSession() = default;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_
