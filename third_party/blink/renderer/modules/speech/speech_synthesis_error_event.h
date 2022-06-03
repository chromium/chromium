// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_SYNTHESIS_ERROR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_SYNTHESIS_ERROR_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_speech_synthesis_error_event_init.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_event.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SpeechSynthesisErrorEvent : public SpeechSynthesisEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SpeechSynthesisErrorEvent* Create(
      const AtomicString& type,
      const SpeechSynthesisErrorEventInit* init);

  SpeechSynthesisErrorEvent(const AtomicString& type,
                            const SpeechSynthesisErrorEventInit* init);

  const String error() const { return error_; }

 private:
  const String error_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_SYNTHESIS_ERROR_EVENT_H_
