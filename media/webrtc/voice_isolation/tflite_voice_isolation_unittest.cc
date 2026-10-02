// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace media {
namespace {

// Layout of the TfLiteVoiceIsolation frame: two DFTs of `kDftSize` floats
// each. The first two floats of every DFT hold the DC and Nyquist components,
// which bypass the model. The remaining `kDftSize - 2` floats are fed to the
// model and the last two model inputs are zero padding.
constexpr size_t kNumDfts = 2;
constexpr size_t kDftSize = 320;
constexpr size_t kNumBypassedFloats = 2;

// Constants of the stateful test model.
constexpr float kModelInputOffset = 1.0f;
constexpr float kModelConvGain = 0.5f;
constexpr float kModelLeakyAlpha = 0.5f;

// Step and period of the deterministic test input. All values are multiples of
// a power of two so that the model output is exact in float32.
constexpr float kTestInputStep = 0.25f;
constexpr int kTestInputPeriod = 17;
constexpr float kTestInputFrameOffset = 0.5f;

float LeakyRelu(float value) {
  return value >= 0.0f ? value : kModelLeakyAlpha * value;
}

// Returns a deterministic input frame that covers both branches of the
// LeakyRelu and changes from frame to frame.
std::vector<float> MakeTestInput(size_t frame_size, int frame_index) {
  std::vector<float> input(frame_size);
  const int half_period = kTestInputPeriod / 2;
  for (size_t i = 0; i < frame_size; ++i) {
    const int step = static_cast<int>(i) % kTestInputPeriod - half_period;
    input[i] = kTestInputStep * step + kTestInputFrameOffset * frame_index;
  }
  return input;
}

// Closed-form reference of `test_model_stateful_1_2_160_2.tflite` as seen
// through TfLiteVoiceIsolation, including the bias subtraction. Per model
// input `k`, with state `s` starting at zero:
//   prev_0 = s, prev_1 = x_0 + 1
//   y_t = x_t + leaky_relu(0.5 * prev_t)
//   s <- x_1 + 1
// The model also outputs a second channel `2 * y_t + 42`, which
// TfLiteVoiceIsolation must ignore. It is not a constant offset of `y_t`, so
// reading the wrong channel changes the output even after the bias
// subtraction.
class StatefulTestModelReference {
 public:
  StatefulTestModelReference() : state_(kDftSize, 0.0f) {}

  std::vector<float> ProcessAudio(const std::vector<float>& input) {
    CHECK_EQ(input.size(), kNumDfts * kDftSize);

    // Unpack the model input tensor, zero padding the last two values.
    std::vector<std::vector<float>> x(kNumDfts,
                                      std::vector<float>(kDftSize, 0.0f));
    for (size_t t = 0; t < kNumDfts; ++t) {
      for (size_t k = 0; k < kDftSize - kNumBypassedFloats; ++k) {
        x[t][k] = input[t * kDftSize + kNumBypassedFloats + k];
      }
    }

    // Evaluate the model and the bias (model output for zero input and zero
    // state), then write the output in the TfLiteVoiceIsolation layout.
    std::vector<float> output(input.size());
    for (size_t t = 0; t < kNumDfts; ++t) {
      output[t * kDftSize] = input[t * kDftSize];
      output[t * kDftSize + 1] = input[t * kDftSize + 1];
      for (size_t k = 0; k < kDftSize - kNumBypassedFloats; ++k) {
        const float prev = t == 0 ? state_[k] : x[0][k] + kModelInputOffset;
        const float bias_prev = t == 0 ? 0.0f : kModelInputOffset;
        const float y = x[t][k] + LeakyRelu(kModelConvGain * prev);
        const float bias = LeakyRelu(kModelConvGain * bias_prev);
        output[t * kDftSize + kNumBypassedFloats + k] = y - bias;
      }
    }

    // Update the state with the last frame.
    for (size_t k = 0; k < kDftSize; ++k) {
      state_[k] = x[kNumDfts - 1][k] + kModelInputOffset;
    }
    return output;
  }

 private:
  std::vector<float> state_;
};

}  // namespace

