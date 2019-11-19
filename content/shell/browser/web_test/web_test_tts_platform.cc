// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_tts_platform.h"

#include "content/public/browser/tts_controller.h"

// static
WebTestTtsPlatform* WebTestTtsPlatform::GetInstance() {
  return base::Singleton<WebTestTtsPlatform>::get();
}

bool WebTestTtsPlatform::PlatformImplAvailable() {
  return true;
}

bool WebTestTtsPlatform::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  return false;
}

void WebTestTtsPlatform::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  std::move(on_speak_finished).Run(true);
  content::TtsController* controller = content::TtsController::GetInstance();
  int len = int{utterance.size()};
  controller->OnTtsEvent(utterance_id, content::TTS_EVENT_START, 0, len,
                         std::string());
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

void WebTestTtsPlatform::Pause() {}

void WebTestTtsPlatform::Resume() {}

void WebTestTtsPlatform::WillSpeakUtteranceWithVoice(
    content::TtsUtterance* utterance,
    const content::VoiceData& voice_data) {}

std::string WebTestTtsPlatform::GetError() {
  return {};
}

void WebTestTtsPlatform::ClearError() {}

void WebTestTtsPlatform::SetError(const std::string& error) {}

WebTestTtsPlatform::WebTestTtsPlatform() {}

WebTestTtsPlatform::~WebTestTtsPlatform() {}
