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

#include "third_party/blink/renderer/modules/speech/speech_synthesis_utterance.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SpeechSynthesisUtterance* SpeechSynthesisUtterance::Create(
    ExecutionContext* context) {
  return MakeGarbageCollected<SpeechSynthesisUtterance>(context, String());
}

SpeechSynthesisUtterance* SpeechSynthesisUtterance::Create(
    ExecutionContext* context,
    const String& text) {
  return MakeGarbageCollected<SpeechSynthesisUtterance>(context, text);
}

SpeechSynthesisUtterance::SpeechSynthesisUtterance(ExecutionContext* context,
                                                   const String& text)
    : ExecutionContextClient(context),
      receiver_(this, context),
      mojom_utterance_(mojom::blink::SpeechSynthesisUtterance::New()) {
  // Set default values. |voice| intentionally left null.
  mojom_utterance_->text = text;
  mojom_utterance_->lang = String("");
  mojom_utterance_->volume = mojom::blink::kSpeechSynthesisDoublePrefNotSet;
  mojom_utterance_->rate = mojom::blink::kSpeechSynthesisDoublePrefNotSet;
  mojom_utterance_->pitch = mojom::blink::kSpeechSynthesisDoublePrefNotSet;
}

SpeechSynthesisUtterance::~SpeechSynthesisUtterance() = default;

const AtomicString& SpeechSynthesisUtterance::InterfaceName() const {
  return event_target_names::kSpeechSynthesisUtterance;
}

SpeechSynthesisVoice* SpeechSynthesisUtterance::voice() const {
  return voice_.Get();
}

void SpeechSynthesisUtterance::setVoice(SpeechSynthesisVoice* voice) {
  // Cache our own version of the SpeechSynthesisVoice so that we don't have to
  // do some lookup to go from the platform voice back to the speech synthesis
  // voice in the read property.
  voice_ = voice;

  mojom_utterance_->voice = voice_ ? voice_->name() : String();
}

float SpeechSynthesisUtterance::volume() const {
  return mojom_utterance_->volume ==
                 mojom::blink::kSpeechSynthesisDoublePrefNotSet
             ? mojom::blink::kSpeechSynthesisDefaultVolume
             : mojom_utterance_->volume;
}

float SpeechSynthesisUtterance::rate() const {
  return mojom_utterance_->rate ==
                 mojom::blink::kSpeechSynthesisDoublePrefNotSet
             ? mojom::blink::kSpeechSynthesisDefaultRate
             : mojom_utterance_->rate;
}

float SpeechSynthesisUtterance::pitch() const {
  return mojom_utterance_->pitch ==
                 mojom::blink::kSpeechSynthesisDoublePrefNotSet
             ? mojom::blink::kSpeechSynthesisDefaultPitch
             : mojom_utterance_->pitch;
}

void SpeechSynthesisUtterance::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(synthesis_);
  visitor->Trace(voice_);
  ExecutionContextClient::Trace(visitor);
  EventTarget::Trace(visitor);
}

void SpeechSynthesisUtterance::OnStartedSpeaking() {
  DCHECK(synthesis_);
  synthesis_->DidStartSpeaking(this);
}

void SpeechSynthesisUtterance::OnFinishedSpeaking(
    mojom::blink::SpeechSynthesisErrorCode error_code) {
  DCHECK(synthesis_);
  finished_ = true;
  synthesis_->DidFinishSpeaking(this, error_code);
}

void SpeechSynthesisUtterance::OnPausedSpeaking() {
  DCHECK(synthesis_);
  synthesis_->DidPauseSpeaking(this);
}

void SpeechSynthesisUtterance::OnResumedSpeaking() {
  DCHECK(synthesis_);
  synthesis_->DidResumeSpeaking(this);
}

void SpeechSynthesisUtterance::OnEncounteredWordBoundary(uint32_t char_index,
                                                         uint32_t char_length) {
  DCHECK(synthesis_);
  synthesis_->WordBoundaryEventOccurred(this, char_index, char_length);
}

void SpeechSynthesisUtterance::OnEncounteredSentenceBoundary(
    uint32_t char_index,
    uint32_t char_length) {
  DCHECK(synthesis_);
  synthesis_->SentenceBoundaryEventOccurred(this, char_index, char_length);
}

void SpeechSynthesisUtterance::OnEncounteredSpeakingError() {
  DCHECK(synthesis_);
  finished_ = true;
  synthesis_->SpeakingErrorOccurred(this);
}

void SpeechSynthesisUtterance::Start(SpeechSynthesis* synthesis) {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;

  finished_ = false;

  mojom::blink::SpeechSynthesisUtterancePtr mojom_utterance_to_send =
      mojom_utterance_->Clone();
  if (mojom_utterance_to_send->voice.IsNull())
    mojom_utterance_to_send->voice = String("");
  if (mojom_utterance_to_send->text.IsNull())
    mojom_utterance_to_send->text = String("");

  receiver_.reset();

  synthesis_ = synthesis;
  synthesis_->MojomSynthesis()->Speak(
      std::move(mojom_utterance_to_send),
      receiver_.BindNewPipeAndPassRemote(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

  // Add a disconnect handler so we can cleanup appropriately.
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &SpeechSynthesisUtterance::OnDisconnected, WrapWeakPersistent(this)));
}

void SpeechSynthesisUtterance::OnDisconnected() {
  // If the remote end disconnects, just simulate that we finished normally.
  if (!finished_)
    OnFinishedSpeaking(mojom::blink::SpeechSynthesisErrorCode::kNoError);
}

}  // namespace blink
