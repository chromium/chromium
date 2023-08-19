// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mime_util.h"

#include <stddef.h>

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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace media::internal {

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// TODO(https://crbug.com/1117275): Remove conditioning of kUsePropCodecs when
// testing *parsing* functions.
const bool kUsePropCodecs = true;
#else
const bool kUsePropCodecs = false;
#endif  //  BUILDFLAG(USE_PROPRIETARY_CODECS)

// MIME type for use with IsCodecSupportedOnAndroid() test; type is ignored in
// all cases except for when paired with the Opus codec.
const char kTestMimeType[] = "foo/foo";

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
// HLS is supported on Android API level 14 and higher and Chrome supports
// API levels 15 and higher, so HLS is always supported on Android.
const bool kHlsSupported = true;
#else
const bool kHlsSupported = false;
#endif

// Helper method for creating a multi-value vector of |kTestStates| if
// |test_all_values| is true or if false, a single value vector containing
// |single_value|.
static std::vector<bool> CreateTestVector(bool test_all_values,
                                          bool single_value) {
  const bool kTestStates[] = {true, false};
  if (test_all_values)
    return std::vector<bool>(kTestStates, kTestStates + std::size(kTestStates));
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
  MAKE_TEST_VECTOR(has_platform_vp9_decoder);
  MAKE_TEST_VECTOR(has_platform_opus_decoder);
#undef MAKE_TEST_VECTOR

  MimeUtil::PlatformInfo info;

#define RUN_TEST_VECTOR_BEGIN(name)                                  \
  for (size_t name##_index = 0; name##_index < name##_states.size(); \
       ++name##_index) {                                             \
    info.name = name##_states[name##_index];
#define RUN_TEST_VECTOR_END() }

  RUN_TEST_VECTOR_BEGIN(has_platform_vp8_decoder)
  RUN_TEST_VECTOR_BEGIN(has_platform_vp9_decoder)
  RUN_TEST_VECTOR_BEGIN(has_platform_opus_decoder)
  for (int codec = MimeUtil::INVALID_CODEC; codec <= MimeUtil::LAST_CODEC;
       ++codec) {
    SCOPED_TRACE(base::StringPrintf(
        "has_platform_vp8_decoder=%d, "
        "has_platform_opus_decoder=%d, "
        "has_platform_vp9_decoder=%d, "
        "codec=%d",
        info.has_platform_vp8_decoder, info.has_platform_opus_decoder,
        info.has_platform_vp9_decoder, codec));
    test_func(info, static_cast<MimeUtil::Codec>(codec));
  }
  RUN_TEST_VECTOR_END()
  RUN_TEST_VECTOR_END()
  RUN_TEST_VECTOR_END()

#undef RUN_TEST_VECTOR_BEGIN
#undef RUN_TEST_VECTOR_END
}

// Helper method for generating the |states_to_vary| value used by
// RunPlatformCodecTest(). Marks all fields to be varied.
static MimeUtil::PlatformInfo VaryAllFields() {
  MimeUtil::PlatformInfo states_to_vary;
  states_to_vary.has_platform_vp8_decoder = true;
  states_to_vary.has_platform_vp9_decoder = true;
  states_to_vary.has_platform_opus_decoder = true;
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

TEST(MimeUtilTest, CommonMediaMimeType) {
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
    const char* const split_results[2];
    const char* const strip_results[2];
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
    for (size_t j = 0; j < test.expected_size; ++j)
      EXPECT_EQ(test.split_results[j], codecs_out[j]);

    StripCodecs(&codecs_out);
    ASSERT_EQ(test.expected_size, codecs_out.size());
    for (size_t j = 0; j < test.expected_size; ++j)
      EXPECT_EQ(test.strip_results[j], codecs_out[j]);
  }
}

// Basic smoke test for API. More exhaustive codec string testing found in
// media_canplaytype_browsertest.cc.
TEST(MimeUtilTest, ParseVideoCodecString) {
  bool out_is_ambiguous;
  VideoCodec out_codec;
  VideoCodecProfile out_profile;
  uint8_t out_level;
  VideoColorSpace out_colorspace;

  // Valid AVC string whenever proprietary codecs are supported.
  EXPECT_EQ(kUsePropCodecs,
            ParseVideoCodecString("video/mp4", "avc3.42E01E", &out_is_ambiguous,
                                  &out_codec, &out_profile, &out_level,
                                  &out_colorspace));
  if (kUsePropCodecs) {
    EXPECT_FALSE(out_is_ambiguous);
    EXPECT_EQ(VideoCodec::kH264, out_codec);
    EXPECT_EQ(H264PROFILE_BASELINE, out_profile);
    EXPECT_EQ(30, out_level);
    EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);
  }

  // Valid VP9 string.
  EXPECT_TRUE(ParseVideoCodecString("video/webm", "vp09.00.10.08",
                                    &out_is_ambiguous, &out_codec, &out_profile,
                                    &out_level, &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kVP9, out_codec);
  EXPECT_EQ(VP9PROFILE_PROFILE0, out_profile);
  EXPECT_EQ(10, out_level);
  EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);

  // Valid VP9 string with REC601 color space.
  EXPECT_TRUE(ParseVideoCodecString("video/webm", "vp09.02.10.10.01.06.06.06",
                                    &out_is_ambiguous, &out_codec, &out_profile,
                                    &out_level, &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kVP9, out_codec);
  EXPECT_EQ(VP9PROFILE_PROFILE2, out_profile);
  EXPECT_EQ(10, out_level);
  EXPECT_EQ(VideoColorSpace::REC601(), out_colorspace);

  // Ambiguous AVC string (when proprietary codecs are supported).
  EXPECT_EQ(
      kUsePropCodecs,
      ParseVideoCodecString("video/mp4", "avc3", &out_is_ambiguous, &out_codec,
                            &out_profile, &out_level, &out_colorspace));
  if (kUsePropCodecs) {
    EXPECT_TRUE(out_is_ambiguous);
    EXPECT_EQ(VideoCodec::kH264, out_codec);
    EXPECT_EQ(VIDEO_CODEC_PROFILE_UNKNOWN, out_profile);
    EXPECT_EQ(0, out_level);
    EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);
  }

  // Audio codecs codec is not valid for video API.
  EXPECT_FALSE(ParseVideoCodecString("video/webm", "opus", &out_is_ambiguous,
                                     &out_codec, &out_profile, &out_level,
                                     &out_colorspace));

  // Made up codec is invalid.
  EXPECT_FALSE(ParseVideoCodecString("video/webm", "bogus", &out_is_ambiguous,
                                     &out_codec, &out_profile, &out_level,
                                     &out_colorspace));
}

// Basic smoke test for API. More exhaustive codec string testing found in
// media_canplaytype_browsertest.cc.
TEST(MimeUtilTest, ParseVideoCodecString_NoMimeType) {
  bool out_is_ambiguous;
  VideoCodec out_codec;
  VideoCodecProfile out_profile;
  uint8_t out_level;
  VideoColorSpace out_colorspace;

  // Invalid to give empty codec without a mime type.
  EXPECT_FALSE(ParseVideoCodecString("", "", &out_is_ambiguous, &out_codec,
                                     &out_profile, &out_level,
                                     &out_colorspace));

  // Valid AVC string whenever proprietary codecs are supported.
  EXPECT_TRUE(ParseVideoCodecString("", "avc3.42E01E", &out_is_ambiguous,
                                    &out_codec, &out_profile, &out_level,
                                    &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kH264, out_codec);
  EXPECT_EQ(H264PROFILE_BASELINE, out_profile);
  EXPECT_EQ(30, out_level);
  EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);

  // Valid VP9 string.
  EXPECT_TRUE(ParseVideoCodecString("", "vp09.00.10.08", &out_is_ambiguous,
                                    &out_codec, &out_profile, &out_level,
                                    &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kVP9, out_codec);
  EXPECT_EQ(VP9PROFILE_PROFILE0, out_profile);
  EXPECT_EQ(10, out_level);
  EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);

  EXPECT_TRUE(ParseVideoCodecString("", "vp09.02.10.10.01.06.06.06",
                                    &out_is_ambiguous, &out_codec, &out_profile,
                                    &out_level, &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kVP9, out_codec);
  EXPECT_EQ(VP9PROFILE_PROFILE2, out_profile);
  EXPECT_EQ(10, out_level);
  EXPECT_EQ(VideoColorSpace::REC601(), out_colorspace);

  // Ambiguous AVC string (when proprietary codecs are supported).
  EXPECT_TRUE(ParseVideoCodecString("", "avc3", &out_is_ambiguous, &out_codec,
                                    &out_profile, &out_level, &out_colorspace));
  EXPECT_TRUE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kH264, out_codec);
  EXPECT_EQ(VIDEO_CODEC_PROFILE_UNKNOWN, out_profile);
  EXPECT_EQ(0, out_level);
  EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);

  // Audio codecs codec is not valid for video API.
  EXPECT_FALSE(ParseVideoCodecString("", "opus", &out_is_ambiguous, &out_codec,
                                     &out_profile, &out_level,
                                     &out_colorspace));

  // Made up codec is invalid.
  EXPECT_FALSE(ParseVideoCodecString("", "bogus", &out_is_ambiguous, &out_codec,
                                     &out_profile, &out_level,
                                     &out_colorspace));
}

