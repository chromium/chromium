// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/mime_util.h"

#include <stddef.h>

#include <array>

#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/mime_util_internal.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace media::internal {

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// TODO(crbug.com/40145071): Remove conditioning of kUsePropCodecs when
// testing *parsing* functions.
const bool kUsePropCodecs = true;
#else
const bool kUsePropCodecs = false;
#endif  //  BUILDFLAG(USE_PROPRIETARY_CODECS)

// MIME type for use with IsCodecSupportedOnAndroid() test; type is ignored in
// all cases except for when paired with the Opus codec.
const char kTestMimeType[] = "foo/foo";

// Helper method for creating a multi-value vector of |kTestStates| if
// |test_all_values| is true or if false, a single value vector containing
// |single_value|.
static std::vector<bool> CreateTestVector(bool test_all_values,
                                          bool single_value) {
  const auto kTestStates = std::to_array<bool>({true, false});
  if (test_all_values) {
    return std::vector<bool>(kTestStates.data(),
                             base::span<const bool>(kTestStates)
                                 .subspan(std::size(kTestStates))
                                 .data());
  }
  return std::vector<bool>(1, single_value);
}

// Helper method for running IsCodecSupportedOnAndroid() tests that will
// iterate over all possible field values for a MimeUtil::PlatformInfo struct.
//
// To request a field be varied, set its value to true in the |states_to_vary|
// struct.  If false, the only value tested will be the field value from
// |test_states|.
//
// |test_func| should have the signature <void(const MimeUtil::PlatformInfo&,
// MimeUtil::Codec)>.
template <typename TestCallback>
static void RunCodecSupportTest(const MimeUtil::PlatformInfo& states_to_vary,
                                const MimeUtil::PlatformInfo& test_states,
                                TestCallback test_func) {
#define MAKE_TEST_VECTOR(name)      \
  std::vector<bool> name##_states = \
      CreateTestVector(states_to_vary.name, test_states.name)

  // Stuff states to test into vectors for easy for_each() iteration.
  MAKE_TEST_VECTOR(has_platform_vp8_decoder);
  ;
#undef MAKE_TEST_VECTOR

  MimeUtil::PlatformInfo info;

#define RUN_TEST_VECTOR_BEGIN(name)                                  \
  for (size_t name##_index = 0; name##_index < name##_states.size(); \
       ++name##_index) {                                             \
    info.name = name##_states[name##_index];
#define RUN_TEST_VECTOR_END() }

  RUN_TEST_VECTOR_BEGIN(has_platform_vp8_decoder)
  for (int codec = MimeUtil::INVALID_CODEC; codec <= MimeUtil::LAST_CODEC;
       ++codec) {
    SCOPED_TRACE(base::StringPrintf("has_platform_vp8_decoder=%d, codec=%d",
                                    info.has_platform_vp8_decoder, codec));
    test_func(info, static_cast<MimeUtil::Codec>(codec));
  }
  RUN_TEST_VECTOR_END()

#undef RUN_TEST_VECTOR_BEGIN
#undef RUN_TEST_VECTOR_END
}

// Helper method for generating the |states_to_vary| value used by
// RunPlatformCodecTest(). Marks all fields to be varied.
static MimeUtil::PlatformInfo VaryAllFields() {
  MimeUtil::PlatformInfo states_to_vary;
  states_to_vary.has_platform_vp8_decoder = true;
  return states_to_vary;
}

// This is to validate MimeUtil::IsCodecSupportedOnPlatform(), which is used
// only on Android platform.
static bool HasDolbyVisionSupport() {
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  return true;
#else
  return false;
#endif
}

static bool HasEac3Support() {
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  return true;
#else
  return false;
#endif
}

static bool HasAc4Support() {
  return false;
}

static bool HasIamfSupport() {
  // TODO (crbug.com/1517114): Enable once IAMF is supported on Android.
  return false;
}

TEST(MimeUtilTest, CommonMediaMimeType) {
  const bool kHlsSupported =
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
      base::FeatureList::IsEnabled(kBuiltInHlsPlayer);
#else
      false;
#endif

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/webm"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/webm"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/wav"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-wav"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/flac"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/ogg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/ogg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/ogg"));

  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("Application/X-MPEGURL"));
  EXPECT_EQ(kHlsSupported,
            IsSupportedMediaMimeType("application/vnd.apple.mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("audio/mpegurl"));
  EXPECT_EQ(kHlsSupported, IsSupportedMediaMimeType("audio/x-mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp4"));

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/x-m4v"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/aac"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/3gpp"));

  // Always an unsupported mime type, even when the parsers are compiled in
  // MediaSource handles reporting support separately in order to not taint
  // the src= support response.
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp2t"));

#else
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/x-m4v"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/aac"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/3gpp"));
#endif  // USE_PROPRIETARY_CODECS
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp3"));

  EXPECT_FALSE(IsSupportedMediaMimeType("video/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("unknown/unknown"));
}

// Note: codecs should only be a list of 2 or fewer; hence the restriction of
// results' length to 2.
TEST(MimeUtilTest, SplitAndStripCodecs) {
  const struct {
    const char* const original;
    size_t expected_size;
    const std::array<const char*, 2> split_results;
    const std::array<const char*, 2> strip_results;
  } tests[] = {
      {"\"bogus\"", 1, {"bogus"}, {"bogus"}},
      {"0", 1, {"0"}, {"0"}},
      {"avc1.42E01E, mp4a.40.2",
       2,
       {"avc1.42E01E", "mp4a.40.2"},
       {"avc1", "mp4a"}},
      {"\"mp4v.20.240, mp4a.40.2\"",
       2,
       {"mp4v.20.240", "mp4a.40.2"},
       {"mp4v", "mp4a"}},
      {"mp4v.20.8, samr", 2, {"mp4v.20.8", "samr"}, {"mp4v", "samr"}},
      {"\"theora, vorbis\"", 2, {"theora", "vorbis"}, {"theora", "vorbis"}},
      {"", 0, {}, {}},
      {"\"\"", 0, {}, {}},
      {"\"   \"", 0, {}, {}},
      {",", 2, {"", ""}, {"", ""}},
  };

  for (const auto& test : tests) {
    std::vector<std::string> codecs_out;

    SplitCodecs(test.original, &codecs_out);
    ASSERT_EQ(test.expected_size, codecs_out.size());
    for (size_t j = 0; j < test.expected_size; ++j) {
      EXPECT_EQ(test.split_results[j], codecs_out[j]);
    }

    StripCodecs(&codecs_out);
    ASSERT_EQ(test.expected_size, codecs_out.size());
    for (size_t j = 0; j < test.expected_size; ++j) {
      EXPECT_EQ(test.strip_results[j], codecs_out[j]);
    }
  }
}

// Basic smoke test for API. More exhaustive codec string testing found in
// media_canplaytype_browsertest.cc.
TEST(MimeUtilTest, ParseVideoCodecString) {
  // Valid AVC string whenever proprietary codecs are supported.
  if (auto result = ParseVideoCodecString("video/mp4", "avc3.42E01E")) {
    EXPECT_TRUE(kUsePropCodecs);
    EXPECT_EQ(VideoCodec::kH264, result->codec);
    EXPECT_EQ(H264PROFILE_BASELINE, result->profile);
    EXPECT_EQ(30u, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Valid VP9 string.
  {
    auto result = ParseVideoCodecString("video/webm", "vp09.00.10.08");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kVP9, result->codec);
    EXPECT_EQ(VP9PROFILE_PROFILE0, result->profile);
    EXPECT_EQ(10u, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Valid VP9 string with REC601 color space.
  {
    auto result =
        ParseVideoCodecString("video/webm", "vp09.02.10.10.01.06.06.06");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kVP9, result->codec);
    EXPECT_EQ(VP9PROFILE_PROFILE2, result->profile);
    EXPECT_EQ(10u, result->level);
    EXPECT_EQ(VideoColorSpace::REC601(), result->color_space);
  }

  // Ambiguous AVC string (when proprietary codecs are supported).
  if (auto result = ParseVideoCodecString("video/mp4", "avc3",
                                          /*allow_ambiguous_matches=*/true)) {
    EXPECT_TRUE(kUsePropCodecs);
    EXPECT_EQ(VideoCodec::kH264, result->codec);
    EXPECT_EQ(VIDEO_CODEC_PROFILE_UNKNOWN, result->profile);
    EXPECT_EQ(kNoVideoCodecLevel, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Audio codecs codec is not valid for video API.
  EXPECT_FALSE(ParseVideoCodecString("video/webm", "opus"));

  // Made up codec is invalid.
  EXPECT_FALSE(ParseVideoCodecString("video/webm", "bogus"));
}

// Basic smoke test for API. More exhaustive codec string testing found in
// media_canplaytype_browsertest.cc.
TEST(MimeUtilTest, ParseVideoCodecString_NoMimeType) {
  // Invalid to give empty codec without a mime type.
  EXPECT_FALSE(ParseVideoCodecString("", ""));

  // Valid AVC string whenever proprietary codecs are supported.
  if (auto result = ParseVideoCodecString("", "avc3.42E01E")) {
    EXPECT_EQ(VideoCodec::kH264, result->codec);
    EXPECT_EQ(H264PROFILE_BASELINE, result->profile);
    EXPECT_EQ(30u, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Valid VP9 string.
  {
    auto result = ParseVideoCodecString("", "vp09.00.10.08");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kVP9, result->codec);
    EXPECT_EQ(VP9PROFILE_PROFILE0, result->profile);
    EXPECT_EQ(10u, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Valid VP9 string with REC601 color space.
  {
    auto result = ParseVideoCodecString("", "vp09.02.10.10.01.06.06.06");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kVP9, result->codec);
    EXPECT_EQ(VP9PROFILE_PROFILE2, result->profile);
    EXPECT_EQ(10u, result->level);
    EXPECT_EQ(VideoColorSpace::REC601(), result->color_space);
  }

  // Ambiguous AVC string (when proprietary codecs are supported).
  if (auto result = ParseVideoCodecString("", "avc3",
                                          /*allow_ambiguous_matches=*/true)) {
    EXPECT_EQ(VideoCodec::kH264, result->codec);
    EXPECT_EQ(VIDEO_CODEC_PROFILE_UNKNOWN, result->profile);
    EXPECT_EQ(kNoVideoCodecLevel, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Audio codecs codec is not valid for video API.
  EXPECT_FALSE(ParseVideoCodecString("", "opus"));

  // Made up codec is invalid.
  EXPECT_FALSE(ParseVideoCodecString("", "bogus"));
}

TEST(MimeUtilTest, ParseAudioCodecString) {
  // Valid Opus string.
  auto result = ParseAudioCodecString("audio/webm", "opus");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kOpus, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  // Valid AAC string when proprietary codecs are supported.
  result = ParseAudioCodecString("audio/mp4", "mp4a.40.2");
  EXPECT_EQ(kUsePropCodecs, result.has_value());
  if (kUsePropCodecs) {
    ASSERT_TRUE(result);
    EXPECT_EQ(AudioCodec::kAAC, result->codec);
  }

  // Valid FLAC string with MP4. Neither decoding nor demuxing is proprietary.
  result = ParseAudioCodecString("audio/mp4", "flac");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kFLAC, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  // Ambiguous AAC string "mp4a.40" should fail without allow_ambiguous_matches.
  EXPECT_FALSE(ParseAudioCodecString("audio/mp4", "mp4a.40"));

  // But it should succeed with allow_ambiguous_matches=true.
  result = ParseAudioCodecString("audio/mp4", "mp4a.40", true);
  EXPECT_EQ(kUsePropCodecs, result.has_value());
  if (kUsePropCodecs) {
    ASSERT_TRUE(result);
    EXPECT_EQ(AudioCodec::kAAC, result->codec);
    EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);
  }

  result = ParseAudioCodecString("audio/mp4", "mp4a.40.42");
  EXPECT_EQ(kUsePropCodecs, result.has_value());
  if (kUsePropCodecs) {
    ASSERT_TRUE(result);
    EXPECT_EQ(AudioCodec::kAAC, result->codec);
    EXPECT_EQ(AudioCodecProfile::kXHE_AAC, result->profile);
  }

  // Valid empty codec string. Codec unambiguously implied by mime type.
  result = ParseAudioCodecString("audio/flac", "");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kFLAC, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  // Valid audio codec should still be allowed with video mime type.
  result = ParseAudioCodecString("video/webm", "opus");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kOpus, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  // Video codec is not valid for audio API.
  EXPECT_FALSE(ParseAudioCodecString("audio/webm", "vp09.00.10.08"));

  // Made up codec is also not valid.
  EXPECT_FALSE(ParseAudioCodecString("audio/webm", "bogus"));
}

TEST(MimeUtilTest, ParseAudioCodecString_NoMimeType) {
  // Invalid to give empty codec without a mime type.
  EXPECT_FALSE(ParseAudioCodecString("", ""));

  // Valid Opus string.
  auto result = ParseAudioCodecString("", "opus");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kOpus, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  // Valid AAC string when proprietary codecs are supported.
  result = ParseAudioCodecString("", "mp4a.40.2");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kAAC, result->codec);

  // Valid FLAC string. Neither decoding nor demuxing is proprietary.
  result = ParseAudioCodecString("", "flac");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kFLAC, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  // Ambiguous AAC string "mp4a.40" should fail without allow_ambiguous_matches.
  EXPECT_FALSE(ParseAudioCodecString("", "mp4a.40"));

  // But it should succeed with allow_ambiguous_matches=true.
  result = ParseAudioCodecString("", "mp4a.40", true);
  ASSERT_TRUE(result);
  if (kUsePropCodecs) {
    EXPECT_EQ(AudioCodec::kAAC, result->codec);
    EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);
  }

  // Video codec is not valid for audio API.
  EXPECT_FALSE(ParseAudioCodecString("", "vp09.00.10.08"));

  // Made up codec is also not valid.
  EXPECT_FALSE(ParseAudioCodecString("", "bogus"));
}

// MP3 is a weird case where we allow either the mime type, codec string, or
// both, and there are several valid codec strings.
TEST(MimeUtilTest, ParseAudioCodecString_Mp3) {
  auto result = ParseAudioCodecString("audio/mpeg", "mp3");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kMP3, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  result = ParseAudioCodecString("audio/mpeg", "");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kMP3, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  result = ParseAudioCodecString("", "mp3");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kMP3, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  result = ParseAudioCodecString("", "mp4a.69");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kMP3, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);

  result = ParseAudioCodecString("", "mp4a.6B");
  ASSERT_TRUE(result);
  EXPECT_EQ(AudioCodec::kMP3, result->codec);
  EXPECT_EQ(AudioCodecProfile::kUnknown, result->profile);
}

// These codecs really only have one profile. Ensure that |out_profile| is
// correctly mapped.
TEST(MimeUtilTest, ParseVideoCodecString_SimpleCodecsHaveProfiles) {
  // Valid VP8 string.
  {
    auto result = ParseVideoCodecString("video/webm", "vp8");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kVP8, result->codec);
    EXPECT_EQ(VP8PROFILE_ANY, result->profile);
    EXPECT_EQ(kNoVideoCodecLevel, result->level);
    EXPECT_EQ(VideoColorSpace::REC709(), result->color_space);
  }

  // Valid Theora string.
  EXPECT_FALSE(ParseVideoCodecString("video/ogg", "theora"));
}

TEST(IsCodecSupportedOnAndroidTest, EncryptedCodecBehavior) {
  // Vary all parameters.
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();

  MimeUtil::PlatformInfo test_states;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        const bool result = MimeUtil::IsCodecSupportedOnAndroid(
            codec, kTestMimeType, true, VIDEO_CODEC_PROFILE_UNKNOWN, info);
        switch (codec) {
          // These codecs are never supported by the Android platform.
          case MimeUtil::INVALID_CODEC:
          case MimeUtil::MPEG_H_AUDIO:
          case MimeUtil::THEORA:
            EXPECT_FALSE(result);
            break;

          // These codecs are always available with platform decoder support.
          case MimeUtil::PCM:
          case MimeUtil::MP3:
          case MimeUtil::MPEG2_AAC:
          case MimeUtil::MPEG4_AAC:
          case MimeUtil::MPEG4_XHE_AAC:
          case MimeUtil::VORBIS:
          case MimeUtil::FLAC:
          case MimeUtil::H264:
          case MimeUtil::VP9:
          case MimeUtil::OPUS:
            EXPECT_TRUE(result);
            break;

          // The remaining codecs are not available on all platforms even when
          // a platform decoder is available.

          case MimeUtil::VP8:
            EXPECT_EQ(info.has_platform_vp8_decoder, result);
            break;

          case MimeUtil::HEVC:
            EXPECT_EQ(BUILDFLAG(ENABLE_PLATFORM_HEVC), result);
            break;

          case MimeUtil::DOLBY_VISION:
            EXPECT_EQ(HasDolbyVisionSupport(), result);
            break;

          case MimeUtil::AC3:
          case MimeUtil::EAC3:
            EXPECT_EQ(HasEac3Support(), result);
            break;

          case MimeUtil::AV1:
            EXPECT_EQ(BUILDFLAG(ENABLE_AV1_DECODER), result);
            break;

          case MimeUtil::DTS:
          case MimeUtil::DTSXP2:
          case MimeUtil::DTSE:
            EXPECT_EQ(BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO), result);
            break;

          case MimeUtil::AC4:
            EXPECT_EQ(HasAc4Support(), result);
            break;

          case MimeUtil::IAMF:
            EXPECT_EQ(HasIamfSupport(), result);
            break;
        }
      });
}

TEST(IsCodecSupportedOnAndroidTest, ClearCodecBehavior) {
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();

  MimeUtil::PlatformInfo test_states;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        const bool result = MimeUtil::IsCodecSupportedOnAndroid(
            codec, kTestMimeType, false, VIDEO_CODEC_PROFILE_UNKNOWN, info);
        switch (codec) {
          // These codecs are never supported by the Android platform.
          case MimeUtil::INVALID_CODEC:
          case MimeUtil::MPEG_H_AUDIO:
          case MimeUtil::THEORA:
            EXPECT_FALSE(result);
            break;

          // These codecs are always supported with the unified pipeline.
          case MimeUtil::FLAC:
          case MimeUtil::H264:
          case MimeUtil::PCM:
          case MimeUtil::MP3:
          case MimeUtil::MPEG2_AAC:
          case MimeUtil::MPEG4_AAC:
          case MimeUtil::OPUS:
          case MimeUtil::VORBIS:
          case MimeUtil::VP8:
          case MimeUtil::VP9:
            EXPECT_TRUE(result);
            break;

          // These codecs are only supported if platform decoders are supported.
          case MimeUtil::MPEG4_XHE_AAC:
            EXPECT_TRUE(result);
            break;

          case MimeUtil::HEVC:
            EXPECT_EQ(BUILDFLAG(ENABLE_PLATFORM_HEVC), result);
            break;

          case MimeUtil::DOLBY_VISION:
            EXPECT_EQ(HasDolbyVisionSupport(), result);
            break;

          case MimeUtil::AC3:
          case MimeUtil::EAC3:
            EXPECT_EQ(HasEac3Support(), result);
            break;

          case MimeUtil::AV1:
            EXPECT_EQ(BUILDFLAG(ENABLE_AV1_DECODER), result);
            break;

          case MimeUtil::DTS:
          case MimeUtil::DTSXP2:
          case MimeUtil::DTSE:
            EXPECT_EQ(BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO), result);
            break;

          case MimeUtil::AC4:
            EXPECT_EQ(HasAc4Support(), result);
            break;

          case MimeUtil::IAMF:
            EXPECT_EQ(HasIamfSupport(), result);
            break;
        }
      });
}

