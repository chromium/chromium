// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_span.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/supported_types.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/gpu_mojo_media_client_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_SYMPHONIA)
#include "media/filters/symphonia_audio_decoder.h"
#endif

#if BUILDFLAG(ENABLE_IAMF_TOOLS)
#include "media/filters/iamf_audio_decoder.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_util.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "media/filters/mac/audio_toolbox_audio_decoder.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/filters/win/media_foundation_audio_decoder.h"
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mpeg/adts_stream_parser.h"
#endif

using testing::Combine;
using testing::TestWithParam;
using testing::Values;
using testing::ValuesIn;

namespace media {

namespace {

// The number of packets to read and then decode from each file.
constexpr size_t kDecodeRuns = 3;

// Corresponds to the 7.1.4 layout.
constexpr int kIamfDiscreteChannelCount = 12;

struct DecodedBufferExpectations {
  int64_t timestamp;
  int64_t duration;
  const char* hash;
};

using DataExpectations = std::array<DecodedBufferExpectations, kDecodeRuns>;

struct TestParams {
  AudioCodec codec;
  const char* filename;
  DataExpectations expectations;
  int first_packet_pts;
  int samples_per_second;
  ChannelLayout channel_layout;
  AudioCodecProfile profile = AudioCodecProfile::kUnknown;

  // When set, the test accepts either the primary or alternate expectations.
  std::optional<DataExpectations> alt_expectations;

