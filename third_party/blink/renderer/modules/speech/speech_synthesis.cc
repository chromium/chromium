/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/speech/speech_synthesis.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event_init.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_event.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_event_init.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

SpeechSynthesis* SpeechSynthesis::Create(ExecutionContext* context) {
  SpeechSynthesis* synthesis = MakeGarbageCollected<SpeechSynthesis>(context);
#if defined(OS_ANDROID)
  // On Android devices we lazily initialize |mojom_synthesis_| to avoid
  // needlessly binding to the TTS service, see https://crbug.com/811929.
  // TODO(crbug/811929): Consider moving this logic into the Android-
  // specific backend implementation.
#else
  synthesis->InitializeMojomSynthesis();
#endif
  return synthesis;
}

SpeechSynthesis* SpeechSynthesis::CreateForTesting(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::SpeechSynthesis> mojom_synthesis) {
  SpeechSynthesis* synthesis = MakeGarbageCollected<SpeechSynthesis>(context);
  synthesis->SetMojomSynthesisForTesting(std::move(mojom_synthesis));
  return synthesis;
}

SpeechSynthesis::SpeechSynthesis(ExecutionContext* context)
    : ContextClient(context) {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsDocument());
}

void SpeechSynthesis::OnSetVoiceList(
    Vector<mojom::blink::SpeechSynthesisVoicePtr> mojom_voices) {
  voice_list_.clear();
  for (auto& mojom_voice : mojom_voices)
    voice_list_.push_back(SpeechSynthesisVoice::Create(std::move(mojom_voice)));
  VoicesDidChange();
}

const HeapVector<Member<SpeechSynthesisVoice>>& SpeechSynthesis::getVoices() {
  // Kick off initialization here to ensure voice list gets populated.
  InitializeMojomSynthesisIfNeeded();
  return voice_list_;
}

bool SpeechSynthesis::speaking() const {
  // If we have a current speech utterance, then that means we're assumed to be
  // in a speaking state. This state is independent of whether the utterance
  // happens to be paused.
  return CurrentSpeechUtterance();
}

bool SpeechSynthesis::pending() const {
  // This is true if there are any utterances that have not started.
  // That means there will be more than one in the queue.
  return utterance_queue_.size() > 1;
}

bool SpeechSynthesis::paused() const {
  return is_paused_;
}

void SpeechSynthesis::speak(SpeechSynthesisUtterance* utterance) {
  DCHECK(utterance);
  Document* document = To<Document>(GetExecutionContext());
  if (!document)
    return;

  // Note: Non-UseCounter based TTS metrics are of the form TextToSpeech.* and
  // are generally global, whereas these are scoped to a single page load.
  UseCounter::Count(document, WebFeature::kTextToSpeech_Speak);
  document->CountUseOnlyInCrossOriginIframe(
      WebFeature::kTextToSpeech_SpeakCrossOrigin);
  if (!IsAllowedToStartByAutoplay()) {
    Deprecation::CountDeprecation(
        document, WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay);
    FireErrorEvent(utterance, 0 /* char_index */, "not-allowed");
    return;
  }

  utterance_queue_.push_back(utterance);

  // If the queue was empty, speak this immediately.
  if (utterance_queue_.size() == 1)
    StartSpeakingImmediately();
}

void SpeechSynthesis::cancel() {
  // Remove all the items from the utterance queue. The platform
  // may still have references to some of these utterances and may
  // fire events on them asynchronously.
  utterance_queue_.clear();

  InitializeMojomSynthesisIfNeeded();
  mojom_synthesis_->Cancel();
}

void SpeechSynthesis::pause() {
  if (is_paused_)
    return;

  InitializeMojomSynthesisIfNeeded();
  mojom_synthesis_->Pause();
}

void SpeechSynthesis::resume() {
  if (!CurrentSpeechUtterance())
    return;

  InitializeMojomSynthesisIfNeeded();
  mojom_synthesis_->Resume();
}

void SpeechSynthesis::DidStartSpeaking(SpeechSynthesisUtterance* utterance) {
  FireEvent(event_type_names::kStart, utterance, 0, 0, String());
}

void SpeechSynthesis::DidPauseSpeaking(SpeechSynthesisUtterance* utterance) {
  is_paused_ = true;
  FireEvent(event_type_names::kPause, utterance, 0, 0, String());
}

void SpeechSynthesis::DidResumeSpeaking(SpeechSynthesisUtterance* utterance) {
  is_paused_ = false;
  FireEvent(event_type_names::kResume, utterance, 0, 0, String());
}

void SpeechSynthesis::DidFinishSpeaking(SpeechSynthesisUtterance* utterance) {
  HandleSpeakingCompleted(utterance, false);
}

void SpeechSynthesis::SpeakingErrorOccurred(
    SpeechSynthesisUtterance* utterance) {
  HandleSpeakingCompleted(utterance, true);
}

void SpeechSynthesis::WordBoundaryEventOccurred(
    SpeechSynthesisUtterance* utterance,
    unsigned char_index,
    unsigned char_length) {
  DEFINE_STATIC_LOCAL(const String, word_boundary_string, ("word"));
  FireEvent(event_type_names::kBoundary, utterance, char_index, char_length,
            word_boundary_string);
}

void SpeechSynthesis::SentenceBoundaryEventOccurred(
    SpeechSynthesisUtterance* utterance,
    unsigned char_index,
    unsigned char_length) {
  DEFINE_STATIC_LOCAL(const String, sentence_boundary_string, ("sentence"));
  FireEvent(event_type_names::kBoundary, utterance, char_index, char_length,
            sentence_boundary_string);
}

