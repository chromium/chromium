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

#include "third_party/blink/renderer/modules/speech/speech_recognition.h"

#include <algorithm>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom-blink.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_controller.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_error_event.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SpeechRecognition* SpeechRecognition::Create(ExecutionContext* context) {
  return MakeGarbageCollected<SpeechRecognition>(To<LocalDOMWindow>(context));
}

void SpeechRecognition::start(ExceptionState& exception_state) {
  // https://wicg.github.io/nav-speculation/prerendering.html#web-speech-patch
  // If this is called in prerendering, it should be deferred.
  if (DomWindow() && DomWindow()->document()->IsPrerendering()) {
    DomWindow()->document()->AddPostPrerenderingActivationStep(
        WTF::BindOnce(&SpeechRecognition::StartInternal,
                      WrapWeakPersistent(this), /*exception_state=*/nullptr));
    return;
  }
  StartInternal(&exception_state);
}

void SpeechRecognition::stopFunction() {
  // https://wicg.github.io/nav-speculation/prerendering.html#web-speech-patch
  // If this is called in prerendering, it should be deferred.
  if (DomWindow() && DomWindow()->document()->IsPrerendering()) {
    DomWindow()->document()->AddPostPrerenderingActivationStep(WTF::BindOnce(
        &SpeechRecognition::stopFunction, WrapWeakPersistent(this)));
    return;
  }

  if (!controller_)
    return;

  if (started_ && !stopping_) {
    stopping_ = true;
    session_->StopCapture();
  }
}

void SpeechRecognition::abort() {
  // https://wicg.github.io/nav-speculation/prerendering.html#web-speech-patch
  // If this is called in prerendering, it should be deferred.
  if (DomWindow() && DomWindow()->document()->IsPrerendering()) {
    DomWindow()->document()->AddPostPrerenderingActivationStep(
        WTF::BindOnce(&SpeechRecognition::abort, WrapWeakPersistent(this)));
    return;
  }

  if (!controller_)
    return;

  if (started_ && !stopping_) {
    stopping_ = true;
    session_->Abort();
  }
}

void SpeechRecognition::ResultRetrieved(
    WTF::Vector<mojom::blink::SpeechRecognitionResultPtr> results) {
  auto* it = std::stable_partition(
      results.begin(), results.end(),
      [](const auto& result) { return !result->is_provisional; });
  wtf_size_t provisional_count = static_cast<wtf_size_t>(results.end() - it);

  // Add the new results to the previous final results.
  HeapVector<Member<SpeechRecognitionResult>> aggregated_results =
      std::move(final_results_);
  aggregated_results.reserve(aggregated_results.size() + results.size());

  for (const auto& result : results) {
    HeapVector<Member<SpeechRecognitionAlternative>> alternatives;
    alternatives.ReserveInitialCapacity(result->hypotheses.size());
    for (const auto& hypothesis : result->hypotheses) {
      alternatives.push_back(MakeGarbageCollected<SpeechRecognitionAlternative>(
          hypothesis->utterance, hypothesis->confidence));
    }
    aggregated_results.push_back(SpeechRecognitionResult::Create(
        std::move(alternatives), !result->is_provisional));
  }

  // |aggregated_results| now contains the following (in the given order):
  //
  // (1) previous final results from |final_results_|
  // (2) new final results from |results|
  // (3) new provisional results from |results|

  // |final_results_| = (1) + (2).
  HeapVector<Member<SpeechRecognitionResult>> new_final_results;
  new_final_results.ReserveInitialCapacity(aggregated_results.size() -
                                           provisional_count);
  new_final_results.AppendRange(aggregated_results.begin(),
                                aggregated_results.end() - provisional_count);
  final_results_ = std::move(new_final_results);

  // We dispatch an event with (1) + (2) + (3).
  DispatchEvent(*SpeechRecognitionEvent::CreateResult(
      aggregated_results.size() - results.size(),
      std::move(aggregated_results)));
}

void SpeechRecognition::ErrorOccurred(
    mojom::blink::SpeechRecognitionErrorPtr error) {
  if (error->code == mojom::blink::SpeechRecognitionErrorCode::kNoMatch) {
    DispatchEvent(*SpeechRecognitionEvent::CreateNoMatch(nullptr));
  } else {
    // TODO(primiano): message?
    DispatchEvent(*SpeechRecognitionErrorEvent::Create(error->code, String()));
  }
}

