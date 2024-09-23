// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
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
#include "media/media_buildflags.h"
#include "media/mojo/services/gpu_mojo_media_client_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
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
  if (skip_samples_size < 4)
    return;
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
        params_(std::get<1>(GetParam())),
        pending_decode_(false),
        pending_reset_(false),
        last_decode_status_(DecoderStatus::Codes::kFailed) {
    AddSupplementalCodecsForTesting();
    switch (decoder_type_) {
      case AudioDecoderType::kFFmpeg:
        decoder_ = std::make_unique<FFmpegAudioDecoder>(
            task_environment_.GetMainThreadTaskRunner(), &media_log_);
        break;
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
    if (!IsSupported())
      GTEST_SKIP() << "Unsupported platform.";
  }

 protected:
  bool IsSupported() const {
    if (params_.profile == AudioCodecProfile::kXHE_AAC) {
      return IsSupportedAudioType(
          {AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC, false});
    }
    return true;
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

  void Initialize() {
    // Load the test data file.
    data_ = ReadTestDataFile(params_.filename);
    protocol_ = std::make_unique<InMemoryUrlProtocol>(data_->data(),
                                                      data_->size(), false);
    reader_ = std::make_unique<AudioFileReader>(protocol_.get());
    ASSERT_TRUE(reader_->OpenDemuxerForTesting());

    // Load the first packet and check its timestamp.
    auto packet = ScopedAVPacket::Allocate();
    ASSERT_TRUE(reader_->ReadPacketForTesting(packet.get()));
    EXPECT_EQ(params_.first_packet_pts, packet->pts);
    start_timestamp_ = ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet->pts);

    // Seek back to the beginning.
    ASSERT_TRUE(reader_->SeekForTesting(start_timestamp_));

    AudioDecoderConfig config;
    ASSERT_TRUE(AVCodecContextToAudioDecoderConfig(
        reader_->codec_context_for_testing(), EncryptionScheme::kUnencrypted,
        &config));

#if (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(USE_PROPRIETARY_CODECS)
    // MediaCodec type requires config->aac_extra_data() for AAC codec. For ADTS
    // streams we need to extract it with a separate procedure.
    if ((decoder_type_ == AudioDecoderType::kMediaCodec ||
         decoder_type_ == AudioDecoderType::kMediaFoundation) &&
        params_.codec == AudioCodec::kAAC && config.aac_extra_data().empty()) {
      int sample_rate;
      ChannelLayout channel_layout;
      std::vector<uint8_t> extra_data;
      ASSERT_GT(ADTSStreamParser().ParseFrameHeader(
                    packet->data, packet->size, nullptr, &sample_rate,
                    &channel_layout, nullptr, nullptr, &extra_data),
                0);
      config.Initialize(AudioCodec::kAAC, kSampleFormatS16, channel_layout,
                        sample_rate, extra_data, EncryptionScheme::kUnencrypted,
                        base::TimeDelta(), 0);
      config.set_aac_extra_data(extra_data);
      ASSERT_FALSE(config.extra_data().empty());
    }
#endif

    av_packet_unref(packet.get());

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

  void Decode() {
    auto packet = ScopedAVPacket::Allocate();
    ASSERT_TRUE(reader_->ReadPacketForTesting(packet.get()));

    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(AVPacketData(*packet));
    buffer->set_timestamp(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet->pts));
    buffer->set_duration(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet->duration));
    if (packet->flags & AV_PKT_FLAG_KEY)
      buffer->set_is_key_frame(true);

    // Don't set discard padding for Opus, it already has discard behavior set
    // based on the codec delay in the AudioDecoderConfig.
    if (decoder_type_ == AudioDecoderType::kFFmpeg &&
        params_.codec != AudioCodec::kOpus) {
      SetDiscardPadding(packet.get(), buffer.get(), params_.samples_per_second);
    }

    // DecodeBuffer() shouldn't need the original packet since it uses the copy.
    av_packet_unref(packet.get());
    DecodeBuffer(std::move(buffer));
  }

  void Reset() {
    ASSERT_FALSE(pending_reset_);
    pending_reset_ = true;
    decoder_->Reset(base::BindOnce(&AudioDecoderTest::ResetFinished,
                                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(pending_reset_);
  }

  void Seek(base::TimeDelta seek_time) {
    Reset();
    decoded_audio_.clear();
    ASSERT_TRUE(reader_->SeekForTesting(seek_time));
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

  // Generates an MD5 hash of the audio signal.  Should not be used for checks
  // across platforms as audio varies slightly across platforms.
  std::string GetDecodedAudioMD5(size_t i) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    std::unique_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    base::MD5Context context;
    base::MD5Init(&context);
    for (int ch = 0; ch < output->channels(); ++ch) {
      base::MD5Update(
          &context,
          std::string_view(reinterpret_cast<char*>(output->channel(ch)),
                           output->frames() * sizeof(*output->channel(ch))));
    }
    base::MD5Digest digest;
    base::MD5Final(&digest, &context);
    return base::MD5DigestToBase16(digest);
  }

  void ExpectDecodedAudio(size_t i, const std::string& exact_hash) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    const DecodedBufferExpectations& sample_info = params_.expectations[i];
    EXPECT_EQ(sample_info.timestamp, buffer->timestamp().InMicroseconds());
    EXPECT_EQ(sample_info.duration, buffer->duration().InMicroseconds());
    EXPECT_FALSE(buffer->end_of_stream());

    std::unique_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    // Generate a lossy hash of the audio used for comparison across platforms.
    if (sample_info.hash) {
      AudioHash audio_hash;
      audio_hash.Update(output.get(), output->frames());
      EXPECT_TRUE(audio_hash.IsEquivalent(sample_info.hash, 0.03))
          << "Audio hashes differ. Expected: " << sample_info.hash
          << " Actual: " << audio_hash.ToString();
    }

    if (!exact_hash.empty()) {
      EXPECT_EQ(exact_hash, GetDecodedAudioMD5(i));

      // Verify different hashes are being generated.  None of our test data
      // files have audio that hashes out exactly the same.
      if (i > 0)
        EXPECT_NE(exact_hash, GetDecodedAudioMD5(i - 1));
    }
  }

  size_t decoded_audio_size() const { return decoded_audio_.size(); }
  base::TimeDelta start_timestamp() const { return start_timestamp_; }
  const DecoderStatus& last_decode_status() const {
    return last_decode_status_;
  }

 private:
  const AudioDecoderType decoder_type_;

  // Current TestParams used to initialize the test and decoder. The initial
  // valie is std::get<1>(GetParam()). Could be overridden by set_param() so
  // that the decoder can be reinitialized with different parameters.
  TestParams params_;

  base::test::SingleThreadTaskEnvironment task_environment_;

#if BUILDFLAG(IS_WIN)
  // MediaFoundationAudioDecoder calls CoInitialize() when creating the decoder.
  base::win::ScopedCOMInitializer com_initializer_;
#endif  // BUILDFLAG(IS_WIN)

  NullMediaLog media_log_;
  scoped_refptr<DecoderBuffer> data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<AudioFileReader> reader_;

  std::unique_ptr<AudioDecoder> decoder_;
  bool pending_decode_;
  bool pending_reset_;
  DecoderStatus last_decode_status_ = DecoderStatus::Codes::kOk;

  base::circular_deque<scoped_refptr<AudioBuffer>> decoded_audio_;
  base::TimeDelta start_timestamp_;
};

