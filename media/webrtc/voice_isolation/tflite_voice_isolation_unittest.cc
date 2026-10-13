// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/aligned_memory.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/speech_mask_estimator.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace media {
namespace {

using ::testing::Each;
using ::testing::FloatNear;

// Layout of the TfLiteVoiceIsolation frame: two DFTs of `kDftSize` floats
// each. The first two floats of every DFT hold the DC and Nyquist components,
// which bypass the model. The remaining `kDftSize - 2` floats are fed to the
// model and the last two model inputs are zero padding.
constexpr size_t kNumDfts = 2;
constexpr size_t kDftSize = 320;
constexpr size_t kNumBypassedFloats = 2;

// The speech mask has one value per bin, from DC to Nyquist.
constexpr size_t kSpeechMaskSize = kDftSize / 2 + 1;

// Number of calls of the speech mask tests. The first call primes the
// overlap-add history of the mask estimation, so the checks of the mask values
// start at the second one.
constexpr size_t kNumSpeechMaskCalls = 4;
constexpr size_t kFirstCheckedSpeechMaskCall = 1;

// Seeds of two different white noise test signals.
constexpr uint64_t kFirstNoiseSeed = 1;
constexpr uint64_t kSecondNoiseSeed = 2;

// TfLiteVoiceIsolation::ProcessAudio() returns kDereverbWeight times the
// dereverberated output of the model plus the rest of its denoising-only
// output.
constexpr float kDereverbWeight = 0.9f;

// Constants of the stateful test model. Its second output is
// kModelDenoisingOnlyGain times the first one plus a constant offset, which the
// bias subtraction removes.
constexpr float kModelInputOffset = 1.0f;
constexpr float kModelConvGain = 0.5f;
constexpr float kModelLeakyAlpha = 0.5f;
constexpr float kModelDenoisingOnlyGain = 2.0f;

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
// The model also outputs a second channel `2 * y_t + 42`, the denoising-only
// output. After the bias subtraction it is twice the first channel, so
// ProcessAudio() returns (0.9 + 0.1 * 2) times the first channel. The second
// channel is not a constant offset of `y_t`, so reading the wrong channel
// changes the output even after the bias subtraction.
class StatefulTestModelReference {
 public:
  StatefulTestModelReference()
      : state_(kDftSize, 0.0f), denoising_only_(kNumDfts * kDftSize, 0.0f) {}

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
    // state), then write the mix of both outputs in the TfLiteVoiceIsolation
    // layout. The denoising-only output keeps the DC and Nyquist components of
    // the input, as in TfLiteVoiceIsolation.
    std::vector<float> output(input.size());
    for (size_t t = 0; t < kNumDfts; ++t) {
      output[t * kDftSize] = input[t * kDftSize];
      output[t * kDftSize + 1] = input[t * kDftSize + 1];
      denoising_only_[t * kDftSize] = input[t * kDftSize];
      denoising_only_[t * kDftSize + 1] = input[t * kDftSize + 1];
      for (size_t k = 0; k < kDftSize - kNumBypassedFloats; ++k) {
        const float prev = t == 0 ? state_[k] : x[0][k] + kModelInputOffset;
        const float bias_prev = t == 0 ? 0.0f : kModelInputOffset;
        const float y = x[t][k] + LeakyRelu(kModelConvGain * prev);
        const float bias = LeakyRelu(kModelConvGain * bias_prev);
        const float dereverberated = y - bias;
        const float denoising_only = kModelDenoisingOnlyGain * dereverberated;
        output[t * kDftSize + kNumBypassedFloats + k] =
            kDereverbWeight * dereverberated +
            (1.0f - kDereverbWeight) * denoising_only;
        denoising_only_[t * kDftSize + kNumBypassedFloats + k] = denoising_only;
      }
    }

    // Update the state with the last frame.
    for (size_t k = 0; k < kDftSize; ++k) {
      state_[k] = x[kNumDfts - 1][k] + kModelInputOffset;
    }
    return output;
  }

  // Denoising-only output DFTs of the last ProcessAudio() call.
  const std::vector<float>& denoising_only() const { return denoising_only_; }

 private:
  std::vector<float> state_;
  std::vector<float> denoising_only_;
};

// Feeds `voice_isolation` the calls of `input_dfts` and returns the speech
// mask after each call.
std::vector<std::vector<float>> ProcessAndGetSpeechMasks(
    TfLiteVoiceIsolation& voice_isolation,
    const std::vector<std::vector<float>>& input_dfts) {
  std::vector<std::vector<float>> masks;
  std::vector<float> output(voice_isolation.FrameSize());
  for (const std::vector<float>& call_dfts : input_dfts) {
    voice_isolation.ProcessAudio(call_dfts, output);
    masks.push_back(base::ToVector(voice_isolation.SpeechMask()));
  }
  return masks;
}

}  // namespace

