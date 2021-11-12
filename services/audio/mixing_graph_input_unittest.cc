// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/mixing_graph.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
// Test fixture to verify the functionality of the mixing graph inputs.
class MixingGraphInputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    output_params_ = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayout::CHANNEL_LAYOUT_MONO, 48000, 480);
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
      mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
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
// previous sample plus one. When using stereo the values of the right channel
// will be the values of the left channel plus one.
class SampleCounter : public media::AudioOutputStream::AudioSourceCallback {
 public:
  explicit SampleCounter(float counter) : counter_(counter) {}
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 media::AudioBus* dest) final {
    // Fill the audio bus with a simple, predictable pattern.
    for (int channel = 0; channel < dest->channels(); ++channel) {
      float* data = dest->channel(channel);
      for (int frame = 0; frame < dest->frames(); frame++) {
        data[frame] = counter_ + frame + channel;
      }
    }
    counter_ += static_cast<float>(dest->frames());
    return 0;
  }
  void OnError(ErrorType type) final {}

 private:
  float counter_ = 0.0f;
};

// Simple audio source callback where all samples are set to the same value.
// The value is incremented by one for each callback.
class CallbackCounter : public media::AudioOutputStream::AudioSourceCallback {
 public:
  explicit CallbackCounter(float counter) : counter_(counter) {}
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 media::AudioBus* dest) final {
    // Fill the audio bus with the counter value.
    for (int channel = 0; channel < dest->channels(); ++channel) {
      float* data = dest->channel(channel);
      for (int frame = 0; frame < dest->frames(); frame++) {
        data[frame] = counter_;
      }
    }
    ++counter_;
    return 0;
  }
  void OnError(ErrorType type) final {}

 private:
  float counter_ = 0.0f;
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
  SampleCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(output_params_);
  input->Start(&source_callback);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/1.0f, /*epsilon=*/0.0f);
  input->Stop();
}

// Verifies the output of the mixing graph when adding multiple inputs.
TEST_F(MixingGraphInputTest, MultipleInputs) {
  constexpr float kInitialCounterValue1 = 1.0f;
  constexpr float kInitialCounterValue2 = 5.0f;
  constexpr float kInitialCounterValue3 = 7.0f;
  SampleCounter source_callback1(kInitialCounterValue1);
  SampleCounter source_callback2(kInitialCounterValue2);
  SampleCounter source_callback3(kInitialCounterValue3);
  auto input1 = mixing_graph_->CreateInput(output_params_);
  input1->Start(&source_callback1);
  auto input2 = mixing_graph_->CreateInput(output_params_);
  input2->Start(&source_callback2);
  auto input3 = mixing_graph_->CreateInput(output_params_);
  input3->Start(&source_callback3);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue1 +
                        kInitialCounterValue2 + kInitialCounterValue3,
                    /*expected_sample_increment=*/3.0f, /*epsilon=*/0.0f);
  input1->Stop();
  input2->Stop();
  input3->Stop();
}

// Verifies the mixing graph output when adding an input in need of channel
// mixing.
TEST_F(MixingGraphInputTest, ChannelMixing) {
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 48000, 480);
  constexpr float kInitialCounterValue = 0.0f;
  SampleCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  // The right channel has the values of the left channel + 1. When
  // down-mixing (averaging) the two channels this will cause a bias of 0.5.
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue + 0.5f,
                    /*expected_sample_increment=*/1.0f, /*epsilon=*/0.0f);
  input->Stop();
}

// Verifies the mixing graph output when adding an input in need of resampling.
TEST_F(MixingGraphInputTest, Resampling) {
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayout::CHANNEL_LAYOUT_MONO, 24000, 480);
  constexpr float kInitialCounterValue = 0.0f;
  SampleCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  // The input signal will increase by 1 for each sample and be upsampled by
  // a factor 2. The output will therefore increase by ~0.5 per
  // sample.
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/0.5f, /*epsilon=*/0.1f);
  input->Stop();
}

