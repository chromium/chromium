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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_CONTROLLER_H_

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/speech/speech_recognizer.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class SpeechGrammarList;

class SpeechRecognitionController final
    : public GarbageCollected<SpeechRecognitionController>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(SpeechRecognitionController);

 public:
  static const char kSupplementName[];

  explicit SpeechRecognitionController(LocalFrame& frame);
  virtual ~SpeechRecognitionController();

  void Start(mojo::PendingReceiver<mojom::blink::SpeechRecognitionSession>
                 session_receiver,
             mojo::PendingRemote<mojom::blink::SpeechRecognitionSessionClient>
                 session_client,
             const SpeechGrammarList& grammars,
             const String& lang,
             bool continuous,
             bool interim_results,
             uint32_t max_alternatives);

  static SpeechRecognitionController* Create(LocalFrame& frame);
  static SpeechRecognitionController* From(LocalFrame* frame) {
    return Supplement<LocalFrame>::From<SpeechRecognitionController>(frame);
  }

 private:
  mojo::Remote<mojom::blink::SpeechRecognizer>& GetSpeechRecognizer();

  mojo::Remote<mojom::blink::SpeechRecognizer> speech_recognizer_;
};

MODULES_EXPORT void ProvideSpeechRecognitionTo(LocalFrame& frame);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SPEECH_SPEECH_RECOGNITION_CONTROLLER_H_
