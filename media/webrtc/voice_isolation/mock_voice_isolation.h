// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_MOCK_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_MOCK_VOICE_ISOLATION_H_

#include <cstddef>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVoiceIsolation : public VoiceIsolation {
 public:
  MockVoiceIsolation();
  ~MockVoiceIsolation() override;

  MOCK_METHOD(void,
              ProcessAudio,
              (const AudioBus& input_bus, AudioBus& output_bus),
              (override));
  MOCK_METHOD(void, ClearBuffers, (), (override));
  MOCK_METHOD(base::TimeDelta, AlgorithmicDelay, (), (const, override));
};

class MockVoiceIsolationComponent : public VoiceIsolationComponent {
 public:
  MockVoiceIsolationComponent();
  ~MockVoiceIsolationComponent() override;

  MOCK_METHOD(void,
              ProcessAudio,
              (base::span<const float> input, base::span<float> output),
              (override));
  MOCK_METHOD(size_t, FrameSize, (), (const, override));
  MOCK_METHOD(size_t, FramesPerSecond, (), (const, override));
  MOCK_METHOD(void, ClearBuffers, (), (override));
  MOCK_METHOD(base::TimeDelta, AlgorithmicDelay, (), (const, override));
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_MOCK_VOICE_ISOLATION_H_
