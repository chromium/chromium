// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/mixing_graph_impl.h"
#include "media/base/channel_layout.h"
#include "media/base/loopback_audio_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
// Wrapper for MixingGraphImpl that exposes converters_ and main_converter_ for
// testing.
class MixingGraphImplUnderTest : public MixingGraphImpl {
 public:
  MixingGraphImplUnderTest(const media::AudioParameters& output_params,
                           OnMoreDataCallback on_more_data_cb,
                           OnErrorCallback on_error_cb,
                           CreateConverterCallback create_converter_cb)
      : MixingGraphImpl(output_params,
                        on_more_data_cb,
                        on_error_cb,
                        create_converter_cb) {}
  MixingGraphImplUnderTest(const media::AudioParameters& output_params,
                           OnMoreDataCallback on_more_data_cb,
                           OnErrorCallback on_error_cb)
      : MixingGraphImpl(output_params, on_more_data_cb, on_error_cb) {}
  const auto& converters() { return converters_; }
  const auto& main_converter() { return main_converter_; }
};

class MockInput : public MixingGraph::Input {
 public:
  explicit MockInput(media::AudioParameters params) : params(params) {}

  const media::AudioParameters& GetParams() const override { return params; }
  MOCK_METHOD(void, SetVolume, (double));
  MOCK_METHOD(void, Start, (media::AudioOutputStream::AudioSourceCallback*));
  MOCK_METHOD(void, Stop, ());
  MOCK_METHOD(double,
              ProvideInput,
              (media::AudioBus*, uint32_t, const media::AudioGlitchInfo&));

  media::AudioParameters params;
};

class MockConverterFactory {
 public:
  std::unique_ptr<media::LoopbackAudioConverter> CreateConverter(
      const media::AudioParameters& input_params,
      const media::AudioParameters& output_params) {
    VerifyInput(input_params.sample_rate(), input_params.channel_layout());
    VerifyOutput(output_params.sample_rate(), output_params.channel_layout());
    return std::make_unique<media::LoopbackAudioConverter>(input_params,
                                                           output_params, true);
  }
  MOCK_METHOD(void, VerifyInput, (int, media::ChannelLayout));
  MOCK_METHOD(void, VerifyOutput, (int, media::ChannelLayout));
};

TEST(MixingGraphImpl, AddInputToMainConverter) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create a new input with the same audio parameters as the output.
  MockInput input(output_params);
  mixing_graph.AddInput(&input);

  // The input should be connected to the main_converter_.
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_FALSE(main_converter.empty());

  mixing_graph.RemoveInput(&input);

  // Expect main_converter to be empty again.
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

TEST(MixingGraphImpl, AddMultipleInputsToMainConverter) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create some new inputs with the same audio parameters as the output.
  MockInput input1(output_params);
  MockInput input2(output_params);
  MockInput input3(output_params);
  mixing_graph.AddInput(&input1);
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_FALSE(main_converter.empty());
  mixing_graph.AddInput(&input2);
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_FALSE(main_converter.empty());
  mixing_graph.AddInput(&input3);
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_FALSE(main_converter.empty());

  mixing_graph.RemoveInput(&input1);
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_FALSE(main_converter.empty());
  mixing_graph.RemoveInput(&input2);
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_FALSE(main_converter.empty());
  mixing_graph.RemoveInput(&input3);
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

TEST(MixingGraphImpl, AddInputWithChannelMixer) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create a new input which differs from the output in number of channels.
  MockInput input(input_params);
  mixing_graph.AddInput(&input);

  // A channel mixer converter should be added to converters_ and connected to
  // the main_converter_.
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  mixing_graph.RemoveInput(&input);

  // Expect the channel mixer to have been removed from converters_ and the
  // main_converter to be empty again.
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

TEST(MixingGraphImpl, AddInputWithResampler) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 16000, 160);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create a new input which differs from the output in sample rate.
  MockInput input(input_params);
  mixing_graph.AddInput(&input);

  // A resampler converter should be added to converters_ and connected to
  // the main_converter_.
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  mixing_graph.RemoveInput(&input);

  // Expect the resampler to have been removed from converters_ and the
  // main_converter to be empty again.
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

