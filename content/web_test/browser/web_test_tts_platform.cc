// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_tts_platform.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/tts_controller.h"

// static
WebTestTtsPlatform* WebTestTtsPlatform::GetInstance() {
  return base::Singleton<WebTestTtsPlatform>::get();
}

bool WebTestTtsPlatform::PlatformImplSupported() {
  return true;
}

bool WebTestTtsPlatform::PlatformImplInitialized() {
  return true;
}

void WebTestTtsPlatform::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {}

void WebTestTtsPlatform::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    OnSpeakFinishedCallback on_speak_finished) {
  content::TtsController* controller = content::TtsController::GetInstance();
  int len = static_cast<int>(utterance.size());
  utterance_id_ = utterance_id;
  controller->OnTtsEvent(utterance_id, content::TTS_EVENT_START, 0, len,
                         std::string());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebTestTtsPlatform::SimulateEndEvent,
                                base::Unretained(this), utterance_id, len,
                                std::move(on_speak_finished)));
}

void WebTestTtsPlatform::SimulateEndEvent(
    int utterance_id,
    int len,
    OnSpeakFinishedCallback on_speak_finished) {
  utterance_id_ = kInvalidUtteranceId;
  std::move(on_speak_finished).Run(true);
  content::TtsController* controller = content::TtsController::GetInstance();
  controller->OnTtsEvent(utterance_id, content::TTS_EVENT_END, len, 0,
                         std::string());
}

bool WebTestTtsPlatform::StopSpeaking() {
  return true;
}

bool WebTestTtsPlatform::IsSpeaking() {
  return false;
}

void WebTestTtsPlatform::GetVoices(
    std::vector<content::VoiceData>* out_voices) {}

void WebTestTtsPlatform::Pause() {
  content::TtsController* controller = content::TtsController::GetInstance();
  controller->OnTtsEvent(utterance_id_, content::TTS_EVENT_PAUSE, 0, 0,
                         std::string());
}

void WebTestTtsPlatform::Resume() {
  content::TtsController* controller = content::TtsController::GetInstance();
  controller->OnTtsEvent(utterance_id_, content::TTS_EVENT_RESUME, 0, 0,
                         std::string());
}

void WebTestTtsPlatform::WillSpeakUtteranceWithVoice(
    content::TtsUtterance* utterance,
    const content::VoiceData& voice_data) {}

std::string WebTestTtsPlatform::GetError() {
  return {};
}

void WebTestTtsPlatform::ClearError() {}

void WebTestTtsPlatform::SetError(const std::string& error) {}

void WebTestTtsPlatform::Shutdown() {}

void WebTestTtsPlatform::FinalizeVoiceOrdering(
    std::vector<content::VoiceData>& voices) {}

void WebTestTtsPlatform::RefreshVoices() {}

content::ExternalPlatformDelegate*
WebTestTtsPlatform::GetExternalPlatformDelegate() {
  return nullptr;
}

WebTestTtsPlatform::WebTestTtsPlatform() = default;

WebTestTtsPlatform::~WebTestTtsPlatform() = default;
