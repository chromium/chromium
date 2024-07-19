// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <array>
#include <limits>
#include "services/audio/mixing_graph.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
// Test fixture to verify the functionality of the mixing graph inputs.
class MixingGraphInputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    output_params_ =
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Mono(), 48000, 480);
    mixing_graph_ = MixingGraph::Create(
        output_params_,
        base::BindRepeating(&MixingGraphInputTest::OnMoreDataCallBack,
                            base::Unretained(this)),
        base::BindRepeating(&MixingGraphInputTest::OnErrorCallback,
                            base::Unretained(this)));
    dest_ = media::AudioBus::Create(output_params_);
  }

  void PullAndVerifyData(int num_runs,
                         float expected_first_sample,
                         float expected_sample_increment,
                         float epsilon) {
    float expected_data = expected_first_sample;
    for (int i = 0; i < num_runs; i++) {
      mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                                dest_.get());
      float* data = dest_.get()->channel(0);
      for (int j = 0; j < dest_.get()->frames(); ++j) {
        EXPECT_NEAR(data[j], expected_data, epsilon);
        expected_data += expected_sample_increment;
      }
    }
  }

  void OnMoreDataCallBack(const media::AudioBus&, base::TimeDelta) {}
  void OnErrorCallback(
      media::AudioOutputStream::AudioSourceCallback::ErrorType) {}

  media::AudioParameters output_params_;
  std::unique_ptr<MixingGraph> mixing_graph_;
  std::unique_ptr<media::AudioBus> dest_;
};

// Simple audio source callback where a sample value is the value of the
// previous sample plus |increment|. When using stereo the values of the right
// channel will be the values of the left channel plus |increment|.
class SampleCounter : public media::AudioOutputStream::AudioSourceCallback {
 public:
  explicit SampleCounter(float counter, float increment)
      : counter_(counter), increment_(increment) {}
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final {
    // Fill the audio bus with a simple, predictable pattern.
    for (int channel = 0; channel < dest->channels(); ++channel) {
      float* data = dest->channel(channel);
      for (int frame = 0; frame < dest->frames(); frame++) {
        data[frame] = counter_ + increment_ * frame + increment_ * channel;
      }
    }
    counter_ += static_cast<float>(increment_ * dest->frames());
    return 0;
  }
  void OnError(ErrorType type) final {}

 private:
  float counter_ = 0.0f;
  const float increment_;
};

// Simple audio source callback where all samples are set to the same value.
// The value is incremented by |increment| for each callback.
class CallbackCounter : public media::AudioOutputStream::AudioSourceCallback {
 public:
  explicit CallbackCounter(float counter, float increment)
      : counter_(counter), increment_(increment) {}
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final {
    // Fill the audio bus with the counter value.
    for (int channel = 0; channel < dest->channels(); ++channel) {
      float* data = dest->channel(channel);
      for (int frame = 0; frame < dest->frames(); frame++) {
        data[frame] = counter_;
      }
    }
    counter_ += increment_;
    return 0;
  }
  void OnError(ErrorType type) final {}

 private:
  float counter_ = 0.0f;
  const float increment_;
};

// Simple audio source callback where all samples are set to a specified value.
class ConstantInput : public media::AudioOutputStream::AudioSourceCallback {
 public:
  explicit ConstantInput(float value) : value_(value) {}
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final {
    // Fill the audio bus with the value specified at construction.
    for (int channel = 0; channel < dest->channels(); ++channel) {
      float* data = dest->channel(channel);
      for (int frame = 0; frame < dest->frames(); frame++) {
        data[frame] = value_;
      }
    }
    return 0;
  }
  void OnError(ErrorType type) final {}

 private:
  const float value_;
};

class GlitchInfoCounter : public media::AudioOutputStream::AudioSourceCallback {
 public:
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final {
    cumulative_glitch_info_ += glitch_info;
    return 0;
  }