  base::raw_span<const uint8_t> extra_data;
  ChannelLayout target_channel_layout = CHANNEL_LAYOUT_NONE;
};

// Tells gtest how to print our TestParams structure.
std::ostream& operator<<(std::ostream& os, const TestParams& params) {
  return os << params.filename;
}

// Marks negative timestamp buffers for discard or transfers FFmpeg's built in
// discard metadata in favor of setting DiscardPadding on the DecoderBuffer.
// Allows better testing of AudioDiscardHelper usage.
void SetDiscardPadding(AVPacket* packet,
                       DecoderBuffer* buffer,
                       double samples_per_second) {
  // Discard negative timestamps.
  if ((buffer->timestamp() + buffer->duration()).is_negative()) {
    buffer->set_discard_padding(
        std::make_pair(kInfiniteDuration, base::TimeDelta()));
    return;
  }
  if (buffer->timestamp().is_negative()) {
    buffer->set_discard_padding(
        std::make_pair(-buffer->timestamp(), base::TimeDelta()));
    return;
  }

  // If the timestamp is positive, try to use FFmpeg's discard data.
  size_t skip_samples_size = 0;
  const uint32_t* skip_samples_ptr =
      reinterpret_cast<const uint32_t*>(av_packet_get_side_data(
          packet, AV_PKT_DATA_SKIP_SAMPLES, &skip_samples_size));
  if (skip_samples_size < 4) {
    return;
  }
  buffer->set_discard_padding(
      std::make_pair(base::Seconds(*skip_samples_ptr / samples_per_second),
                     base::TimeDelta()));
}

}  // namespace

class AudioDecoderTest
    : public TestWithParam<std::tuple<AudioDecoderType, TestParams>> {
 public:
  AudioDecoderTest()
      : decoder_type_(std::get<0>(GetParam())),
        params_(std::get<1>(GetParam())) {
    AddSupplementalCodecsForTesting();
    switch (decoder_type_) {
      case AudioDecoderType::kFFmpeg:
        decoder_ = std::make_unique<FFmpegAudioDecoder>(
            task_environment_.GetMainThreadTaskRunner(), &media_log_);
        break;
      case AudioDecoderType::kOpus:
        decoder_ = std::make_unique<OpusAudioDecoder>(
            task_environment_.GetMainThreadTaskRunner());
        break;
#if BUILDFLAG(ENABLE_IAMF_TOOLS)
      case AudioDecoderType::kIamf:
        decoder_ = std::make_unique<IamfAudioDecoder>(
            task_environment_.GetMainThreadTaskRunner(), &media_log_);
        break;
#endif

#if BUILDFLAG(ENABLE_SYMPHONIA)
      case AudioDecoderType::kSymphonia:
        decoder_ = std::make_unique<SymphoniaAudioDecoder>(
            task_environment_.GetMainThreadTaskRunner(), &media_log_);
        break;
#endif
#if BUILDFLAG(IS_ANDROID)
      case AudioDecoderType::kMediaCodec:
        decoder_ = std::make_unique<MediaCodecAudioDecoder>(
            task_environment_.GetMainThreadTaskRunner());
        break;
#elif BUILDFLAG(IS_MAC)
      case AudioDecoderType::kAudioToolbox:
        decoder_ =
            std::make_unique<AudioToolboxAudioDecoder>(media_log_.Clone());
        break;
#elif BUILDFLAG(IS_WIN)
      case AudioDecoderType::kMediaFoundation:
        decoder_ = MediaFoundationAudioDecoder::Create();
        break;
#endif
      default:
        EXPECT_TRUE(false) << "Decoder is not supported by this test.";
        break;
    }
  }

  AudioDecoderTest(const AudioDecoderTest&) = delete;
  AudioDecoderTest& operator=(const AudioDecoderTest&) = delete;

  virtual ~AudioDecoderTest() {
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

#if BUILDFLAG(ENABLE_SYMPHONIA)
    const std::vector<base::test::FeatureRef> symphonia_features = {
        { kSymphoniaAudioDecoding,
          kSymphoniaMp3Decoding,
          kSymphoniaPcmDecoding,
          kSymphoniaVorbisDecoding }};

    if (decoder_type_ == AudioDecoderType::kSymphonia) {
      enabled_features.insert(enabled_features.end(),
                              symphonia_features.begin(),
                              symphonia_features.end());
    } else {
      disabled_features.insert(disabled_features.end(),
                               symphonia_features.begin(),
                               symphonia_features.end());
    }
#endif

    if (decoder_type_ == AudioDecoderType::kOpus) {
      enabled_features.push_back(kDirectOpusAudioDecoding);
    } else if (decoder_type_ == AudioDecoderType::kFFmpeg) {
      disabled_features.push_back(kDirectOpusAudioDecoding);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    if (!IsSupported()) {
      GTEST_SKIP() << "Unsupported platform.";
    }
  }

 protected:
  bool IsSupported() const {
    if (params_.profile == AudioCodecProfile::kXHE_AAC) {
      return IsDecoderSupportedAudioType(
          {AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC, false});
    }
    switch (decoder_type_) {
#if BUILDFLAG(ENABLE_SYMPHONIA)
      case AudioDecoderType::kSymphonia:
        return SymphoniaAudioDecoder::IsCodecSupported(params_.codec);
#endif
      case AudioDecoderType::kOpus:
        return params_.codec == AudioCodec::kOpus;
      default:
        return true;
    }
  }

  bool IsIamfTest() const { return codec() == AudioCodec::kIAMF; }

  void VerifyIamfOutputLayout() {
    ASSERT_GT(decoded_audio_size(), 0u);
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[0];

    ChannelLayout expected_layout = params_.target_channel_layout;
    if (expected_layout == CHANNEL_LAYOUT_NONE) {
      expected_layout = (params_.channel_layout == CHANNEL_LAYOUT_DISCRETE)
                            ? CHANNEL_LAYOUT_7_1_4
                            : params_.channel_layout;
    }

    if (expected_layout == CHANNEL_LAYOUT_7_1_4 &&
        !base::FeatureList::IsEnabled(kEnableHighChannelLayouts)) {
      expected_layout = CHANNEL_LAYOUT_DISCRETE;
    }

    int expected_channels = (expected_layout == CHANNEL_LAYOUT_DISCRETE)
                                ? kIamfDiscreteChannelCount
                                : ChannelLayoutToChannelCount(expected_layout);

    EXPECT_EQ(expected_layout, buffer->channel_layout());
    EXPECT_EQ(expected_channels, buffer->channel_count());
  }

  void DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
    ASSERT_FALSE(pending_decode_);
    pending_decode_ = true;
    last_decode_status_ = DecoderStatus::Codes::kFailed;

    base::RunLoop run_loop;
    decoder_->Decode(
        std::move(buffer),
        base::BindOnce(&AudioDecoderTest::DecodeFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_FALSE(pending_decode_);
  }

  void SendEndOfStream() { DecodeBuffer(DecoderBuffer::CreateEOSBuffer()); }

  // Set the TestParams explicitly. Can be use to reinitialize the decoder with
  // different TestParams.
  void set_params(const TestParams& params) { params_ = params; }

  void SetReinitializeParams();

  // Initializes the AudioFileReader from the `filename_` property.
  void InitializeReader() {
    // Load the test data file.
    data_ = ReadTestDataFile(params_.filename);
    protocol_ = std::make_unique<InMemoryUrlProtocol>(*data_, false);
    reader_ = std::make_unique<AudioFileReader>(protocol_.get());
    ASSERT_TRUE(reader_->OpenDemuxerForTesting());
  }

  void Initialize() {
    filename_ = params_.filename;
    InitializeReader();

    // Load the first packet and check its timestamp.
    auto packet = ScopedAVPacket::Allocate();
    ASSERT_TRUE(reader_->ReadPacketForTesting(packet.get()));
    EXPECT_EQ(params_.first_packet_pts, packet->pts);
    start_timestamp_ = ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet->pts);

    // Reset the reader back to the beginning.
    InitializeReader();

    AudioDecoderConfig config;
    ASSERT_TRUE(AVCodecContextToAudioDecoderConfig(
        reader_->codec_context_for_testing(), EncryptionScheme::kUnencrypted,
        &config));

#if (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(USE_PROPRIETARY_CODECS)
    // MediaCodec type requires config->extra_data() for AAC codec. For ADTS
    // streams we need to extract it with a separate procedure.
    if ((decoder_type_ == AudioDecoderType::kMediaCodec ||
         decoder_type_ == AudioDecoderType::kMediaFoundation) &&
        codec() == AudioCodec::kAAC && config.extra_data().empty()) {
      const auto header = ADTSStreamParser::ParseHeader(AVPacketData(*packet));
      ASSERT_TRUE(header.has_value());
      config.Initialize(AudioCodec::kAAC, kSampleFormatS16,
                        ChannelLayoutConfig::FromLayout(header->channel_layout),
                        header->sample_rate, header->extra_data,
                        EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
      ASSERT_FALSE(config.extra_data().empty());
    }
#endif

    av_packet_unref(packet.get());

    if (IsIamfTest()) {
      ASSERT_FALSE(params_.extra_data.empty());
      int channels = (params_.channel_layout == CHANNEL_LAYOUT_DISCRETE)
                         ? kIamfDiscreteChannelCount
                         : ChannelLayoutToChannelCount(params_.channel_layout);
      std::vector<uint8_t> extra_data(params_.extra_data.begin(),
                                      params_.extra_data.end());
      config.Initialize(AudioCodec::kIAMF, kSampleFormatS32,
                        ChannelLayoutConfig(params_.channel_layout, channels),
                        params_.samples_per_second, extra_data,
                        EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
      if (params_.target_channel_layout != CHANNEL_LAYOUT_NONE) {
        ChannelLayoutConfig target_config;
        if (params_.target_channel_layout == CHANNEL_LAYOUT_7_1_4 &&
            !base::FeatureList::IsEnabled(kEnableHighChannelLayouts)) {
          target_config = ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE,
                                              kIamfDiscreteChannelCount);
        } else {
          target_config =
              ChannelLayoutConfig::FromLayout(params_.target_channel_layout);
        }
        config.set_target_output_channel_layout(target_config);
      }
    }

    EXPECT_EQ(params_.codec, config.codec());
    EXPECT_EQ(params_.samples_per_second, config.samples_per_second());
    EXPECT_EQ(params_.channel_layout, config.channel_layout());

    InitializeDecoder(config);
  }

  void InitializeDecoder(const AudioDecoderConfig& config) {
    InitializeDecoderWithResult(config, true);
  }

  void InitializeDecoderWithResult(const AudioDecoderConfig& config,
                                   bool success) {
    decoder_->Initialize(config, nullptr,
                         base::BindOnce(
                             [](bool success, DecoderStatus status) {
                               EXPECT_EQ(status.is_ok(), success);
                             },
                             success),
                         base::BindRepeating(&AudioDecoderTest::OnDecoderOutput,
                                             base::Unretained(this)),
                         base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  bool ReadAndDecodeNextPacket() {
    auto packet = ScopedAVPacket::Allocate();
    if (!reader_->ReadPacketForTesting(packet.get())) {
      return false;
    }

    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(AVPacketData(*packet));
    buffer->set_timestamp(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet->pts));
    buffer->set_duration(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet->duration));
    if (packet->flags & AV_PKT_FLAG_KEY) {
      buffer->set_is_key_frame(true);
    }

    // Don't set discard padding for Opus, it already has discard behavior set
    // based on the codec delay in the AudioDecoderConfig.
    if (decoder_type_ == AudioDecoderType::kFFmpeg &&
        params_.codec != AudioCodec::kOpus) {
      SetDiscardPadding(packet.get(), buffer.get(), params_.samples_per_second);
    }

    // DecodeBuffer() shouldn't need the original packet since it uses the copy.
    av_packet_unref(packet.get());
    DecodeBuffer(std::move(buffer));
    return true;
  }

  void Decode() { ASSERT_TRUE(ReadAndDecodeNextPacket()); }

  // Decodes all remaining packets from reader_ until EOF and sends EOS.
  void DecodeAllPackets() {
    while (ReadAndDecodeNextPacket()) {
    }
    SendEndOfStream();
  }

  int GetTotalDecodedFrames() const {
    int total_frames = 0;
    for (const auto& buffer : decoded_audio_) {
      total_frames += buffer->frame_count();
    }
    return total_frames;
  }

  void ResetDecoder() {
    ASSERT_FALSE(pending_reset_);
    pending_reset_ = true;

    base::RunLoop run_loop;
    decoder_->Reset(
        base::BindOnce(&AudioDecoderTest::ResetFinished, base::Unretained(this))
            .Then(run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_FALSE(pending_reset_);
  }

  void ResetReader() {
    ResetDecoder();
    decoded_audio_.clear();
    InitializeReader();
  }

  void OnDecoderOutput(scoped_refptr<AudioBuffer> buffer) {
    EXPECT_FALSE(buffer->end_of_stream());
    decoded_audio_.push_back(std::move(buffer));
  }

  void DecodeFinished(base::OnceClosure quit_closure, DecoderStatus status) {
    EXPECT_TRUE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
    pending_decode_ = false;
    last_decode_status_ = std::move(status);
    std::move(quit_closure).Run();
  }

  void ResetFinished() {
    EXPECT_TRUE(pending_reset_);
    EXPECT_FALSE(pending_decode_);
    pending_reset_ = false;
  }

  // Generates a SHA-256 hash of the audio signal.  Should not be used for
  // checks across platforms as audio varies slightly across platforms.
  std::string GetDecodedAudioSHA256(size_t i) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    std::unique_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    crypto::hash::Hasher hasher(crypto::hash::kSha256);
    for (int ch = 0; ch < output->channels(); ++ch) {
      // This is a bit dangerous: equivalent floats do not necessarily have the
      // same bit-for-bit representation, which is why we have to pass
      // allow_nonunique_obj in here. That makes this test a bit
      // over-conservative, since it will require that the output be a sequence
      // of bit-for-bit identical floats rather than a sequence of equivalent
      // floats, but that's okay.
      hasher.Update(
          base::as_byte_span(base::allow_nonunique_obj, output->channel(ch)));
    }
    std::array<uint8_t, crypto::hash::kSha256Size> digest;
    hasher.Finish(digest);
    return base::HexEncodeLower(digest);
  }

  void ExpectDecodedAudio(size_t i, const std::string& exact_hash) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    const DecodedBufferExpectations& sample_info = params_.expectations[i];

    const DecodedBufferExpectations* matched_info = &sample_info;
    if (params_.alt_expectations.has_value()) {
      const DecodedBufferExpectations& alt_info =
          (*params_.alt_expectations)[i];
      if (buffer->timestamp().InMicroseconds() == alt_info.timestamp &&
          buffer->duration().InMicroseconds() == alt_info.duration) {
        matched_info = &alt_info;
      } else {
        EXPECT_EQ(sample_info.timestamp, buffer->timestamp().InMicroseconds());
        EXPECT_EQ(sample_info.duration, buffer->duration().InMicroseconds());
      }
    } else {
      EXPECT_EQ(sample_info.timestamp, buffer->timestamp().InMicroseconds());
      EXPECT_EQ(sample_info.duration, buffer->duration().InMicroseconds());
    }
    EXPECT_FALSE(buffer->end_of_stream());

    std::unique_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    // Generate a lossy hash of the audio used for comparison across platforms.
    if (matched_info->hash) {
      AudioHash audio_hash;
      audio_hash.Update(output.get(), output->frames());
      EXPECT_TRUE(audio_hash.IsEquivalent(matched_info->hash, 0.03))
          << "Audio hashes differ. Expected: " << matched_info->hash
          << " Actual: " << audio_hash.ToString();
    }

    if (!exact_hash.empty()) {
      EXPECT_EQ(exact_hash, GetDecodedAudioSHA256(i));

      // Verify different hashes are being generated.  None of our test data
      // files have audio that hashes out exactly the same.
      if (i > 0) {
        EXPECT_NE(exact_hash, GetDecodedAudioSHA256(i - 1));
      }
    }
  }

  size_t decoded_audio_size() const { return decoded_audio_.size(); }
  base::TimeDelta start_timestamp() const { return start_timestamp_; }
  const DecoderStatus& last_decode_status() const {
    return last_decode_status_;
  }
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  AudioCodec codec() const { return params_.codec; }
  AudioDecoderType decoder_type() const { return decoder_type_; }

  std::unique_ptr<AudioFileReader> reader_;
  std::unique_ptr<AudioDecoder> decoder_;
  base::circular_deque<scoped_refptr<AudioBuffer>> decoded_audio_;

 private:
  const AudioDecoderType decoder_type_;

  // Current TestParams used to initialize the test and decoder. The initial
  // value is std::get<1>(GetParam()). Could be overridden by set_param() so
  // that the decoder can be reinitialized with different parameters.
  TestParams params_;

  base::test::SingleThreadTaskEnvironment task_environment_;

#if BUILDFLAG(IS_WIN)
  // MediaFoundationAudioDecoder calls CoInitialize() when creating the decoder.
  base::win::ScopedCOMInitializer com_initializer_;
#endif  // BUILDFLAG(IS_WIN)

  NullMediaLog media_log_;
  scoped_refptr<DecoderBuffer> data_;
  const char* filename_ = nullptr;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  bool pending_decode_ = false;
  bool pending_reset_ = false;
  DecoderStatus last_decode_status_ = DecoderStatus::Codes::kFailed;

  base::TimeDelta start_timestamp_;
};

constexpr DataExpectations kBearOpusExpectations = {{
    {500, 3500, "-0.26,0.87,1.36,0.84,-0.30,-1.22,"},
    {4000, 10000, "0.09,0.23,0.21,0.03,-0.17,-0.24,"},
    {14000, 10000, "0.10,0.24,0.23,0.04,-0.14,-0.23,"},
}};

constexpr TestParams kSfxOpusParams = {
    AudioCodec::kOpus,
    "sfx-opus.ogg",
    {{
        {0, 13500, "-2.70,-1.41,-0.78,-1.27,-2.56,-3.73,"},
        {13500, 20000, "5.48,5.93,6.05,5.83,5.54,5.46,"},
        {33500, 20000, "-3.44,-3.34,-3.57,-4.11,-4.74,-5.13,"},
    }},
    -312,
    48000,
    CHANNEL_LAYOUT_MONO};

constexpr TestParams kBearOpusParams = {
    AudioCodec::kOpus,    "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
    CHANNEL_LAYOUT_STEREO};

constexpr TestParams kOpusTestParams[] = {kSfxOpusParams, kBearOpusParams};

// Test params to test decoder reinitialization. Choose opus because it is
// supported on all platforms we test on.
constexpr const TestParams& kReinitializeTestParams = kBearOpusParams;

constexpr TestParams kHatBrokenParams = {
    AudioCodec::kPCM,
    "hat_broken.wav",
    {{{0, 0, nullptr}, {0, 0, nullptr}, {0, 0, nullptr}}},
    0,
    44100,
    CHANNEL_LAYOUT_MONO};

#if BUILDFLAG(IS_ANDROID)
constexpr TestParams kMediaCodecTestParams[] = {
    kBearOpusParams,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {AudioCodec::kAAC,
     "sfx.adts",
     {{
         {0, 23219, "-1.80,-1.49,-0.23,1.11,1.54,-0.11,"},
         {23219, 23219, "-1.90,-1.53,-0.15,1.28,1.23,-0.33,"},
         {46439, 23219, "0.54,0.88,2.19,3.54,3.24,1.63,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_MONO},
    {AudioCodec::kAAC,
     "bear-audio-implicit-he-aac-v2.aac",
     {{
         {0, 42666, "-1.76,-0.12,1.72,1.45,0.10,-1.32,"},
         {42666, 42666, "-1.78,-0.13,1.70,1.44,0.09,-1.32,"},
         {85333, 42666, "-1.78,-0.13,1.70,1.44,0.08,-1.33,"},
     }},
     0,
     24000,
     CHANNEL_LAYOUT_MONO},
#endif  // defined(USE_PROPRIETARY_CODECS)
};
#endif  // BUILDFLAG(IS_ANDROID)

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(USE_PROPRIETARY_CODECS)
// Note: We don't test hashes for xHE-AAC content since the decoder is provided
// by the operating system and will apply DRC based on device specific params.
//
// On Windows, the AAC MFT may apply decoder delay compensation for xHE-AAC
// (USAC), stripping 5ms of peak limiter delay from the first decoded buffer
// and shifting subsequent timestamps.
// TODO(crbug.com/503857970): Remove once the old MFT is no longer in use.
constexpr TestParams kXheAacTestParams[] = {
    {AudioCodec::kAAC,
     "noise-xhe-aac.mp4",
     {{
         {0, 42666, nullptr},
         {42666, 42666, nullptr},
         {85333, 42666, nullptr},
     }},
     0,
     48000,
     CHANNEL_LAYOUT_STEREO,
     AudioCodecProfile::kXHE_AAC,
#if BUILDFLAG(IS_WIN)
     DataExpectations({{
         {0, 37666, nullptr},
         {37666, 42666, nullptr},
         {80333, 42666, nullptr},
     }})
#endif
    },
// Windows doesn't support 29.4kHz
#if !BUILDFLAG(IS_WIN)
    {AudioCodec::kAAC,
     "noise-xhe-aac-mono.mp4",
     {{
         {0, 34829, nullptr},
         {34829, 34829, nullptr},
         {69659, 34829, nullptr},
     }},
     0,
     29400,
     CHANNEL_LAYOUT_UNSUPPORTED,
     AudioCodecProfile::kXHE_AAC},
#endif
    {AudioCodec::kAAC,
     "noise-xhe-aac-44kHz.mp4",
     {{
         {0, 23219, nullptr},
         {23219, 23219, nullptr},
         {46439, 23219, nullptr},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_STEREO,
     AudioCodecProfile::kXHE_AAC,
#if BUILDFLAG(IS_WIN)
     DataExpectations({{
         {0, 18231, nullptr},
         {18231, 23219, nullptr},
         {41451, 23219, nullptr},
     }})
#endif
    },
};
#endif  // (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) &&
        // BUILDFLAG(USE_PROPRIETARY_CODECS)

constexpr DataExpectations kSfxFlacExpectations = {{
    {0, 104489, "-2.42,-1.12,0.71,1.70,1.09,-0.68,"},
    {104489, 104489, "-1.99,-0.67,1.18,2.19,1.60,-0.16,"},
    {208979, 79433, "2.84,2.70,3.23,4.06,4.59,4.44,"},
}};

constexpr DataExpectations kBearFlac192kHzExpectations = {
    {{0, 85333, "-0.30,-0.76,0.01,0.52,2.09,0.90,"},
     {85333, 85333, "-3.54,-1.84,-3.22,-0.56,-1.13,-0.17,"},
     {170666, 85333, "0.79,-0.39,1.11,0.89,3.22,1.28,"}}};

constexpr DataExpectations kSfxVorbisSymphoniaExpectations = {{
    {0, 13061, nullptr},
    {13061, 23219, nullptr},
    {36281, 23219, nullptr},
}};

constexpr DataExpectations kBearVorbisSymphoniaExpectations = {{
    {0, 2902, nullptr},
    {2902, 13061, nullptr},
    {15963, 23219, nullptr},
}};

constexpr TestParams kFlacMonoParams = {
    AudioCodec::kFLAC,  "sfx-flac.mp4", kSfxFlacExpectations, 0, 44100,
    CHANNEL_LAYOUT_MONO};
constexpr TestParams kFlacStereoParams = {AudioCodec::kFLAC,
                                          "bear-flac-192kHz.mp4",
                                          kBearFlac192kHzExpectations,
                                          0,
                                          192000,
                                          CHANNEL_LAYOUT_STEREO};

constexpr TestParams kCommonTestParams[] = {
    {AudioCodec::kMP3,
     "sfx.mp3",
     {{
         {0, 1065, "2.81,3.99,4.53,4.10,3.08,2.46,"},
         {1065, 26122, "-3.81,-4.14,-3.90,-3.36,-3.03,-3.23,"},
         {27188, 26122, "4.24,3.95,4.22,4.78,5.13,4.93,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_MONO,
     AudioCodecProfile::kUnknown,
     DataExpectations{{
         {0, 26122, nullptr},
         {26122, 26122, nullptr},
         {52244, 26122, nullptr},
     }}},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {AudioCodec::kAAC,
     "sfx.adts",
     {{
         {0, 23219, "-1.90,-1.53,-0.15,1.28,1.23,-0.33,"},
         {23219, 23219, "0.54,0.88,2.19,3.54,3.24,1.63,"},
         {46439, 23219, "1.42,1.69,2.95,4.23,4.02,2.36,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_MONO},
#endif
    kFlacMonoParams,
    {AudioCodec::kFLAC, "sfx.flac", kSfxFlacExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    kFlacStereoParams,
    {AudioCodec::kPCM,
     "sfx_f32le.wav",
     {{
         {0, 23219, "-1.23,-0.87,0.47,1.85,1.88,0.29,"},
         {23219, 23219, "0.75,1.10,2.43,3.78,3.53,1.93,"},
         {46439, 23219, "1.27,1.56,2.83,4.13,3.87,2.23,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_MONO},
    {AudioCodec::kPCM,
     "4ch.wav",
     {{
         {0, 11609, "-1.68,1.68,0.89,-3.45,1.52,1.15,"},
         {11609, 11609, "43.26,9.06,18.27,35.98,19.45,7.46,"},
         {23219, 11609, "36.37,9.45,16.04,27.67,18.81,10.15,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_QUAD},
    {AudioCodec::kVorbis,
     "sfx.ogg",
     {{
         {0, 13061, "-0.33,1.25,2.86,3.26,2.09,0.14,"},
         {13061, 23219, "-2.79,-2.42,-1.06,0.33,0.93,-0.64,"},
         {36281, 23219, "-1.19,-0.80,0.57,1.97,2.08,0.51,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_MONO,
     AudioCodecProfile::kUnknown,
     kSfxVorbisSymphoniaExpectations},
    {AudioCodec::kVorbis,
     "bear.ogv",
     {{
         {0, 2902, "-1.08,0.61,0.81,0.43,-0.60,-1.06,"},
         {2902, 23219, "-1.44,-1.27,0.18,1.37,1.95,0.13,"},
         {26122, 23219, "-1.80,-1.41,-0.13,1.30,1.65,0.01,"},
     }},
     -704,
     44100,
     CHANNEL_LAYOUT_STEREO,
     AudioCodecProfile::kUnknown,
     kBearVorbisSymphoniaExpectations},
    kSfxOpusParams,
    kBearOpusParams};

#if BUILDFLAG(ENABLE_IAMF_TOOLS)
constexpr DataExpectations kIamfExpectations = {{
    {0, 20000, nullptr},      // Timestamp 0us, Duration 20ms
    {20000, 20000, nullptr},  // Timestamp 20ms, Duration 20ms
    {40000, 20000, nullptr}   // Timestamp 40ms, Duration 20ms
}};

static const uint8_t kIamf714ExtraData[] = {
    0xf8, 0x06, 0x69, 0x61, 0x6d, 0x66, 0x00, 0x00, 0x00, 0x14, 0x00, 0x4f,
    0x70, 0x75, 0x73, 0xc0, 0x07, 0xff, 0xfc, 0x01, 0x02, 0x01, 0x38, 0x00,
    0x00, 0xbb, 0x80, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x01, 0x00, 0x00, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x01, 0x00, 0x80, 0xf7,
    0x02, 0x00, 0xc0, 0x07, 0xc0, 0x07, 0x00, 0x00, 0x20, 0x70, 0x07, 0x05,
    0x10, 0x41, 0x03, 0x01, 0x65, 0x6e, 0x2d, 0x75, 0x73, 0x00, 0x64, 0x65,
    0x66, 0x61, 0x75, 0x6c, 0x74, 0x5f, 0x6d, 0x69, 0x78, 0x5f, 0x70, 0x72,
    0x65, 0x73, 0x65, 0x6e, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x01,
    0x01, 0x01, 0x37, 0x2e, 0x31, 0x2e, 0x34, 0x00, 0x40, 0x00, 0x65, 0x80,
    0xf7, 0x02, 0x80, 0x00, 0x00, 0x64, 0x80, 0xf7, 0x02, 0x80, 0x00, 0x00,
    0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t kIamfStereoExtraData[] = {
    0xf8, 0x06, 0x69, 0x61, 0x6d, 0x66, 0x00, 0x00, 0x00, 0x14, 0x00,
    0x4f, 0x70, 0x75, 0x73, 0xc0, 0x07, 0xff, 0xfc, 0x01, 0x02, 0x01,
    0x38, 0x00, 0x00, 0xbb, 0x80, 0x00, 0x00, 0x00, 0x08, 0x0a, 0x01,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x10, 0x01, 0x01, 0x10, 0x42,
    0x03, 0x01, 0x65, 0x6e, 0x2d, 0x75, 0x73, 0x00, 0x64, 0x65, 0x66,
    0x61, 0x75, 0x6c, 0x74, 0x5f, 0x6d, 0x69, 0x78, 0x5f, 0x70, 0x72,
    0x65, 0x73, 0x65, 0x6e, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00,
    0x01, 0x01, 0x01, 0x73, 0x74, 0x65, 0x72, 0x65, 0x6f, 0x00, 0x00,
    0x00, 0x65, 0x80, 0xf7, 0x02, 0x80, 0x00, 0x00, 0x64, 0x80, 0xf7,
    0x02, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};

// Only IAMF Audio Streams can be decoded by this decoder.
constexpr TestParams kIamfTestParams[] = {
    {AudioCodec::kIAMF, "iamf_alternating_sine_waves_714.mp4",
     kIamfExpectations, 0, 48000, CHANNEL_LAYOUT_DISCRETE,
     AudioCodecProfile::kUnknown, std::nullopt, kIamf714ExtraData,
     CHANNEL_LAYOUT_NONE},
    {AudioCodec::kIAMF, "iamf_alternating_sine_waves_714.mp4",
     kIamfExpectations, 0, 48000, CHANNEL_LAYOUT_DISCRETE,
     AudioCodecProfile::kUnknown, std::nullopt, kIamf714ExtraData,
     CHANNEL_LAYOUT_STEREO},
    {AudioCodec::kIAMF, "iamf_alternating_sine_waves_stereo.mp4",
     kIamfExpectations, 0, 48000, CHANNEL_LAYOUT_STEREO,
     AudioCodecProfile::kUnknown, std::nullopt, kIamfStereoExtraData,
     CHANNEL_LAYOUT_NONE},
    {AudioCodec::kIAMF, "iamf_alternating_sine_waves_stereo.mp4",
     kIamfExpectations, 0, 48000, CHANNEL_LAYOUT_STEREO,
     AudioCodecProfile::kUnknown, std::nullopt, kIamfStereoExtraData,
     CHANNEL_LAYOUT_7_1_4},
};
#endif

void AudioDecoderTest::SetReinitializeParams() {
#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(USE_PROPRIETARY_CODECS)
  // AudioToolbox and MediaFoundation only support xHE-AAC, so we can't use the
  // Opus params. We can instead just swap between the two test parameter sets.
  if (decoder_type_ == AudioDecoderType::kAudioToolbox ||
      decoder_type_ == AudioDecoderType::kMediaFoundation) {
    set_params(params_.channel_layout == kXheAacTestParams[0].channel_layout
                   ? kXheAacTestParams[1]
                   : kXheAacTestParams[0]);
    return;
  }
#endif

#if BUILDFLAG(ENABLE_SYMPHONIA)
  // Currently, Symphonia does not support Opus audio, so we can't use the Opus
  // params for reinitialization. Modify the channel layout instead.
  if (decoder_type_ == AudioDecoderType::kSymphonia) {
    set_params(params_.channel_layout == kFlacMonoParams.channel_layout
                   ? kFlacStereoParams
                   : kFlacMonoParams);
    return;
  }
#endif

#if BUILDFLAG(ENABLE_IAMF_TOOLS)
  if (decoder_type_ == AudioDecoderType::kIamf) {
    set_params(kIamfTestParams[0]);
    return;
  }
#endif

  set_params(kReinitializeTestParams);
}

TEST_P(AudioDecoderTest, Initialize) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
}

TEST_P(AudioDecoderTest, Reinitialize_AfterInitialize) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  SetReinitializeParams();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
}

TEST_P(AudioDecoderTest, Reinitialize_AfterDecode) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
  SetReinitializeParams();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
}

TEST_P(AudioDecoderTest, Reinitialize_AfterReset) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
  ResetDecoder();
  SetReinitializeParams();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
}

// Verifies decode audio as well as the Decode() -> ResetDecoder() sequence.
TEST_P(AudioDecoderTest, ProduceAudioSamples) {
  ASSERT_NO_FATAL_FAILURE(Initialize());

  // Run the test multiple times with a reset back to the beginning in between.
  std::vector<std::string> decoded_audio_sha256_hashes;
  for (int i = 0; i < 2; ++i) {
    // Run decoder until we get at least |kDecodeRuns| output buffers.
    // Keeping Decode() in a loop seems to be the simplest way to guarantee that
    // the predefined number of output buffers are produced without draining
    // (i.e. decoding EOS).
    do {
      ASSERT_NO_FATAL_FAILURE(Decode());
      ASSERT_TRUE(last_decode_status().is_ok());
    } while (decoded_audio_size() < kDecodeRuns);

    // With MediaCodecAudioDecoder the output buffers might appear after
    // some delay. Since we keep decoding in a loop, the number of output
    // buffers when they eventually appear might exceed |kDecodeRuns|.
    ASSERT_LE(kDecodeRuns, decoded_audio_size());

    // On the first pass record the exact SHA-256 hash for each decoded buffer.
    if (i == 0) {
      for (size_t j = 0; j < kDecodeRuns; ++j) {
        decoded_audio_sha256_hashes.push_back(GetDecodedAudioSHA256(j));
      }
    }

    // On the first pass verify the basic audio hash and sample info.  On the
    // second, verify the exact SHA-256 for each packet.  It shouldn't change.
    for (size_t j = 0; j < kDecodeRuns; ++j) {
      SCOPED_TRACE(base::StringPrintf("i = %d, j = %" PRIuS, i, j));
      ExpectDecodedAudio(j, i == 0 ? "" : decoded_audio_sha256_hashes[j]);
    }

    SendEndOfStream();
    ResetReader();
  }
}

TEST_P(AudioDecoderTest, VerifyIamfOutputLayout) {
  if (!IsIamfTest()) {
    GTEST_SKIP() << "Only for IAMF";
  }
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(Decode());
  EXPECT_TRUE(last_decode_status().is_ok());
  VerifyIamfOutputLayout();
}

// Verifies that disabling decoder delay discard (disable_discard_decoder_delay)
// causes the decoder to output all decoded samples (including the initial
// codec delay / pre-skip samples) rather than discarding them.
TEST_P(AudioDecoderTest, OpusDisableDiscardDecoderDelay) {
  if (codec() != AudioCodec::kOpus) {
    GTEST_SKIP() << "Only for Opus";
  }

  if (decoder_type() != AudioDecoderType::kOpus &&
      decoder_type() != AudioDecoderType::kFFmpeg) {
    GTEST_SKIP()
        << "Decoder type does not support disable_discard_decoder_delay";
  }

  // 1. Decode the entire file with default discard_decoder_delay enabled.
  ASSERT_NO_FATAL_FAILURE(Initialize());
  DecodeAllPackets();
  const int total_frames_default = GetTotalDecodedFrames();

  // 2. Re-initialize the decoder with disable_discard_decoder_delay().
  ResetReader();
  decoded_audio_.clear();

  AudioDecoderConfig config;
  ASSERT_TRUE(AVCodecContextToAudioDecoderConfig(
      reader_->codec_context_for_testing(), EncryptionScheme::kUnencrypted,
      &config));
  config.disable_discard_decoder_delay();
  EXPECT_FALSE(config.should_discard_decoder_delay());

  InitializeDecoder(config);
  DecodeAllPackets();
  const int total_frames_disabled_discard = GetTotalDecodedFrames();

  // 3. Verify that disabling delay discard outputs extra samples corresponding
  // to the Opus header skip_samples (312 samples for sfx-opus and bear-opus).
  EXPECT_GT(total_frames_disabled_discard, total_frames_default);
  EXPECT_EQ(total_frames_disabled_discard - total_frames_default, 312);
}

TEST_P(AudioDecoderTest, OpusMismatchedExtraDataChannels) {
  if (codec() != AudioCodec::kOpus ||
      decoder_type() != AudioDecoderType::kOpus) {
    GTEST_SKIP() << "Only for OpusAudioDecoder";
  }

  ASSERT_NO_FATAL_FAILURE(Initialize());

  // Construct Opus extradata for 2 channels with Vorbis mapping family 1.
  const uint8_t extra_data_vorbis2ch[] = {
      'O',  'p',  'u',  's',  'H', 'e', 'a', 'd',  // Magic
      1,                                           // Version
      2,                                           // Channels = 2
      0x38, 0x01,                                  // Skip samples = 312
      0x80, 0xBB, 0x00, 0x00,                      // Sample rate = 48000
      0x00, 0x00,                                  // Gain = 0
      1,       // Channel mapping family = 1 (Vorbis)
      1,       // Streams = 1
      1,       // Coupled = 1
      0,    1  // Stream map (2 bytes)
  };

  AudioDecoderConfig config;
  config.Initialize(AudioCodec::kOpus, kSampleFormatF32,
                    ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>(),
                    48000,
                    std::vector<uint8_t>(std::begin(extra_data_vorbis2ch),
                                         std::end(extra_data_vorbis2ch)),
                    EncryptionScheme::kUnencrypted, base::TimeDelta(), 312);

  InitializeDecoderWithResult(config, false);
}

TEST_P(AudioDecoderTest, Decode) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
  EXPECT_TRUE(last_decode_status().is_ok());
}

TEST_P(AudioDecoderTest, MismatchedSubsampleBuffer) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  DecodeBuffer(CreateMismatchedBufferForTest());
  EXPECT_TRUE(!last_decode_status().is_ok());
}

// The AudioDecoders do not support encrypted buffers since they were
// initialized without cdm_context.
TEST_P(AudioDecoderTest, EncryptedBuffer) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  DecodeBuffer(CreateFakeEncryptedBuffer());
  EXPECT_TRUE(!last_decode_status().is_ok());
}

TEST_P(AudioDecoderTest, DecodeEOSFirst) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  SendEndOfStream();
  EXPECT_TRUE(last_decode_status().is_ok());
}

TEST_P(AudioDecoderTest, DecodeEOSFirstThenResetAndDecode) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  SendEndOfStream();
  EXPECT_TRUE(last_decode_status().is_ok());
  ResetDecoder();
  ASSERT_NO_FATAL_FAILURE(Decode());
  EXPECT_TRUE(last_decode_status().is_ok());
}

TEST_P(AudioDecoderTest, Reset) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ResetDecoder();
}

TEST_P(AudioDecoderTest, NoTimestamp) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  auto buffer = base::MakeRefCounted<DecoderBuffer>(0);
  buffer->set_timestamp(kNoTimestamp);
  DecodeBuffer(std::move(buffer));
  EXPECT_THAT(last_decode_status(), IsDecodeErrorStatus());
}

TEST_P(AudioDecoderTest, EmptyBuffer) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  auto buffer = base::MakeRefCounted<DecoderBuffer>(0);
  buffer->set_timestamp(base::Microseconds(0));
  DecodeBuffer(std::move(buffer));
  EXPECT_TRUE(last_decode_status().is_ok());
}

TEST_P(AudioDecoderTest, EOSBuffer) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  EXPECT_TRUE(last_decode_status().is_ok());
}

class WavOddChunkTest : public AudioDecoderTest {
 public:
  WavOddChunkTest() = default;
};

TEST_P(WavOddChunkTest, DecodeWavWithOddChunk) {
  ASSERT_NO_FATAL_FAILURE(Initialize());

  auto packet = ScopedAVPacket::Allocate();
  while (reader_->ReadPacketForTesting(packet.get())) {
    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(AVPacketData(*packet));

    bool decode_done = false;
    base::RunLoop decode_run_loop;
    decoder_->Decode(std::move(buffer),
                     base::BindOnce(
                         [](bool* decode_done, base::OnceClosure quit_closure,
                            DecoderStatus status) {
                           EXPECT_TRUE(status.is_ok());
                           *decode_done = true;
                           std::move(quit_closure).Run();
                         },
                         &decode_done, decode_run_loop.QuitClosure()));

    decode_run_loop.Run();
    EXPECT_TRUE(decode_done);
    av_packet_unref(packet.get());
  }

  int total_frames = 0;
  for (const auto& buffer : decoded_audio_) {
    total_frames += buffer->frame_count();
  }
  EXPECT_EQ(6615, total_frames);
}

INSTANTIATE_TEST_SUITE_P(
    WavOddChunk,
    WavOddChunkTest,
    testing::Values(std::make_tuple(AudioDecoderType::kFFmpeg, kHatBrokenParams)
#if BUILDFLAG(ENABLE_SYMPHONIA)
                        ,
                    std::make_tuple(AudioDecoderType::kSymphonia,
                                    kHatBrokenParams)
#endif
                        ));

INSTANTIATE_TEST_SUITE_P(FFmpeg,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kFFmpeg),
                                 ValuesIn(kCommonTestParams)));

INSTANTIATE_TEST_SUITE_P(Opus,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kOpus),
                                 ValuesIn(kOpusTestParams)));

#if BUILDFLAG(ENABLE_SYMPHONIA)
INSTANTIATE_TEST_SUITE_P(Symphonia,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kSymphonia),
                                 ValuesIn(kCommonTestParams)));
#endif

#if BUILDFLAG(ENABLE_IAMF_TOOLS)
INSTANTIATE_TEST_SUITE_P(Iamf,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kIamf),
                                 ValuesIn(kIamfTestParams)));
#endif

#if BUILDFLAG(IS_ANDROID)
std::vector<TestParams> GetAndroidParams() {
  std::vector<TestParams> params;
  params.insert(params.end(), std::cbegin(kMediaCodecTestParams),
                std::cend(kMediaCodecTestParams));
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  params.insert(params.end(), std::cbegin(kXheAacTestParams),
                std::cend(kXheAacTestParams));
#endif
  return params;
}

INSTANTIATE_TEST_SUITE_P(MediaCodec,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kMediaCodec),
                                 ValuesIn(GetAndroidParams())));
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
INSTANTIATE_TEST_SUITE_P(AudioToolbox,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kAudioToolbox),
                                 ValuesIn(kXheAacTestParams)));
#elif BUILDFLAG(IS_WIN)
INSTANTIATE_TEST_SUITE_P(MediaFoundation,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kMediaFoundation),
                                 ValuesIn(kXheAacTestParams)));
#endif
#endif

#if BUILDFLAG(ENABLE_SYMPHONIA)
TEST(SymphoniaAudioDecoderStandaloneTest, ToSymphoniaPacketNullTimestamp) {
  auto buffer = base::MakeRefCounted<DecoderBuffer>(10);
  buffer->set_timestamp(base::Microseconds(100));
  SymphoniaPacket packet = ToSymphoniaPacket(*buffer, std::nullopt);
  EXPECT_EQ(packet.timestamp_us, 0uL);
}
#endif

}  // namespace media
