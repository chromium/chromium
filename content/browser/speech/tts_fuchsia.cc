// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_platform_impl.h"

namespace content {

// Dummy implementation to prevent a browser crash, see crbug.com/1019511
// TODO(crbug.com/1019819): Provide an implementation for Fuchsia.
class TtsPlatformImplFuchsia : public TtsPlatformImpl {
 public:
  TtsPlatformImplFuchsia() = default;
  ~TtsPlatformImplFuchsia() override = default;

  // TtsPlatform implementation.
  bool PlatformImplAvailable() override { return false; }
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
  static TtsPlatformImplFuchsia* GetInstance() {
    return base::Singleton<TtsPlatformImplFuchsia>::get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplFuchsia);
};

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplFuchsia::GetInstance();
}

}  // namespace content
