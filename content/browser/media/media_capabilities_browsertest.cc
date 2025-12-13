// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/cdm/clear_key_cdm_common.h"

namespace {

constexpr std::string_view kDecodeTestFile = "decode_capabilities_test.html";
constexpr std::string_view kSupported = "SUPPORTED";
constexpr std::u16string_view kSupported16 = u"SUPPORTED";
constexpr std::string_view kUnsupported = "UNSUPPORTED";
constexpr std::u16string_view kUnsupported16 = u"UNSUPPORTED";
constexpr std::string_view kError = "ERROR";
constexpr std::u16string_view kError16 = u"ERROR";
constexpr std::string_view kFileString = "file";
constexpr std::string_view kMediaSourceString = "media-source";
constexpr std::string_view kWebRtcString = "webrtc";
constexpr std::string_view kInvalid = "INVALID";

constexpr std::string_view kSrgb = "srgb";
constexpr std::string_view kP3 = "p3";
constexpr std::string_view kRec2020 = "rec2020";
constexpr std::string_view kPq = "pq";
constexpr std::string_view kHlg = "hlg";
constexpr std::string_view kSmpteSt2086 = "smpteSt2086";
constexpr std::string_view kSmpteSt2094_10 = "smpteSt2094-10";
constexpr std::string_view kSmpteSt2094_40 = "smpteSt2094-40";

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
constexpr std::string_view kPropSupported = kSupported;
#else
constexpr std::string_view kPropSupported = kUnsupported;
#endif  // USE_PROPRIETARY_CODECS

enum StreamType {
  kAudio,
  kVideo,
  kAudioWithSpatialRendering,
  kVideoWithHdrMetadata,
  kVideoWithoutHdrMetadata,
  kVideoWithHdrMetadataAndKeySystem,
  kVideoWithDolbyVisionAndKeySystem,
  kAudioWithKeySystem,
  kAudioAndVideo,
  kAudioAndVideoWithKeySystem,
};

enum ConfigType { kFile, kMediaSource, kWebRtc };

}  // namespace

namespace content {

class MediaCapabilitiesTest : public ContentBrowserTest {
 public:
  MediaCapabilitiesTest() = default;

  MediaCapabilitiesTest(const MediaCapabilitiesTest&) = delete;
  MediaCapabilitiesTest& operator=(const MediaCapabilitiesTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaCapabilitiesSpatialAudio");
    scoped_feature_list_.InitWithFeatures({media::kExternalClearKeyForTesting},
                                          {});
  }

  std::string CanDecodeAudio(std::string_view config_type,
                             std::string_view content_type) {
    return CanDecode(config_type, content_type, StreamType::kAudio);
  }

  std::string CanDecodeVideo(std::string_view config_type,
                             std::string_view content_type) {
    return CanDecode(config_type, content_type, StreamType::kVideo);
  }

  std::string CanDecodeAudioWithSpatialRendering(std::string_view config_type,
                                                 std::string_view content_type,
                                                 bool spatial_rendering) {
    return CanDecode(config_type, content_type,
                     StreamType::kAudioWithSpatialRendering, spatial_rendering);
  }

  std::string CanDecodeVideoWithHdrMetadata(
      std::string_view config_type,
      std::string_view content_type,
      std::string_view color_gamut,
      std::string_view transfer_function,
      std::string_view hdr_metadata_type = "") {
    StreamType stream_type = StreamType::kVideoWithHdrMetadata;
    if (hdr_metadata_type == "")
      stream_type = StreamType::kVideoWithoutHdrMetadata;

    return CanDecode(config_type, content_type, stream_type,
                     /* spatialRendering */ false, hdr_metadata_type,
                     color_gamut, transfer_function);
  }

  std::string CanDecodeVideoWithHdrMetadataAndKeySystem(
      std::string_view config_type,
      std::string_view content_type,
      std::string_view color_gamut,
      std::string_view transfer_function,
      std::string_view hdr_metadata_type,
      std::string_view key_system) {
    return CanDecode(config_type, content_type,
                     StreamType::kVideoWithHdrMetadataAndKeySystem,
                     /* spatialRendering */ false, hdr_metadata_type,
                     color_gamut, transfer_function, key_system);
  }

  std::string CanDecodeVideoWithKeySystem(std::string_view config_type,
                                          std::string_view content_type,
                                          std::string_view key_system) {
    return CanDecode(config_type, content_type,
                     StreamType::kVideoWithDolbyVisionAndKeySystem,
                     /* spatialRendering */ false, "", "", "", key_system);
  }