void SpeechSynthesis::VoicesDidChange() {
  if (GetExecutionContext())
    DispatchEvent(*Event::Create(event_type_names::kVoiceschanged));
}

void SpeechSynthesis::StartSpeakingImmediately() {
  SpeechSynthesisUtterance* utterance = CurrentSpeechUtterance();
  DCHECK(utterance);

  double millis;
  if (!GetElapsedTimeMillis(&millis))
    return;

  utterance->SetStartTime(millis / 1000.0);
  is_paused_ = false;

  InitializeMojomSynthesisIfNeeded();
  utterance->Start(this);
}

void SpeechSynthesis::HandleSpeakingCompleted(
    SpeechSynthesisUtterance* utterance,
    bool error_occurred) {
  DCHECK(utterance);

  bool should_start_speaking = false;
  // If the utterance that completed was the one we're currently speaking,
  // remove it from the queue and start speaking the next one.
  if (utterance == CurrentSpeechUtterance()) {
    utterance_queue_.pop_front();
    should_start_speaking = !utterance_queue_.empty();
  }

  // Always fire the event, because the platform may have asynchronously
  // sent an event on an utterance before it got the message that we
  // canceled it, and we should always report to the user what actually
  // happened.
  if (error_occurred) {
    // TODO(csharrison): Actually pass the correct message. For now just use a
    // generic error.
    FireErrorEvent(utterance, 0, "synthesis-failed");
  } else {
    FireEvent(event_type_names::kEnd, utterance, 0, 0, String());
  }

  // Start the next utterance if we just finished one and one was pending.
  if (should_start_speaking && !utterance_queue_.IsEmpty())
    StartSpeakingImmediately();
}

void SpeechSynthesis::FireEvent(const AtomicString& type,
                                SpeechSynthesisUtterance* utterance,
                                uint32_t char_index,
                                uint32_t char_length,
                                const String& name) {
  double millis;
  if (!GetElapsedTimeMillis(&millis))
    return;

  SpeechSynthesisEventInit* init = SpeechSynthesisEventInit::Create();
  init->setUtterance(utterance);
  init->setCharIndex(char_index);
  init->setCharLength(char_length);
  init->setElapsedTime(millis - (utterance->StartTime() * 1000.0));
  init->setName(name);
  utterance->DispatchEvent(*SpeechSynthesisEvent::Create(type, init));
}

void SpeechSynthesis::FireErrorEvent(SpeechSynthesisUtterance* utterance,
                                     uint32_t char_index,
                                     const String& error) {
  double millis;
  if (!GetElapsedTimeMillis(&millis))
    return;

  SpeechSynthesisErrorEventInit* init = SpeechSynthesisErrorEventInit::Create();
  init->setUtterance(utterance);
  init->setCharIndex(char_index);
  init->setElapsedTime(millis - (utterance->StartTime() * 1000.0));
  init->setError(error);
  utterance->DispatchEvent(
      *SpeechSynthesisErrorEvent::Create(event_type_names::kError, init));
}

SpeechSynthesisUtterance* SpeechSynthesis::CurrentSpeechUtterance() const {
  if (utterance_queue_.IsEmpty())
    return nullptr;

  return utterance_queue_.front();
}

void SpeechSynthesis::Trace(blink::Visitor* visitor) {
  visitor->Trace(voice_list_);
  visitor->Trace(utterance_queue_);
  ContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

bool SpeechSynthesis::GetElapsedTimeMillis(double* millis) {
  if (!GetExecutionContext())
    return false;
  Document* delegate_document = To<Document>(GetExecutionContext());
  if (!delegate_document || delegate_document->IsStopped())
    return false;
  LocalDOMWindow* delegate_dom_window = delegate_document->domWindow();
  if (!delegate_dom_window)
    return false;

  *millis = DOMWindowPerformance::performance(*delegate_dom_window)->now();
  return true;
}

bool SpeechSynthesis::IsAllowedToStartByAutoplay() const {
  Document* document = To<Document>(GetExecutionContext());
  DCHECK(document);

  // Note: could check the utterance->volume here, but that could be overriden
  // in the case of SSML.
  if (AutoplayPolicy::GetAutoplayPolicyForDocument(*document) !=
      AutoplayPolicy::Type::kDocumentUserActivationRequired) {
    return true;
  }
  return AutoplayPolicy::IsDocumentAllowedToPlay(*document);
}

void SpeechSynthesis::SetMojomSynthesisForTesting(
    mojo::PendingRemote<mojom::blink::SpeechSynthesis> mojom_synthesis) {
  mojom_synthesis_.Bind(std::move(mojom_synthesis));
  receiver_.reset();
  mojom_synthesis_->AddVoiceListObserver(receiver_.BindNewPipeAndPassRemote());
}

void SpeechSynthesis::InitializeMojomSynthesis() {
  DCHECK(!mojom_synthesis_);

  auto receiver = mojom_synthesis_.BindNewPipeAndPassReceiver();

  // The frame could be detached. In that case, calls on mojom_synthesis_ will
  // just get dropped. That's okay and is simpler than having to null-check
  // mojom_synthesis_ before each use.
  ExecutionContext* context = GetExecutionContext();
  if (context) {
    context->GetBrowserInterfaceBroker().GetInterface(std::move(receiver));
  }

  mojom_synthesis_->AddVoiceListObserver(receiver_.BindNewPipeAndPassRemote());
}

void SpeechSynthesis::InitializeMojomSynthesisIfNeeded() {
  if (!mojom_synthesis_)
    InitializeMojomSynthesis();
}

const AtomicString& SpeechSynthesis::InterfaceName() const {
  return event_target_names::kSpeechSynthesis;
}

}  // namespace blink
