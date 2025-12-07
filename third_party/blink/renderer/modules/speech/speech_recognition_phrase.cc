// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_phrase.h"

namespace blink {

SpeechRecognitionPhrase* SpeechRecognitionPhrase::Create(
    const String& phrase,
    float boost,
    ExceptionState& exception_state) {
  if (boost < 0 || boost > 10) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Speech recognition phrase boost value "
                                      "must be inside the range [0, 10].");
    return nullptr;
  }
  return MakeGarbageCollected<SpeechRecognitionPhrase>(phrase, boost);
}

SpeechRecognitionPhrase::SpeechRecognitionPhrase(const String& phrase,
                                                 float boost)
    : phrase_(phrase), boost_(boost) {}

}  // namespace blink