TEST(MixingGraphImpl, OutputDiscreteChannelLayout) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      {media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE, 2}, 48000, 480);
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create a new input which differs from the output in number of channels.
  MockInput input(input_params);
  mixing_graph.AddInput(&input);

  // A channel mixer converter is used between the non-discrete input and the
  // discrete output even though the number of channels is the same.
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  mixing_graph.RemoveInput(&input);

  // Expect the channel mixer to have been removed from converters_ and the
  // main_converter to be empty again.
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

TEST(MixingGraphImpl, AddInputWithDiscreteChannelLayout) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      {media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE, 1}, 48000, 480);
  media::AudioParameters input_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      {media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE, 2}, 48000, 480);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create a new input which differs from the output in number of channels.
  MockInput input(input_params);
  mixing_graph.AddInput(&input);

  // A channel mixer converter should be added to converters_ and connected to
  // the main_converter_.
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  mixing_graph.RemoveInput(&input);

  // Expect the channel mixer to have been removed from converters_ and the
  // main_converter to be empty again.
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

TEST(MixingGraphImpl, BuildComplexGraph) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImplUnderTest mixing_graph(output_params, on_more_data_cb,
                                        on_error_cb);
  const auto& converters = mixing_graph.converters();
  const auto& main_converter = mixing_graph.main_converter();
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());

  // Create a new input which differs from the output in sample rate.
  media::AudioParameters input_params_resamp(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 16000, 160);
  MockInput input1(input_params_resamp);
  mixing_graph.AddInput(&input1);
  // Graph:
  // main <--- converter (resampler) <--- input1
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  // Adding a new input with the same configuration should not change the
  // number of converters.
  MockInput input2(input_params_resamp);
  mixing_graph.AddInput(&input2);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  // Create a new input which differs from the output in channel layout.
  media::AudioParameters input_params_downmix(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  MockInput input3(input_params_downmix);
  mixing_graph.AddInput(&input3);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //      <--- converter (ch. mixer) <--- input3
  EXPECT_EQ(converters.size(), 2UL);
  EXPECT_FALSE(main_converter.empty());

  // Create a new input which needs resampling (re-used) and channel mixing
  // (new).
  media::AudioParameters input_params_resamp_downmix(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 16000, 160);
  MockInput input4(input_params_resamp_downmix);
  mixing_graph.AddInput(&input4);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //      <--- converter (ch. mixer) <--- input3
  EXPECT_EQ(converters.size(), 3UL);
  EXPECT_FALSE(main_converter.empty());

  // Adding a new input with the same configuration should not change the
  // number of converters.
  MockInput input5(input_params_resamp_downmix);
  mixing_graph.AddInput(&input5);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //                                                            <--- input5
  //      <--- converter (ch. mixer) <--- input3
  EXPECT_EQ(converters.size(), 3UL);
  EXPECT_FALSE(main_converter.empty());

  // Adding a couple of inputs connected directly to the main_converter_
  // should not change the number of converters.
  MockInput input6(output_params);
  MockInput input7(output_params);
  mixing_graph.AddInput(&input6);
  mixing_graph.AddInput(&input7);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //                                                            <--- input5
  //      <--- converter (ch. mixer) <--- input3
  //      <--- input6
  //      <--- input7
  EXPECT_EQ(converters.size(), 3UL);
  EXPECT_FALSE(main_converter.empty());

  // Create a new input which needs resampling (re-used) and channel mixing due
  // to having a discrete channel layout.
  media::AudioParameters input_params_resamp_discrete(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      {media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE, 1}, 48000, 160);
  MockInput input8(input_params_resamp_discrete);
  mixing_graph.AddInput(&input8);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //                                                            <--- input5
  //                                 <--- converter (ch. mixer) <--- input8
  //      <--- converter (ch. mixer) <--- input3
  //      <--- input6
  //      <--- input7
  EXPECT_EQ(converters.size(), 4UL);
  EXPECT_FALSE(main_converter.empty());

  // Removing input{1,2,4,6,7} will not allow any converters to be removed.
  mixing_graph.RemoveInput(&input1);
  mixing_graph.RemoveInput(&input2);
  mixing_graph.RemoveInput(&input4);
  mixing_graph.RemoveInput(&input6);
  mixing_graph.RemoveInput(&input7);
  // Graph:
  // main <--- converter (resampler) <--- converter (ch. mixer) <--- input5
  //                                 <--- converter (ch. mixer) <--- input8
  //      <--- converter (ch. mixer) <--- input3
  EXPECT_EQ(converters.size(), 4UL);
  EXPECT_FALSE(main_converter.empty());

  // Removing input8 should cause removal of a channel mixer.
  mixing_graph.RemoveInput(&input8);
  // Graph:
  // main <--- converter (resampler) <--- converter (ch. mixer) <--- input5
  //      <--- converter (ch. mixer) <--- input3
  EXPECT_EQ(converters.size(), 3UL);
  EXPECT_FALSE(main_converter.empty());

  // Removing input5 should cause removal of the resampler and a channel
  // mixer.
  mixing_graph.RemoveInput(&input5);
  // Graph:
  // main <--- converter (ch. mixer) <--- input3
  EXPECT_EQ(converters.size(), 1UL);
  EXPECT_FALSE(main_converter.empty());

  // Removing the last input should result in no converters left and an empty
  // main_converter_.
  mixing_graph.RemoveInput(&input3);
  // Graph:
  // main
  EXPECT_EQ(converters.size(), 0UL);
  EXPECT_TRUE(main_converter.empty());
}

