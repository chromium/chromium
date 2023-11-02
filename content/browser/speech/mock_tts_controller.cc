// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "content/public/browser/tts_controller.h"

namespace content {

UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(1.0), pitch(1.0), volume(1.0) {}

VoiceData::VoiceData() : remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

class MockTtsController : public TtsController {
 public:
  static MockTtsController* GetInstance() {
    return base::Singleton<MockTtsController>::get();
  }

  MockTtsController() {}

  MockTtsController(const MockTtsController&) = delete;
  MockTtsController& operator=(const MockTtsController&) = delete;

  bool IsSpeaking() override { return false; }

  void SpeakOrEnqueue(std::unique_ptr<TtsUtterance> utterance) override {}

  void Stop() override {}

  void Stop(const GURL& source_url) override {}

  void Pause() override {}

  void Resume() override {}

  void OnTtsEvent(int utterance_id,
                  TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override {}

  void GetVoices(BrowserContext* browser_context,
                 std::vector<VoiceData>* out_voices) override {}

  void VoicesChanged() override {}

  void AddVoicesChangedDelegate(VoicesChangedDelegate* delegate) override {}

  void RemoveVoicesChangedDelegate(VoicesChangedDelegate* delegate) override {}

  void RemoveUtteranceEventDelegate(UtteranceEventDelegate* delegate) override {
  }

  void SetTtsEngineDelegate(TtsEngineDelegate* delegate) override {}

  TtsEngineDelegate* GetTtsEngineDelegate() override { return nullptr; }

  void SetTtsPlatform(TtsPlatform* tts_platform) override {}

  int QueueSize() override { return 0; }

  void StripSSML(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> callback) override {}

 private:
  friend struct base::DefaultSingletonTraits<MockTtsController>;
};

// static
TtsController* TtsController::GetInstance() {
  return MockTtsController::GetInstance();
}

}  // namespace content
