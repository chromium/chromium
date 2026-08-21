// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/iamf_audio_decoder.h"

#include <array>
#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/mock_media_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const uint8_t kIamfExtraData[] = {
    0xf8, 0x06, 0x69, 0x61, 0x6d, 0x66, 0x00, 0x00, 0x00, 0x14, 0x00, 0x4f,
    0x70, 0x75, 0x73, 0xc0, 0x07, 0xff, 0xfc, 0x01, 0x02, 0x01, 0x38, 0x00,
    0x00, 0xbb, 0x80, 0x00, 0x00, 0x00, 0x08, 0x10, 0x01, 0x00, 0x00, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x20, 0x70, 0x07, 0x05,
    0x10, 0x41, 0x03, 0x01, 0x65, 0x6e, 0x2d, 0x75, 0x73, 0x00, 0x64, 0x65,
    0x66, 0x61, 0x75, 0x6c, 0x74, 0x5f, 0x6d, 0x69, 0x78, 0x5f, 0x70, 0x72,
    0x65, 0x73, 0x65, 0x6e, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x01,
    0x01, 0x01, 0x37, 0x2e, 0x31, 0x2e, 0x34, 0x00, 0x40, 0x00, 0x65, 0x80,
    0xf7, 0x02, 0x80, 0x00, 0x00, 0x64, 0x80, 0xf7, 0x02, 0x80, 0x00, 0x00,
    0x01, 0x80, 0x00, 0xec, 0x66, 0xfe, 0x80};

class IamfAudioDecoderTest : public testing::Test {
 public:
  IamfAudioDecoderTest() = default;
  ~IamfAudioDecoderTest() override = default;

  ChannelLayoutConfig GetOutputLayoutConfig(const IamfAudioDecoder* decoder) {
    return decoder->output_layout_config_;
  }

  DecoderStatus InitializeAndGetStatus(IamfAudioDecoder* decoder,
                                       const AudioDecoderConfig& config) {
    DecoderStatus result = DecoderStatus::Codes::kFailed;
    base::RunLoop run_loop;
    decoder->Initialize(
        config, nullptr,
        base::BindOnce(
            [](base::OnceClosure quit_closure, DecoderStatus* result_out,
               DecoderStatus status) {
              *result_out = status;
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), &result),
        base::DoNothing(), base::DoNothing());
    run_loop.Run();
    return result;
  }

