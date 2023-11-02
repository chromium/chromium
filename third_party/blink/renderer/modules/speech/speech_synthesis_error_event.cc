// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event.h"

namespace blink {

// static
SpeechSynthesisErrorEvent* SpeechSynthesisErrorEvent::Create(
    const AtomicString& type,
    const SpeechSynthesisErrorEventInit* init) {
  return MakeGarbageCollected<SpeechSynthesisErrorEvent>(type, init);
}

SpeechSynthesisErrorEvent::SpeechSynthesisErrorEvent(
    const AtomicString& type,
    const SpeechSynthesisErrorEventInit* init)
    : SpeechSynthesisEvent(type,
                           init->utterance(),
                           init->charIndex(),
                           init->charLength(),
                           init->elapsedTime(),
                           init->name()),
      error_(init->error()) {}

}  // namespace blink