// Verifies the use of FIFO when an input produces less data per call than
// requested by the mixing graph.
TEST_F(MixingGraphInputTest, Buffering1) {
  // Input produces 5 ms of audio. Output consumes 10 ms.
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayout::CHANNEL_LAYOUT_MONO, 48000, 240);
  constexpr float kInitialCounterValue = 0.0f;
  SampleCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/1.0f, /*epsilon=*/0.0f);
  input->Stop();
}

// Verifies the use of FIFO when an input produces more data per call than
// requested by the mixing graph.
TEST_F(MixingGraphInputTest, Buffering2) {
  // Input produces 15 ms of audio. Output consumes 10 ms.
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayout::CHANNEL_LAYOUT_MONO, 48000, 720);
  constexpr float kInitialCounterValue = 0.0f;
  SampleCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);
  PullAndVerifyData(/*num_runs=*/2,
                    /*expected_first_sample=*/kInitialCounterValue,
                    /*expected_sample_increment=*/1.0f, /*epsilon=*/0.0f);
  input->Stop();
}

// Verifies that no left-over samples from the FIFO are pulled after stopping
// and restarting the input.
TEST_F(MixingGraphInputTest, BufferClearedAtRestart) {
  // Input produces 15 ms of audio. Output consumes 10 ms.
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayout::CHANNEL_LAYOUT_MONO, 48000, 720);
  constexpr float kInitialCounterValue = 0.0f;
  CallbackCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(input_params);
  input->Start(&source_callback);

  // Get the last sample of the first output.
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                            dest_.get());
  float last_sample = dest_.get()->channel(0)[dest_.get()->frames() - 1];

  // Stop and restart.
  input->Stop();
  input->Start(&source_callback);

  // Get the first sample of the second output.
  mixing_graph_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
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
  constexpr float kInitialCounterValue1 = 1.0f;
  constexpr float kInitialCounterValue2 = 5.0f;
  SampleCounter source_callback1(kInitialCounterValue1);
  SampleCounter source_callback2(kInitialCounterValue2);
  auto input1 = mixing_graph_->CreateInput(output_params_);
  auto input2 = mixing_graph_->CreateInput(output_params_);

  // Start the first input.
  input1->Start(&source_callback1);
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/kInitialCounterValue1,
                    /*expected_sample_increment=*/1.0f, /*epsilon=*/0.0f);

  // Start the second input.
  input2->Start(&source_callback2);
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/kInitialCounterValue1 +
                        dest_->frames() + kInitialCounterValue2,
                    /*expected_sample_increment=*/2.0f, /*epsilon=*/0.0f);

  // Stop the first input.
  input1->Stop();
  PullAndVerifyData(
      /*num_runs=*/1,
      /*expected_first_sample=*/kInitialCounterValue2 + dest_->frames(),
      /*expected_sample_increment=*/1.0f, /*epsilon=*/0.0f);

  // Stop the second input.
  input2->Stop();
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/0.f,
                    /*expected_sample_increment=*/0.0f, /*epsilon=*/0.0f);
}

// Verifies that the volume is applied correctly.
TEST_F(MixingGraphInputTest, SetVolume) {
  constexpr float kInitialCounterValue = 0.0f;
  constexpr float kVolume1 = 0.77f;
  constexpr float kVolume2 = 0.13f;
  SampleCounter source_callback(kInitialCounterValue);
  auto input = mixing_graph_->CreateInput(output_params_);
  input->Start(&source_callback);
  input->SetVolume(kVolume1);
  PullAndVerifyData(/*num_runs=*/1,
                    /*expected_first_sample=*/kInitialCounterValue * kVolume1,
                    /*expected_sample_increment=*/kVolume1, /*epsilon=*/1e-2f);
  input->SetVolume(kVolume2);
  PullAndVerifyData(
      /*num_runs=*/1,
      /*expected_first_sample=*/(kInitialCounterValue + dest_->frames()) *
          kVolume2,
      /*expected_sample_increment=*/kVolume2, /*epsilon=*/1e-2f);
  input->Stop();
}
}  // namespace audio