constexpr DataExpectations kBearOpusExpectations = {{
    {500, 3500, "-0.26,0.87,1.36,0.84,-0.30,-1.22,"},
    {4000, 10000, "0.09,0.23,0.21,0.03,-0.17,-0.24,"},
    {14000, 10000, "0.10,0.24,0.23,0.04,-0.14,-0.23,"},
}};

// Test params to test decoder reinitialization. Choose opus because it is
// supported on all platforms we test on.
constexpr TestParams kReinitializeTestParams = {
    AudioCodec::kOpus,    "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
    CHANNEL_LAYOUT_STEREO};

#if BUILDFLAG(IS_ANDROID)
constexpr TestParams kMediaCodecTestParams[] = {
    {AudioCodec::kOpus, "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
     CHANNEL_LAYOUT_STEREO},
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
     AudioCodecProfile::kXHE_AAC},
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
     AudioCodecProfile::kXHE_AAC},
};
#endif  // (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) &&
        // BUILDFLAG(USE_PROPRIETARY_CODECS)

constexpr DataExpectations kSfxFlacExpectations = {{
    {0, 104489, "-2.42,-1.12,0.71,1.70,1.09,-0.68,"},
    {104489, 104489, "-1.99,-0.67,1.18,2.19,1.60,-0.16,"},
    {208979, 79433, "2.84,2.70,3.23,4.06,4.59,4.44,"},
}};

