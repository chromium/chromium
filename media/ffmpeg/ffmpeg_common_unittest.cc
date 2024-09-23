// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/ffmpeg/ffmpeg_common.h"

#include <stddef.h>
#include <stdint.h>

#include <cstring>

#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FFmpegCommonTest : public testing::Test {
 public:
  FFmpegCommonTest() {}
  ~FFmpegCommonTest() override = default;
};

uint8_t kExtraData[5] = {0x00, 0x01, 0x02, 0x03, 0x04};

template <typename T>
void TestConfigConvertExtraData(
    AVStream* stream,
    T* decoder_config,
    const base::RepeatingCallback<bool(const AVStream*, T*)>& converter_fn) {
  // Should initially convert.
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));

  // Store orig to let FFmpeg free whatever it allocated.
  AVCodecParameters* codec_parameters = stream->codecpar;
  uint8_t* orig_extradata = codec_parameters->extradata;
  int orig_extradata_size = codec_parameters->extradata_size;

  // Valid combination: extra_data = nullptr && size = 0.
  codec_parameters->extradata = nullptr;
  codec_parameters->extradata_size = 0;
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));
  EXPECT_EQ(static_cast<size_t>(codec_parameters->extradata_size),
            decoder_config->extra_data().size());

  // Valid combination: extra_data = non-nullptr && size > 0.
  codec_parameters->extradata = &kExtraData[0];
  codec_parameters->extradata_size = std::size(kExtraData);
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));
  EXPECT_EQ(static_cast<size_t>(codec_parameters->extradata_size),
            decoder_config->extra_data().size());
  EXPECT_EQ(
      0, memcmp(codec_parameters->extradata, &decoder_config->extra_data()[0],
                decoder_config->extra_data().size()));

  // Possible combination: extra_data = nullptr && size != 0, but the converter
  // function considers this valid and having no extra_data, due to behavior of
  // avcodec_parameters_to_context().
  codec_parameters->extradata = nullptr;
  codec_parameters->extradata_size = 10;
  EXPECT_TRUE(converter_fn.Run(stream, decoder_config));
  EXPECT_EQ(0UL, decoder_config->extra_data().size());

  // Invalid combination: extra_data = non-nullptr && size = 0.
  codec_parameters->extradata = &kExtraData[0];
  codec_parameters->extradata_size = 0;
  EXPECT_FALSE(converter_fn.Run(stream, decoder_config));

  // Restore orig values for sane cleanup.
  codec_parameters->extradata = orig_extradata;
  codec_parameters->extradata_size = orig_extradata_size;
}

void VerifyProfileTest(const char* file_name,
                       VideoCodecProfile expected_profile) {
  // Open a file to get a real AVStreams from FFmpeg.
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(GetTestDataFilePath(file_name)));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());
  AVFormatContext* format_context = glue.format_context();

  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVStream* stream = format_context->streams[i];
    AVCodecParameters* codec_parameters = stream->codecpar;
    AVMediaType codec_type = codec_parameters->codec_type;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
      VideoDecoderConfig video_config;
      EXPECT_TRUE(AVStreamToVideoDecoderConfig(stream, &video_config));
      EXPECT_EQ(expected_profile, video_config.profile());
    } else {
      // Only process video.
      continue;
    }
  }
}