// Builds the same graph as above, but verifies the converter parameters.
TEST(MixingGraphImpl, VerifyConverters) {
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  MockConverterFactory mock_converter_factory;
  MixingGraph::OnMoreDataCallback on_more_data_cb;
  MixingGraph::OnErrorCallback on_error_cb;
  MixingGraphImpl::CreateConverterCallback create_converter_cb =
      base::BindRepeating(&MockConverterFactory::CreateConverter,
                          base::Unretained(&mock_converter_factory));
  MixingGraphImpl mixing_graph(output_params, on_more_data_cb, on_error_cb,
                               create_converter_cb);

  // Create a new input which differs from the output in sample rate.
  media::AudioParameters input_params_resamp(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 16000, 160);
  MockInput input1(input_params_resamp);
  EXPECT_CALL(mock_converter_factory,
              VerifyInput(16000, media::ChannelLayout::CHANNEL_LAYOUT_MONO));
  EXPECT_CALL(mock_converter_factory,
              VerifyOutput(48000, media::ChannelLayout::CHANNEL_LAYOUT_MONO));
  mixing_graph.AddInput(&input1);
  // Graph:
  // main <--- converter (resampler) <--- input1

  // Adding a new input with the same configuration should not change the
  // number of converters.
  MockInput input2(input_params_resamp);
  mixing_graph.AddInput(&input2);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2

  // Create a new input which differs from the output in channel layout.
  media::AudioParameters input_params_downmix(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 480);
  MockInput input3(input_params_downmix);
  EXPECT_CALL(mock_converter_factory,
              VerifyInput(48000, media::ChannelLayout::CHANNEL_LAYOUT_STEREO));
  EXPECT_CALL(mock_converter_factory,
              VerifyOutput(48000, media::ChannelLayout::CHANNEL_LAYOUT_MONO));
  mixing_graph.AddInput(&input3);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //      <--- converter (ch. mixer) <--- input3

  // Create a new input which needs resampling (re-used) and channel mixing
  // (new).
  media::AudioParameters input_params_resamp_downmix(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 16000, 160);
  MockInput input4(input_params_resamp_downmix);
  EXPECT_CALL(mock_converter_factory,
              VerifyInput(16000, media::ChannelLayout::CHANNEL_LAYOUT_STEREO));
  EXPECT_CALL(mock_converter_factory,
              VerifyOutput(16000, media::ChannelLayout::CHANNEL_LAYOUT_MONO));
  mixing_graph.AddInput(&input4);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //      <--- converter (ch. mixer) <--- input3

  // Adding a new input with the same configuration should not change the
  // number of converters.
  MockInput input5(input_params_resamp_downmix);
  mixing_graph.AddInput(&input5);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //                                                            <--- input5
  //      <--- converter (ch. mixer) <--- input3

  // Create a new input which needs resampling (re-used) and channel mixing due
  // to having a discrete channel layout.
  media::AudioParameters input_params_resamp_discrete(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      {media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE, 1}, 48000, 160);
  MockInput input6(input_params_resamp_discrete);
  EXPECT_CALL(
      mock_converter_factory,
      VerifyInput(48000, media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE));
  EXPECT_CALL(mock_converter_factory,
              VerifyOutput(48000, media::ChannelLayout::CHANNEL_LAYOUT_MONO));
  mixing_graph.AddInput(&input6);
  // Graph:
  // main <--- converter (resampler) <--- input1
  //                                 <--- input2
  //                                 <--- converter (ch. mixer) <--- input4
  //                                                            <--- input5
  //                                 <--- converter (ch. mixer) <--- input6
  //      <--- converter (ch. mixer) <--- input3

  mixing_graph.RemoveInput(&input1);
  mixing_graph.RemoveInput(&input2);
  mixing_graph.RemoveInput(&input3);
  mixing_graph.RemoveInput(&input4);
  mixing_graph.RemoveInput(&input5);
  mixing_graph.RemoveInput(&input6);
}

