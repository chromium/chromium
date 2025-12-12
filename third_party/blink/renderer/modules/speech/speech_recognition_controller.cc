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

#include "third_party/blink/renderer/modules/speech/speech_recognition_controller.h"

#include <memory>
#include <optional>

#include "media/mojo/mojom/speech_recognition_recognition_context.mojom-blink.h"
#include "media/mojo/mojom/speech_recognizer.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/speech/speech_grammar_list.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition_phrase.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const char SpeechRecognitionController::kSupplementName[] =
    "SpeechRecognitionController";

SpeechRecognitionController* SpeechRecognitionController::From(
    LocalDOMWindow& window) {
  SpeechRecognitionController* controller =
      Supplement<LocalDOMWindow>::From<SpeechRecognitionController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<SpeechRecognitionController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  return controller;
}

SpeechRecognitionController::SpeechRecognitionController(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      speech_recognizer_(&window),
      on_device_speech_recognition_(&window) {}

SpeechRecognitionController::~SpeechRecognitionController() {
  // FIXME: Call m_client->pageDestroyed(); once we have implemented a client.
}

media::mojom::blink::StartSpeechRecognitionRequestParamsPtr
SpeechRecognitionController::BuildStartSpeechRecognitionRequestParams(
    mojo::PendingReceiver<media::mojom::blink::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::blink::SpeechRecognitionSessionClient>
        session_client,
    const SpeechGrammarList& grammars,
    const V8ObservableArraySpeechRecognitionPhrase* phrases,
    const String& lang,
    bool continuous,
    bool interim_results,
    uint32_t max_alternatives,
    bool on_device,
    bool allow_cloud_fallback,
    mojo::PendingReceiver<media::mojom::blink::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    std::optional<media::AudioParameters> audio_parameters) {
  media::mojom::blink::StartSpeechRecognitionRequestParamsPtr params =
      media::mojom::blink::StartSpeechRecognitionRequestParams::New();
  for (unsigned i = 0; i < grammars.length(); i++) {
    SpeechGrammar* grammar = grammars.item(i);
    params->grammars.push_back(
        media::mojom::blink::SpeechRecognitionGrammar::New(grammar->src(),
                                                           grammar->weight()));
  }
  if (!phrases->empty()) {
    params->recognition_context =
        media::mojom::blink::SpeechRecognitionRecognitionContext::New();
    for (const auto& phrase : *phrases) {
      params->recognition_context->phrases.push_back(
          media::mojom::blink::SpeechRecognitionPhrase::New(phrase->phrase(),
                                                            phrase->boost()));
    }
  }
  params->language = lang.IsNull() ? g_empty_string : lang;
  params->max_hypotheses = max_alternatives;
  params->continuous = continuous;
  params->interim_results = interim_results;
  params->on_device = on_device;
  params->allow_cloud_fallback = allow_cloud_fallback;
  params->client = std::move(session_client);
  params->session_receiver = std::move(session_receiver);

  if (audio_forwarder.is_valid()) {
    params->audio_forwarder = std::move(audio_forwarder);
    params->channel_count = audio_parameters.value().channels();
    params->sample_rate = audio_parameters.value().sample_rate();
  }

  return params;
}

void SpeechRecognitionController::Start(
    media::mojom::blink::StartSpeechRecognitionRequestParamsPtr params) {
  GetSpeechRecognizer()->Start(std::move(params));
}

void SpeechRecognitionController::AvailableOnDevice(
    const Vector<String>& languages,
    base::OnceCallback<void(media::mojom::blink::AvailabilityStatus)>
        callback) {
  GetOnDeviceSpeechRecognition()->Available(languages, std::move(callback));
}

void SpeechRecognitionController::Install(
    const Vector<String>& languages,
    base::OnceCallback<void(bool)> callback) {
  GetOnDeviceSpeechRecognition()->Install(languages, std::move(callback));
}

void SpeechRecognitionController::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  visitor->Trace(speech_recognizer_);
  visitor->Trace(on_device_speech_recognition_);
}

media::mojom::blink::SpeechRecognizer*
SpeechRecognitionController::GetSpeechRecognizer() {
  if (!speech_recognizer_.is_bound()) {
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        speech_recognizer_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return speech_recognizer_.get();
}

media::mojom::blink::OnDeviceSpeechRecognition*
SpeechRecognitionController::GetOnDeviceSpeechRecognition() {
  if (!on_device_speech_recognition_.is_bound()) {
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        on_device_speech_recognition_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return on_device_speech_recognition_.get();
}

}  // namespace blink
