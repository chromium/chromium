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

#include "media/mojo/mojom/speech_recognizer.mojom-blink.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/speech/speech_grammar_list.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_result.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class LocalDOMWindow;
class MediaStreamTrack;
class SpeechRecognitionController;
class SpeechRecognitionMediaStreamAudioSink;

class MODULES_EXPORT SpeechRecognition final
    : public EventTarget,
      public ActiveScriptWrappable<SpeechRecognition>,
      public ExecutionContextLifecycleObserver,
      public media::mojom::blink::SpeechRecognitionSessionClient,
      public PageVisibilityObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SpeechRecognition* Create(ExecutionContext*);

  SpeechRecognition(LocalDOMWindow*);
  ~SpeechRecognition() override;

  // SpeechRecognition.idl implemementation.
  // Attributes.
  SpeechGrammarList* grammars() const { return grammars_.Get(); }
  void setGrammars(SpeechGrammarList* grammars) { grammars_ = grammars; }
  String lang() const { return lang_; }
  void setLang(const String& lang) { lang_ = lang; }
  bool continuous() const { return continuous_; }
  void setContinuous(bool continuous) { continuous_ = continuous; }
  bool interimResults() const { return interim_results_; }
  void setInterimResults(bool interim_results) {
    interim_results_ = interim_results;
  }
  unsigned maxAlternatives() const { return max_alternatives_; }
  void setMaxAlternatives(unsigned max_alternatives) {
    max_alternatives_ = max_alternatives;
  }
  void setLocalService(bool local_service) { local_service_ = local_service; }
  bool localService() const { return local_service_; }
  void setAllowCloudFallback(bool allow_cloud_fallback) {
    allow_cloud_fallback_ = allow_cloud_fallback;
  }
  bool allowCloudFallback() const { return allow_cloud_fallback_; }

  // Callable by the user. Methods may be called after the execution context is
  // destroyed.
  void start(ExceptionState&);
  void start(MediaStreamTrack*, ExceptionState&);
  void stopFunction();
  void abort();
  ScriptPromise<IDLBoolean> onDeviceWebSpeechAvailable(ScriptState*,
                                                       const String& lang,
                                                       ExceptionState&);
  ScriptPromise<IDLBoolean> installOnDeviceSpeechRecognition(ScriptState*,
                                                             const String& lang,
                                                             ExceptionState&);

  // media::mojom::blink::SpeechRecognitionSessionClient
  void ResultRetrieved(
      WTF::Vector<media::mojom::blink::WebSpeechRecognitionResultPtr> results)
      override;
  void ErrorOccurred(
      media::mojom::blink::SpeechRecognitionErrorPtr error) override;
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

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

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

  void Trace(Visitor*) const override;

 private:
  void OnConnectionError();
  void StartInternal(ExceptionState* exception_state);
  Member<MediaStreamTrack> stream_track_;
  Member<SpeechGrammarList> grammars_;
  String lang_;
  bool continuous_ = false;
  bool interim_results_ = false;
  uint32_t max_alternatives_ = 1;
  bool local_service_ = true;
  bool allow_cloud_fallback_ = true;

  Member<SpeechRecognitionMediaStreamAudioSink> sink_;
  Member<SpeechRecognitionController> controller_;
  bool started_ = false;
  bool stopping_ = false;
  HeapVector<Member<SpeechRecognitionResult>> final_results_;
  HeapMojoReceiver<media::mojom::blink::SpeechRecognitionSessionClient,
                   SpeechRecognition>
      receiver_;
  HeapMojoRemote<media::mojom::blink::SpeechRecognitionSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_H_
