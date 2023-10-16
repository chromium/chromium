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

#include "third_party/blink/renderer/modules/speech/speech_grammar_list.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

SpeechGrammarList* SpeechGrammarList::Create() {
  return MakeGarbageCollected<SpeechGrammarList>();
}

SpeechGrammar* SpeechGrammarList::item(unsigned index) const {
  if (index >= grammars_.size())
    return nullptr;

  return grammars_[index].Get();
}

void SpeechGrammarList::addFromUri(ScriptState* script_state,
                                   const String& src,
                                   double weight) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  grammars_.push_back(SpeechGrammar::Create(context->CompleteURL(src), weight));
}

void SpeechGrammarList::addFromString(const String& string, double weight) {
  String url_string =
      String("data:application/xml,") + EncodeWithURLEscapeSequences(string);
  grammars_.push_back(
      SpeechGrammar::Create(KURL(NullURL(), url_string), weight));
}

SpeechGrammarList::SpeechGrammarList() = default;

void SpeechGrammarList::Trace(Visitor* visitor) const {
  visitor->Trace(grammars_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
