// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_manager.h"

#include <optional>

#include "base/logging.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/formats/hls/multivariant_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

namespace {

std::string MakeVariantStr(uint64_t bandwidth,
                           std::optional<std::string_view> resolution,
                           std::optional<std::string_view> framerate) {
  std::stringstream stream_inf;
  stream_inf << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=" << bandwidth;
  if (resolution.has_value()) {
    stream_inf << ",RESOLUTION=" << resolution.value();
  }
  if (framerate.has_value()) {
    stream_inf << ",FRAME-RATE=" << framerate.value();
  }
  return stream_inf.str();
}

RenditionManager::CodecSupportType GetCodecSupportType(
    std::string_view container,
    base::span<const std::string> codecs) {
  bool has_audio = false;
  bool has_video = false;
  for (const auto& codec : codecs) {
    if (codec == "V") {
      has_video = true;
    } else if (codec == "A") {
      has_audio = true;
    } else if (codec == "audio.codec") {
      has_audio = true;
    } else if (codec == "video.codec") {
      has_video = true;
    } else if (codec == "av.codec") {
      return RenditionManager::CodecSupportType::kSupportedAudioVideo;
    }
  }
  if (has_audio && has_video) {
    return RenditionManager::CodecSupportType::kSupportedAudioVideo;
  } else if (has_audio) {
    return RenditionManager::CodecSupportType::kSupportedAudioOnly;
  } else if (has_video) {
    return RenditionManager::CodecSupportType::kSupportedVideoOnly;
  }
  return RenditionManager::CodecSupportType::kUnsupported;
}

RenditionManager::CodecSupportType GetCodecSupportForSoftwareOnlyLinux(
    std::string_view container,
    base::span<const std::string> codecs) {
  bool has_audio = false;
  bool has_video = false;
  for (const auto& codec : codecs) {
    if (codec == "avc1.640020") {
      // h264
      has_video = true;
    } else if (codec == "avc1.64002a") {
      // Nope!
      has_video = true;
    } else if (codec == "mp4a.40.2") {
      // AAC-LC
      has_audio = true;
    } else if (codec == "avc1.64001f") {
      // h264
      has_video = true;
    } else if (codec == "ac-3") {
      // Nope!
      return RenditionManager::CodecSupportType::kUnsupported;
    } else if (codec == "hvc1.2.4.L123.B0") {
      // Nope!
      return RenditionManager::CodecSupportType::kUnsupported;
    } else if (codec == "ec-3") {
      // Nope!
      return RenditionManager::CodecSupportType::kUnsupported;
    } else {
      LOG(ERROR) << "UNHANDLED CODEC: " << codec;
    }
  }
  if (has_audio && has_video) {
    return RenditionManager::CodecSupportType::kSupportedAudioVideo;
  } else if (has_audio) {
    return RenditionManager::CodecSupportType::kSupportedAudioOnly;
  } else if (has_video) {
    return RenditionManager::CodecSupportType::kSupportedVideoOnly;
  }
  return RenditionManager::CodecSupportType::kUnsupported;
}

}  // namespace

using testing::_;

class HlsRenditionManagerTest : public testing::Test {
 public:
  MOCK_METHOD(void, VariantSelected, (std::string, std::string), ());

  void _VariantSelected(AdaptationReason,
                        const VariantStream* vs,
                        const AudioRendition* ar) {
    std::string variant_path = "NONE";
    std::string rendition_path = "NONE";
    if (vs) {
      variant_path = vs->GetPrimaryRenditionUri().path();
    }
    if (ar) {
      CHECK(ar->GetUri().has_value());
      rendition_path = ar->GetUri()->path();
    }
    VariantSelected(variant_path, rendition_path);
  }

  decltype(auto) GetVariantCb() {
    return base::BindRepeating(&HlsRenditionManagerTest::_VariantSelected,
                               base::Unretained(this),
                               AdaptationReason::kUserSelection);
  }

  decltype(auto) GetVariantWithAdaptation() {
    return base::BindRepeating(&HlsRenditionManagerTest::_VariantSelected,
                               base::Unretained(this));
  }

