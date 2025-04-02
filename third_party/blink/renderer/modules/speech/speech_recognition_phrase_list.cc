// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_phrase_list.h"

namespace blink {

SpeechRecognitionPhraseList* SpeechRecognitionPhraseList::Create(
    const HeapVector<Member<SpeechRecognitionPhrase>>& phrases) {
  return MakeGarbageCollected<SpeechRecognitionPhraseList>(phrases);
}

SpeechRecognitionPhraseList::SpeechRecognitionPhraseList(
    const HeapVector<Member<SpeechRecognitionPhrase>>& phrases)
    : phrases_(phrases) {}

void SpeechRecognitionPhraseList::Trace(Visitor* visitor) const {
  visitor->Trace(phrases_);
  ScriptWrappable::Trace(visitor);
}

SpeechRecognitionPhrase* SpeechRecognitionPhraseList::item(
    wtf_size_t index,
    ExceptionState& exception_state) const {
  if (index < 0 || index >= length()) {
    exception_state.ThrowRangeError("Index is out of range.");
    return nullptr;
  }
  return phrases_[index].Get();
}

void SpeechRecognitionPhraseList::addItem(SpeechRecognitionPhrase* item) {
  phrases_.push_back(item);
}

void SpeechRecognitionPhraseList::removeItem(wtf_size_t index,
                                             ExceptionState& exception_state) {
  if (index < 0 || index >= length()) {
    exception_state.ThrowRangeError("Index is out of range.");
  }
  phrases_.EraseAt(index);
}

}  // namespace blink