  void OnError(ErrorType type) final {}

  media::AudioGlitchInfo cumulative_glitch_info() const {
    return cumulative_glitch_info_;
  }

 private:
  media::AudioGlitchInfo cumulative_glitch_info_;
};

// Verifies that the mixing graph outputs zeros when no inputs have been added.
TEST_F(MixingGraphInputTest, NoInputs) {
  // The mixing graph is expected to output zeros when it has no inputs.
  PullAndVerifyData(/*num_runs=*/2, /*expected_first_sample=*/0.0f,
                    /*expected_sample_increment=*/0.0f, /*epsilon=*/0.0f);
}

// Verifies the output of a single input with the same parameters as the mixing
// graph.
TEST_F(MixingGraphInputTest, SingleInput) {
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(output_params_);
  input->Start(&source_callback);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/kCounterIncrement,
                    /*epsilon=*/1e-5f);
  input->Stop();
}

// Verifies the output of the mixing graph when adding multiple inputs.
TEST_F(MixingGraphInputTest, MultipleInputs) {
  constexpr float kInitialCounterValue1 = 0.1f;
  constexpr float kInitialCounterValue2 = 0.5f;
  constexpr float kInitialCounterValue3 = -0.7f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback1(kInitialCounterValue1, kCounterIncrement);
  SampleCounter source_callback2(kInitialCounterValue2, kCounterIncrement);
  SampleCounter source_callback3(kInitialCounterValue3, kCounterIncrement);
  auto input1 = mixing_graph_->CreateInput(output_params_);
  input1->Start(&source_callback1);
  auto input2 = mixing_graph_->CreateInput(output_params_);
  input2->Start(&source_callback2);
  auto input3 = mixing_graph_->CreateInput(output_params_);
  input3->Start(&source_callback3);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue1 +
                        kInitialCounterValue2 + kInitialCounterValue3,
                    /*expected_sample_increment=*/3.0f * kCounterIncrement,
                    /*epsilon=*/1e-5f);
  input1->Stop();
  input2->Stop();
  input3->Stop();
}

// Verifies the mixing graph output when adding an input in need of channel
// mixing.
TEST_F(MixingGraphInputTest, ChannelMixing) {
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  // The right channel has the values of the left channel + kCounterIncrement.
  // When down-mixing (averaging) the two channels this will cause a bias of
  // kCounterIncrement/2.
  PullAndVerifyData(
      /*num_runs=*/2,
      /*expected_first_sample=*/kInitialCounterValue + 0.5f * kCounterIncrement,
      /*expected_sample_increment=*/kCounterIncrement, /*epsilon=*/1e-5f);
  input->Stop();
}

// Verifies the mixing graph output when adding an input in need of resampling.
TEST_F(MixingGraphInputTest, Resampling) {
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 24000, 480);
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  // The input signal will increase by kCounterIncrement for each sample and be
  // upsampled by a factor 2. The output will therefore increase by
  // ~kCounterIncrement/2 per sample.
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/0.5f * kCounterIncrement,
                    /*epsilon=*/1e-5f);
  input->Stop();
}

// Verifies the use of FIFO when an input produces less data per call than
// requested by the mixing graph.
TEST_F(MixingGraphInputTest, Buffering1) {
  // Input produces 5 ms of audio. Output consumes 10 ms.
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 240);
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/kCounterIncrement,
                    /*epsilon=*/1e-5f);
  input->Stop();
}

// Verifies the use of FIFO when an input produces more data per call than
// requested by the mixing graph.
TEST_F(MixingGraphInputTest, Buffering2) {
  // Input produces 15 ms of audio. Output consumes 10 ms.
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 720);
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/kCounterIncrement,
                    /*epsilon=*/1e-5f);
  input->Stop();
}