TEST_F(FFmpegCommonTest, AVStreamToDecoderConfig) {
  // Open a file to get a real AVStreams from FFmpeg.
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(GetTestDataFilePath("bear-320x240.webm")));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());
  AVFormatContext* format_context = glue.format_context();

  // Find the audio and video streams and test valid and invalid combinations
  // for extradata and extradata_size.
  bool found_audio = false;
  bool found_video = false;
  for (size_t i = 0;
       i < format_context->nb_streams && (!found_audio || !found_video);
       ++i) {
    AVStream* stream = format_context->streams[i];
    AVCodecParameters* codec_parameters = stream->codecpar;
    AVMediaType codec_type = codec_parameters->codec_type;

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (found_audio)
        continue;
      found_audio = true;
      AudioDecoderConfig audio_config;
      TestConfigConvertExtraData(
          stream, &audio_config,
          base::BindRepeating(&AVStreamToAudioDecoderConfig));
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if (found_video)
        continue;
      found_video = true;
      VideoDecoderConfig video_config;
      TestConfigConvertExtraData(
          stream, &video_config,
          base::BindRepeating(&AVStreamToVideoDecoderConfig));
    } else {
      // Only process audio/video.
      continue;
    }
  }

  ASSERT_TRUE(found_audio);
  ASSERT_TRUE(found_video);
}

TEST_F(FFmpegCommonTest, AVStreamToAudioDecoderConfig_OpusAmbisonics_4ch) {
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(
      GetTestDataFilePath("bear-opus-end-trimming-4ch-channelmapping2.webm")));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  AVFormatContext* format_context = glue.format_context();
  EXPECT_EQ(static_cast<unsigned int>(1), format_context->nb_streams);
  AVStream* stream = format_context->streams[0];

  AVCodecParameters* codec_parameters = stream->codecpar;
  EXPECT_EQ(AVMEDIA_TYPE_AUDIO, codec_parameters->codec_type);

  AudioDecoderConfig audio_config;
  ASSERT_TRUE(AVStreamToAudioDecoderConfig(stream, &audio_config));

  EXPECT_EQ(AudioCodec::kOpus, audio_config.codec());
  EXPECT_EQ(CHANNEL_LAYOUT_QUAD, audio_config.channel_layout());
  EXPECT_EQ(4, audio_config.channels());
}

TEST_F(FFmpegCommonTest, AVStreamToAudioDecoderConfig_OpusAmbisonics_11ch) {
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(
      GetTestDataFilePath("bear-opus-end-trimming-11ch-channelmapping2.webm")));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  AVFormatContext* format_context = glue.format_context();
  EXPECT_EQ(static_cast<unsigned int>(1), format_context->nb_streams);
  AVStream* stream = format_context->streams[0];

  AVCodecParameters* codec_parameters = stream->codecpar;
  EXPECT_EQ(AVMEDIA_TYPE_AUDIO, codec_parameters->codec_type);

  AudioDecoderConfig audio_config;
  ASSERT_TRUE(AVStreamToAudioDecoderConfig(stream, &audio_config));

  EXPECT_EQ(AudioCodec::kOpus, audio_config.codec());
  EXPECT_EQ(CHANNEL_LAYOUT_DISCRETE, audio_config.channel_layout());
  EXPECT_EQ(11, audio_config.channels());
}

TEST_F(FFmpegCommonTest, AVStreamToAudioDecoderConfig_9ch_wav) {
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(GetTestDataFilePath("9ch.wav")));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  AVFormatContext* format_context = glue.format_context();
  EXPECT_EQ(static_cast<unsigned int>(1), format_context->nb_streams);
  AVStream* stream = format_context->streams[0];

  AVCodecParameters* codec_parameters = stream->codecpar;
  EXPECT_EQ(AVMEDIA_TYPE_AUDIO, codec_parameters->codec_type);

  AudioDecoderConfig audio_config;
  ASSERT_TRUE(AVStreamToAudioDecoderConfig(stream, &audio_config));

  EXPECT_EQ(AudioCodec::kPCM, audio_config.codec());
  EXPECT_EQ(CHANNEL_LAYOUT_DISCRETE, audio_config.channel_layout());
  EXPECT_EQ(9, audio_config.channels());
}

