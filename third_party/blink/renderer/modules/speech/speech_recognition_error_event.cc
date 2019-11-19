/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/speech/speech_recognition_error_event.h"

#include "third_party/blink/public/mojom/speech/speech_recognition_error_code.mojom-blink.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

static String ErrorCodeToString(mojom::blink::SpeechRecognitionErrorCode code) {
  switch (code) {
    case mojom::blink::SpeechRecognitionErrorCode::kNone:
      return "other";
    case mojom::blink::SpeechRecognitionErrorCode::kNoSpeech:
      return "no-speech";
    case mojom::blink::SpeechRecognitionErrorCode::kAborted:
      return "aborted";
    case mojom::blink::SpeechRecognitionErrorCode::kAudioCapture:
      return "audio-capture";
    case mojom::blink::SpeechRecognitionErrorCode::kNetwork:
      return "network";
    case mojom::blink::SpeechRecognitionErrorCode::kNotAllowed:
      return "not-allowed";
    case mojom::blink::SpeechRecognitionErrorCode::kServiceNotAllowed:
      return "service-not-allowed";
    case mojom::blink::SpeechRecognitionErrorCode::kBadGrammar:
      return "bad-grammar";
    case mojom::blink::SpeechRecognitionErrorCode::kLanguageNotSupported:
      return "language-not-supported";
    case mojom::blink::SpeechRecognitionErrorCode::kNoMatch:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return String();
}

SpeechRecognitionErrorEvent* SpeechRecognitionErrorEvent::Create(
    mojom::blink::SpeechRecognitionErrorCode code,
    const String& message) {
  return MakeGarbageCollected<SpeechRecognitionErrorEvent>(
      ErrorCodeToString(code), message);
}

SpeechRecognitionErrorEvent* SpeechRecognitionErrorEvent::Create(
    const AtomicString& event_name,
    const SpeechRecognitionErrorEventInit* initializer) {
  return MakeGarbageCollected<SpeechRecognitionErrorEvent>(event_name,
                                                           initializer);
}

SpeechRecognitionErrorEvent::SpeechRecognitionErrorEvent(const String& error,
                                                         const String& message)
    : Event(event_type_names::kError, Bubbles::kNo, Cancelable::kNo),
      error_(error),
      message_(message) {}

SpeechRecognitionErrorEvent::SpeechRecognitionErrorEvent(
    const AtomicString& event_name,
    const SpeechRecognitionErrorEventInit* initializer)
    : Event(event_name, initializer) {
  if (initializer->hasError())
    error_ = initializer->error();
  if (initializer->hasMessage())
    message_ = initializer->message();
}

const AtomicString& SpeechRecognitionErrorEvent::InterfaceName() const {
  return event_interface_names::kSpeechRecognitionErrorEvent;
}

}  // namespace blink
