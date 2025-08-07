// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_MOCK_LOOPBACK_SOURCE_H_
#define SERVICES_AUDIO_TEST_MOCK_LOOPBACK_SOURCE_H_

#include "media/base/audio_parameters.h"
#include "services/audio/loopback_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

class MockLoopbackSource : public LoopbackSource {
 public:
  MockLoopbackSource();
  ~MockLoopbackSource() override;

  MockLoopbackSource(const MockLoopbackSource&) = delete;
  MockLoopbackSource& operator=(const MockLoopbackSource&) = delete;

  MOCK_CONST_METHOD0(GetAudioParameters, const media::AudioParameters&());
  MOCK_METHOD1(StartSnooping, void(Snooper* snooper));
  MOCK_METHOD1(StopSnooping, void(Snooper* snooper));
  MOCK_METHOD0(StartMuting, void());
  MOCK_METHOD0(StopMuting, void());
  MOCK_METHOD0(IsMuting, bool());
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_MOCK_LOOPBACK_SOURCE_H_