  template <typename... Strings>
  RenditionManager GetRenditionManager(Strings... strings) {
    MultivariantPlaylistTestBuilder builder;
    builder.AppendLine("#EXTM3U");
    ([&] { builder.AppendLine(strings); }(), ...);
    return RenditionManager(builder.Parse(),
                            base::BindRepeating(GetVariantWithAdaptation()),
                            base::BindRepeating(&GetCodecSupportType));
  }

  template <typename... Strings>
  RenditionManager GetCustomSupportRenditionManager(
      base::RepeatingCallback<RenditionManager::CodecSupportType(
          std::string_view,
          base::span<const std::string>)> support_cb,
      Strings... strings) {
    MultivariantPlaylistTestBuilder builder;
    builder.AppendLine("#EXTM3U");
    ([&] { builder.AppendLine(strings); }(), ...);
    return RenditionManager(builder.Parse(),
                            base::BindRepeating(GetVariantWithAdaptation()),
                            std::move(support_cb));
  }
};

TEST_F(HlsRenditionManagerTest, MixedAVTypes) {
  auto rm = GetRenditionManager(
      "#EXT-X-STREAM-INF:BANDWIDTH=1280000,AVERAGE-BANDWIDTH=1000000",
      "http://example.com/low.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=2560000,AVERAGE-BANDWIDTH=2000000",
      "http://example.com/mid.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=7680000,AVERAGE-BANDWIDTH=6000000",
      "http://example.com/hi.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"audio.codec\"",
      "http://example.com/audio-only.m3u8");

  EXPECT_CALL(*this, VariantSelected("/hi.m3u8", "NONE"));
  rm.Reselect(GetVariantCb());
}

TEST_F(HlsRenditionManagerTest, NoSupportedCodecs) {
  auto rm = GetRenditionManager(
      "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"vvc1.00.00\"",
      "http://example.com/audio-only.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"sheet.music\"",
      "http://example.com/audio-only.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"av02.00.00\"",
      "http://example.com/audio-only.m3u8");
  ASSERT_FALSE(rm.HasAnyVariants());
  EXPECT_CALL(*this, VariantSelected("NONE", "NONE"));
  rm.Reselect(GetVariantCb());
}

TEST_F(HlsRenditionManagerTest, MultipleVariantResolutions) {
  auto rm = GetRenditionManager(
      "#EXT-X-STREAM-INF:BANDWIDTH=10,CODECS=\"V\",RESOLUTION=320x200",
      "video/cga.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=10,CODECS=\"V\",RESOLUTION=480x320",
      "video/hvga.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=10,CODECS=\"V\",RESOLUTION=800x480",
      "video/wvga.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=10,CODECS=\"V\",RESOLUTION=1920x1080",
      "video/fhd.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=10,CODECS=\"V\",RESOLUTION=2560x1440",
      "video/wqhd.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=10,CODECS=\"V\",RESOLUTION=7680x4320",
      "video/8kuhd.m3u8");

  EXPECT_CALL(*this, VariantSelected("/video/8kuhd.m3u8", "NONE"));
  rm.Reselect(GetVariantCb());

  EXPECT_CALL(*this, VariantSelected("/video/fhd.m3u8", "NONE"));
  rm.UpdatePlayerResolution({1920, 1080});

  EXPECT_CALL(*this, VariantSelected("/video/wvga.m3u8", "NONE"));
  rm.UpdatePlayerResolution({1000, 1000});

  // The comparison is area based.
  EXPECT_CALL(*this, VariantSelected("/video/fhd.m3u8", "NONE"));
  rm.UpdatePlayerResolution({1080, 1920});

  EXPECT_CALL(*this, VariantSelected("/video/hvga.m3u8", "NONE"));
  rm.UpdatePlayerResolution({400, 600});

  EXPECT_CALL(*this, VariantSelected("/video/8kuhd.m3u8", "NONE"));
  rm.UpdatePlayerResolution({8192, 8192});
}