void SpeechRecognition::Started() {
  DispatchEvent(*Event::Create(event_type_names::kStart));
}

void SpeechRecognition::AudioStarted() {
  DispatchEvent(*Event::Create(event_type_names::kAudiostart));
}

void SpeechRecognition::SoundStarted() {
  DispatchEvent(*Event::Create(event_type_names::kSoundstart));
  DispatchEvent(*Event::Create(event_type_names::kSpeechstart));
}

void SpeechRecognition::SoundEnded() {
  DispatchEvent(*Event::Create(event_type_names::kSpeechend));
  DispatchEvent(*Event::Create(event_type_names::kSoundend));
}

void SpeechRecognition::AudioEnded() {
  DispatchEvent(*Event::Create(event_type_names::kAudioend));
}

void SpeechRecognition::Ended() {
  started_ = false;
  stopping_ = false;
  session_.reset();
  receiver_.reset();
  DispatchEvent(*Event::Create(event_type_names::kEnd));
}

const AtomicString& SpeechRecognition::InterfaceName() const {
  return event_target_names::kSpeechRecognition;
}

ExecutionContext* SpeechRecognition::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void SpeechRecognition::ContextDestroyed() {
  controller_ = nullptr;
}

bool SpeechRecognition::HasPendingActivity() const {
  return started_;
}

void SpeechRecognition::PageVisibilityChanged() {
#if BUILDFLAG(IS_ANDROID)
  if (!GetPage()->IsPageVisible())
    abort();
#endif
}

void SpeechRecognition::OnConnectionError() {
  ErrorOccurred(mojom::blink::SpeechRecognitionError::New(
      mojom::blink::SpeechRecognitionErrorCode::kNetwork,
      mojom::blink::SpeechAudioErrorDetails::kNone));
  Ended();
}

void SpeechRecognition::StartInternal(ExceptionState* exception_state) {
  if (!controller_ || !GetExecutionContext())
    return;

  if (started_) {
    // https://wicg.github.io/speech-api/#dom-speechrecognition-start
    // The spec says that if the start method is called on an already started
    // object (that is, start has previously been called, and no error or end
    // event has fired on the object), the user agent must throw an
    // "InvalidStateError" DOMException and ignore the call. But, if it's called
    // after prerendering activation, `exception_state` is null since it's
    // STACK_ALLOCATED and it can't be passed.
    if (exception_state) {
      exception_state->ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                         "recognition has already started.");
    }
    return;
  }
  final_results_.clear();

  mojo::PendingRemote<mojom::blink::SpeechRecognitionSessionClient>
      session_client;
  // See https://bit.ly/2S0zRAS for task types.
  receiver_.Bind(
      session_client.InitWithNewPipeAndPassReceiver(),
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &SpeechRecognition::OnConnectionError, WrapWeakPersistent(this)));

  controller_->Start(
      session_.BindNewPipeAndPassReceiver(
          GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      std::move(session_client), *grammars_, lang_, continuous_,
      interim_results_, max_alternatives_);
  started_ = true;
}

SpeechRecognition::SpeechRecognition(LocalDOMWindow* window)
    : ActiveScriptWrappable<SpeechRecognition>({}),
      ExecutionContextLifecycleObserver(window),
      PageVisibilityObserver(window->GetFrame() ? window->GetFrame()->GetPage()
                                                : nullptr),
      grammars_(SpeechGrammarList::Create()),  // FIXME: The spec is not clear
                                               // on the default value for the
                                               // grammars attribute.
      continuous_(false),
      interim_results_(false),
      max_alternatives_(1),
      controller_(SpeechRecognitionController::From(*window)),
      started_(false),
      stopping_(false),
      receiver_(this, window),
      session_(window) {}

SpeechRecognition::~SpeechRecognition() = default;

void SpeechRecognition::Trace(Visitor* visitor) const {
  visitor->Trace(grammars_);
  visitor->Trace(controller_);
  visitor->Trace(final_results_);
  visitor->Trace(receiver_);
  visitor->Trace(session_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

}  // namespace blink
