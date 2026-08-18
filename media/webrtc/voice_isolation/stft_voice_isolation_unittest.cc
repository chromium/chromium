// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/stft_voice_isolation.h"

#include <cmath>
#include <complex>
#include <vector>

#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::_;
using testing::Return;

namespace {

class MockVoiceIsolationComponent : public VoiceIsolationComponent {
 public:
  MOCK_METHOD(void,
              ProcessAudio,
              (base::span<const float> input, base::span<float> output),
              (override));
  MOCK_METHOD(size_t, FrameSize, (), (const, override));
  MOCK_METHOD(size_t, FramesPerSecond, (), (const, override));
};

}  // namespace

TEST(VoiceIsolationWindowedFftTest, WindowOlaProperty) {
  // Check Overlap-Add.
  constexpr unsigned int kFftSize = 480;
  WindowedFft windowed_fft(kFftSize);

  const std::vector<float>& window = windowed_fft.fft_window_;
  const std::vector<float>& inv_window = windowed_fft.inv_fft_window_;

  ASSERT_EQ(window.size(), kFftSize);
  ASSERT_EQ(window.size(), inv_window.size());

  // The OLA property states that for a hop size of N/2:
  // window[i] * inv_window[i] + window[i + N/2] * inv_window[i + N/2] = 1.0
  // for i in [0, N/2 - 1].
  const int half_size = kFftSize / 2;
  for (int i = 0; i < half_size; ++i) {
    SCOPED_TRACE(::testing::Message() << "index=" << i);
    float val1 = window[i] * inv_window[i];
    float val2 = window[i + half_size] * inv_window[i + half_size];
    EXPECT_FLOAT_EQ(val1 + val2, 1.0f);
  }
}

TEST(StftVoiceIsolationTest, Creation) {
  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(320));
  EXPECT_CALL(*mock_inner, FramesPerSecond()).WillRepeatedly(Return(50));

  StftVoiceIsolation stft(std::move(mock_inner));
}

TEST(StftVoiceIsolationTest, FrameSizeAndDelay) {
  // Expectation: we do two FFT of 20ms each. The input is 20ms. We use an
  // overlapping window. We return 160 complex values per FFT.
  constexpr size_t kFftSize = 2 * 2 * 160;
  constexpr size_t kFrameSize = 2 * 160;
  constexpr size_t kFramesPerSecond = 50;

  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(kFftSize));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));

  StftVoiceIsolation stft(std::move(mock_inner));
  EXPECT_EQ(stft.FrameSize(), kFrameSize);
  EXPECT_EQ(stft.FramesPerSecond(), kFramesPerSecond);
}

TEST(StftVoiceIsolationTest, ProcessAudioLoopback) {
  // Test perfect reconstruction (or near perfect) with passthrough internal.
  auto mock_passthrough_inner = std::make_unique<MockVoiceIsolationComponent>();
  auto* mock_passthrough_inner_ptr = mock_passthrough_inner.get();

  constexpr unsigned int kFftSize = 2 * 2 * 160;
  EXPECT_CALL(*mock_passthrough_inner_ptr, FrameSize())
      .WillRepeatedly(Return(2 * kFftSize));
  EXPECT_CALL(*mock_passthrough_inner_ptr, FramesPerSecond())
      .WillRepeatedly(Return(50));

  EXPECT_CALL(*mock_passthrough_inner_ptr, ProcessAudio(_, _))
      .WillRepeatedly(
          [](base::span<const float> input, base::span<float> output) {
            // Passthrough frequency domain data.
            output.copy_from_nonoverlapping(input);
          });

  StftVoiceIsolation stft(std::move(mock_passthrough_inner));

  ASSERT_EQ(stft.FrameSize(), kFftSize);

  size_t frame_size = stft.FrameSize();
  std::vector<float> input(frame_size, 0.0f);
  std::vector<float> output(frame_size, 0.0f);

  // Send a sine wave.
  // We need to send enough frames to flush the overlap-add delay.
  // Delay is usually one hop (frame_size).
  // So Output[N] depends on Input[N] and Input[N-1] (due to window overlap).
  // Actually, wait. algorithmic delay of fft_size / 2.
  // So first frame output will be fade-in.

  constexpr int kNumFrames = 10;
  std::vector<float> full_input;
  std::vector<float> full_output;

  for (int i = 0; i < kNumFrames; ++i) {
    for (size_t j = 0; j < frame_size; ++j) {
      input[j] = std::sin((i * frame_size + j) * 0.01f) +
                 std::sin((42 + i * frame_size + j) * 0.1f);
      full_input.push_back(input[j]);
    }
    stft.ProcessAudio(input, output);
    for (float v : output) {
      full_output.push_back(v);
    }
  }

  // Compare input and output accounting for delay. Delay is half a frame_size.
  int delay = frame_size / 2;
  for (size_t i = delay; i < full_output.size() - delay; ++i) {
    EXPECT_NEAR(full_output[i], full_input[i - delay], 1e-4f) << "Frame " << i;
  }
}

}  // namespace media
