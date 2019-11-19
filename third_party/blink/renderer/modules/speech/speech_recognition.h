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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/speech/speech_recognizer.mojom-blink.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/speech/speech_grammar_list.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_result.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class LocalFrame;
class SpeechRecognitionController;

class MODULES_EXPORT SpeechRecognition final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<SpeechRecognition>,
      public ContextLifecycleObserver,
      public mojom::blink::SpeechRecognitionSessionClient,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(SpeechRecognition);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SpeechRecognition* Create(ExecutionContext*);

  SpeechRecognition(LocalFrame*, ExecutionContext*);
  ~SpeechRecognition() override;

  // SpeechRecognition.idl implemementation.
  // Attributes.
  SpeechGrammarList* grammars() { return grammars_; }
  void setGrammars(SpeechGrammarList* grammars) { grammars_ = grammars; }
  String lang() { return lang_; }
  void setLang(const String& lang) { lang_ = lang; }
  bool continuous() { return continuous_; }
  void setContinuous(bool continuous) { continuous_ = continuous; }
  bool interimResults() { return interim_results_; }
  void setInterimResults(bool interim_results) {
    interim_results_ = interim_results;
  }
  unsigned maxAlternatives() { return max_alternatives_; }
  void setMaxAlternatives(unsigned max_alternatives) {
    max_alternatives_ = max_alternatives;
  }

  // Callable by the user.
  void start(ExceptionState&);
  void stopFunction();
  void abort();

  // mojom::blink::SpeechRecognitionSessionClient
  void ResultRetrieved(
      WTF::Vector<mojom::blink::SpeechRecognitionResultPtr> results) override;
  void ErrorOccurred(mojom::blink::SpeechRecognitionErrorPtr error) override;
  void Started() override;
  void AudioStarted() override;
  void SoundStarted() override;
  void SoundEnded() override;
  void AudioEnded() override;
  void Ended() override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // PageVisibilityObserver
  void PageVisibilityChanged() override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(audiostart, kAudiostart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(soundstart, kSoundstart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(speechstart, kSpeechstart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(speechend, kSpeechend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(soundend, kSoundend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(audioend, kAudioend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(result, kResult)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(nomatch, kNomatch)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(start, kStart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end, kEnd)

  void Trace(blink::Visitor*) override;

 private:
  void OnConnectionError();

  Member<SpeechGrammarList> grammars_;
  String lang_;
  bool continuous_;
  bool interim_results_;
  uint32_t max_alternatives_;

  Member<SpeechRecognitionController> controller_;
  bool started_;
  bool stopping_;
  HeapVector<Member<SpeechRecognitionResult>> final_results_;
  mojo::Receiver<mojom::blink::SpeechRecognitionSessionClient> receiver_;
  mojo::Remote<mojom::blink::SpeechRecognitionSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_H_