TEST(TfLiteVoiceIsolation, CreateWorks) {
  auto model = LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(auto voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));
  EXPECT_EQ(voice_isolation->FrameSize(), 640u);
  EXPECT_EQ(voice_isolation->AlgorithmicDelay(), base::TimeDelta());
}

TEST(TfLiteVoiceIsolation, ProcessAudioWorks) {
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

// ProcessAudio() returns 0.9 times the dereverberated output plus 0.1 times
// the denoising-only output. After the bias subtraction, the denoising-only
// output of the stateful test model is twice its dereverberated output, so the
// closed form expects 1.1 times the dereverberated output: returning the
// dereverberated output alone, or unpacking the wrong channel, gives 1.0 and
// swapped weights give 1.9. DC and Nyquist bypass the model.
TEST(TfLiteVoiceIsolation,
     ProcessAudioMixesDereverberatedAndDenoisingOnlyOutputs) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadStatefulVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  // The first call depends on the model state that the bias computation in
  // MaybeCreate() leaves behind. Every call overwrites the whole state, so
  // after one priming call the output follows the closed form exactly.
  // TODO(barrerap): Check from frame 0 without priming and deduplicate with
  // ProcessAudioMatchesClosedForm once TfLiteVoiceIsolation resets the model
  // resource variables after computing the bias.
  StatefulTestModelReference reference;
  std::vector<float> output(voice_isolation->FrameSize());
  const std::vector<float> priming_input =
      MakeTestInput(voice_isolation->FrameSize(), /*frame_index=*/0);
  reference.ProcessAudio(priming_input);
  voice_isolation->ProcessAudio(priming_input, output);

  constexpr int kNumCheckedFrames = 3;
  for (int frame = 1; frame <= kNumCheckedFrames; ++frame) {
    const std::vector<float> input =
        MakeTestInput(voice_isolation->FrameSize(), frame);
    const std::vector<float> expected = reference.ProcessAudio(input);

    voice_isolation->ProcessAudio(input, output);

    // The test model output is exact in float32, and the mix runs the same
    // float operations as the reference.
    EXPECT_EQ(output, expected) << "frame " << frame;
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

TEST(TfLiteVoiceIsolation, SpeechMaskStartsAtOne) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  EXPECT_EQ(voice_isolation->SpeechMask().size(), kSpeechMaskSize);
  EXPECT_THAT(base::ToVector(voice_isolation->SpeechMask()), Each(1.0f));
}

// Both outputs of the test model equal its input, so the denoising-only output
// equals the input DFTs and nothing is suppressed.
TEST(TfLiteVoiceIsolation, SpeechMaskIsNearOneForIdentityModel) {
  constexpr float kTolerance = 1e-3f;
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  const std::vector<std::vector<float>> masks = ProcessAndGetSpeechMasks(
      *voice_isolation, MakeNoiseDfts(kFirstNoiseSeed, /*dft_size=*/kDftSize,
                                      /*num_calls=*/kNumSpeechMaskCalls));

  for (size_t call = kFirstCheckedSpeechMaskCall; call < kNumSpeechMaskCalls;
       ++call) {
    EXPECT_THAT(masks[call], Each(FloatNear(1.0f, kTolerance)))
        << "call " << call;
  }
}

// The speech mask comes from the denoising-only output alone, not from the
// output mix: it matches a SpeechMaskEstimator fed the closed-form
// denoising-only output of the stateful test model, whose mask differs from
// that of the mix ProcessAudio() returns.
TEST(TfLiteVoiceIsolation, SpeechMaskIgnoresDereverberatedOutput) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadStatefulVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));

  // Reference estimators fed the closed-form denoising-only output, which the
  // mask must come from, and the closed-form mix, which it must not.
  StatefulTestModelReference reference;
  SpeechMaskEstimator denoising_only_estimator(/*dft_size=*/kDftSize);
  SpeechMaskEstimator mix_estimator(/*dft_size=*/kDftSize);
  base::AlignedHeapArray<float> reference_dfts = base::AlignedUninit<float>(
      kNumDfts * kDftSize, SpeechMaskEstimator::kDftAlignment);
  std::vector<float> output(voice_isolation->FrameSize());

  // Feeds one frame to TfLiteVoiceIsolation and to both reference estimators.
  const auto process_frame = [&](int frame) {
    const std::vector<float> input =
        MakeTestInput(voice_isolation->FrameSize(), frame);
    const std::vector<float> mix = reference.ProcessAudio(input);

    base::span(reference_dfts).copy_from(reference.denoising_only());
    denoising_only_estimator.Estimate(input, reference_dfts);
    base::span(reference_dfts).copy_from(mix);
    mix_estimator.Estimate(input, reference_dfts);

    voice_isolation->ProcessAudio(input, output);
  };

  // The first DFT of the first call depends on the model state that the bias
  // computation in MaybeCreate() leaves behind, so the first call only primes
  // the model and the overlap-add history of the estimators.
  process_frame(/*frame=*/0);

  constexpr int kNumCheckedFrames = 3;
  for (int frame = 1; frame <= kNumCheckedFrames; ++frame) {
    process_frame(frame);
    const std::vector<float> expected_mask =
        base::ToVector(denoising_only_estimator.mask());

    // The test input must tell the two sources of the mask apart.
    ASSERT_NE(expected_mask, base::ToVector(mix_estimator.mask()))
        << "frame " << frame;
    EXPECT_EQ(base::ToVector(voice_isolation->SpeechMask()), expected_mask)
        << "frame " << frame;
  }
}