TEST_F(FFmpegCommonTest, TimeBaseConversions) {
  const int64_t test_data[][5] = {
      {1, 2, 1, 500000, 1}, {1, 3, 1, 333333, 1}, {1, 3, 2, 666667, 2},
  };

  for (size_t i = 0; i < std::size(test_data); ++i) {
    SCOPED_TRACE(i);

    AVRational time_base;
    time_base.num = static_cast<int>(test_data[i][0]);
    time_base.den = static_cast<int>(test_data[i][1]);

    base::TimeDelta time_delta =
        ConvertFromTimeBase(time_base, test_data[i][2]);

    EXPECT_EQ(time_delta.InMicroseconds(), test_data[i][3]);
    EXPECT_EQ(ConvertToTimeBase(time_base, time_delta), test_data[i][4]);
  }
}

TEST_F(FFmpegCommonTest, VerifyFormatSizes) {
  for (AVSampleFormat format = AV_SAMPLE_FMT_NONE;
       format < AV_SAMPLE_FMT_NB;
       format = static_cast<AVSampleFormat>(format + 1)) {
    std::vector<AVCodecID> codec_ids(1, AV_CODEC_ID_NONE);
    if (format == AV_SAMPLE_FMT_S32)
      codec_ids.push_back(AV_CODEC_ID_PCM_S24LE);
    for (const auto& codec_id : codec_ids) {
      SampleFormat sample_format =
          AVSampleFormatToSampleFormat(format, codec_id);
      if (sample_format == kUnknownSampleFormat) {
        // This format not supported, so skip it.
        continue;
      }

      // Have FFMpeg compute the size of a buffer of 1 channel / 1 frame
      // with 1 byte alignment to make sure the sizes match.
      int single_buffer_size =
          av_samples_get_buffer_size(NULL, 1, 1, format, 1);
      int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
      EXPECT_EQ(bytes_per_channel, single_buffer_size);
    }
  }
}