TEST(MimeUtilTest, ParseAudioCodecString) {
  bool out_is_ambiguous;
  AudioCodec out_codec;

  // Valid Opus string.
  EXPECT_TRUE(ParseAudioCodecString("audio/webm", "opus", &out_is_ambiguous,
                                    &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kOpus, out_codec);

  // Valid AAC string when proprietary codecs are supported.
  EXPECT_EQ(kUsePropCodecs,
            ParseAudioCodecString("audio/mp4", "mp4a.40.2", &out_is_ambiguous,
                                  &out_codec));
  if (kUsePropCodecs) {
    EXPECT_FALSE(out_is_ambiguous);
    EXPECT_EQ(AudioCodec::kAAC, out_codec);
  }

  // Valid FLAC string with MP4. Neither decoding nor demuxing is proprietary.
  EXPECT_TRUE(ParseAudioCodecString("audio/mp4", "flac", &out_is_ambiguous,
                                    &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kFLAC, out_codec);

  // Ambiguous AAC string.
  // TODO(chcunningha): This can probably be allowed. I think we treat all
  // MPEG4_AAC the same.
  EXPECT_EQ(kUsePropCodecs,
            ParseAudioCodecString("audio/mp4", "mp4a.40", &out_is_ambiguous,
                                  &out_codec));
  if (kUsePropCodecs) {
    EXPECT_TRUE(out_is_ambiguous);
    EXPECT_EQ(AudioCodec::kAAC, out_codec);
  }

  // Valid empty codec string. Codec unambiguously implied by mime type.
  EXPECT_TRUE(
      ParseAudioCodecString("audio/flac", "", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kFLAC, out_codec);

  // Valid audio codec should still be allowed with video mime type.
  EXPECT_TRUE(ParseAudioCodecString("video/webm", "opus", &out_is_ambiguous,
                                    &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kOpus, out_codec);

  // Video codec is not valid for audio API.
  EXPECT_FALSE(ParseAudioCodecString("audio/webm", "vp09.00.10.08",
                                     &out_is_ambiguous, &out_codec));

  // Made up codec is also not valid.
  EXPECT_FALSE(ParseAudioCodecString("audio/webm", "bogus", &out_is_ambiguous,
                                     &out_codec));
}

TEST(MimeUtilTest, ParseAudioCodecString_NoMimeType) {
  bool out_is_ambiguous;
  AudioCodec out_codec;

  // Invalid to give empty codec without a mime type.
  EXPECT_FALSE(ParseAudioCodecString("", "", &out_is_ambiguous, &out_codec));

  // Valid Opus string.
  EXPECT_TRUE(ParseAudioCodecString("", "opus", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kOpus, out_codec);

  // Valid AAC string when proprietary codecs are supported.
  EXPECT_TRUE(
      ParseAudioCodecString("", "mp4a.40.2", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kAAC, out_codec);

  // Valid FLAC string. Neither decoding nor demuxing is proprietary.
  EXPECT_TRUE(ParseAudioCodecString("", "flac", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kFLAC, out_codec);

  // Ambiguous AAC string.
  // TODO(chcunningha): This can probably be allowed. I think we treat all
  // MPEG4_AAC the same.
  EXPECT_TRUE(
      ParseAudioCodecString("", "mp4a.40", &out_is_ambiguous, &out_codec));
  if (kUsePropCodecs) {
    EXPECT_TRUE(out_is_ambiguous);
    EXPECT_EQ(AudioCodec::kAAC, out_codec);
  }

  // Video codec is not valid for audio API.
  EXPECT_FALSE(ParseAudioCodecString("", "vp09.00.10.08", &out_is_ambiguous,
                                     &out_codec));

  // Made up codec is also not valid.
  EXPECT_FALSE(
      ParseAudioCodecString("", "bogus", &out_is_ambiguous, &out_codec));
}

// MP3 is a weird case where we allow either the mime type, codec string, or
// both, and there are several valid codec strings.
TEST(MimeUtilTest, ParseAudioCodecString_Mp3) {
  bool out_is_ambiguous;
  AudioCodec out_codec;

  EXPECT_TRUE(ParseAudioCodecString("audio/mpeg", "mp3", &out_is_ambiguous,
                                    &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kMP3, out_codec);

  EXPECT_TRUE(
      ParseAudioCodecString("audio/mpeg", "", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kMP3, out_codec);

  EXPECT_TRUE(ParseAudioCodecString("", "mp3", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kMP3, out_codec);

  EXPECT_TRUE(
      ParseAudioCodecString("", "mp4a.69", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kMP3, out_codec);

  EXPECT_TRUE(
      ParseAudioCodecString("", "mp4a.6B", &out_is_ambiguous, &out_codec));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(AudioCodec::kMP3, out_codec);
}

// These codecs really only have one profile. Ensure that |out_profile| is
// correctly mapped.
TEST(MimeUtilTest, ParseVideoCodecString_SimpleCodecsHaveProfiles) {
  bool out_is_ambiguous;
  VideoCodec out_codec;
  VideoCodecProfile out_profile;
  uint8_t out_level;
  VideoColorSpace out_colorspace;

  // Valid VP8 string.
  EXPECT_TRUE(ParseVideoCodecString("video/webm", "vp8", &out_is_ambiguous,
                                    &out_codec, &out_profile, &out_level,
                                    &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kVP8, out_codec);
  EXPECT_EQ(VP8PROFILE_ANY, out_profile);
  EXPECT_EQ(0, out_level);
  EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);

// Valid Theora string.
#if BUILDFLAG(IS_ANDROID)
  // Theora not supported on Android.
  EXPECT_FALSE(ParseVideoCodecString("video/ogg", "theora", &out_is_ambiguous,
                                     &out_codec, &out_profile, &out_level,
                                     &out_colorspace));
#else
  EXPECT_TRUE(ParseVideoCodecString("video/ogg", "theora", &out_is_ambiguous,
                                    &out_codec, &out_profile, &out_level,
                                    &out_colorspace));
  EXPECT_FALSE(out_is_ambiguous);
  EXPECT_EQ(VideoCodec::kTheora, out_codec);
  EXPECT_EQ(THEORAPROFILE_ANY, out_profile);
  EXPECT_EQ(0, out_level);
  EXPECT_EQ(VideoColorSpace::REC709(), out_colorspace);
#endif
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
            EXPECT_TRUE(result);
            break;

          // The remaining codecs are not available on all platforms even when
          // a platform decoder is available.
          case MimeUtil::OPUS:
            EXPECT_EQ(info.has_platform_opus_decoder, result);
            break;

          case MimeUtil::VP8:
            EXPECT_EQ(info.has_platform_vp8_decoder, result);
            break;

          case MimeUtil::VP9:
            EXPECT_EQ(info.has_platform_vp9_decoder, result);
            break;

          case MimeUtil::HEVC:
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
            EXPECT_EQ(info.has_platform_hevc_decoder, result);
#else
            EXPECT_FALSE(result);
#endif
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
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
            EXPECT_EQ(info.has_platform_hevc_decoder, result);
#else
            EXPECT_FALSE(result);
#endif
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
  info.has_platform_hevc_decoder = false;

  EXPECT_FALSE(MimeUtil::IsCodecSupportedOnAndroid(
      MimeUtil::HEVC, kTestMimeType, false, VIDEO_CODEC_PROFILE_UNKNOWN, info));

  info.has_platform_hevc_decoder = true;
  EXPECT_TRUE(MimeUtil::IsCodecSupportedOnAndroid(
      MimeUtil::HEVC, kTestMimeType, false, VIDEO_CODEC_PROFILE_UNKNOWN, info));
}
#endif

TEST(IsCodecSupportedOnAndroidTest, AndroidHLSAAC) {
  const std::string hls_mime_types[] = {"application/x-mpegurl",
                                        "application/vnd.apple.mpegurl",
                                        "audio/mpegurl", "audio/x-mpegurl"};

  const std::string mpeg2_aac_codec_strings[] = {"mp4a.66", "mp4a.67",
                                                 "mp4a.68"};

  const std::string mpeg4_aac_codec_strings[] = {
      "mp4a.40.2", "mp4a.40.02", "mp4a.40.5", "mp4a.40.05", "mp4a.40.29"};

  bool out_is_ambiguous;
  AudioCodec out_codec;
  for (const auto& hls_mime_type : hls_mime_types) {
    // MPEG2_AAC is never supported with HLS. Even when HLS on android is
    // supported, MediaPlayer lacks the needed MPEG2_AAC demuxers.
    // See https://crbug.com/544268.
    for (const auto& mpeg2_aac_string : mpeg2_aac_codec_strings) {
      EXPECT_FALSE(ParseAudioCodecString(hls_mime_type, mpeg2_aac_string,
                                         &out_is_ambiguous, &out_codec));
    }

    // MPEG4_AAC is supported with HLS whenever HLS is supported.
    for (const auto& mpeg4_aac_string : mpeg4_aac_codec_strings) {
      EXPECT_EQ(kHlsSupported,
                ParseAudioCodecString(hls_mime_type, mpeg4_aac_string,
                                      &out_is_ambiguous, &out_codec));
    }
  }

  // NOTE
  // We do not call IsCodecSupportedOnAndroid because the following checks
  // are made at a higher level in mime code (parsing rather than checks for
  // platform support).
}

}  // namespace media::internal
