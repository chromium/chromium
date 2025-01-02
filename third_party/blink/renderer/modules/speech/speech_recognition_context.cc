// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_context.h"

namespace blink {

SpeechRecognitionContext* SpeechRecognitionContext::Create(
    SpeechRecognitionPhraseList* phrases) {
  return MakeGarbageCollected<SpeechRecognitionContext>(phrases);
}

SpeechRecognitionContext::SpeechRecognitionContext(
    SpeechRecognitionPhraseList* phrases)
    : phrases_(phrases) {}

void SpeechRecognitionContext::Trace(Visitor* visitor) const {
  visitor->Trace(phrases_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
