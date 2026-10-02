// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/stft_voice_isolation.h"

#include <cmath>
#include <complex>
#include <vector>

#include "base/time/time.h"
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
  MOCK_METHOD(void, ClearBuffers, (), (override));
  MOCK_METHOD(base::TimeDelta, AlgorithmicDelay, (), (const, override));
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
  // Expectation: The inner component processes frequency-domain data from two
  // FFTs per step (640 floats total, representing 160 complex bins per FFT).
  // The outer StftVoiceIsolation operates in the time domain with a frame size
  // of 320 samples (20 ms at 16 kHz, with 50 frames per second).
  constexpr size_t kFftSize = 2 * 2 * 160;
  constexpr size_t kFrameSize = 2 * 160;
  constexpr size_t kFramesPerSecond = 50;

  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(kFftSize));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));

  // The inner component introduces its own algorithmic delay (e.g. 5 ms).
  EXPECT_CALL(*mock_inner, AlgorithmicDelay())
      .WillRepeatedly(Return(base::Milliseconds(5)));

  StftVoiceIsolation stft(std::move(mock_inner));
  EXPECT_EQ(stft.FrameSize(), kFrameSize);
  EXPECT_EQ(stft.FramesPerSecond(), kFramesPerSecond);

  // The STFT overlap-add synthesis introduces an algorithmic lookahead delay
  // equal to half the FFT window (one hop):
  //   hop_delay = (fft_size_ / 2) / (fft_size_ * FramesPerSecond())
  //             = 0.5 / 50 = 10 ms.
  // AlgorithmicDelay() aggregates the inner component's delay and the STFT
  // synthesis lookahead delay:
  //   total_delay = inner_delay (5 ms) + stft_delay (10 ms) = 15 ms.
  EXPECT_EQ(stft.AlgorithmicDelay(), base::Milliseconds(15));
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

TEST(StftVoiceIsolationTest, ClearBuffersResetsHistoryAndForwards) {
  auto mock_passthrough_inner = std::make_unique<MockVoiceIsolationComponent>();
  MockVoiceIsolationComponent* const mock_passthrough_inner_ptr =
      mock_passthrough_inner.get();

  constexpr unsigned int kFftSize = 320;
  EXPECT_CALL(*mock_passthrough_inner_ptr, FrameSize())
      .WillRepeatedly(Return(2 * kFftSize));
  EXPECT_CALL(*mock_passthrough_inner_ptr, FramesPerSecond())
      .WillRepeatedly(Return(50));
  EXPECT_CALL(*mock_passthrough_inner_ptr, ProcessAudio(_, _))
      .WillRepeatedly(
          [](base::span<const float> input, base::span<float> output) {
            output.copy_from_nonoverlapping(input);
          });

  StftVoiceIsolation stft(std::move(mock_passthrough_inner));
  const size_t frame_size = stft.FrameSize();
  const std::vector<float> ones(frame_size, 1.0f);
  const std::vector<float> zeros(frame_size, 0.0f);
  std::vector<float> output(frame_size, 0.0f);

  stft.ProcessAudio(ones, output);

  // Verify that processing input produces non-zero output so that the effect
  // of ClearBuffers() below is unambiguous.
  bool has_non_zero_output = false;
  for (float val : output) {
    if (val != 0.0f) {
      has_non_zero_output = true;
      break;
    }
  }
  EXPECT_TRUE(has_non_zero_output);

  // Clearing buffers should discard internal state so a subsequent frame of
  // silence produces pure silence.
  EXPECT_CALL(*mock_passthrough_inner_ptr, ClearBuffers()).Times(1);
  stft.ClearBuffers();

  stft.ProcessAudio(zeros, output);
  for (size_t i = 0; i < frame_size; ++i) {
    EXPECT_FLOAT_EQ(output[i], 0.0f);
  }
}

}  // namespace media