TEST_F(HlsRenditionManagerTest, MP4SplitCodecs) {
  auto rm = GetCustomSupportRenditionManager(
      base::BindRepeating(&GetCodecSupportForSoftwareOnlyLinux),
      "#EXT-X-INDEPENDENT-SEGMENTS",

      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a1\",NAME=\"English\",LANGUAGE=\"en-"
      "US\",AUTOSELECT=YES,DEFAULT=YES,CHANNELS=\"2\",URI=\"a1/"
      "prog_index.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a2\",NAME=\"English\",LANGUAGE=\"en-"
      "US\",AUTOSELECT=YES,DEFAULT=YES,CHANNELS=\"6\",URI=\"a2/"
      "prog_index.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a3\",NAME=\"English\",LANGUAGE=\"en-"
      "US\",AUTOSELECT=YES,DEFAULT=YES,CHANNELS=\"6\",URI=\"a3/"
      "prog_index.m3u8\"",

      "#EXT-X-MEDIA:TYPE=CLOSED-CAPTIONS,GROUP-ID=\"cc\",LANGUAGE=\"en\",NAME="
      "\"English\",DEFAULT=YES,AUTOSELECT=YES,INSTREAM-ID=\"CC1\"",

      "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"sub1\",LANGUAGE=\"en\",NAME="
      "\"English\",AUTOSELECT=YES,DEFAULT=YES,FORCED=NO,URI=\"s1/en/"
      "prog_index.m3u8\"",

      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=928091,BANDWIDTH=1015727,"
      "CODECS=\"avc1.640028\",RESOLUTION=1920x1080,URI=\"tp5/"
      "iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=731514,BANDWIDTH=760174,"
      "CODECS=\"avc1.64001f\",RESOLUTION=1280x720,URI=\"tp4/"
      "iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=509153,BANDWIDTH=520162,"
      "CODECS=\"avc1.64001f\",RESOLUTION=960x540,URI=\"tp3/iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=176942,BANDWIDTH=186651,"
      "CODECS=\"avc1.64001f\",RESOLUTION=640x360,URI=\"tp2/iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=90796,BANDWIDTH=95410,"
      "CODECS=\"avc1.64001f\",RESOLUTION=480x270,URI=\"tp1/iframe_index.m3u8\"",

      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2190673,BANDWIDTH=2523597,CODECS="
      "\"avc1.640020,mp4a.40.2\",RESOLUTION=960x540,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v5/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=8052613,BANDWIDTH=9873268,CODECS="
      "\"avc1.64002a,mp4a.40.2\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v9/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=6133114,BANDWIDTH=7318337,CODECS="
      "\"avc1.64002a,mp4a.40.2\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v8/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=4681537,BANDWIDTH=5421720,CODECS="
      "\"avc1.64002a,mp4a.40.2\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v7/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=3183969,BANDWIDTH=3611257,CODECS="
      "\"avc1.640020,mp4a.40.2\",RESOLUTION=1280x720,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v6/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1277747,BANDWIDTH=1475903,CODECS="
      "\"avc1.64001f,mp4a.40.2\",RESOLUTION=768x432,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v4/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=890848,BANDWIDTH=1017705,CODECS="
      "\"avc1.64001f,mp4a.40.2\",RESOLUTION=640x360,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v3/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=533420,BANDWIDTH=582820,CODECS="
      "\"avc1.64001f,mp4a.40.2\",RESOLUTION=480x270,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v2/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=303898,BANDWIDTH=339404,CODECS="
      "\"avc1.64001f,mp4a.40.2\",RESOLUTION=416x234,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v1/prog_index.m3u8",

      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2413172,BANDWIDTH=2746096,CODECS="
      "\"avc1.640020,ac-3\",RESOLUTION=960x540,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v5/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=8275112,BANDWIDTH=10095767,CODECS="
      "\"avc1.64002a,ac-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v9/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=6355613,BANDWIDTH=7540836,CODECS="
      "\"avc1.64002a,ac-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v8/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=4904036,BANDWIDTH=5644219,CODECS="
      "\"avc1.64002a,ac-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v7/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=3406468,BANDWIDTH=3833756,CODECS="
      "\"avc1.640020,ac-3\",RESOLUTION=1280x720,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v6/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1500246,BANDWIDTH=1698402,CODECS="
      "\"avc1.64001f,ac-3\",RESOLUTION=768x432,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v4/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1113347,BANDWIDTH=1240204,CODECS="
      "\"avc1.64001f,ac-3\",RESOLUTION=640x360,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v3/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=755919,BANDWIDTH=805319,CODECS="
      "\"avc1.64001f,ac-3\",RESOLUTION=480x270,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v2/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=526397,BANDWIDTH=561903,CODECS="
      "\"avc1.64001f,ac-3\",RESOLUTION=416x234,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v1/prog_index.m3u8",

      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2221172,BANDWIDTH=2554096,CODECS="
      "\"avc1.640020,ec-3\",RESOLUTION=960x540,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v5/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=8083112,BANDWIDTH=9903767,CODECS="
      "\"avc1.64002a,ec-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v9/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=6163613,BANDWIDTH=7348836,CODECS="
      "\"avc1.64002a,ec-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v8/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=4712036,BANDWIDTH=5452219,CODECS="
      "\"avc1.64002a,ec-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v7/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=3214468,BANDWIDTH=3641756,CODECS="
      "\"avc1.640020,ec-3\",RESOLUTION=1280x720,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v6/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1308246,BANDWIDTH=1506402,CODECS="
      "\"avc1.64001f,ec-3\",RESOLUTION=768x432,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v4/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=921347,BANDWIDTH=1048204,CODECS="
      "\"avc1.64001f,ec-3\",RESOLUTION=640x360,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v3/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=563919,BANDWIDTH=613319,CODECS="
      "\"avc1.64001f,ec-3\",RESOLUTION=480x270,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v2/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=334397,BANDWIDTH=369903,CODECS="
      "\"avc1.64001f,ec-3\",RESOLUTION=416x234,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v1/prog_index.m3u8",

      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=287207,BANDWIDTH=328352,"
      "CODECS=\"hvc1.2.4.L123.B0\",RESOLUTION=1920x1080,URI=\"tp10/"
      "iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=216605,BANDWIDTH=226274,"
      "CODECS=\"hvc1.2.4.L123.B0\",RESOLUTION=1280x720,URI=\"tp9/"
      "iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=154000,BANDWIDTH=159037,"
      "CODECS=\"hvc1.2.4.L123.B0\",RESOLUTION=960x540,URI=\"tp8/"
      "iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=90882,BANDWIDTH=92800,"
      "CODECS=\"hvc1.2.4.L123.B0\",RESOLUTION=640x360,URI=\"tp7/"
      "iframe_index.m3u8\"",
      "#EXT-X-I-FRAME-STREAM-INF:AVERAGE-BANDWIDTH=50569,BANDWIDTH=51760,"
      "CODECS=\"hvc1.2.4.L123.B0\",RESOLUTION=480x270,URI=\"tp6/"
      "iframe_index.m3u8\"",

      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1966314,BANDWIDTH=2164328,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=960x540,FRAME-RATE=60.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v14/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=6105163,BANDWIDTH=6664228,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=1920x1080,FRAME-RATE=60.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v18/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=4801073,BANDWIDTH=5427899,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=1920x1080,FRAME-RATE=60.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v17/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=3441312,BANDWIDTH=4079770,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=1920x1080,FRAME-RATE=60.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v16/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2635933,BANDWIDTH=2764701,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=1280x720,FRAME-RATE=60.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v15/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1138612,BANDWIDTH=1226255,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=768x432,FRAME-RATE=30.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v13/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=829339,BANDWIDTH=901770,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=640x360,FRAME-RATE=30.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v12/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=522229,BANDWIDTH=548927,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=480x270,FRAME-RATE=30.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v11/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=314941,BANDWIDTH=340713,CODECS="
      "\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=416x234,FRAME-RATE=30.000,"
      "CLOSED-CAPTIONS=\"cc\",AUDIO=\"a1\",SUBTITLES=\"sub1\"",
      "v10/prog_index.m3u8",

      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2188813,BANDWIDTH=2386827,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=960x540,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v14/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=6327662,BANDWIDTH=6886727,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v18/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=5023572,BANDWIDTH=5650398,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v17/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=3663811,BANDWIDTH=4302269,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v16/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2858432,BANDWIDTH=2987200,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=1280x720,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v15/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1361111,BANDWIDTH=1448754,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=768x432,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v13/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1051838,BANDWIDTH=1124269,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=640x360,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v12/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=744728,BANDWIDTH=771426,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=480x270,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v11/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=537440,BANDWIDTH=563212,CODECS="
      "\"hvc1.2.4.L123.B0,ac-3\",RESOLUTION=416x234,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a2\",SUBTITLES=\"sub1\"",
      "v10/prog_index.m3u8",

      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1996813,BANDWIDTH=2194827,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=960x540,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v14/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=6135662,BANDWIDTH=6694727,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v18/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=4831572,BANDWIDTH=5458398,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v17/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=3471811,BANDWIDTH=4110269,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=1920x1080,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v16/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=2666432,BANDWIDTH=2795200,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=1280x720,FRAME-RATE=60.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v15/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=1169111,BANDWIDTH=1256754,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=768x432,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v13/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=859838,BANDWIDTH=932269,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=640x360,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v12/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=552728,BANDWIDTH=579426,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=480x270,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v11/prog_index.m3u8",
      "#EXT-X-STREAM-INF:AVERAGE-BANDWIDTH=345440,BANDWIDTH=371212,CODECS="
      "\"hvc1.2.4.L123.B0,ec-3\",RESOLUTION=416x234,FRAME-RATE=30.000,CLOSED-"
      "CAPTIONS=\"cc\",AUDIO=\"a3\",SUBTITLES=\"sub1\"",
      "v10/prog_index.m3u8");

  EXPECT_CALL(*this,
              VariantSelected("/v9/prog_index.m3u8", "/a1/prog_index.m3u8"));
  rm.Reselect(GetVariantCb());
}

