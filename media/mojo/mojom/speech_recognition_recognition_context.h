// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RECOGNITION_CONTEXT_H_
#define MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RECOGNITION_CONTEXT_H_

#include <string>
#include <vector>

namespace media {

// Definition of the phrase for speech recognition biasing.
struct SpeechRecognitionPhrase {
  SpeechRecognitionPhrase();
  SpeechRecognitionPhrase(const std::string phrase, float boost);
  SpeechRecognitionPhrase(const SpeechRecognitionPhrase&);
  ~SpeechRecognitionPhrase();

  bool operator==(const SpeechRecognitionPhrase& rhs) const;

  // Text to be boosted for speech recognition biasing.
  std::string phrase;

  // Represents approximately the natural log of the number of times more likely
  // you think this phrase is than what the recognizer knows. A reasonable boost
  // value should be inside the range [0, 10], with a default value of 1. A
  // boost value of 0 means the phrase is not boosted at all, and a higher boost
  // value means the phrase is more likely to appear.
  float boost = 0;
};

// A collection of recognition context for speech recognition biasing.
struct SpeechRecognitionRecognitionContext {
  SpeechRecognitionRecognitionContext();
  SpeechRecognitionRecognitionContext(
      const std::vector<SpeechRecognitionPhrase> phrases);
  SpeechRecognitionRecognitionContext(
      const SpeechRecognitionRecognitionContext&);
  ~SpeechRecognitionRecognitionContext();

  bool operator==(const SpeechRecognitionRecognitionContext& rhs) const;

  // A list of speech recognition phrases to be plugged into the model.
  std::vector<SpeechRecognitionPhrase> phrases;
};

}  // namespace media

#endif  // MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RECOGNITION_CONTEXT_H_
