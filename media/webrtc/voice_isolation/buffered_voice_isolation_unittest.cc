// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/buffered_voice_isolation.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/mock_voice_isolation.h"
#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using testing::_;
using testing::Each;
using testing::Return;

// Realistic configuration: a 20 ms internal frame at 48 kHz, driven by 10 ms
// APM frames.
constexpr size_t kInternalFrameSize = 960;
constexpr size_t kInternalFramesPerSecond = 50;
constexpr size_t kExternalFrameSize = kInternalFrameSize / 2;
constexpr size_t kExternalFramesPerSecond = kInternalFramesPerSecond * 2;
constexpr size_t kNumStreamedFrames = 10;

// Values used to tell apart the two external frames and the internal output.
constexpr float kFirstInputValue = 1.0f;
constexpr float kSecondInputValue = 2.0f;
constexpr float kThirdInputValue = 3.0f;
constexpr float kFirstHalfOutputValue = 10.0f;
constexpr float kSecondHalfOutputValue = 20.0f;

// Returns a mock internal component with the realistic configuration. NiceMock
// silences the uninteresting getter calls made by the constructor.
std::unique_ptr<MockVoiceIsolationComponent> CreateMockInternal() {
  auto mock_internal =
      std::make_unique<testing::NiceMock<MockVoiceIsolationComponent>>();
  ON_CALL(*mock_internal, FrameSize())
      .WillByDefault(Return(kInternalFrameSize));
  ON_CALL(*mock_internal, FramesPerSecond())
      .WillByDefault(Return(kInternalFramesPerSecond));
  return mock_internal;
}

std::unique_ptr<BufferedVoiceIsolation> CreateWithPassthrough() {
  return std::make_unique<BufferedVoiceIsolation>(
      std::make_unique<PassthroughVoiceIsolation>(kInternalFrameSize,
                                                  kInternalFramesPerSecond));
}

// Checks that the external frame size is half, and the calling frequency
// double, those of the internal component.
TEST(BufferedVoiceIsolationTest, FrameSizeAndFramesPerSecond) {
  BufferedVoiceIsolation buffered(CreateMockInternal());

  EXPECT_EQ(buffered.FrameSize(), kExternalFrameSize);
  EXPECT_EQ(buffered.FramesPerSecond(), kExternalFramesPerSecond);
}

// Checks that the buffering adds one external frame, 10 ms at 48 kHz, on top of
// the delay of the internal component.
TEST(BufferedVoiceIsolationTest, AlgorithmicDelayAddsOneExternalFrame) {
  constexpr base::TimeDelta kInternalDelay = base::Milliseconds(5);
  constexpr base::TimeDelta kExternalFrameDuration = base::Milliseconds(10);
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  ON_CALL(*mock_internal, AlgorithmicDelay())
      .WillByDefault(Return(kInternalDelay));
  BufferedVoiceIsolation buffered(std::move(mock_internal));

  EXPECT_EQ(buffered.AlgorithmicDelay(),
            kInternalDelay + kExternalFrameDuration);
}

// Checks that two external frames are concatenated into one internal call and
// that the internal output is returned half by half, one frame behind.
TEST(BufferedVoiceIsolationTest, ProcessAudioBuffering) {
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  MockVoiceIsolationComponent* mock_internal_ptr = mock_internal.get();
  BufferedVoiceIsolation buffered(std::move(mock_internal));

  // First call: only buffers the input and outputs silence.
  std::vector<float> input1(kExternalFrameSize, kFirstInputValue);
  std::vector<float> output1(kExternalFrameSize, kFirstInputValue);
  EXPECT_CALL(*mock_internal_ptr, ProcessAudio(_, _)).Times(0);
  buffered.ProcessAudio(input1, output1);
  EXPECT_THAT(output1, Each(0.0f));
  testing::Mock::VerifyAndClearExpectations(mock_internal_ptr);

  // Second call: runs the internal component on both frames and outputs the
  // first half of its output.
  std::vector<float> input2(kExternalFrameSize, kSecondInputValue);
  std::vector<float> output2(kExternalFrameSize);
  EXPECT_CALL(*mock_internal_ptr, ProcessAudio(_, _))
      .WillOnce([](base::span<const float> input, base::span<float> output) {
        ASSERT_EQ(input.size(), kInternalFrameSize);
        ASSERT_EQ(output.size(), kInternalFrameSize);
        EXPECT_THAT(input.first(kExternalFrameSize), Each(kFirstInputValue));
        EXPECT_THAT(input.subspan(kExternalFrameSize), Each(kSecondInputValue));
        base::span<float> first_half = output.first(kExternalFrameSize);
        base::span<float> second_half = output.subspan(kExternalFrameSize);
        std::fill(first_half.begin(), first_half.end(), kFirstHalfOutputValue);
        std::fill(second_half.begin(), second_half.end(),
                  kSecondHalfOutputValue);
      });
  buffered.ProcessAudio(input2, output2);
  EXPECT_THAT(output2, Each(kFirstHalfOutputValue));
  testing::Mock::VerifyAndClearExpectations(mock_internal_ptr);

  // Third call: only buffers and outputs the second half of the previous
  // internal output.
  std::vector<float> input3(kExternalFrameSize, kThirdInputValue);
  std::vector<float> output3(kExternalFrameSize);
  EXPECT_CALL(*mock_internal_ptr, ProcessAudio(_, _)).Times(0);
  buffered.ProcessAudio(input3, output3);
  EXPECT_THAT(output3, Each(kSecondHalfOutputValue));
}

