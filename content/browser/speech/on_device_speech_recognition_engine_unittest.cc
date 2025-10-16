// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/speech/on_device_speech_recognition_engine_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/audio_parameters.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(OnDeviceSpeechRecognitionEngine, ConvertAccumulatedAudioData) {
  // A BrowserTaskEnvironment is necessary because the
  // OnDeviceSpeechRecognitionEngine post tasks.
  BrowserTaskEnvironment task_environment;

  // Verifies that ConvertAccumulatedAudioData correctly normalizes audio
  // samples.
  OnDeviceSpeechRecognitionEngine engine((SpeechRecognitionSessionConfig()));

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(), 16000, 160);
  engine.SetAudioParameters(params);

  // The int16_t values 16384 and -32768 should normalize to 0.5 and -1.0.
  engine.accumulated_audio_data_ = {16384, -32768};

  on_device_model::mojom::AudioDataPtr converted_audio_data =
      engine.ConvertAccumulatedAudioData();

  EXPECT_THAT(converted_audio_data->data,
              testing::Pointwise(testing::FloatEq(), {0.5f, -1.0f}));
}

}  // namespace content
