// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_PHRASE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_PHRASE_LIST_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_phrase.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class MODULES_EXPORT SpeechRecognitionPhraseList final
    : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SpeechRecognitionPhraseList* Create();

  explicit SpeechRecognitionPhraseList() = default;
  ~SpeechRecognitionPhraseList() override = default;

  // ScriptWrappable:
  void Trace(Visitor* visitor) const override;

  // SpeechRecognitionPhraseList:
  uint64_t length() const { return phrases_.size(); }
  SpeechRecognitionPhrase* item(uint64_t index) const;
  void addItem(SpeechRecognitionPhrase* item);

 private:
  HeapVector<Member<SpeechRecognitionPhrase>> phrases_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_PHRASE_LIST_H_
