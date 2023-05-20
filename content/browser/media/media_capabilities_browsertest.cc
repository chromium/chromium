// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
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

namespace {

constexpr base::StringPiece kDecodeTestFile = "decode_capabilities_test.html";
constexpr base::StringPiece kSupported = "SUPPORTED";
constexpr base::StringPiece16 kSupported16 = u"SUPPORTED";
constexpr base::StringPiece kUnsupported = "UNSUPPORTED";
constexpr base::StringPiece16 kUnsupported16 = u"UNSUPPORTED";
constexpr base::StringPiece kError = "ERROR";
constexpr base::StringPiece16 kError16 = u"ERROR";
constexpr base::StringPiece kFileString = "file";
constexpr base::StringPiece kMediaSourceString = "media-source";
constexpr base::StringPiece kWebRtcString = "webrtc";
constexpr base::StringPiece kInvalid = "INVALID";

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
constexpr base::StringPiece kPropSupported = kSupported;
#else
constexpr base::StringPiece kPropSupported = kUnsupported;
#endif  // USE_PROPRIETARY_CODECS

enum StreamType {
  kAudio,
  kVideo,
  kAudioWithSpatialRendering,
  kVideoWithHdrMetadata,
  kVideoWithoutHdrMetadata
};

enum ConfigType { kFile, kMediaSource, kWebRtc };

}  // namespace

namespace content {

class MediaCapabilitiesTest : public ContentBrowserTest {
 public:
  MediaCapabilitiesTest()
      : scoped_feature_list_(media::kSupportSmpteSt2086HdrMetadata) {}

  MediaCapabilitiesTest(const MediaCapabilitiesTest&) = delete;
  MediaCapabilitiesTest& operator=(const MediaCapabilitiesTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaCapabilitiesSpatialAudio");
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaCapabilitiesDynamicRange");
  }

  std::string CanDecodeAudio(base::StringPiece config_type,
                             base::StringPiece content_type) {
    return CanDecode(config_type, content_type, StreamType::kAudio);
  }

  std::string CanDecodeVideo(base::StringPiece config_type,
                             base::StringPiece content_type) {
    return CanDecode(config_type, content_type, StreamType::kVideo);
  }

  std::string CanDecodeAudioWithSpatialRendering(base::StringPiece config_type,
                                                 base::StringPiece content_type,
                                                 bool spatial_rendering) {
    return CanDecode(config_type, content_type,
                     StreamType::kAudioWithSpatialRendering, spatial_rendering);
  }

  std::string CanDecodeVideoWithHdrMetadata(
      base::StringPiece config_type,
      base::StringPiece content_type,
      base::StringPiece color_gamut,
      base::StringPiece transfer_function,
      base::StringPiece hdr_metadata_type = "") {
    StreamType stream_type = StreamType::kVideoWithHdrMetadata;
    if (hdr_metadata_type == "")
      stream_type = StreamType::kVideoWithoutHdrMetadata;

    return CanDecode(config_type, content_type, stream_type,
                     /* spatialRendering */ false, hdr_metadata_type,
                     color_gamut, transfer_function);
  }

  std::string CanDecode(base::StringPiece config_type,
                        base::StringPiece content_type,
                        StreamType stream_type,
                        bool spatial_rendering = false,
                        base::StringPiece hdr_metadata_type = "",
                        base::StringPiece color_gamut = "",
                        base::StringPiece transfer_function = "") {
    std::string command;
    if (stream_type == StreamType::kAudio) {
      base::StringAppendF(&command, "testAudioConfig(");
    } else if (stream_type == StreamType::kAudioWithSpatialRendering) {
      base::StringAppendF(&command, "testAudioConfigWithSpatialRendering(%s,",
                          spatial_rendering ? "true" : "false");
    } else if (stream_type == StreamType::kVideoWithHdrMetadata) {
      command.append("testVideoConfigWithHdrMetadata(");
      for (auto x : {hdr_metadata_type, color_gamut, transfer_function}) {
        DCHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
    } else if (stream_type == StreamType::kVideoWithoutHdrMetadata) {
      command.append("testVideoConfigWithoutHdrMetadata(");
      for (auto x : {color_gamut, transfer_function}) {
        DCHECK(!x.empty());
        base::StringAppendF(&command, "\"%.*s\",", static_cast<int>(x.size()),
                            x.data());
      }
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Adds param for query type (file vs media-source) to
class MediaCapabilitiesTestWithConfigType
    : public MediaCapabilitiesTest,
      public testing::WithParamInterface<ConfigType> {
 public:
  base::StringPiece GetTypeString() const {
    switch (GetParam()) {
      case ConfigType::kFile:
        return kFileString;
      case ConfigType::kMediaSource:
        return kMediaSourceString;
      case ConfigType::kWebRtc:
        return kWebRtcString;
    }
    NOTREACHED_NORETURN();
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
  const base::StringPiece type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const base::StringPiece prop_type_supported =
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

// Cover basic codec support. See media_canplaytype_browsertest.cc for more
// exhaustive codec string testing.
IN_PROC_BROWSER_TEST_P(MediaCapabilitiesTestWithConfigType,
                       CommonAudioDecodeTypes) {
  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // Content types below are not supported for WebRtc.
  const base::StringPiece type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const base::StringPiece prop_type_supported =
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
  const base::StringPiece type_supported =
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
  const base::StringPiece type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const base::StringPiece prop_type_supported =
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
  constexpr base::StringPiece kSrgb = "srgb";
  constexpr base::StringPiece kP3 = "p3";
  constexpr base::StringPiece kRec2020 = "rec2020";
  constexpr base::StringPiece kPq = "pq";
  constexpr base::StringPiece kHlg = "hlg";
  constexpr base::StringPiece kSmpteSt2086 = "smpteSt2086";
  constexpr base::StringPiece kSmpteSt2094_10 = "smpteSt2094-10";
  constexpr base::StringPiece kSmpteSt2094_40 = "smpteSt2094-40";

  base::FilePath file_path =
      media::GetTestDataFilePath(std::string(kDecodeTestFile));

  const auto config_type = GetTypeString();

  // None of the content types below are supported for WebRtc.
  const base::StringPiece type_supported =
      GetParam() != kWebRtc ? kSupported : kUnsupported;
  const base::StringPiece prop_type_supported =
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

      // HdrMetadataType smpteSt2086 is supported
      EXPECT_EQ(is_invalid ? kError : type_supported,
                CanDecodeVideoWithHdrMetadata(
                    config_type, "'video/webm; codecs=\"vp8\"'", color_gamut,
                    transfer_function,
                    /* hdrMetadataType */ kSmpteSt2086));

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