// Verifies there are no collisions of the codec name hashes used for UMA.  Also
// includes code for updating the histograms XML.
TEST_F(FFmpegCommonTest, VerifyUmaCodecHashes) {
  const AVCodecDescriptor* desc = avcodec_descriptor_next(nullptr);

  std::map<int32_t, const char*> sorted_hashes;
  while (desc) {
    const int32_t hash = HashCodecName(desc->name);
    // Ensure there are no collisions.
    ASSERT_TRUE(sorted_hashes.find(hash) == sorted_hashes.end());
    sorted_hashes[hash] = desc->name;

    desc = avcodec_descriptor_next(desc);
  }

  // Add a none entry for when no codec is detected.
  static const char kUnknownCodec[] = "none";
  const int32_t hash = HashCodecName(kUnknownCodec);
  ASSERT_TRUE(sorted_hashes.find(hash) == sorted_hashes.end());
  sorted_hashes[hash] = kUnknownCodec;

  // Uncomment the following lines to generate the "FFmpegCodecHashes" enum for
  // usage in the histogram metrics file.  While it regenerates *ALL* values, it
  // should only be used to *ADD* values to histograms file.  Never delete any
  // values; diff should verify.
#if 0
  static const std::vector<std::pair<std::string, int32_t>> kDeprecatedHashes =
      {
          {"brender_pix_deprecated", -1866047250},
          {"adpcm_vima_deprecated", -1782518388},
          {"pcm_s32le_planar_deprecated", -1328796639},
          {"webp_deprecated", -993429906},
          {"paf_video_deprecated", -881893142},
          {"vima_deprecated", -816209197},
          {"iff_byterun1", -777478450},
          {"paf_audio_deprecated", -630356729},

          {"exr_deprecated", -418117523},
          {"hevc_deprecated", -414733739},
          {"vp7_deprecated", -197551526},
          {"escape130_deprecated", 73149662},
          {"tak_deprecated", 1041617024},
          {"opus_deprecated", 1165132763},
          {"g2m_deprecated", 1194572884},

          {"pcm_s24le_planar_deprecated", 1535518292},
          {"sanm_deprecated", 2047102762},

          {"mpegvideo_xvmc_deprecated", 1550758811},
          {"voxware_deprecated", 1656834662}
      };

  for (auto& kv : kDeprecatedHashes)
    sorted_hashes[kv.second] = kv.first.c_str();
  printf("<enum name=\"FFmpegCodecHashes\">\n");
  for (const auto& kv : sorted_hashes)
    printf("  <int value=\"%d\" label=\"%s\"/>\n", kv.first, kv.second);
  printf("</enum>\n");
#endif
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(FFmpegCommonTest, VerifyH264Profile) {
  VerifyProfileTest("bear-1280x720.mp4", H264PROFILE_HIGH);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
TEST_F(FFmpegCommonTest, VerifyH265MainProfile) {
  VerifyProfileTest("bear-1280x720-hevc.mp4", HEVCPROFILE_MAIN);
}

TEST_F(FFmpegCommonTest, VerifyH265Main10Profile) {
  VerifyProfileTest("bear-1280x720-hevc-10bit.mp4", HEVCPROFILE_MAIN10);
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// Verifies that the HDR Metadata and VideoColorSpace are correctly parsed.
TEST_F(FFmpegCommonTest, VerifyHDRMetadataAndColorSpaceInfo) {
  // Open a file to get a real AVStreams from FFmpeg.
  base::MemoryMappedFile file;
  ASSERT_TRUE(file.Initialize(GetTestDataFilePath("colour.webm")));
  InMemoryUrlProtocol protocol(file.data(), file.length(), false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());
  AVFormatContext* format_context = glue.format_context();
  ASSERT_EQ(format_context->nb_streams, 1u);

  AVStream* stream = format_context->streams[0];
  AVCodecParameters* codec_parameters = stream->codecpar;
  AVMediaType codec_type = codec_parameters->codec_type;
  ASSERT_EQ(codec_type, AVMEDIA_TYPE_VIDEO);

  VideoDecoderConfig video_config;
  EXPECT_TRUE(AVStreamToVideoDecoderConfig(stream, &video_config));
  ASSERT_TRUE(video_config.hdr_metadata().has_value());
  const auto& smpte_st_2086 =
      video_config.hdr_metadata()->smpte_st_2086.value();
  EXPECT_EQ(30.0, smpte_st_2086.luminance_min);
  EXPECT_EQ(40.0, smpte_st_2086.luminance_max);
  EXPECT_EQ(0.1f, smpte_st_2086.primaries.fRX);
  EXPECT_EQ(0.2f, smpte_st_2086.primaries.fRY);
  EXPECT_EQ(0.1f, smpte_st_2086.primaries.fGX);
  EXPECT_EQ(0.2f, smpte_st_2086.primaries.fGY);
  EXPECT_EQ(0.1f, smpte_st_2086.primaries.fBX);
  EXPECT_EQ(0.2f, smpte_st_2086.primaries.fBY);
  EXPECT_EQ(0.1f, smpte_st_2086.primaries.fWX);
  EXPECT_EQ(0.2f, smpte_st_2086.primaries.fWY);
  const auto& cta_861_3 = video_config.hdr_metadata()->cta_861_3.value();
  EXPECT_EQ(11.0f, cta_861_3.max_content_light_level);
  EXPECT_EQ(12.0f, cta_861_3.max_frame_average_light_level);
  EXPECT_EQ(VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST428_1,
                            VideoColorSpace::TransferID::LOG,
                            VideoColorSpace::MatrixID::RGB,
                            gfx::ColorSpace::RangeID::FULL),
            video_config.color_space_info());
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST_F(FFmpegCommonTest, VerifyAv1Profiles) {
  VerifyProfileTest("blackwhite_yuv444p_av1.mp4", AV1PROFILE_PROFILE_HIGH);
  VerifyProfileTest("blackwhite_yuv444p_av1.webm", AV1PROFILE_PROFILE_HIGH);
  VerifyProfileTest("bear-av1.mp4", AV1PROFILE_PROFILE_MAIN);
}
#endif

}  // namespace media