TEST(IsCodecSupportedOnAndroidTest, OpusOggSupport) {
  // Vary all parameters; thus use default initial state.
  MimeUtil::PlatformInfo states_to_vary = VaryAllFields();
  MimeUtil::PlatformInfo test_states;

  RunCodecSupportTest(
      states_to_vary, test_states,
      [](const MimeUtil::PlatformInfo& info, MimeUtil::Codec codec) {
        EXPECT_TRUE(MimeUtil::IsCodecSupportedOnAndroid(
            MimeUtil::OPUS, "audio/ogg", false, VIDEO_CODEC_PROFILE_UNKNOWN,
            info));
      });
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
TEST(IsCodecSupportedOnAndroidTest, HEVCSupport) {
  MimeUtil::PlatformInfo info;
  EXPECT_TRUE(MimeUtil::IsCodecSupportedOnAndroid(
      MimeUtil::HEVC, kTestMimeType, false, VIDEO_CODEC_PROFILE_UNKNOWN, info));
}
#endif

TEST(IsCodecSupportedOnAndroidTest, AndroidHLSAAC) {
  const bool kHlsSupported =
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
      base::FeatureList::IsEnabled(kBuiltInHlsPlayer);
#else
      false;
#endif

  const std::string hls_mime_types[] = {"application/x-mpegurl",
                                        "application/vnd.apple.mpegurl",
                                        "audio/mpegurl", "audio/x-mpegurl"};

  const std::string mpeg2_aac_codec_strings[] = {"mp4a.66", "mp4a.67",
                                                 "mp4a.68"};

  const std::string mpeg4_aac_codec_strings[] = {
      "mp4a.40.2", "mp4a.40.02", "mp4a.40.5", "mp4a.40.05", "mp4a.40.29"};

  for (const auto& hls_mime_type : hls_mime_types) {
    // MPEG2_AAC is never supported with HLS. Even when HLS on android is
    // supported, MediaPlayer lacks the needed MPEG2_AAC demuxers.
    // See https://crbug.com/544268.
    for (const auto& mpeg2_aac_string : mpeg2_aac_codec_strings) {
      EXPECT_FALSE(ParseAudioCodecString(hls_mime_type, mpeg2_aac_string));
    }

    // MPEG4_AAC is supported with HLS whenever HLS is supported.
    for (const auto& mpeg4_aac_string : mpeg4_aac_codec_strings) {
      EXPECT_EQ(
          kHlsSupported,
          ParseAudioCodecString(hls_mime_type, mpeg4_aac_string).has_value());
    }
  }

  // NOTE
  // We do not call IsCodecSupportedOnAndroid because the following checks
  // are made at a higher level in mime code (parsing rather than checks for
  // platform support).
}

}  // namespace media::internal