  DecoderStatus DecodeAndGetStatus(IamfAudioDecoder* decoder,
                                   scoped_refptr<DecoderBuffer> buffer) {
    DecoderStatus result = DecoderStatus::Codes::kFailed;
    base::RunLoop run_loop;
    decoder->Decode(std::move(buffer),
                    base::BindOnce(
                        [](base::OnceClosure quit_closure,
                           DecoderStatus* result_out, DecoderStatus status) {
                          *result_out = status;
                          std::move(quit_closure).Run();
                        },
                        run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  ChannelLayoutConfig ConvertIamfLayout(
      const iamf_tools::api::OutputLayout& iamf_layout) {
    return IamfAudioDecoder::ConvertIamfLayout(iamf_layout);
  }

  iamf_tools::api::OutputLayout ConvertMediaLayoutToIamfLayout(
      const ChannelLayoutConfig& layout_config) {
    return IamfAudioDecoder::ConvertMediaLayoutToIamfLayout(layout_config);
  }

 protected:
  std::unique_ptr<IamfAudioDecoder> InitializeDecoder(
      const ChannelLayoutConfig& config_layout,
      std::optional<ChannelLayoutConfig> target_layout = std::nullopt) {
    auto decoder = std::make_unique<IamfAudioDecoder>(
        task_environment_.GetMainThreadTaskRunner(), &media_log_);

    AudioDecoderConfig config;
    config.Initialize(AudioCodec::kIAMF, kSampleFormatS32, config_layout, 48000,
                      std::vector<uint8_t>(std::begin(kIamfExtraData),
                                           std::end(kIamfExtraData)),
                      EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);

    if (target_layout.has_value()) {
      config.set_target_output_channel_layout(target_layout.value());
    }
    DecoderStatus status = InitializeAndGetStatus(decoder.get(), config);
    EXPECT_TRUE(status.is_ok());

    return decoder;
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockMediaLog> media_log_;
};

TEST_F(IamfAudioDecoderTest, GetDecoderType) {
  auto decoder = std::make_unique<IamfAudioDecoder>(
      task_environment_.GetMainThreadTaskRunner(), &media_log_);
  EXPECT_EQ(AudioDecoderType::kIamf, decoder->GetDecoderType());
}

TEST_F(IamfAudioDecoderTest, InitializeUnsupportedCodec) {
  auto decoder = std::make_unique<IamfAudioDecoder>(
      task_environment_.GetMainThreadTaskRunner(), &media_log_);

  AudioDecoderConfig config;
  config.Initialize(AudioCodec::kOpus, kSampleFormatS32,
                    ChannelLayoutConfig::Stereo(), 48000, {},
                    EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);

  DecoderStatus status = InitializeAndGetStatus(decoder.get(), config);

  EXPECT_EQ(status.code(), DecoderStatus::Codes::kUnsupportedCodec);
}

TEST_F(IamfAudioDecoderTest, InitializeEncryptedConfig) {
  auto decoder = std::make_unique<IamfAudioDecoder>(
      task_environment_.GetMainThreadTaskRunner(), &media_log_);

  AudioDecoderConfig config;
  config.Initialize(AudioCodec::kIAMF, kSampleFormatS32,
                    ChannelLayoutConfig::Stereo(), 48000, {},
                    EncryptionScheme::kCenc, base::TimeDelta(), 0);

  DecoderStatus status = InitializeAndGetStatus(decoder.get(), config);

  EXPECT_EQ(status.code(), DecoderStatus::Codes::kUnsupportedEncryptionMode);
}

TEST_F(IamfAudioDecoderTest, LayoutConversion) {
  // Test ConvertMediaLayoutToIamfLayout.
  EXPECT_EQ(iamf_tools::api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0,
            ConvertMediaLayoutToIamfLayout(
                ChannelLayoutConfig(CHANNEL_LAYOUT_MONO, 1)));
  EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0,
            ConvertMediaLayoutToIamfLayout(
                ChannelLayoutConfig(CHANNEL_LAYOUT_STEREO, 2)));
  EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemB_0_5_0,
            ConvertMediaLayoutToIamfLayout(
                ChannelLayoutConfig(CHANNEL_LAYOUT_5_1, 6)));
  EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemI_0_7_0,
            ConvertMediaLayoutToIamfLayout(
                ChannelLayoutConfig(CHANNEL_LAYOUT_7_1, 8)));

  // Discrete cases.
  EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemJ_4_7_0,
            ConvertMediaLayoutToIamfLayout(
                ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 12)));
  EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemD_4_5_0,
            ConvertMediaLayoutToIamfLayout(
                ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 10)));
  EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0,
            ConvertMediaLayoutToIamfLayout(ChannelLayoutConfig(
                CHANNEL_LAYOUT_DISCRETE, 8)));  // fallback to stereo

  // ConvertIamfLayout high-channel layout mapping check.
  {
    auto config_d = ConvertIamfLayout(
        iamf_tools::api::OutputLayout::kItu2051_SoundSystemD_4_5_0);
    EXPECT_EQ(CHANNEL_LAYOUT_5_1_4, config_d.channel_layout());
    EXPECT_EQ(10, config_d.channels());

    auto config_j = ConvertIamfLayout(
        iamf_tools::api::OutputLayout::kItu2051_SoundSystemJ_4_7_0);
    EXPECT_EQ(CHANNEL_LAYOUT_7_1_4, config_j.channel_layout());
    EXPECT_EQ(12, config_j.channels());

    // High channel ConvertMediaLayoutToIamfLayout.
    EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemD_4_5_0,
              ConvertMediaLayoutToIamfLayout(
                  ChannelLayoutConfig(CHANNEL_LAYOUT_5_1_4, 10)));
    EXPECT_EQ(iamf_tools::api::OutputLayout::kItu2051_SoundSystemJ_4_7_0,
              ConvertMediaLayoutToIamfLayout(
                  ChannelLayoutConfig(CHANNEL_LAYOUT_7_1_4, 12)));
  }
}

TEST_F(IamfAudioDecoderTest, InitializeWithTargetLayout) {
  auto decoder =
      InitializeDecoder(ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>(),
                        ChannelLayoutConfig::Stereo());

  EXPECT_EQ(CHANNEL_LAYOUT_STEREO,
            GetOutputLayoutConfig(decoder.get()).channel_layout());
}

TEST_F(IamfAudioDecoderTest, InitializeWithoutTargetLayout) {
  auto decoder = InitializeDecoder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>());

  EXPECT_EQ(CHANNEL_LAYOUT_7_1_4,
            GetOutputLayoutConfig(decoder.get()).channel_layout());
}

