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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_SYNTHESIS_UTTERANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_SYNTHESIS_UTTERANCE_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/speech/speech_synthesis_voice.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {
class SpeechSynthesis;

class SpeechSynthesisUtterance final
    : public EventTargetWithInlineData,
      public ContextClient,
      public mojom::blink::SpeechSynthesisClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SpeechSynthesisUtterance);

 public:
  static SpeechSynthesisUtterance* Create(ExecutionContext*, const String&);

  SpeechSynthesisUtterance(ExecutionContext*, const String&);
  ~SpeechSynthesisUtterance() override;

  const String& text() const { return mojom_utterance_->text; }
  void setText(const String& text) { mojom_utterance_->text = text; }

  const String& lang() const { return mojom_utterance_->lang; }
  void setLang(const String& lang) { mojom_utterance_->lang = lang; }

  SpeechSynthesisVoice* voice() const;
  void setVoice(SpeechSynthesisVoice*);

  float volume() const { return mojom_utterance_->volume; }
  void setVolume(float volume) {
    mojom_utterance_->volume = clampTo(volume, 0.0f, 1.0f);
  }

  float rate() const { return mojom_utterance_->rate; }
  void setRate(float rate) {
    mojom_utterance_->rate = clampTo(rate, 0.1f, 10.0f);
  }

  float pitch() const { return mojom_utterance_->pitch; }
  void setPitch(float pitch) {
    mojom_utterance_->pitch = clampTo(pitch, 0.0f, 2.0f);
  }

  double StartTime() const { return start_time_; }
  void SetStartTime(double start_time) { start_time_ = start_time; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(start, kStart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end, kEnd)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pause, kPause)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resume, kResume)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(mark, kMark)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(boundary, kBoundary)

  ExecutionContext* GetExecutionContext() const override {
    return ContextClient::GetExecutionContext();
  }

  void Trace(blink::Visitor*) override;

  // mojom::blink::SpeechSynthesisClient
  void OnStartedSpeaking() override;
  void OnFinishedSpeaking() override;
  void OnPausedSpeaking() override;
  void OnResumedSpeaking() override;
  void OnEncounteredWordBoundary(uint32_t char_index,
                                 uint32_t char_length) override;
  void OnEncounteredSentenceBoundary(uint32_t char_index,
                                     uint32_t char_length) override;
  void OnEncounteredSpeakingError() override;

  void Start(SpeechSynthesis* synthesis);

 private:
  void OnDisconnected();

  // EventTarget
  const AtomicString& InterfaceName() const override;

  mojo::Receiver<mojom::blink::SpeechSynthesisClient> receiver_{this};
  mojom::blink::SpeechSynthesisUtterancePtr mojom_utterance_;
  Member<SpeechSynthesis> synthesis_;
  Member<SpeechSynthesisVoice> voice_;
  double start_time_ = 0.0;
  bool finished_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_SYNTHESIS_UTTERANCE_H_
