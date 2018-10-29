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

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event_init.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_event.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_event_init.h"
#include "third_party/blink/renderer/platform/speech/platform_speech_synthesis_voice.h"

namespace blink {

SpeechSynthesis* SpeechSynthesis::Create(ExecutionContext* context) {
  return new SpeechSynthesis(context);
}

SpeechSynthesis::SpeechSynthesis(ExecutionContext* context)
    : ContextClient(context),
      platform_speech_synthesizer_(PlatformSpeechSynthesizer::Create(this)),
      is_paused_(false) {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsDocument());
}

void SpeechSynthesis::SetPlatformSynthesizer(
    PlatformSpeechSynthesizer* synthesizer) {
  platform_speech_synthesizer_ = synthesizer;
}

void SpeechSynthesis::VoicesDidChange() {
  voice_list_.clear();
  if (GetExecutionContext())
    DispatchEvent(*Event::Create(EventTypeNames::voiceschanged));
}

const HeapVector<Member<SpeechSynthesisVoice>>& SpeechSynthesis::getVoices() {
  if (voice_list_.size())
    return voice_list_;

  // If the voiceList is empty, that's the cue to get the voices from the
  // platform again.
  const Vector<scoped_refptr<PlatformSpeechSynthesisVoice>>& platform_voices =
      platform_speech_synthesizer_->GetVoiceList();
  for (auto voice : platform_voices)
    voice_list_.push_back(SpeechSynthesisVoice::Create(voice));

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

void SpeechSynthesis::StartSpeakingImmediately() {
  SpeechSynthesisUtterance* utterance = CurrentSpeechUtterance();
  DCHECK(utterance);

  double millis;
  if (!GetElapsedTimeMillis(&millis))
    return;

  utterance->SetStartTime(millis / 1000.0);
  is_paused_ = false;
  platform_speech_synthesizer_->Speak(utterance->PlatformUtterance());
}

void SpeechSynthesis::speak(SpeechSynthesisUtterance* utterance) {
  DCHECK(utterance);
  Document* document = To<Document>(GetExecutionContext());
  if (!document)
    return;

  // Note: Non-UseCounter based TTS metrics are of the form TextToSpeech.* and
  // are generally global, whereas these are scoped to a single page load.
  UseCounter::Count(document, WebFeature::kTextToSpeech_Speak);
  UseCounter::CountCrossOriginIframe(
      *document, WebFeature::kTextToSpeech_SpeakCrossOrigin);
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
  platform_speech_synthesizer_->Cancel();
}

void SpeechSynthesis::pause() {
  if (!is_paused_)
    platform_speech_synthesizer_->Pause();
}

void SpeechSynthesis::resume() {
  if (!CurrentSpeechUtterance())
    return;
  platform_speech_synthesizer_->Resume();
}

void SpeechSynthesis::FireEvent(const AtomicString& type,
                                SpeechSynthesisUtterance* utterance,
                                uint32_t char_index,
                                const String& name) {
  double millis;
  if (!GetElapsedTimeMillis(&millis))
    return;

  SpeechSynthesisEventInit init;
  init.setUtterance(utterance);
  init.setCharIndex(char_index);
  init.setElapsedTime(millis - (utterance->StartTime() * 1000.0));
  init.setName(name);
  utterance->DispatchEvent(*SpeechSynthesisEvent::Create(type, init));
}

void SpeechSynthesis::FireErrorEvent(SpeechSynthesisUtterance* utterance,
                                     unsigned long char_index,
                                     const String& error) {
  double millis;
  if (!GetElapsedTimeMillis(&millis))
    return;

  SpeechSynthesisErrorEventInit init;
  init.setUtterance(utterance);
  init.setCharIndex(char_index);
  init.setElapsedTime(millis - (utterance->StartTime() * 1000.0));
  init.setError(error);
  utterance->DispatchEvent(
      *SpeechSynthesisErrorEvent::Create(EventTypeNames::error, init));
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
    should_start_speaking = !!utterance_queue_.size();
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
    FireEvent(EventTypeNames::end, utterance, 0, String());
  }

  // Start the next utterance if we just finished one and one was pending.
  if (should_start_speaking && !utterance_queue_.IsEmpty())
    StartSpeakingImmediately();
}

void SpeechSynthesis::BoundaryEventOccurred(
    PlatformSpeechSynthesisUtterance* utterance,
    SpeechBoundary boundary,
    unsigned char_index) {
  DEFINE_STATIC_LOCAL(const String, word_boundary_string, ("word"));
  DEFINE_STATIC_LOCAL(const String, sentence_boundary_string, ("sentence"));

  switch (boundary) {
    case kSpeechWordBoundary:
      FireEvent(EventTypeNames::boundary,
                static_cast<SpeechSynthesisUtterance*>(utterance->Client()),
                char_index, word_boundary_string);
      break;
    case kSpeechSentenceBoundary:
      FireEvent(EventTypeNames::boundary,
                static_cast<SpeechSynthesisUtterance*>(utterance->Client()),
                char_index, sentence_boundary_string);
      break;
    default:
      NOTREACHED();
  }
}

void SpeechSynthesis::DidStartSpeaking(
    PlatformSpeechSynthesisUtterance* utterance) {
  if (utterance->Client())
    FireEvent(EventTypeNames::start,
              static_cast<SpeechSynthesisUtterance*>(utterance->Client()), 0,
              String());
}

void SpeechSynthesis::DidPauseSpeaking(
    PlatformSpeechSynthesisUtterance* utterance) {
  is_paused_ = true;
  if (utterance->Client())
    FireEvent(EventTypeNames::pause,
              static_cast<SpeechSynthesisUtterance*>(utterance->Client()), 0,
              String());
}

void SpeechSynthesis::DidResumeSpeaking(
    PlatformSpeechSynthesisUtterance* utterance) {
  is_paused_ = false;
  if (utterance->Client())
    FireEvent(EventTypeNames::resume,
              static_cast<SpeechSynthesisUtterance*>(utterance->Client()), 0,
              String());
}

void SpeechSynthesis::DidFinishSpeaking(
    PlatformSpeechSynthesisUtterance* utterance) {
  if (utterance->Client())
    HandleSpeakingCompleted(
        static_cast<SpeechSynthesisUtterance*>(utterance->Client()), false);
}

void SpeechSynthesis::SpeakingErrorOccurred(
    PlatformSpeechSynthesisUtterance* utterance) {
  if (utterance->Client())
    HandleSpeakingCompleted(
        static_cast<SpeechSynthesisUtterance*>(utterance->Client()), true);
}

SpeechSynthesisUtterance* SpeechSynthesis::CurrentSpeechUtterance() const {
  if (utterance_queue_.IsEmpty())
    return nullptr;

  return utterance_queue_.front();
}

const AtomicString& SpeechSynthesis::InterfaceName() const {
  return EventTargetNames::SpeechSynthesis;
}

void SpeechSynthesis::Trace(blink::Visitor* visitor) {
  visitor->Trace(platform_speech_synthesizer_);
  visitor->Trace(voice_list_);
  visitor->Trace(utterance_queue_);
  PlatformSpeechSynthesizerClient::Trace(visitor);
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

}  // namespace blink
