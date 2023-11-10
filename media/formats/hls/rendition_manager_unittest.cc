// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_manager.h"

#include "base/logging.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/test_helpers.h"
#include "media/formats/hls/multivariant_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

namespace {

RenditionManager::CodecSupportType GetCodecSupportType(
    base::StringPiece container,
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

}  // namespace

using testing::_;

class HlsRenditionManagerTest : public testing::Test {
 public:
  MOCK_METHOD(void, VariantSelected, (std::string, std::string), ());

  void _VariantSelected(const VariantStream* vs, const AudioRendition* ar) {
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
                               base::Unretained(this));
  }

  template <typename... Strings>
  RenditionManager GetRenditionManager(Strings... strings) {
    MultivariantPlaylistTestBuilder builder;
    builder.AppendLine("#EXTM3U");
    ([&] { builder.AppendLine(strings); }(), ...);
    return RenditionManager(builder.Parse(),
                            base::BindRepeating(GetVariantCb()),
                            base::BindRepeating(&GetCodecSupportType));
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
  const auto renditions = rm.GetSelectableAudioRenditions();
  ASSERT_EQ(renditions.size(), 3u);
  ASSERT_EQ(std::get<1>(renditions[0]), "English");
  ASSERT_EQ(std::get<1>(renditions[1]), "Dubbing");
  ASSERT_EQ(std::get<1>(renditions[2]), "German");

  // Select the dubbing rendition, and get a change.
  const auto dubbing_id = std::get<0>(renditions[1]);
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
  const auto german_id = std::get<0>(renditions[2]);
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
  rm.SetPreferredAudioRendition(absl::nullopt);
}

}  // namespace media::hls
