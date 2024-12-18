// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/speech_recognition_recognition_context.h"

namespace media {

SpeechRecognitionPhrase::SpeechRecognitionPhrase() = default;
SpeechRecognitionPhrase::SpeechRecognitionPhrase(const std::string phrase,
                                                 float boost)
    : phrase(std::move(phrase)), boost(boost) {}
SpeechRecognitionPhrase::SpeechRecognitionPhrase(
    const SpeechRecognitionPhrase&) = default;
SpeechRecognitionPhrase::~SpeechRecognitionPhrase() = default;

bool SpeechRecognitionPhrase::operator==(
    const SpeechRecognitionPhrase& rhs) const {
  return phrase == rhs.phrase && boost == rhs.boost;
}

SpeechRecognitionRecognitionContext::SpeechRecognitionRecognitionContext() =
    default;
SpeechRecognitionRecognitionContext::SpeechRecognitionRecognitionContext(
    const std::vector<SpeechRecognitionPhrase> phrases)
    : phrases(std::move(phrases)) {}
SpeechRecognitionRecognitionContext::SpeechRecognitionRecognitionContext(
    const SpeechRecognitionRecognitionContext&) = default;
SpeechRecognitionRecognitionContext::~SpeechRecognitionRecognitionContext() =
    default;

bool SpeechRecognitionRecognitionContext::operator==(
    const SpeechRecognitionRecognitionContext& rhs) const {
  return phrases == rhs.phrases;
}

}  // namespace media