TEST_F(HlsRenditionManagerTest, MultipleRenditionGroupsVariantsOutOfOrder) {
  auto rm = GetRenditionManager(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"stereo\",LANGUAGE=\"en\",NAME="
      "\"English\",DEFAULT=YES,AUTOSELECT=YES,URI=\"audio/stereo/en/"
      "128kbit.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"stereo\",LANGUAGE=\"dubbing\",NAME="
      "\"Dubbing\",DEFAULT=NO,AUTOSELECT=YES,URI=\"audio/stereo/none/"
      "128kbit.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"stereo\",LANGUAGE=\"de\",NAME="
      "\"German\",DEFAULT=YES,AUTOSELECT=YES,URI=\"audio/stereo/de/"
      "128kbit.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"surround\",LANGUAGE=\"en\",NAME="
      "\"English\",DEFAULT=YES,AUTOSELECT=YES,URI=\"audio/surround/en/"
      "320kbit.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"surround\",LANGUAGE=\"dubbing\",NAME="
      "\"Dubbing\",DEFAULT=NO,AUTOSELECT=YES,URI=\"audio/surround/none/"
      "320kbit.m3u8\"",
      "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Deutsch\",DEFAULT="
      "NO,AUTOSELECT=YES,FORCED=NO,LANGUAGE=\"de\",URI=\"subtitles_de.m3u8\"",
      "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"English\",DEFAULT="
      "YES,AUTOSELECT=YES,FORCED=NO,LANGUAGE=\"en\",URI=\"subtitles_en.m3u8\"",
      "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Espanol\",DEFAULT="
      "NO,AUTOSELECT=YES,FORCED=NO,LANGUAGE=\"es\",URI=\"subtitles_es.m3u8\"",
      "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Français\",DEFAULT="
      "NO,AUTOSELECT=YES,FORCED=NO,LANGUAGE=\"fr\",URI=\"subtitles_fr.m3u8\"",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=258157,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"stereo\",RESOLUTION=422x180,SUBTITLES=\"subs\"",
      "video/250kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=520929,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"stereo\",RESOLUTION=638x272,SUBTITLES=\"subs\"",
      "video/500kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=831270,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"stereo\",RESOLUTION=638x272,SUBTITLES=\"subs\"",
      "video/800kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1144430,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"surround\",RESOLUTION=958x408,SUBTITLES=\"subs\"",
      "video/1100kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1558322,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"surround\",RESOLUTION=1277x554,SUBTITLES=\"subs\"",
      "video/1500kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=4149264,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"surround\",RESOLUTION=1921x818,SUBTITLES=\"subs\"",
      "video/4000kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=10285391,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"surround\",RESOLUTION=4096x1744,SUBTITLES="
      "\"subs\"",
      "video/10000kbit.m3u8",
      "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=6214307,CODECS=\"video.codec,"
      "audio.codec\",AUDIO=\"surround\",RESOLUTION=1921x818,SUBTITLES=\"subs\"",
      "video/6000kbit.m3u8");

  // All variants are playable, so the best one selected. The default audio
  // override is also selected.
  EXPECT_CALL(*this, VariantSelected("/video/10000kbit.m3u8",
                                     "/audio/surround/en/320kbit.m3u8"));
  rm.Reselect(GetVariantCb());

  // Notify a network downgrade, but not one that would preclude our 10285kbps
  // stream. Verify no response.
  EXPECT_CALL(*this, VariantSelected(_, _)).Times(0);
  rm.UpdateNetworkSpeed(10285395);

  // Notify a network downgrade which would knock us down to a lower bitrate
  // video
  EXPECT_CALL(*this, VariantSelected("/video/6000kbit.m3u8",
                                     "/audio/surround/en/320kbit.m3u8"));
  rm.UpdateNetworkSpeed(10285300);

  // Notify a network upgrade, and go back up to the highest level.
  EXPECT_CALL(*this, VariantSelected("/video/10000kbit.m3u8",
                                     "/audio/surround/en/320kbit.m3u8"));
  rm.UpdateNetworkSpeed(10285395);

  // This network downgrade pushes us into the stereo variants, so a new audio
  // override rendition is selected as well.
  EXPECT_CALL(*this, VariantSelected("/video/800kbit.m3u8",
                                     "/audio/stereo/en/128kbit.m3u8"));
  rm.UpdateNetworkSpeed(831280);

  // Now lets check the available renditions for this selected variant. These
  // Should be in the same order as the manifest.
  const auto renditions = rm.GetSelectableRenditions();
  ASSERT_EQ(renditions.size(), 3u);
  ASSERT_EQ(renditions[0].label().value(), "English");
  ASSERT_EQ(renditions[1].label().value(), "Dubbing");
  ASSERT_EQ(renditions[2].label().value(), "German");

  // Select the dubbing rendition, and get a change.
  const auto dubbing_id = renditions[1].track_id();
  EXPECT_CALL(*this, VariantSelected("/video/800kbit.m3u8",
                                     "/audio/stereo/none/128kbit.m3u8"));
  rm.SetPreferredAudioRendition(dubbing_id);

  // Increase the network speed to full again. Because the user has selected
  // the dubbing track, we try to match the language.
  EXPECT_CALL(*this, VariantSelected("/video/10000kbit.m3u8",
                                     "/audio/surround/none/320kbit.m3u8"));
  rm.UpdateNetworkSpeed(10285395);

  // Drop the network speed again to ensure we stick to dubbing ways.
  EXPECT_CALL(*this, VariantSelected("/video/800kbit.m3u8",
                                     "/audio/stereo/none/128kbit.m3u8"));
  rm.UpdateNetworkSpeed(831280);

  // Select the german rendition, and get a change.
  const auto german_id = renditions[2].track_id();
  EXPECT_CALL(*this, VariantSelected("/video/800kbit.m3u8",
                                     "/audio/stereo/de/128kbit.m3u8"));
  rm.SetPreferredAudioRendition(german_id);

  // Increase the network speed to full again. Because the user has selected
  // the german track, but the surround sound has no german audio, we switch
  // back to whatever the default is.
  EXPECT_CALL(*this, VariantSelected("/video/10000kbit.m3u8",
                                     "/audio/surround/en/320kbit.m3u8"));
  rm.UpdateNetworkSpeed(10285395);

  // Finally, drop back down low network again, and ensure we switch back to
  // german.
  EXPECT_CALL(*this, VariantSelected("/video/800kbit.m3u8",
                                     "/audio/stereo/de/128kbit.m3u8"));
  rm.UpdateNetworkSpeed(831280);

  // Unselect a preferred rendition, which switches back to english.
  EXPECT_CALL(*this, VariantSelected("/video/800kbit.m3u8",
                                     "/audio/stereo/en/128kbit.m3u8"));
  rm.SetPreferredAudioRendition(std::nullopt);
}

