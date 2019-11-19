/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/speech/dom_window_speech_synthesis.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

DOMWindowSpeechSynthesis::DOMWindowSpeechSynthesis(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

const char DOMWindowSpeechSynthesis::kSupplementName[] =
    "DOMWindowSpeechSynthesis";

// static
DOMWindowSpeechSynthesis& DOMWindowSpeechSynthesis::From(
    LocalDOMWindow& window) {
  DOMWindowSpeechSynthesis* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowSpeechSynthesis>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowSpeechSynthesis>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
SpeechSynthesis* DOMWindowSpeechSynthesis::speechSynthesis(
    ScriptState* script_state,
    LocalDOMWindow& window) {
  return DOMWindowSpeechSynthesis::From(window).speechSynthesis(script_state);
}

void DOMWindowSpeechSynthesis::Trace(blink::Visitor* visitor) {
  visitor->Trace(speech_synthesis_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void DOMWindowSpeechSynthesis::SetSpeechSynthesisForTesting(
    SpeechSynthesis* synthesis) {
  speech_synthesis_ = synthesis;
}

SpeechSynthesis* DOMWindowSpeechSynthesis::speechSynthesis(
    ScriptState* script_state) {
  if (!speech_synthesis_) {
    speech_synthesis_ =
        SpeechSynthesis::Create(ExecutionContext::From(script_state));
  }
  return speech_synthesis_;
}

}  // namespace blink