// Checks that the internal component is called once every two external calls.
TEST(BufferedVoiceIsolationTest, CorrectTotalNumberOfCalls) {
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  MockVoiceIsolationComponent* mock_internal_ptr = mock_internal.get();
  BufferedVoiceIsolation buffered(std::move(mock_internal));

  std::vector<float> input(kExternalFrameSize, kFirstInputValue);
  std::vector<float> output(kExternalFrameSize);

  EXPECT_CALL(*mock_internal_ptr, ProcessAudio(_, _))
      .Times(kNumStreamedFrames / 2);
  for (size_t frame = 0; frame < kNumStreamedFrames; ++frame) {
    buffered.ProcessAudio(input, output);
  }
}

// Streams constant-valued frames through a passthrough component and checks
// that each output equals the previous input, i.e. exactly one external frame
// of latency.
TEST(BufferedVoiceIsolationTest, StreamingAddsOneExternalFrameOfLatency) {
  std::unique_ptr<BufferedVoiceIsolation> buffered = CreateWithPassthrough();
  std::vector<float> input(kExternalFrameSize);
  std::vector<float> output(kExternalFrameSize);

  for (size_t frame = 0; frame < kNumStreamedFrames; ++frame) {
    std::fill(input.begin(), input.end(), static_cast<float>(frame + 1));
    buffered->ProcessAudio(input, output);
    EXPECT_THAT(output, Each(static_cast<float>(frame))) << "frame " << frame;
  }
}

// Checks that `input` and `output` may refer to the same buffer, as used by
// VoiceIsolation for mono audio processed in place.
TEST(BufferedVoiceIsolationTest,
     InPlaceProcessingAddsOneExternalFrameOfLatency) {
  std::unique_ptr<BufferedVoiceIsolation> buffered = CreateWithPassthrough();
  std::vector<float> buffer(kExternalFrameSize);

  for (size_t frame = 0; frame < kNumStreamedFrames; ++frame) {
    std::fill(buffer.begin(), buffer.end(), static_cast<float>(frame + 1));
    buffered->ProcessAudio(buffer, buffer);
    EXPECT_THAT(buffer, Each(static_cast<float>(frame))) << "frame " << frame;
  }
}

// Checks that ClearBuffers() drops the buffered audio and restarts at the
// first half, even when called in the middle of an internal frame.
TEST(BufferedVoiceIsolationTest, ClearBuffersResetsPhaseAndHistory) {
  std::unique_ptr<BufferedVoiceIsolation> buffered = CreateWithPassthrough();
  std::vector<float> input(kExternalFrameSize, kFirstInputValue);
  std::vector<float> output(kExternalFrameSize);

  // Leave the wrapper mid-frame with non-zero history.
  buffered->ProcessAudio(input, output);
  buffered->ProcessAudio(input, output);
  buffered->ProcessAudio(input, output);

  // After a reset, the first call outputs silence again.
  buffered->ClearBuffers();
  std::fill(input.begin(), input.end(), kSecondInputValue);
  buffered->ProcessAudio(input, output);
  EXPECT_THAT(output, Each(0.0f));

  // The second call completes a fresh internal frame.
  buffered->ProcessAudio(input, output);
  EXPECT_THAT(output, Each(kSecondInputValue));
}

// Checks that ClearBuffers() is forwarded to the internal component.
TEST(BufferedVoiceIsolationTest, ClearBuffersPropagatesToInternalComponent) {
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  EXPECT_CALL(*mock_internal, ClearBuffers()).Times(1);
  BufferedVoiceIsolation buffered(std::move(mock_internal));

  buffered.ClearBuffers();
}

TEST(BufferedVoiceIsolationDeathTest, OddInternalFrameSizeCrashes) {
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  ON_CALL(*mock_internal, FrameSize())
      .WillByDefault(Return(kInternalFrameSize + 1));

  EXPECT_CHECK_DEATH(BufferedVoiceIsolation(std::move(mock_internal)));
}

// A zero frame size is even, so it must be rejected by its own check.
TEST(BufferedVoiceIsolationDeathTest, ZeroInternalFrameSizeCrashes) {
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  ON_CALL(*mock_internal, FrameSize()).WillByDefault(Return(0u));

  EXPECT_CHECK_DEATH(BufferedVoiceIsolation(std::move(mock_internal)));
}

// A zero rate would otherwise only fail later, inside AlgorithmicDelay().
TEST(BufferedVoiceIsolationDeathTest, ZeroInternalFramesPerSecondCrashes) {
  std::unique_ptr<MockVoiceIsolationComponent> mock_internal =
      CreateMockInternal();
  ON_CALL(*mock_internal, FramesPerSecond()).WillByDefault(Return(0u));

  EXPECT_CHECK_DEATH(BufferedVoiceIsolation(std::move(mock_internal)));
}

TEST(BufferedVoiceIsolationDeathTest, NullInternalComponentCrashes) {
  EXPECT_CHECK_DEATH(
      BufferedVoiceIsolation(/*internal_voice_isolation=*/nullptr));
}

TEST(BufferedVoiceIsolationDeathTest, WrongInputSizeCrashes) {
  std::unique_ptr<BufferedVoiceIsolation> buffered = CreateWithPassthrough();
  std::vector<float> input(kExternalFrameSize + 1);
  std::vector<float> output(kExternalFrameSize);

  EXPECT_CHECK_DEATH(buffered->ProcessAudio(input, output));
}

TEST(BufferedVoiceIsolationDeathTest, WrongOutputSizeCrashes) {
  std::unique_ptr<BufferedVoiceIsolation> buffered = CreateWithPassthrough();
  std::vector<float> input(kExternalFrameSize);
  std::vector<float> output(kExternalFrameSize + 1);

  EXPECT_CHECK_DEATH(buffered->ProcessAudio(input, output));
}

}  // namespace
}  // namespace media
