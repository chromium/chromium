// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SPEECH_RECOGNITION_CLIENT_H_
#define MEDIA_BASE_SPEECH_RECOGNITION_CLIENT_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;
class AudioParameters;

// The interface for the speech recognition client used to transcribe audio into
// captions.
class MEDIA_EXPORT SpeechRecognitionClient {
 public:
  using OnReadyCallback = base::OnceCallback<void()>;

  virtual ~SpeechRecognitionClient() = default;

  // Adds audio for transcription.
  // Optionally, `media_start_pts` is the media presentation timestamp of the
  // first frame in `buffer`. If `media_start_pts` is omitted after being
  // provided, the client can assume that `buffer`s received are contiguous
  // since the last `media_start_pts` are continuous. A new `media_start_pts`
  // should be sent after any seeks or large gaps in the original media's
  // timestamps.
  virtual void AddAudio(scoped_refptr<AudioBuffer> buffer,
                        std::optional<base::TimeDelta> media_start_pts) = 0;

  // This should not perform any memory allocations so that it can be called on
  // audio rendering threads. Must call Reconfigure() first and can't be called
  // concurrently with Reconfigure().
  virtual void AddAudio(const media::AudioBus& audio_bus) = 0;

  virtual bool IsSpeechRecognitionAvailable() = 0;

  virtual void SetOnReadyCallback(OnReadyCallback callback) = 0;

  // Must not be called concurrently with AddAudio().
  virtual void Reconfigure(const media::AudioParameters& audio_parameters) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_SPEECH_RECOGNITION_CLIENT_H_
