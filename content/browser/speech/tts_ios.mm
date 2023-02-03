// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_platform_impl.h"

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace content {

// Dummy implementation provide an implementation for iOS.
class TtsPlatformImplIOS : public TtsPlatformImpl {
 public:
  TtsPlatformImplIOS() = default;
  TtsPlatformImplIOS(const TtsPlatformImplIOS&) = delete;
  TtsPlatformImplIOS& operator=(const TtsPlatformImplIOS&) = delete;

  // TtsPlatform implementation.
  bool PlatformImplSupported() override { return false; }
  bool PlatformImplInitialized() override { return false; }
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {
    std::move(on_speak_finished).Run(false);
  }
  bool StopSpeaking() override { return false; }
  bool IsSpeaking() override { return false; }
  void GetVoices(std::vector<VoiceData>* out_voices) override {}
  void Pause() override {}
  void Resume() override {}

  // Get the single instance of this class.
  static TtsPlatformImplIOS* GetInstance() {
    static base::NoDestructor<TtsPlatformImplIOS> tts_platform;
    return tts_platform.get();
  }
};

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplIOS::GetInstance();
}

}  // namespace content