constexpr TestParams kFFmpegTestParams[] = {
    {AudioCodec::kMP3,
     "sfx.mp3",
     {{
         {0, 1065, "2.81,3.99,4.53,4.10,3.08,2.46,"},
         {1065, 26122, "-3.81,-4.14,-3.90,-3.36,-3.03,-3.23,"},
         {27188, 26122, "4.24,3.95,4.22,4.78,5.13,4.93,"},
     }},
     0,
     44100,
     CHANNEL_LAYOUT_MONO},
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
    {AudioCodec::kFLAC, "sfx-flac.mp4", kSfxFlacExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {AudioCodec::kFLAC, "sfx.flac", kSfxFlacExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
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
     CHANNEL_LAYOUT_MONO},
    // Note: bear.ogv is incorrectly muxed such that valid samples are given
    // negative timestamps, this marks them for discard per the ogg vorbis spec.
    {AudioCodec::kVorbis,
     "bear.ogv",
     {{
         {0, 13061, "-1.25,0.10,2.11,2.29,1.50,-0.68,"},
         {13061, 23219, "-1.80,-1.41,-0.13,1.30,1.65,0.01,"},
         {36281, 23219, "-1.43,-1.25,0.11,1.29,1.86,0.14,"},
     }},
     -704,
     44100,
     CHANNEL_LAYOUT_STEREO},
    {AudioCodec::kOpus,
     "sfx-opus.ogg",
     {{
         {0, 13500, "-2.70,-1.41,-0.78,-1.27,-2.56,-3.73,"},
         {13500, 20000, "5.48,5.93,6.05,5.83,5.54,5.46,"},
         {33500, 20000, "-3.44,-3.34,-3.57,-4.11,-4.74,-5.13,"},
     }},
     -312,
     48000,
     CHANNEL_LAYOUT_MONO},
    {AudioCodec::kOpus, "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
     CHANNEL_LAYOUT_STEREO},
};

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
  Reset();
  SetReinitializeParams();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
}

// Verifies decode audio as well as the Decode() -> Reset() sequence.
TEST_P(AudioDecoderTest, ProduceAudioSamples) {
  ASSERT_NO_FATAL_FAILURE(Initialize());

  // Run the test multiple times with a seek back to the beginning in between.
  std::vector<std::string> decoded_audio_md5_hashes;
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

    // On the first pass record the exact MD5 hash for each decoded buffer.
    if (i == 0) {
      for (size_t j = 0; j < kDecodeRuns; ++j)
        decoded_audio_md5_hashes.push_back(GetDecodedAudioMD5(j));
    }

    // On the first pass verify the basic audio hash and sample info.  On the
    // second, verify the exact MD5 sum for each packet.  It shouldn't change.
    for (size_t j = 0; j < kDecodeRuns; ++j) {
      SCOPED_TRACE(base::StringPrintf("i = %d, j = %" PRIuS, i, j));
      ExpectDecodedAudio(j, i == 0 ? "" : decoded_audio_md5_hashes[j]);
    }

    SendEndOfStream();

    // Seek back to the beginning.  Calls Reset() on the decoder.
    Seek(start_timestamp());
  }
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

TEST_P(AudioDecoderTest, Reset) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Reset();
}

TEST_P(AudioDecoderTest, NoTimestamp) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  auto buffer = base::MakeRefCounted<DecoderBuffer>(0);
  buffer->set_timestamp(kNoTimestamp);
  DecodeBuffer(std::move(buffer));
  EXPECT_THAT(last_decode_status(), IsDecodeErrorStatus());
}

TEST_P(AudioDecoderTest, EOSBuffer) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  EXPECT_TRUE(last_decode_status().is_ok());
}

INSTANTIATE_TEST_SUITE_P(FFmpeg,
                         AudioDecoderTest,
                         Combine(Values(AudioDecoderType::kFFmpeg),
                                 ValuesIn(kFFmpegTestParams)));

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

}  // namespace media
