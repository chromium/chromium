// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_TTS_PLATFORM_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_TTS_PLATFORM_H_

#include "base/memory/singleton.h"
#include "content/public/browser/tts_platform.h"

// Dummy implementation of TtsPlatform for web tests.
// Currently does nothing interesting but could be extended to enable more
// detailed testing.
class WebTestTtsPlatform : public content::TtsPlatform {
 public:
  using OnSpeakFinishedCallback = base::OnceCallback<void(bool)>;

  static WebTestTtsPlatform* GetInstance();

  WebTestTtsPlatform(const WebTestTtsPlatform&) = delete;
  WebTestTtsPlatform& operator=(const WebTestTtsPlatform&) = delete;

  // content::TtsControllerDelegate overrides.
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             OnSpeakFinishedCallback on_speak_finished) override;
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
  void Shutdown() override;
  void FinalizeVoiceOrdering(std::vector<content::VoiceData>& voices) override;
  void RefreshVoices() override;
  content::ExternalPlatformDelegate* GetExternalPlatformDelegate() override;

 private:
  static const int kInvalidUtteranceId = -1;

  WebTestTtsPlatform();
  virtual ~WebTestTtsPlatform();
  void SimulateEndEvent(int utterance_id,
                        int len,
                        OnSpeakFinishedCallback on_speak_finished);

  int utterance_id_ = kInvalidUtteranceId;

  friend struct base::DefaultSingletonTraits<WebTestTtsPlatform>;
};

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_TTS_PLATFORM_H_