  std::string CanDecodeAudioWithKeySystem(std::string_view config_type,
                                          std::string_view content_type,
                                          std::string_view key_system) {
    return CanDecode(config_type, content_type, StreamType::kAudioWithKeySystem,
                     /* spatialRendering */ false, "", "", "", key_system);
  }

  std::string CanDecodeAudioAndVideo(std::string_view config_type,
                                     std::string_view video_content_type,
                                     std::string_view audio_content_type) {
    return CanDecode(
        config_type, video_content_type, StreamType::kAudioAndVideo,
        /* spatialRendering */ false, "", "", "", "", audio_content_type);
  }

  std::string CanDecodeAudioAndVideoWithKeySystem(
      std::string_view config_type,
      std::string_view video_content_type,
      std::string_view audio_content_type,
      std::string_view key_system) {
    return CanDecode(config_type, video_content_type,
                     StreamType::kAudioAndVideoWithKeySystem,
                     /* spatialRendering */ false, "", "", "", key_system,
                     audio_content_type);
  }

  std::string CanDecode(std::string_view config_type,
                        std::string_view content_type,
                        StreamType stream_type,
                        bool spatial_rendering = false,
                        std::string_view hdr_metadata_type = "",
                        std::string_view color_gamut = "",
                        std::string_view transfer_function = "",
                        std::string_view key_system = "",
                        std::string_view specified_audio_content_type = "") {
    std::string command;
    if (stream_type == StreamType::kAudio) {
      base::StringAppendF(&command, "testAudioConfig(");
    } else if (stream_type == StreamType::kAudioWithSpatialRendering) {
      base::StringAppendF(&command, "testAudioConfigWithSpatialRendering(%s,",
                          base::ToString(spatial_rendering));
    } else if (stream_type == StreamType::kVideoWithHdrMetadata) {
      command.append("testVideoConfigWithHdrMetadata(");
      for (auto x : {hdr_metadata_type, color_gamut, transfer_function}) {
        CHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kVideoWithoutHdrMetadata) {
      command.append("testVideoConfigWithoutHdrMetadata(");
      for (auto x : {color_gamut, transfer_function}) {
        CHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kVideoWithHdrMetadataAndKeySystem) {
      command.append("testVideoConfigWithHdrMetadataAndKeySystem(");
      for (auto x :
           {hdr_metadata_type, color_gamut, transfer_function, key_system}) {
        CHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kVideoWithDolbyVisionAndKeySystem) {
      command.append("testVideoConfigWithKeySystem(");
      for (auto x : {key_system}) {
        CHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kAudioWithKeySystem) {
      command.append("testAudioConfigWithKeySystem(");
      for (auto x : {key_system}) {
        CHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kAudioAndVideo) {
      command.append("testAudioAndVideoConfig(");
      for (auto x : {specified_audio_content_type}) {
        CHECK(!x.empty());
        base::StringAppendF(&command, "%.*s,", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kAudioAndVideoWithKeySystem) {
      command.append("testAudioAndVideoConfigWithKeySystem(");

      // Cannot do a loop, since the string appending for content type versus
      // extra parameters like key system are different.
      CHECK(!specified_audio_content_type.empty());
      base::StringAppendF(&command, "%.*s,",
                          static_cast<int>(specified_audio_content_type.size()),
                          specified_audio_content_type.data());

      CHECK(!key_system.empty());
      base::StringAppendF(&command, "\"%.*s\",",
                          static_cast<int>(key_system.size()),
                          key_system.data());
    } else {
      command.append("testVideoConfig(");
    }

    base::StringAppendF(&command, "\"%.*s\",",
                        static_cast<int>(config_type.size()),
                        config_type.data());
    base::StringAppendF(&command, "%.*s);",
                        static_cast<int>(content_type.size()),
                        content_type.data());

    EXPECT_TRUE(ExecJs(shell(), command));

    TitleWatcher title_watcher(shell()->web_contents(),
                               std::u16string(kSupported16));
    title_watcher.AlsoWaitForTitle(std::u16string(kUnsupported16));
    title_watcher.AlsoWaitForTitle(std::u16string(kError16));
    return base::UTF16ToASCII(title_watcher.WaitAndGetTitle());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Adds param for query type (file vs media-source) to
class MediaCapabilitiesTestWithConfigType
    : public MediaCapabilitiesTest,
      public testing::WithParamInterface<ConfigType> {
 public:
  std::string_view GetTypeString() const {
    switch (GetParam()) {
      case ConfigType::kFile:
        return kFileString;
      case ConfigType::kMediaSource:
        return kMediaSourceString;
      case ConfigType::kWebRtc:
        return kWebRtcString;
    }
    NOTREACHED();
  }
};

// Cover basic codec support of content types where the answer of support
// (or not) should be common to both "media-source" and "file" query types.
// for more exhaustive codec string testing.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       CommonVideoDecodeTypes) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // Content types below are not supported for WebRtc.
  const std::string_view type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const std::string_view prop_type_supported =
      GetParam() != kWebRtc ? kPropSupported : kUnsupported;

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  EXPECT_EQ(type_supported,
            CanDecodeVideo(config_type, "'video/webm; codecs=\"vp8\"'"));

  // Only support the new vp09 format which provides critical profile
  // information.
  EXPECT_EQ(kUnsupported,
            CanDecodeVideo(config_type, "'video/webm; codecs=\"vp9\"'"));
  // Requires command line flag switches::kEnableNewVp9CodecString
  EXPECT_EQ(
      type_supported,
      CanDecodeVideo(config_type, "'video/webm; codecs=\"vp09.00.10.08\"'"));

  // VP09 is available in MP4 container irrespective of USE_PROPRIETARY_CODECS.
  EXPECT_EQ(
      type_supported,
      CanDecodeVideo(config_type, "'video/mp4; codecs=\"vp09.00.10.08\"'"));

  // Supported when built with USE_PROPRIETARY_CODECS
  EXPECT_EQ(prop_type_supported,
            CanDecodeVideo(config_type, "'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(prop_type_supported,
            CanDecodeVideo(config_type, "'video/mp4; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(prop_type_supported,
            CanDecodeVideo(config_type, "'video/mp4; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(prop_type_supported,
            CanDecodeVideo(config_type, "'video/mp4; codecs=\"avc1.42F01E\"'"));

  // Test a handful of invalid strings.
  EXPECT_EQ(kUnsupported,
            CanDecodeVideo(config_type, "'video/webm; codecs=\"theora\"'"));
  EXPECT_EQ(
      kUnsupported,
      CanDecodeVideo(config_type, "'video/webm; codecs=\"avc1.42E01E\"'"));
  // Only new vp09 format is supported with MP4.
  EXPECT_EQ(kUnsupported,
            CanDecodeVideo(config_type, "'video/mp4; codecs=\"vp9\"'"));
}

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
// Cover Dolby Vision codec support of content types where the answer of support
// (or not) should be common to both "media-source" and "file" query types.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       DolbyVisionVideoDecodeTypes) {
  if (GetParam() == kWebRtc) {
    GTEST_SKIP() << "The keySystemConfiguration object cannot be set for "
                    "webrtc MediaDecodingType.";
  }

  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  EXPECT_EQ(kSupported, CanDecodeVideoWithKeySystem(
                            config_type, "'video/mp4; codecs=\"dvh1.05.06\"'",
                            media::kExternalClearKeyKeySystem));

  EXPECT_EQ(kSupported, CanDecodeVideoWithKeySystem(
                            config_type, "'video/mp4; codecs=\"dvhe.08.07\"'",
                            media::kExternalClearKeyKeySystem));
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

// Cover basic codec support. See media_canplaytype_browsertest.cc for more
// exhaustive codec string testing.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       CommonAudioDecodeTypes) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // Content types below are not supported for WebRtc.
  const std::string_view type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const std::string_view prop_type_supported =
      GetParam() != kWebRtc ? kPropSupported : kUnsupported;

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/webm; codecs=\"opus\"'"));
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/webm; codecs=\"vorbis\"'"));
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/mp4; codecs=\"flac\"'"));
  EXPECT_EQ(type_supported, CanDecodeAudio(config_type, "'audio/mpeg'"));

  // Supported when built with USE_PROPRIETARY_CODECS
  EXPECT_EQ(prop_type_supported,
            CanDecodeAudio(config_type, "'audio/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(prop_type_supported, CanDecodeAudio(config_type, "'audio/aac'"));

  // Test a handful of invalid strings.
  EXPECT_EQ(kUnsupported,
            CanDecodeAudio(config_type, "'audio/wav; codecs=\"mp3\"'"));
  EXPECT_EQ(kUnsupported,
            CanDecodeAudio(config_type, "'audio/webm; codecs=\"vp8\"'"));
}

IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       NonMediaSourceDecodeTypes) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // Content types below are supported for src=, but not MediaSource or WebRtc.
  const std::string_view type_supported =
      GetParam() == kFile ? kSupported : kUnsupported;

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/wav; codecs=\"1\"'"));

  // Flac is only supported in mp4 for MSE.
  EXPECT_EQ(type_supported, CanDecodeAudio(config_type, "'audio/flac'"));

  // Ogg is not supported in MSE.
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/ogg; codecs=\"flac\"'"));
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/ogg; codecs=\"opus\"'"));

  // MP3 is only supported via audio/mpeg for MSE.
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/mp4; codecs=\"mp4a.69\"'"));

  // Ogg not supported in MSE.
  EXPECT_EQ(type_supported,
            CanDecodeAudio(config_type, "'audio/ogg; codecs=\"vorbis\"'"));
}

