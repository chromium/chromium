// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_
#define CONTENT_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_

#include <string>

#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"

namespace content {

// Abstract platform implementation.
class TtsPlatformImpl : public TtsPlatform {
 public:
  static TtsPlatformImpl* GetInstance();

  TtsPlatformImpl(const TtsPlatformImpl&) = delete;
  TtsPlatformImpl& operator=(const TtsPlatformImpl&) = delete;

  // TtsPlatform overrides.
  void LoadBuiltInTtsEngine(BrowserContext* browser_context) override;
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;
  void Shutdown() override;
  void FinalizeVoiceOrdering(std::vector<VoiceData>& voices) override;
  void RefreshVoices() override {}

  ExternalPlatformDelegate* GetExternalPlatformDelegate() override;

 protected:
  TtsPlatformImpl() {}

  // On some platforms this may be a leaky singleton - do not rely on the
  // destructor being called!  http://crbug.com/122026
  virtual ~TtsPlatformImpl() {}

  std::string error_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_
