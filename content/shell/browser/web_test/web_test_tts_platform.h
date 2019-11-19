// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_TTS_PLATFORM_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_TTS_PLATFORM_H_

#include "base/memory/singleton.h"
#include "content/public/browser/tts_platform.h"

// Dummy implementation of TtsPlatform for web tests.
// Currently does nothing interesting but could be extended to enable more
// detailed testing.
class WebTestTtsPlatform : public content::TtsPlatform {
 public:
  static WebTestTtsPlatform* GetInstance();

  // content::TtsControllerDelegate overrides.
  bool PlatformImplAvailable() override;
  bool LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<content::VoiceData>* out_voices) override;
  void Pause() override;
  void Resume() override;
  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;

 private:
  WebTestTtsPlatform();
  virtual ~WebTestTtsPlatform();

  friend struct base::DefaultSingletonTraits<WebTestTtsPlatform>;

  DISALLOW_COPY_AND_ASSIGN(WebTestTtsPlatform);
};

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_TTS_PLATFORM_H_