// Cover basic spatial rendering support.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       AudioTypesWithSpatialRendering) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // Content types below are not supported for WebRtc.
  const std::string_view type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const std::string_view prop_type_supported =
      GetParam() != kWebRtc ? kPropSupported : kUnsupported;

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  // These common codecs are not associated with a spatial audio format.
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/webm; codecs=\"opus\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/webm; codecs=\"vorbis\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/mp4; codecs=\"flac\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioWithSpatialRendering(config_type, "'audio/mpeg'",
                                               /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/mp4; codecs=\"mp4a.40.02\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioWithSpatialRendering(config_type, "'audio/aac'",
                                               /*spatial_rendering*/ true));

  // Supported codecs should remain supported when querying with
  // spatialRendering set to false.
  EXPECT_EQ(type_supported, CanDecodeAudioWithSpatialRendering(
                                config_type, "'audio/webm; codecs=\"opus\"'",
                                /*spatial_rendering*/ false));
  EXPECT_EQ(type_supported, CanDecodeAudioWithSpatialRendering(
                                config_type, "'audio/webm; codecs=\"vorbis\"'",
                                /*spatial_rendering*/ false));
  EXPECT_EQ(type_supported, CanDecodeAudioWithSpatialRendering(
                                config_type, "'audio/mp4; codecs=\"flac\"'",
                                /*spatial_rendering*/ false));
  EXPECT_EQ(type_supported,
            CanDecodeAudioWithSpatialRendering(config_type, "'audio/mpeg'",
                                               /*spatial_rendering*/ false));
  EXPECT_EQ(prop_type_supported,
            CanDecodeAudioWithSpatialRendering(
                config_type, "'audio/mp4; codecs=\"mp4a.40.02\"'",
                /*spatial_rendering*/ false));
  EXPECT_EQ(prop_type_supported,
            CanDecodeAudioWithSpatialRendering(config_type, "'audio/aac'",
                                               /*spatial_rendering*/ false));

  // Test a handful of invalid strings.
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/wav; codecs=\"mp3\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/webm; codecs=\"vp8\"'",
                              /*spatial_rendering*/ true));

  // Dolby Atmos = Dolby Digital Plus + Spatial Rendering. Currently not
  // supported.
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/mp4; codecs=\"ec-3\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/mp4; codecs=\"mp4a.a6\"'",
                              /*spatial_rendering*/ true));
  EXPECT_EQ(kUnsupported, CanDecodeAudioWithSpatialRendering(
                              config_type, "'audio/mp4; codecs=\"mp4a.A6\"'",
                              /*spatial_rendering*/ true));
}