TEST_F(HlsRenditionManagerTest, VariantNames) {
  {
    // Differentiated by resolution
    auto variants =
        GetRenditionManager(
            MakeVariantStr(1234, "1920x1080", std::nullopt), "playlist1.m3u8",
            MakeVariantStr(1234, "1366x768", std::nullopt), "playlist2.m3u8")
            .GetSelectableVariants();
    ASSERT_EQ(variants[0].label().value(), "1920x1080");
    ASSERT_EQ(variants[1].label().value(), "1366x768");
  }

  {
    // No differentiation
    auto variants =
        GetRenditionManager(
            MakeVariantStr(1234, "1920x1080", std::nullopt), "playlist1.m3u8",
            MakeVariantStr(1234, "1920x1080", std::nullopt), "playlist2.m3u8")
            .GetSelectableVariants();
    ASSERT_EQ(variants[0].label().value(), "Stream: 1");
    ASSERT_EQ(variants[1].label().value(), "Stream: 2");
  }

  {
    // Same resolution, differentiated by framerate
    auto variants =
        GetRenditionManager(
            MakeVariantStr(1234, "1920x1080", "24.00"), "playlist1.m3u8",
            MakeVariantStr(1234, "1920x1080", "60.00"), "playlist2.m3u8")
            .GetSelectableVariants();
    ASSERT_EQ(variants[0].label().value(), "24fps");
    ASSERT_EQ(variants[1].label().value(), "60fps");
  }

  {
    // No differentiation
    auto variants =
        GetRenditionManager(
            MakeVariantStr(1234, "1920x1080", "60.00"), "playlist1.m3u8",
            MakeVariantStr(1234, "1920x1080", "60.00"), "playlist2.m3u8")
            .GetSelectableVariants();
    ASSERT_EQ(variants[0].label().value(), "Stream: 1");
    ASSERT_EQ(variants[1].label().value(), "Stream: 2");
  }

  {
    // Only bandwidth differentiation
    auto variants =
        GetRenditionManager(
            MakeVariantStr(831270, "1920x1080", "60.00"), "playlist1.m3u8",
            MakeVariantStr(1144430, "1920x1080", "60.00"), "playlist2.m3u8")
            .GetSelectableVariants();
    ASSERT_EQ(variants[0].label().value(), "831 Kbps");
    ASSERT_EQ(variants[1].label().value(), "1 Mbps");
  }

  {
    // Differentiated by cross prodoct of resolution and bandwidth
    auto variants =
        GetRenditionManager(
            MakeVariantStr(831270, "1920x1080", "60.00"), "playlist1.m3u8",
            MakeVariantStr(1144430, "1920x1080", "24.00"), "playlist2.m3u8",
            MakeVariantStr(1234, "1366x768", "60.00"), "playlist3.m3u8",
            MakeVariantStr(67989, "1366x768", "24.00"), "playlist4.m3u8")
            .GetSelectableVariants();
    ASSERT_EQ(variants[0].label().value(), "1366x768 60fps");
    ASSERT_EQ(variants[1].label().value(), "1366x768 24fps");
    ASSERT_EQ(variants[2].label().value(), "1920x1080 60fps");
    ASSERT_EQ(variants[3].label().value(), "1920x1080 24fps");
  }
}

}  // namespace media::hls
