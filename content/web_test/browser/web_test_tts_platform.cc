// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_tts_platform.h"

#include "base/callback.h"
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
    base::OnceCallback<void(bool)> on_speak_finished) {
  std::move(on_speak_finished).Run(true);
  content::TtsController* controller = content::TtsController::GetInstance();
  int len = static_cast<int>(utterance.size());
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

void WebTestTtsPlatform::Shutdown() {}

void WebTestTtsPlatform::FinalizeVoiceOrdering(
    std::vector<content::VoiceData>& voices) {}

void WebTestTtsPlatform::GetVoicesForBrowserContext(
    content::BrowserContext* browser_context,
    const GURL& source_url,
    std::vector<content::VoiceData>* out_voices) {}

void WebTestTtsPlatform::RefreshVoices() {}

WebTestTtsPlatform::WebTestTtsPlatform() = default;

WebTestTtsPlatform::~WebTestTtsPlatform() = default;