// Cover basic HDR support.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       VideoTypesWithDynamicRange) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // None of the content types below are supported for WebRtc.
  const std::string_view type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const std::string_view prop_type_supported =
      GetParam() != kWebRtc ? kPropSupported : kUnsupported;

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  for (auto color_gamut : {kSrgb, kP3, kRec2020, kInvalid}) {
    for (auto transfer_function : {kSrgb, kPq, kHlg, kInvalid}) {
      // All valid color gamuts and transfer functions without HDR metadata
      // should be supported.
      auto is_invalid =
          color_gamut == kInvalid || transfer_function == kInvalid;
      EXPECT_EQ(is_invalid ? kError : type_supported,
                CanDecodeVideoWithHdrMetadata(config_type,
                                              "'video/webm; codecs=\"vp8\"'",
                                              color_gamut, transfer_function));

      // HdrMetadataType smpteSt2086 is supported w/ PQ transfer functions.
      auto can_use_hdr_metadata = transfer_function == kPq;
      EXPECT_EQ(
          is_invalid ? kError
                     : (can_use_hdr_metadata ? type_supported : kUnsupported),
          CanDecodeVideoWithHdrMetadata(config_type,
                                        "'video/webm; codecs=\"vp8\"'",
                                        color_gamut, transfer_function,
                                        /*hdr_metadata_type=*/kSmpteSt2086));

      // No other HdrMetadataType is currently supported.
      for (auto hdr_metadata_type :
           {kSmpteSt2094_10, kSmpteSt2094_40, kInvalid}) {
        EXPECT_EQ(
            is_invalid || hdr_metadata_type == kInvalid ? kError : kUnsupported,
            CanDecodeVideoWithHdrMetadata(
                config_type, "'video/webm; codecs=\"vp8\"'", color_gamut,
                transfer_function, hdr_metadata_type));
      }
    }
  }

  // Make sure results are expected with some USE_PROPRIETARY_CODECS
  EXPECT_EQ(prop_type_supported,
            CanDecodeVideoWithHdrMetadata(config_type,
                                          "'video/mp4; codecs=\"avc1.42E01E\"'",
                                          /* colorGamut */ kP3,
                                          /* transferFunction */ kPq));
  EXPECT_EQ(prop_type_supported,
            CanDecodeVideoWithHdrMetadata(config_type,
                                          "'video/mp4; codecs=\"avc1.42101E\"'",
                                          /* colorGamut */ kSrgb,
                                          /* transferFunction */ kSrgb));
}

