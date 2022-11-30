// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_
#define MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_

#include <stdint.h>

#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MockAudioSourceCallback();

  MockAudioSourceCallback(const MockAudioSourceCallback&) = delete;
  MockAudioSourceCallback& operator=(const MockAudioSourceCallback&) = delete;

  ~MockAudioSourceCallback() override;

  MOCK_METHOD4(OnMoreData,
               int(base::TimeDelta,
                   base::TimeTicks,
                   const AudioGlitchInfo& glitch_info,
                   AudioBus*));
  MOCK_METHOD1(OnError, void(ErrorType));
};

}  // namespace media

#endif  // MEDIA_AUDIO_MOCK_AUDIO_SOURCE_CALLBACK_H_