// Verifies operator< of AudioConverterKey.
TEST(MixingGraphImpl, AudioConverterKeySorting) {
  auto MakeKey = [](media::ChannelLayout channel_layout, int sample_rate) {
    return MixingGraphImpl::AudioConverterKey(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        {channel_layout, media::ChannelLayoutToChannelCount(channel_layout)},
        sample_rate, sample_rate / 100));
  };
  auto MakeDiscreteKey = [](int channels, int sample_rate) {
    media::AudioParameters params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        {media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE, channels}, sample_rate,
        sample_rate / 100);
    return MixingGraphImpl::AudioConverterKey(params);
  };

  // Identical keys.
  EXPECT_FALSE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/48000));

  // Sample rate has the highest precedence when sorting.
  EXPECT_TRUE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      /*sample_rate=*/16000) <
              MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      /*sample_rate=*/48000));
  EXPECT_FALSE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/16000));
  EXPECT_TRUE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                      /*sample_rate=*/16000) <
              MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      /*sample_rate=*/48000));
  EXPECT_FALSE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                       /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/16000));
  EXPECT_TRUE(MakeDiscreteKey(/*channels=*/2,
                              /*sample_rate=*/16000) <
              MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      /*sample_rate=*/48000));
  EXPECT_FALSE(MakeDiscreteKey(/*channels=*/2,
                               /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/16000));
  EXPECT_FALSE(MakeDiscreteKey(/*channels=*/1,
                               /*sample_rate=*/48000) <
               MakeDiscreteKey(/*channels=*/2,
                               /*sample_rate=*/16000));

  // Channel layout has the second highest precedence when sorting.
  EXPECT_TRUE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      /*sample_rate=*/48000) <
              MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                      /*sample_rate=*/48000));
  EXPECT_FALSE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                       /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/48000));
  EXPECT_TRUE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                      /*sample_rate=*/48000) <
              MakeDiscreteKey(/*channels=*/2,
                              /*sample_rate=*/48000));
  EXPECT_FALSE(MakeDiscreteKey(/*channels=*/2,
                               /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                       /*sample_rate=*/48000));
  EXPECT_TRUE(MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                      /*sample_rate=*/48000) <
              MakeDiscreteKey(/*channels=*/2,
                              /*sample_rate=*/48000));
  EXPECT_FALSE(MakeDiscreteKey(/*channels=*/2,
                               /*sample_rate=*/48000) <
               MakeKey(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                       /*sample_rate=*/48000));

  // Number of channels determines sort order when comparing two keys with the
  // same sampling rate and discrete channel layout.
  EXPECT_FALSE(MakeDiscreteKey(/*channels=*/1,
                               /*sample_rate=*/48000) <
               MakeDiscreteKey(/*channels=*/1,
                               /*sample_rate=*/48000));
  EXPECT_TRUE(MakeDiscreteKey(/*channels=*/1,
                              /*sample_rate=*/48000) <
              MakeDiscreteKey(/*channels=*/2,
                              /*sample_rate=*/48000));
  EXPECT_FALSE(MakeDiscreteKey(/*channels=*/2,
                               /*sample_rate=*/48000) <
               MakeDiscreteKey(/*channels=*/1,
                               /*sample_rate=*/48000));
}

}  // namespace audio
