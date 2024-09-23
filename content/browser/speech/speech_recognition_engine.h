// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_

#include <vector>

#include "components/speech/audio_buffer.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"

namespace blink {
namespace mojom {
class SpeechRecognitionError;
}  // namespace mojom
}  // namespace blink

namespace content {

// This is the interface for any speech recognition engine, local or network.
//
// The expected call sequence is:
// StartRecognition      Mandatory at beginning of SR.
//   TakeAudioChunk      For every audio chunk pushed.
//   AudioChunksEnded    Finalize the audio stream (omitted in case of errors).
// EndRecognition        Mandatory at end of SR (even on errors).
//
// No delegate callbacks are performed before StartRecognition or after
// EndRecognition. If a recognition was started, the caller can free the
// SpeechRecognitionEngine only after calling EndRecognition.

class CONTENT_EXPORT SpeechRecognitionEngine {
 public:
  class Delegate {
   public:
    // Called whenever a result is retrieved.
    virtual void OnSpeechRecognitionEngineResults(
        const std::vector<media::mojom::WebSpeechRecognitionResultPtr>&
            results) = 0;
    virtual void OnSpeechRecognitionEngineEndOfUtterance() = 0;
    virtual void OnSpeechRecognitionEngineError(
        const media::mojom::SpeechRecognitionError& error) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  SpeechRecognitionEngine() = default;
  virtual ~SpeechRecognitionEngine() = default;
  virtual void StartRecognition() = 0;
  virtual void EndRecognition() = 0;
  virtual void TakeAudioChunk(const AudioChunk& data) = 0;
  virtual void AudioChunksEnded() = 0;
  virtual int GetDesiredAudioChunkDurationMs() const = 0;

  void SetAudioParameters(media::AudioParameters audio_parameters);

  // set_delegate detached from constructor for lazy dependency injection.
  void set_delegate(Delegate* delegate);

 protected:
  raw_ptr<Delegate> delegate_ = nullptr;
  media::AudioParameters audio_parameters_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
