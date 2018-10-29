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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_controller.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_error.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

SpeechRecognition* SpeechRecognition::Create(ExecutionContext* context) {
  Document& document = To<Document>(*context);
  return new SpeechRecognition(document.GetFrame(), context);
}

void SpeechRecognition::start(ExceptionState& exception_state) {
  if (!controller_)
    return;

  if (started_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "recognition has already started.");
    return;
  }

  final_results_.clear();

  mojom::blink::SpeechRecognitionSessionClientPtrInfo session_client;
  binding_.Bind(mojo::MakeRequest(&session_client),
                GetExecutionContext()->GetInterfaceInvalidator());
  binding_.set_connection_error_handler(WTF::Bind(
      &SpeechRecognition::OnConnectionError, WrapWeakPersistent(this)));

  mojom::blink::SpeechRecognitionSessionRequest session_request =
      MakeRequest(&session_, GetExecutionContext()->GetInterfaceInvalidator());

  controller_->Start(std::move(session_request), std::move(session_client),
                     *grammars_, lang_, continuous_, interim_results_,
                     max_alternatives_);
  started_ = true;
}

void SpeechRecognition::stopFunction() {
  if (!controller_)
    return;

  if (started_ && !stopping_) {
    stopping_ = true;
    session_->StopCapture();
  }
}

void SpeechRecognition::abort() {
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
  aggregated_results.ReserveCapacity(aggregated_results.size() +
                                     results.size());

  for (const auto& result : results) {
    HeapVector<Member<SpeechRecognitionAlternative>> alternatives;
    alternatives.ReserveInitialCapacity(result->hypotheses.size());
    for (const auto& hypothesis : result->hypotheses) {
      alternatives.push_back(SpeechRecognitionAlternative::Create(
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
    DispatchEvent(*SpeechRecognitionError::Create(error->code, String()));
  }
}

void SpeechRecognition::Started() {
  DispatchEvent(*Event::Create(EventTypeNames::start));
}

void SpeechRecognition::AudioStarted() {
  DispatchEvent(*Event::Create(EventTypeNames::audiostart));
}

void SpeechRecognition::SoundStarted() {
  DispatchEvent(*Event::Create(EventTypeNames::soundstart));
  DispatchEvent(*Event::Create(EventTypeNames::speechstart));
}

void SpeechRecognition::SoundEnded() {
  DispatchEvent(*Event::Create(EventTypeNames::speechend));
  DispatchEvent(*Event::Create(EventTypeNames::soundend));
}

void SpeechRecognition::AudioEnded() {
  DispatchEvent(*Event::Create(EventTypeNames::audioend));
}

void SpeechRecognition::Ended() {
  started_ = false;
  stopping_ = false;
  session_.reset();
  binding_.Close();
  DispatchEvent(*Event::Create(EventTypeNames::end));
}

const AtomicString& SpeechRecognition::InterfaceName() const {
  return EventTargetNames::SpeechRecognition;
}

ExecutionContext* SpeechRecognition::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void SpeechRecognition::ContextDestroyed(ExecutionContext*) {
  controller_ = nullptr;
}

bool SpeechRecognition::HasPendingActivity() const {
  return started_;
}

void SpeechRecognition::PageVisibilityChanged() {
#if defined(OS_ANDROID)
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

SpeechRecognition::SpeechRecognition(LocalFrame* frame,
                                     ExecutionContext* context)
    : ContextLifecycleObserver(context),
      PageVisibilityObserver(frame ? frame->GetPage() : nullptr),
      grammars_(SpeechGrammarList::Create()),  // FIXME: The spec is not clear
                                               // on the default value for the
                                               // grammars attribute.
      continuous_(false),
      interim_results_(false),
      max_alternatives_(1),
      controller_(SpeechRecognitionController::From(frame)),
      started_(false),
      stopping_(false),
      binding_(this) {}

SpeechRecognition::~SpeechRecognition() = default;

void SpeechRecognition::Trace(blink::Visitor* visitor) {
  visitor->Trace(grammars_);
  visitor->Trace(controller_);
  visitor->Trace(final_results_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

}  // namespace blink