// This is to test setting both MediaCapabilities and the KeySystem to verify
// that MediaCapabilities does not support configurations when KeySystem is
// specified.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       VideoTypeWithHdrMetadataAndKeySystem) {
  if (GetParam() == kWebRtc) {
    GTEST_SKIP() << "The keySystemConfiguration object cannot be set for "
                    "webrtc MediaDecodingType.";
  }

  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  // This HDR metadata is supported.
  EXPECT_EQ(kSupported,
            CanDecodeVideoWithHdrMetadataAndKeySystem(
                config_type, "'video/webm; codecs=\"vp8\"'", kRec2020, kPq,
                kSmpteSt2086, media::kClearKeyKeySystem));

  // This HDR metadata is unsupported, so no matter the key system, Chromium
  // should return unsupported.
  EXPECT_EQ(kUnsupported,
            CanDecodeVideoWithHdrMetadataAndKeySystem(
                config_type, "'video/webm; codecs=\"vp8\"'", kRec2020, kPq,
                kSmpteSt2094_10, media::kClearKeyKeySystem));
}

IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       AudioOnlyWithKeySystem) {
  if (GetParam() == kWebRtc) {
    GTEST_SKIP() << "The keySystemConfiguration object cannot be set for "
                    "webrtc MediaDecodingType.";
  }

  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  // Test that valid codec and key system combinations work.
  EXPECT_EQ(kSupported, CanDecodeAudioWithKeySystem(
                            config_type, "'audio/webm; codecs=\"vorbis\"'",
                            media::kClearKeyKeySystem));
  EXPECT_EQ(kSupported, CanDecodeAudioWithKeySystem(
                            config_type, "'audio/mp4; codecs=\"flac\"'",
                            media::kClearKeyKeySystem));

  // Test that proprietary audio codec and key system combination work.
  EXPECT_EQ(kPropSupported,
            CanDecodeAudioWithKeySystem(config_type,
                                        "'audio/mp4; codecs=\"mp4a.40.2\"'",
                                        media::kClearKeyKeySystem));

  // Test unsupported audio-only configs with key system specified.
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioWithKeySystem(config_type,
                                        "'audio/mp4; codecs=\"nonexistent\"'",
                                        media::kClearKeyKeySystem));

  EXPECT_EQ(kUnsupported, CanDecodeAudioWithKeySystem(
                              config_type, "'audio/mp4; codecs=\"mp4a.40.2\"'",
                              "com.invalid.keySystem"));
}

IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       CombinedAudioAndVideoDecodeTypes) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // WebRtc not supported.
  const std::string_view type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const std::string_view prop_type_supported =
      GetParam() != kWebRtc ? kPropSupported : kUnsupported;

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  EXPECT_EQ(type_supported,
            CanDecodeAudioAndVideo(config_type, "'video/webm; codecs=\"vp8\"'",
                                   "'audio/webm; codecs=\"opus\"'"));
  EXPECT_EQ(type_supported,
            CanDecodeAudioAndVideo(config_type,
                                   "'video/webm; codecs=\"vp09.00.10.08\"'",
                                   "'audio/webm; codecs=\"vorbis\"'"));

  // Test with proprietary codecs
  EXPECT_EQ(
      prop_type_supported,
      CanDecodeAudioAndVideo(config_type, "'video/mp4; codecs=\"avc1.42E01E\"'",
                             "'audio/mp4; codecs=\"mp4a.40.02\"'"));

  // The browser reports mixed containers as supported, and this is expected.
  EXPECT_EQ(prop_type_supported,
            CanDecodeAudioAndVideo(config_type, "'video/webm; codecs=\"vp8\"'",
                                   "'audio/mp4; codecs=\"mp4a.40.02\"'"));

  //  Test with invalid audio codec.
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioAndVideo(config_type,
                                   "'video/webm; codecs=\"vp09.00.10.08\"'",
                                   "'audio/webm; codecs=\"invalid\"'"));

  // Test with invalid video codec.
  EXPECT_EQ(kUnsupported, CanDecodeAudioAndVideo(
                              config_type, "'video/webm; codecs=\"invalid\"'",
                              "'audio/webm; codecs=\"vorbis\"'"));

  // Test with invalid video codec and invalid audio codec.
  EXPECT_EQ(kUnsupported, CanDecodeAudioAndVideo(
                              config_type, "'video/webm; codecs=\"invalid\"'",
                              "'audio/webm; codecs=\"invalid\"'"));
}

IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       CombinedAudioVideoWithKeySystem) {
  if (GetParam() == kWebRtc) {
    GTEST_SKIP() << "The keySystemConfiguration object cannot be set for "
                    "webrtc MediaDecodingType.";
  }
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  EXPECT_TRUE(
      NavigateToURL(shell(), content::GetFileUrlWithQuery(file_path, "")));

  EXPECT_EQ(kSupported,
            CanDecodeAudioAndVideoWithKeySystem(
                config_type, "'video/webm; codecs=\"vp8\"'",
                "'audio/webm; codecs=\"opus\"'", media::kClearKeyKeySystem));
  EXPECT_EQ(kSupported,
            CanDecodeAudioAndVideoWithKeySystem(
                config_type, "'video/webm; codecs=\"vp09.00.10.08\"'",
                "'audio/webm; codecs=\"vorbis\"'", media::kClearKeyKeySystem));

  // Test with proprietary codecs
  EXPECT_EQ(
      kPropSupported,
      CanDecodeAudioAndVideoWithKeySystem(
          config_type, "'video/mp4; codecs=\"avc1.42E01E\"'",
          "'audio/mp4; codecs=\"mp4a.40.02\"'", media::kClearKeyKeySystem));

  // The browser reports mixed containers as supported, and this is expected.
  EXPECT_EQ(kPropSupported, CanDecodeAudioAndVideoWithKeySystem(
                                config_type, "'video/webm; codecs=\"vp8\"'",
                                "'audio/mp4; codecs=\"mp4a.40.02\"'",
                                media::kClearKeyKeySystem));

  //  Test with invalid audio codec.
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioAndVideoWithKeySystem(
                config_type, "'video/webm; codecs=\"vp09.00.10.08\"'",
                "'audio/webm; codecs=\"invalid\"'", media::kClearKeyKeySystem));

  // Test with invalid video codec.
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioAndVideoWithKeySystem(
                config_type, "'video/webm; codecs=\"invalid\"'",
                "'audio/webm; codecs=\"vorbis\"'", media::kClearKeyKeySystem));

  // Test with invalid key system.
  EXPECT_EQ(kUnsupported,
            CanDecodeAudioAndVideoWithKeySystem(
                config_type, "'video/webm; codecs=\"vp09.00.10.08\"'",
                "'audio/webm; codecs=\"vorbis\"'", "com.invalid.keySystem"));
}

INSTANTIATE_TEST_SUITE_P(File,
                         MediaCapabilitiesTestWithConfigType,
                         ::testing::Values(ConfigType::kFile));
INSTANTIATE_TEST_SUITE_P(MediaSource,
                         MediaCapabilitiesTestWithConfigType,
                         ::testing::Values(ConfigType::kMediaSource));
INSTANTIATE_TEST_SUITE_P(WebRtc,
                         MediaCapabilitiesTestWithConfigType,
                         ::testing::Values(ConfigType::kWebRtc));

}  // namespace content