TEST_F(IamfAudioDecoderTest, InitializeWithDiscreteTargetLayout) {
  auto decoder =
      InitializeDecoder(ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>(),
                        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 12));

  EXPECT_EQ(CHANNEL_LAYOUT_7_1_4,
            GetOutputLayoutConfig(decoder.get()).channel_layout());
}

TEST_F(IamfAudioDecoderTest, InitializeWithUnsupportedTargetLayoutFallback) {
  auto decoder = InitializeDecoder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_SURROUND>());

  EXPECT_EQ(CHANNEL_LAYOUT_STEREO,
            GetOutputLayoutConfig(decoder.get()).channel_layout());
}

TEST_F(IamfAudioDecoderTest, DecodeAfterFinishedReturnsOk) {
  auto decoder = InitializeDecoder(ChannelLayoutConfig::Stereo());

  DecoderStatus status =
      DecodeAndGetStatus(decoder.get(), DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(status.code(), DecoderStatus::Codes::kOk);

  const uint8_t kData[] = {1, 2, 3};
  status = DecodeAndGetStatus(decoder.get(), DecoderBuffer::CopyFrom(kData));
  EXPECT_EQ(status.code(), DecoderStatus::Codes::kOk);

  status = DecodeAndGetStatus(decoder.get(), DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(status.code(), DecoderStatus::Codes::kOk);
}

TEST_F(IamfAudioDecoderTest, UmaMetrics) {
  base::HistogramTester histogram_tester;
  constexpr std::array<uint8_t, 3> kDummyData = {1, 2, 3};
  constexpr int kExpectedDecodes = 13;

  auto decoder = InitializeDecoder(ChannelLayoutConfig::Stereo());

  for (int i = 0; i < kExpectedDecodes; ++i) {
    scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(kDummyData);
    buffer->set_timestamp(base::TimeDelta());
    DecoderStatus dummy_decode_status =
        DecodeAndGetStatus(decoder.get(), buffer);
    EXPECT_TRUE(dummy_decode_status.is_ok());

    base::RunLoop run_loop;
    decoder->Reset(run_loop.QuitClosure());
    run_loop.Run();
  }

  DecoderStatus eos_decode_status =
      DecodeAndGetStatus(decoder.get(), DecoderBuffer::CreateEOSBuffer());

  EXPECT_TRUE(eos_decode_status.is_ok());

  // Verify that there was exactly 1 initialization that succeeded with kOk.
  histogram_tester.ExpectUniqueSample(
      "Media.Audio.Iamf.InitStatus",
      static_cast<int>(DecoderStatus::Codes::kOk),
      /*expected_bucket_count=*/1);

  // Verify that the input channel count was recorded exactly once and matched
  // the config (Stereo = 2 channels).
  histogram_tester.ExpectUniqueSample("Media.Audio.Iamf.InputChannelCount", 2,
                                      /*expected_bucket_count=*/1);

  // Verify that the IAMF specific output layout enum was recorded exactly once.
  histogram_tester.ExpectUniqueSample(
      "Media.Audio.Iamf.OutputLayout",
      static_cast<int>(
          iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0),
      /*expected_bucket_count=*/1);

  // Verify that the MixMode was correctly logged as No mixing (0)
  // because input config and output layout are both Stereo.
  histogram_tester.ExpectUniqueSample("Media.Audio.Iamf.MixMode", 0,
                                      /*expected_bucket_count=*/1);
}

TEST_F(IamfAudioDecoderTest, UmaMetrics_Downmix) {
  base::HistogramTester histogram_tester;

  // 5.1 input (6 channels), but target layout is stereo (2 channels) -> Downmix
  auto decoder =
      InitializeDecoder(ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>(),
                        ChannelLayoutConfig::Stereo());

  histogram_tester.ExpectUniqueSample("Media.Audio.Iamf.MixMode", 1, 1);
}

TEST_F(IamfAudioDecoderTest, UmaMetrics_Upmix) {
  base::HistogramTester histogram_tester;

  // Mono input (1 channel), but target layout is stereo (2 channels) -> Upmix
  auto decoder = InitializeDecoder(ChannelLayoutConfig::Mono(),
                                   ChannelLayoutConfig::Stereo());

  histogram_tester.ExpectUniqueSample("Media.Audio.Iamf.MixMode", 2, 1);
}

}  // namespace media