// Verifies that no left-over samples from the FIFO are pulled after stopping
// and restarting the input.
TEST_F(MixingGraphInputTest, BufferClearedAtRestart) {
  // Input produces 15 ms of audio. Output consumes 10 ms.
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 720);
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  CallbackCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);

  // Get the last sample of the first output.
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                            dest_.get());
  float last_sample = dest_.get()->channel(0)[dest_.get()->frames() - 1];

  // Stop and restart.
  input->Stop();
  input->Start(&source_callback);

  // Get the first sample of the second output.
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                            dest_.get());
  float first_sample = dest_.get()->channel(0)[0];

  // If the first sample of the second output is equal to the last sample of
  // the first output left-over data has been consumed.
  EXPECT_NE(first_sample, last_sample);
  input->Stop();
}

// Verifies the output of the mixing graph when adding and removing inputs on
// the fly.
TEST_F(MixingGraphInputTest, AddingAndRemovingInputs) {
  constexpr float kInitialCounterValue1 = -0.3f;
  constexpr float kInitialCounterValue2 = 0.2f;
  constexpr float kCounterIncrement = 1e-4f;
  SampleCounter source_callback1(kInitialCounterValue1, kCounterIncrement);
  SampleCounter source_callback2(kInitialCounterValue2, kCounterIncrement);
  auto input1 = mixing_graph_->CreateInput(output_params_);
  auto input2 = mixing_graph_->CreateInput(output_params_);

  // Start the first input.
  input1->Start(&source_callback1);
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/kInitialCounterValue1,
                    /*expected_sample_increment=*/kCounterIncrement,
                    /*epsilon=*/1e-5f);

  // Start the second input.
  input2->Start(&source_callback2);
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/kInitialCounterValue1 +
                        kCounterIncrement * dest_->frames() +
                        kInitialCounterValue2,
                    /*expected_sample_increment=*/2.0f * kCounterIncrement,
                    /*epsilon=*/1e-5f);

  // Stop the first input.
  input1->Stop();
  PullAndVerifyData(
      /*num_runs=*/1,
      /*expected_first_sample=*/kInitialCounterValue2 +
          kCounterIncrement * dest_->frames(),
      /*expected_sample_increment=*/kCounterIncrement, /*epsilon=*/1e-5f);

  // Stop the second input.
  input2->Stop();
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/0.f,
                    /*expected_sample_increment=*/0.0f, /*epsilon=*/0.0f);
}

// Verifies that the volume is applied correctly.
TEST_F(MixingGraphInputTest, SetVolume) {
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kCounterIncrement = 1e-4f;
  constexpr float kVolume1 = 0.77f;
  constexpr float kVolume2 = 0.13f;
  SampleCounter source_callback(kInitialCounterValue, kCounterIncrement);
  auto input = mixing_graph_->CreateInput(output_params_);
  input->Start(&source_callback);
  input->SetVolume(kVolume1);
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/kInitialCounterValue * kVolume1,
                    /*expected_sample_increment=*/kVolume1 * kCounterIncrement,
                    /*epsilon=*/1e-5f);
  input->SetVolume(kVolume2);
  PullAndVerifyData(
      /*num_runs=*/1,
      /*expected_first_sample=*/
      (kInitialCounterValue + kCounterIncrement * dest_->frames()) * kVolume2,
      /*expected_sample_increment=*/kVolume2 * kCounterIncrement,
      /*epsilon=*/1e-5f);
  input->Stop();
}

// Verifies that out-of-range output values are sanitized.
TEST_F(MixingGraphInputTest, OutOfRange) {
  constexpr float kInputValue1 = -0.6f;
  constexpr float kInputValue2 = -0.5f;
  ConstantInput source_callback1(kInputValue1);
  ConstantInput source_callback2(kInputValue2);
  auto input1 = mixing_graph_->CreateInput(output_params_);
  input1->Start(&source_callback1);
  auto input2 = mixing_graph_->CreateInput(output_params_);
  input2->Start(&source_callback2);
  // The two inputs should add to -1.1 and be clamped to -1.0.
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/-1.0f,
                    /*expected_sample_increment=*/0.0f, /*epsilon=*/0.0f);
  // Lowering the volume of input 1 removes the need of clamping.
  input1->SetVolume(0.5f);
  PullAndVerifyData(
      /*num_runs=*/1,
      /*expected_first_sample=*/kInputValue1 * 0.5f + kInputValue2,
      /*expected_sample_increment=*/0.0f, /*epsilon=*/0.0f);
  input1->Stop();
  input2->Stop();
}