// TODO(crbug.com/568298417): Enable on UBSan once the tests are fixed.
#if defined(UNDEFINED_SANITIZER)
#define MAYBE_CreateWorks DISABLED_CreateWorks
#define MAYBE_ProcessAudioWorks DISABLED_ProcessAudioWorks
#define MAYBE_FailsOnIncompatibleModel DSIABLED_FailsOnIncompatibleModel
#else
#define MAYBE_CreateWorks CreateWorks
#define MAYBE_ProcessAudioWorks ProcessAudioWorks
#define MAYBE_FailsOnIncompatibleModel FailsOnIncompatibleModel
#endif
TEST(TfLiteVoiceIsolation, MAYBE_CreateWorks) {
  auto model = LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));
  EXPECT_EQ(voice_isolation->FrameSize(), 640u);
  EXPECT_EQ(voice_isolation->AlgorithmicDelay(), base::TimeDelta());
}

TEST(TfLiteVoiceIsolation, MAYBE_ProcessAudioWorks) {
  auto model = LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  std::vector<float> input(voice_isolation->FrameSize(), 1.0f);
  std::vector<float> output(voice_isolation->FrameSize(), 0.0f);

  voice_isolation->ProcessAudio(input, output);

  for (auto x : output) {
    EXPECT_NE(x, 0.0f);
  }
}

// TODO(barrerap): Enable once TfLiteVoiceIsolation resets the model resource
// variables in ClearBuffers() and after computing the bias.
TEST(TfLiteVoiceIsolation, DISABLED_ProcessAudioMatchesClosedForm) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  // A freshly created object must start from the initial model state.
  StatefulTestModelReference reference;
  std::vector<float> output(voice_isolation->FrameSize(), 0.0f);
  constexpr int kNumFrames = 3;
  for (int frame = 0; frame < kNumFrames; ++frame) {
    const std::vector<float> input =
        MakeTestInput(voice_isolation->FrameSize(), frame);
    const std::vector<float> expected = reference.ProcessAudio(input);

    voice_isolation->ProcessAudio(input, output);

    // The test model output is exact in float32.
    EXPECT_EQ(output, expected) << "frame " << frame;
  }
}

// TODO(barrerap): Enable once TfLiteVoiceIsolation resets the model resource
// variables in ClearBuffers() and after computing the bias.
TEST(TfLiteVoiceIsolation, DISABLED_ClearBuffersRestoresInitialState) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  const std::vector<float> input =
      MakeTestInput(voice_isolation->FrameSize(), /*frame_index=*/0);
  std::vector<float> output1(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output2(voice_isolation->FrameSize(), 0.0f);
  std::vector<float> output3(voice_isolation->FrameSize(), 0.0f);

  // Process the same input twice. The model state carries over, so the second
  // output must differ from the first one. Otherwise this test cannot detect
  // whether ClearBuffers() resets anything.
  voice_isolation->ProcessAudio(input, output1);
  voice_isolation->ProcessAudio(input, output2);
  ASSERT_NE(output1, output2);

  // After clearing, the model must behave as a freshly created one.
  voice_isolation->ClearBuffers();
  voice_isolation->ProcessAudio(input, output3);
  EXPECT_EQ(output1, output3);
}

// Verifies that attempting to initialize TfLiteVoiceIsolation with a FlatBuffer
// model that passes buffer verification but lacks valid execution subgraphs or
// operators fails safely and returns kInterpreterCreationFailed without
// crashing.
TEST(TfLiteVoiceIsolation, FailsOnEmptyModel) {
  FakeModel bogus = BuildBogusModel();
  ASSERT_NE(bogus.model, nullptr);

  auto result = TfLiteVoiceIsolation::MaybeCreate(bogus.model.get());
  EXPECT_THAT(result,
              base::test::ErrorIs(
                  VoiceIsolationCreationResult::kInterpreterCreationFailed));
}

// Verifies that a FlatBuffer model with incompatible tensor configuration (such
// as an unexpected frame size or identical input and output tensors) is safely
// rejected with kIncompatibleModel.
TEST(TfLiteVoiceIsolation, MAYBE_FailsOnIncompatibleModel) {
  FakeModel fake_model =
      BuildModelWithSameInputOutputTensor(/*tensor_size=*/320);
  ASSERT_NE(fake_model.model, nullptr);

  auto result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
}
}  // namespace media
