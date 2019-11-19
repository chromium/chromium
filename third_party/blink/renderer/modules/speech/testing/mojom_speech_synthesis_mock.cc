/*
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/modules/speech/testing/mojom_speech_synthesis_mock.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

mojo::PendingRemote<mojom::blink::SpeechSynthesis>
MojomSpeechSynthesisMock::Create(ExecutionContext* context) {
  mojo::PendingRemote<mojom::blink::SpeechSynthesis> proxy;
  mojo::MakeSelfOwnedReceiver(base::WrapUnique<mojom::blink::SpeechSynthesis>(
                                  new MojomSpeechSynthesisMock(context)),
                              proxy.InitWithNewPipeAndPassReceiver());
  return proxy;
}

MojomSpeechSynthesisMock::MojomSpeechSynthesisMock(ExecutionContext* context)
    : speaking_error_occurred_timer_(
          context->GetTaskRunner(TaskType::kInternalTest),
          this,
          &MojomSpeechSynthesisMock::SpeakingErrorOccurred),
      speaking_finished_timer_(context->GetTaskRunner(TaskType::kInternalTest),
                               this,
                               &MojomSpeechSynthesisMock::SpeakingFinished) {}

MojomSpeechSynthesisMock::~MojomSpeechSynthesisMock() = default;

void MojomSpeechSynthesisMock::SpeakingErrorOccurred(TimerBase*) {
  DCHECK(current_utterance_);

  current_client_->OnEncounteredSpeakingError();
  SpeakNext();
}

void MojomSpeechSynthesisMock::SpeakingFinished(TimerBase*) {
  DCHECK(current_utterance_);
  current_client_->OnFinishedSpeaking();
  SpeakNext();
}

void MojomSpeechSynthesisMock::SpeakNext() {
  if (speaking_error_occurred_timer_.IsActive())
    return;

  current_utterance_.reset();
  current_client_.reset();

  if (queued_requests_.IsEmpty())
    return;

  SpeechRequest next_request = queued_requests_.TakeFirst();

  Speak(std::move(next_request.utterance),
        std::move(next_request.pending_client));
}

void MojomSpeechSynthesisMock::AddVoiceListObserver(
    mojo::PendingRemote<mojom::blink::SpeechSynthesisVoiceListObserver>
        pending_observer) {
  Vector<mojom::blink::SpeechSynthesisVoicePtr> voice_list;
  mojom::blink::SpeechSynthesisVoicePtr voice;

  voice = mojom::blink::SpeechSynthesisVoice::New();
  voice->voice_uri = String("mock.voice.bruce");
  voice->name = String("bruce");
  voice->lang = String("en-US");
  voice->is_local_service = true;
  voice->is_default = true;
  voice_list.push_back(std::move(voice));

  voice = mojom::blink::SpeechSynthesisVoice::New();
  voice->voice_uri = String("mock.voice.clark");
  voice->name = String("clark");
  voice->lang = String("en-US");
  voice->is_local_service = true;
  voice->is_default = false;
  voice_list.push_back(std::move(voice));

  voice = mojom::blink::SpeechSynthesisVoice::New();
  voice->voice_uri = String("mock.voice.logan");
  voice->name = String("logan");
  voice->lang = String("fr-CA");
  voice->is_local_service = true;
  voice->is_default = true;
  voice_list.push_back(std::move(voice));

  // We won't notify the observer again, but we still retain the remote as
  // that's the expected contract of the API.
  mojo::Remote<mojom::blink::SpeechSynthesisVoiceListObserver> observer(
      std::move(pending_observer));
  observer->OnSetVoiceList(std::move(voice_list));
  voice_list_observers_.emplace_back(std::move(observer));
}

void MojomSpeechSynthesisMock::Speak(
    mojom::blink::SpeechSynthesisUtterancePtr utterance,
    mojo::PendingRemote<mojom::blink::SpeechSynthesisClient> pending_client) {
  DCHECK(utterance);
  DCHECK(pending_client);

  if (current_utterance_) {
    queued_requests_.emplace_back(
        SpeechRequest{std::move(utterance), std::move(pending_client)});
    return;
  }

  current_utterance_ = std::move(utterance);
  current_client_.Bind(std::move(pending_client));

  current_client_->OnStartedSpeaking();

  // Fire a fake word and then sentence boundary event.
  int char_length = current_utterance_->text.find(' ');
  int sentence_length = current_utterance_->text.length();
  current_client_->OnEncounteredWordBoundary(0, char_length);
  current_client_->OnEncounteredSentenceBoundary(0, sentence_length);

  // Give the fake speech job some time so that pause and other functions have
  // time to be called.
  speaking_finished_timer_.StartOneShot(base::TimeDelta::FromMilliseconds(100),
                                        FROM_HERE);
}

void MojomSpeechSynthesisMock::Cancel() {
  if (!current_utterance_)
    return;

  // Per spec, removes all queued utterances.
  queued_requests_.clear();

  speaking_finished_timer_.Stop();
  speaking_error_occurred_timer_.StartOneShot(
      base::TimeDelta::FromMilliseconds(100), FROM_HERE);
}

void MojomSpeechSynthesisMock::Pause() {
  if (!current_utterance_)
    return;

  current_client_->OnPausedSpeaking();
}

void MojomSpeechSynthesisMock::Resume() {
  if (!current_utterance_)
    return;

  current_client_->OnResumedSpeaking();
}

}  // namespace blink
