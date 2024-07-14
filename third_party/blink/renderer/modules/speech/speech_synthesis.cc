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

#include <tuple>

#include "build/build_config.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_speech_synthesis_error_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_speech_synthesis_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_error_event.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_event.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_voice.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

const char SpeechSynthesis::kSupplementName[] = "SpeechSynthesis";

SpeechSynthesisBase* SpeechSynthesis::Create(LocalDOMWindow& window) {
  return MakeGarbageCollected<SpeechSynthesis>(window);
}

SpeechSynthesis* SpeechSynthesis::speechSynthesis(LocalDOMWindow& window) {
  SpeechSynthesis* synthesis =
      Supplement<LocalDOMWindow>::From<SpeechSynthesis>(window);
  if (!synthesis) {
    synthesis = MakeGarbageCollected<SpeechSynthesis>(window);
    ProvideTo(window, synthesis);
#if BUILDFLAG(IS_ANDROID)
    // On Android devices we lazily initialize |mojom_synthesis_| to avoid
    // needlessly binding to the TTS service, see https://crbug.com/811929.
    // TODO(crbug/811929): Consider moving this logic into the Android-
    // specific backend implementation.
#else
    std::ignore = synthesis->TryEnsureMojomSynthesis();
#endif
  }
  return synthesis;
}

void SpeechSynthesis::CreateForTesting(
    LocalDOMWindow& window,
    mojo::PendingRemote<mojom::blink::SpeechSynthesis> mojom_synthesis) {
  DCHECK(!Supplement<LocalDOMWindow>::From<SpeechSynthesis>(window));
  SpeechSynthesis* synthesis = MakeGarbageCollected<SpeechSynthesis>(window);
  ProvideTo(window, synthesis);
  synthesis->SetMojomSynthesisForTesting(std::move(mojom_synthesis));
}

SpeechSynthesis::SpeechSynthesis(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      receiver_(this, &window),
      mojom_synthesis_(&window) {}

void SpeechSynthesis::OnSetVoiceList(
    Vector<mojom::blink::SpeechSynthesisVoicePtr> mojom_voices) {
  voice_list_.clear();
  for (auto& mojom_voice : mojom_voices) {
    voice_list_.push_back(
        MakeGarbageCollected<SpeechSynthesisVoice>(std::move(mojom_voice)));
  }
  VoicesDidChange();
}

const HeapVector<Member<SpeechSynthesisVoice>>& SpeechSynthesis::getVoices() {
  // Kick off initialization here to ensure voice list gets populated.
  std::ignore = TryEnsureMojomSynthesis();
  RecordVoicesForIdentifiability();
  return voice_list_;
}

void SpeechSynthesis::RecordVoicesForIdentifiability() const {
  constexpr IdentifiableSurface surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature,
      WebFeature::kSpeechSynthesis_GetVoices_Method);
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleSurface(surface))
    return;
  if (!GetSupplementable()->GetFrame())
    return;

  IdentifiableTokenBuilder builder;
  for (const auto& voice : voice_list_) {
    builder.AddToken(IdentifiabilityBenignStringToken(voice->voiceURI()));
    builder.AddToken(IdentifiabilityBenignStringToken(voice->lang()));
    builder.AddToken(IdentifiabilityBenignStringToken(voice->name()));
    builder.AddToken(voice->localService());
  }
  IdentifiabilityMetricBuilder(GetSupplementable()->UkmSourceID())
      .Add(surface, builder.GetToken())
      .Record(GetSupplementable()->UkmRecorder());
}

bool SpeechSynthesis::Speaking() const {
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

void SpeechSynthesis::Speak(const String& text, const String& lang) {
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetSupplementable()->GetFrame());
  SpeechSynthesisUtterance* utterance =
      SpeechSynthesisUtterance::Create(GetSupplementable(), text);
  utterance->setLang(lang);
  speak(script_state, utterance);
}

void SpeechSynthesis::speak(ScriptState* script_state,
                            SpeechSynthesisUtterance* utterance) {
  DCHECK(utterance);
  if (!script_state->ContextIsValid())
    return;

  // Note: Non-UseCounter based TTS metrics are of the form TextToSpeech.* and
  // are generally global, whereas these are scoped to a single page load.
  UseCounter::Count(GetSupplementable(), WebFeature::kTextToSpeech_Speak);
  GetSupplementable()->CountUseOnlyInCrossOriginIframe(
      WebFeature::kTextToSpeech_SpeakCrossOrigin);
  if (!IsAllowedToStartByAutoplay()) {
    Deprecation::CountDeprecation(
        GetSupplementable(),
        WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay);
    FireErrorEvent(utterance, 0 /* char_index */, "not-allowed");
    return;
  }

  utterance_queue_.push_back(utterance);

  // If the queue was empty, speak this immediately.
  if (utterance_queue_.size() == 1)
    StartSpeakingImmediately();
}

void SpeechSynthesis::Cancel() {
  // Remove all the items from the utterance queue. The platform
  // may still have references to some of these utterances and may
  // fire events on them asynchronously.
  utterance_queue_.clear();

  if (mojom::blink::SpeechSynthesis* mojom_synthesis =
          TryEnsureMojomSynthesis())
    mojom_synthesis->Cancel();
}