// Verifies that invalid input is sanitized.
TEST_F(MixingGraphInputTest, InvalidInput) {
  // Pairs of input values and expected output values.
  std::array<std::pair<float, float>, 8> test_values = {{
      {-1.5f, -1.0f},                                    // Negative overflow.
      {2.0f, 1.0f},                                      // Positive overflow.
      {-0.8, -0.8f},                                     // Valid.
      {0.3, 0.3f},                                       // Valid.
      {0.0, 0.0f},                                       // Valid.
      {std::numeric_limits<float>::infinity(), 1.0f},    // Positive infinity.
      {-std::numeric_limits<float>::infinity(), -1.0f},  // Negative infinity.
      {NAN, 1.0f},                                       // NaN.
  }};
  for (const auto& test_pair : test_values) {
    float input_value = test_pair.first;
    float expected_output_value = test_pair.second;

    auto input = mixing_graph_->CreateInput(output_params_);
    ConstantInput source_callback(input_value);
    input->Start(&source_callback);
    PullAndVerifyData(
        /*num_runs=*/1,
        /*expected_first_sample=*/expected_output_value,
        /*expected_sample_increment=*/0.0f, /*epsilon=*/0.0f);
    input->Stop();
  }
}

// Verifies that the graph propagates glitch info to all inputs.
TEST_F(MixingGraphInputTest, PropagatesGlitchInfo) {
  GlitchInfoCounter source_callback1;
  GlitchInfoCounter source_callback2;
  GlitchInfoCounter source_callback3;
  auto input1 = mixing_graph_->CreateInput(output_params_);
  input1->Start(&source_callback1);
  auto input2 = mixing_graph_->CreateInput(output_params_);
  input2->Start(&source_callback2);

  // Add an input that needs resampling to make the graph more interesting.
  media::AudioParameters different_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), output_params_.sample_rate() * 2,
      480);
  auto input3 = mixing_graph_->CreateInput(different_params);
  input3->Start(&source_callback3);

  // Send empty glitch info.
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                            dest_.get());
  EXPECT_EQ(source_callback1.cumulative_glitch_info(),
            media::AudioGlitchInfo());
  EXPECT_EQ(source_callback2.cumulative_glitch_info(),
            media::AudioGlitchInfo());
  EXPECT_EQ(source_callback3.cumulative_glitch_info(),
            media::AudioGlitchInfo());

  // Send some glitch info and expect this to be propagated.
  media::AudioGlitchInfo glitch_info{.duration = base::Seconds(5),
                                     .count = 123};
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(),
                            glitch_info, dest_.get());
  EXPECT_EQ(source_callback1.cumulative_glitch_info(), glitch_info);
  EXPECT_EQ(source_callback2.cumulative_glitch_info(), glitch_info);
  EXPECT_EQ(source_callback3.cumulative_glitch_info(), glitch_info);

  // Send empty glitch info, and expect the cumulative glitch info to remain the
  // same.
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                            dest_.get());
  EXPECT_EQ(source_callback1.cumulative_glitch_info(), glitch_info);
  EXPECT_EQ(source_callback2.cumulative_glitch_info(), glitch_info);
  EXPECT_EQ(source_callback3.cumulative_glitch_info(), glitch_info);

  input1->Stop();
  input2->Stop();
  input3->Stop();
}
}  // namespace audio
