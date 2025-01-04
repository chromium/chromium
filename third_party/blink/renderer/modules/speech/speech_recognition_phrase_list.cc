// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_phrase_list.h"

namespace blink {

SpeechRecognitionPhraseList* SpeechRecognitionPhraseList::Create() {
  return MakeGarbageCollected<SpeechRecognitionPhraseList>();
}

void SpeechRecognitionPhraseList::Trace(Visitor* visitor) const {
  visitor->Trace(phrases_);
  ScriptWrappable::Trace(visitor);
}

SpeechRecognitionPhrase* SpeechRecognitionPhraseList::item(
    uint64_t index) const {
  if (index < 0 || index >= length()) {
    return nullptr;
  }
  return phrases_[index].Get();
}

void SpeechRecognitionPhraseList::addItem(SpeechRecognitionPhrase* item) {
  phrases_.push_back(item);
}

}  // namespace blink