void SpeechSynthesis::Pause() {
  if (is_paused_)
    return;

  if (mojom::blink::SpeechSynthesis* mojom_synthesis =
          TryEnsureMojomSynthesis())
    mojom_synthesis->Pause();
}

void SpeechSynthesis::Resume() {
  if (!CurrentSpeechUtterance())
    return;

  if (mojom::blink::SpeechSynthesis* mojom_synthesis =
          TryEnsureMojomSynthesis())
    mojom_synthesis->Resume();
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

void SpeechSynthesis::DidFinishSpeaking(
    SpeechSynthesisUtterance* utterance,
    mojom::blink::SpeechSynthesisErrorCode error_code) {
  HandleSpeakingCompleted(utterance, error_code);
}

void SpeechSynthesis::SpeakingErrorOccurred(
    SpeechSynthesisUtterance* utterance) {
  HandleSpeakingCompleted(
      utterance, mojom::blink::SpeechSynthesisErrorCode::kErrorOccurred);
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

  if (TryEnsureMojomSynthesis())
    utterance->Start(this);
}

void SpeechSynthesis::HandleSpeakingCompleted(
    SpeechSynthesisUtterance* utterance,
    mojom::blink::SpeechSynthesisErrorCode error_code) {
  DCHECK(utterance);

  // Special handling for audio descriptions.
  SpeechSynthesisBase::HandleSpeakingCompleted();

  bool should_start_speaking = false;
  // If the utterance that completed was the one we're currently speaking,
  // remove it from the queue and start speaking the next one.
  if (utterance == CurrentSpeechUtterance()) {
    utterance_queue_.pop_front();
    should_start_speaking = !utterance_queue_.empty();
  }

  // https://wicg.github.io/speech-api/#speechsynthesiserrorevent-attributes
  // The below errors are matched with SpeechSynthesisErrorCode values.
  static constexpr char kErrorCanceled[] = "canceled";
  static constexpr char kErrorInterrupted[] = "interrupted";
  static constexpr char kErrorSynthesisFailed[] = "synthesis-failed";

  // Always fire the event, because the platform may have asynchronously
  // sent an event on an utterance before it got the message that we
  // canceled it, and we should always report to the user what actually
  // happened.
  switch (error_code) {
    case mojom::blink::SpeechSynthesisErrorCode::kInterrupted:
      FireErrorEvent(utterance, 0, kErrorInterrupted);
      break;
    case mojom::blink::SpeechSynthesisErrorCode::kCancelled:
      FireErrorEvent(utterance, 0, kErrorCanceled);
      break;
    case mojom::blink::SpeechSynthesisErrorCode::kErrorOccurred:
      // TODO(csharrison): Actually pass the correct message. For now just use a
      // generic error.
      FireErrorEvent(utterance, 0, kErrorSynthesisFailed);
      break;
    case mojom::blink::SpeechSynthesisErrorCode::kNoError:
      FireEvent(event_type_names::kEnd, utterance, 0, 0, String());
      break;
  }

  // Start the next utterance if we just finished one and one was pending.
  if (should_start_speaking && !utterance_queue_.empty())
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
  if (utterance_queue_.empty())
    return nullptr;

  return utterance_queue_.front().Get();
}

ExecutionContext* SpeechSynthesis::GetExecutionContext() const {
  return GetSupplementable();
}

void SpeechSynthesis::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(mojom_synthesis_);
  visitor->Trace(voice_list_);
  visitor->Trace(utterance_queue_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  EventTarget::Trace(visitor);
  SpeechSynthesisBase::Trace(visitor);
}

bool SpeechSynthesis::GetElapsedTimeMillis(double* millis) {
  if (!GetSupplementable()->GetFrame())
    return false;
  if (GetSupplementable()->document()->IsStopped())
    return false;

  *millis = DOMWindowPerformance::performance(*GetSupplementable())->now();
  return true;
}

bool SpeechSynthesis::IsAllowedToStartByAutoplay() const {
  Document* document = GetSupplementable()->document();
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
  mojom_synthesis_.Bind(
      std::move(mojom_synthesis),
      GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  receiver_.reset();
  mojom_synthesis_->AddVoiceListObserver(receiver_.BindNewPipeAndPassRemote(
      GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
}

mojom::blink::SpeechSynthesis* SpeechSynthesis::TryEnsureMojomSynthesis() {
  if (mojom_synthesis_.is_bound())
    return mojom_synthesis_.get();

  // The frame could be detached. In that case, calls on mojom_synthesis_ will
  // just get dropped. That's okay and is simpler than having to null-check
  // mojom_synthesis_ before each use.
  LocalDOMWindow* window = GetSupplementable();
  if (!window->GetFrame())
    return nullptr;

  auto receiver = mojom_synthesis_.BindNewPipeAndPassReceiver(
      window->GetTaskRunner(TaskType::kMiscPlatformAPI));

  window->GetBrowserInterfaceBroker().GetInterface(std::move(receiver));

  mojom_synthesis_->AddVoiceListObserver(receiver_.BindNewPipeAndPassRemote(
      window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  return mojom_synthesis_.get();
}

const AtomicString& SpeechSynthesis::InterfaceName() const {
  return event_target_names::kSpeechSynthesis;
}

}  // namespace blink
