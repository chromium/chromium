// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_
#define CONTENT_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"

namespace content {

// Abstract platform implementation.
class TtsPlatformImpl : public TtsPlatform {
 public:
  static TtsPlatformImpl* GetInstance();

  // TtsPlatform overrides.
  bool LoadBuiltInTtsEngine(BrowserContext* browser_context) override;
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;

 protected:
  TtsPlatformImpl() {}

  // On some platforms this may be a leaky singleton - do not rely on the
  // destructor being called!  http://crbug.com/122026
  virtual ~TtsPlatformImpl() {}

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_
