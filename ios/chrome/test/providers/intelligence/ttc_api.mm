// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"

#import <optional>

#import "base/no_destructor.h"
#import "ios/chrome/test/providers/intelligence/test_ttc_api.h"

namespace ios::provider {

namespace {

TTCConfig GetDefaultMockConfig() {
  return TTCConfig{
      .system_instruction = "Mock system instruction for testing.",
      .model = "models/mock-live-model",
      .voice_name = "MockVoice",
      .endpoint_url = "wss://mock.example.com/ws",
      .api_key = "mock-api-key",
  };
}

std::optional<TTCConfig>& GetCustomConfig() {
  static base::NoDestructor<std::optional<TTCConfig>> custom_config;
  return *custom_config;
}

__strong id<TTCAudioEngineProtocol> custom_audio_engine = nil;

}  // namespace

namespace test {

void SetTTCConfigForTesting(TTCConfig config) {
  GetCustomConfig() = config;
}

void ResetTTCConfigForTesting() {
  GetCustomConfig().reset();
}

void SetTTCAudioEngineForTesting(id<TTCAudioEngineProtocol> engine) {
  custom_audio_engine = engine;
}

}  // namespace test

TTCConfig GetTTCConfig() {
  return GetCustomConfig().value_or(GetDefaultMockConfig());
}

id<TTCAudioEngineProtocol> CreateTTCAudioEngine() {
  return custom_audio_engine;
}

}  // namespace ios::provider