TEST(TfLiteVoiceIsolation, ProcessAudioSupportsInPlaceAliasing) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> out_of_place_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> in_place_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));

  const std::vector<std::vector<float>> input_dfts =
      MakeNoiseDfts(kFirstNoiseSeed, /*dft_size=*/kDftSize,
                    /*num_calls=*/kNumSpeechMaskCalls);
  std::vector<float> out_of_place_output(
      out_of_place_voice_isolation->FrameSize());

  for (size_t call = 0; call < kNumSpeechMaskCalls; ++call) {
    out_of_place_voice_isolation->ProcessAudio(input_dfts[call],
                                               out_of_place_output);

    std::vector<float> in_place_buffer = input_dfts[call];
    in_place_voice_isolation->ProcessAudio(in_place_buffer, in_place_buffer);

    EXPECT_EQ(in_place_buffer, out_of_place_output) << "call " << call;
    EXPECT_EQ(base::ToVector(in_place_voice_isolation->SpeechMask()),
              base::ToVector(out_of_place_voice_isolation->SpeechMask()))
        << "call " << call;
  }
}

TEST(TfLiteVoiceIsolation, ClearBuffersResetsSpeechMask) {
  std::unique_ptr<tflite::FlatBufferModel> model =
      LoadVoiceIsolationTestModel();
  ASSERT_NE(model, nullptr);

  // Both signals have the same level: a much louder first signal would mostly
  // raise the magnitudes of the second one, which the cap of the mask at 1
  // hides.
  const std::vector<std::vector<float>> first_signal =
      MakeNoiseDfts(kFirstNoiseSeed, /*dft_size=*/kDftSize,
                    /*num_calls=*/kNumSpeechMaskCalls);
  const std::vector<std::vector<float>> second_signal =
      MakeNoiseDfts(kSecondNoiseSeed, /*dft_size=*/kDftSize,
                    /*num_calls=*/kNumSpeechMaskCalls);

  // Reference: a fresh instance fed the second signal.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> fresh_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));
  const std::vector<std::vector<float>> expected_masks =
      ProcessAndGetSpeechMasks(*fresh_voice_isolation, second_signal);

  // Without ClearBuffers(), the first signal changes the mask of the second
  // one. Otherwise this test could not detect whether ClearBuffers() resets the
  // mask estimation.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TfLiteVoiceIsolation> uncleared_voice_isolation,
      TfLiteVoiceIsolation::MaybeCreate(model.get()));
  ProcessAndGetSpeechMasks(*uncleared_voice_isolation, first_signal);
  ASSERT_NE(ProcessAndGetSpeechMasks(*uncleared_voice_isolation, second_signal),
            expected_masks);

  // ClearBuffers() resets the mask to all ones, and the mask estimation then
  // behaves as in a fresh instance.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TfLiteVoiceIsolation> voice_isolation,
                       TfLiteVoiceIsolation::MaybeCreate(model.get()));
  ProcessAndGetSpeechMasks(*voice_isolation, first_signal);
  voice_isolation->ClearBuffers();
  EXPECT_THAT(base::ToVector(voice_isolation->SpeechMask()), Each(1.0f));
  EXPECT_EQ(ProcessAndGetSpeechMasks(*voice_isolation, second_signal),
            expected_masks);
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
TEST(TfLiteVoiceIsolation, FailsOnIncompatibleModel) {
  FakeModel fake_model =
      BuildModelWithSameInputOutputTensor(/*tensor_size=*/320);
  ASSERT_NE(fake_model.model, nullptr);

  // TODO(crbug.com/568298417): Remove the UBSan skip once TFLite no longer
  // calls memcpy() with a null pointer for models without operators.
#if defined(UNDEFINED_SANITIZER)
  GTEST_SKIP() << "TFLite has undefined behavior for models without operators.";
#else
  auto result = TfLiteVoiceIsolation::MaybeCreate(fake_model.model.get());
  EXPECT_THAT(result, base::test::ErrorIs(
                          VoiceIsolationCreationResult::kIncompatibleModel));
#endif
}
}  // namespace media
