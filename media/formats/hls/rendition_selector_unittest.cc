// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_selector.h"

#include "media/formats/hls/multivariant_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

namespace {

RenditionSelector::CodecSupportType GetCodecSupportType(
    base::StringPiece container,
    base::span<const std::string> codecs) {
  bool has_audio = false;
  bool has_video = false;
  for (const auto& codec : codecs) {
    if (codec == "audio.codec") {
      has_audio = true;
    } else if (codec == "video.codec") {
      has_video = true;
    } else if (codec == "av.codec") {
      return RenditionSelector::CodecSupportType::kSupportedAudioVideo;
    }
  }
  if (has_audio && has_video) {
    return RenditionSelector::CodecSupportType::kSupportedAudioVideo;
  } else if (has_audio) {
    return RenditionSelector::CodecSupportType::kSupportedAudioOnly;
  } else if (has_video) {
    return RenditionSelector::CodecSupportType::kSupportedVideoOnly;
  }
  return RenditionSelector::CodecSupportType::kUnsupported;
}

template <typename... Strings>
RenditionSelector GetRenditionSelector(Strings... strings) {
  MultivariantPlaylistTestBuilder builder;
  builder.AppendLine("#EXTM3U");
  ([&] { builder.AppendLine(strings); }(), ...);
  return RenditionSelector(builder.Parse(),
                           base::BindRepeating(&GetCodecSupportType));
}

void CheckSelections(const RenditionSelector& selector,
                     RenditionSelector::VideoPlaybackPreferences video_prefs,
                     RenditionSelector::AudioPlaybackPreferences audio_prefs,
                     absl::optional<std::string> selected,
                     absl::optional<std::string> audio_override) {
  auto variants = selector.GetPreferredVariants(video_prefs, audio_prefs);
  if (selected.has_value()) {
    ASSERT_NE(variants.selected_variant, nullptr);
    ASSERT_EQ(variants.selected_variant->GetPrimaryRenditionUri(),
              GURL(*selected));
  } else {
    ASSERT_EQ(variants.selected_variant, nullptr);
  }

  if (audio_override.has_value()) {
    ASSERT_NE(variants.audio_override_rendition, nullptr);
    ASSERT_NE(variants.audio_override_rendition->GetUri(), absl::nullopt);
    ASSERT_EQ(*variants.audio_override_rendition->GetUri(),
              GURL(*audio_override));
  } else {
    ASSERT_EQ(variants.audio_override_rendition, nullptr);
  }
}

}  // namespace

TEST(HlsRenditionSelectorTest, SimpleVideoStreamSelection) {
  auto rs = GetRenditionSelector(
      "#EXT-X-STREAM-INF:BANDWIDTH=1280000,AVERAGE-BANDWIDTH=1000000",
      "http://example.com/low.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=2560000,AVERAGE-BANDWIDTH=2000000",
      "http://example.com/mid.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=7680000,AVERAGE-BANDWIDTH=6000000",
      "http://example.com/high.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"audio.codec\"",
      "http://example.com/audio-only.m3u8");

  // With no preferences for bandwidth, resolution, or language, we pick the
  // highest bandwidth stream. Since that stream has no audio group, we don't
  // do any audio rendition override selection.
  CheckSelections(rs,
                  /*video_prefs=*/{absl::nullopt, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/high.m3u8", absl::nullopt);

  // If we have a bandwidth cap, filter out everything too high, and pick the
  // highest remaining variant. Again, there is no audio group, so there are no
  // audio override renditions.

  CheckSelections(rs,
                  /*video_prefs=*/{2600000, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/mid.m3u8", absl::nullopt);

  // If we have a bandwidth cap, filter out everything too high, and pick the
  // highest remaining variant. This time, there's no video stream under the
  // cap, but we have to pick _something_, so pick the lowest. Note that
  // although the audio codec is lower than our preference, it's got a codec
  // line which we don't detect as having audio present, so it can't be used
  // for video. And finally, we still have no audio group on the selected
  // variant, so there is no audio override.
  CheckSelections(rs,
                  /*video_prefs=*/{70000, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/low.m3u8", absl::nullopt);

  // If we have a bandwidth cap, filter out everything too high, and pick the
  // highest remaining variant. This time, there's no video stream OR audio
  // stream under the cap, but we have to pick _something_, so pick the lowest.
  CheckSelections(rs,
                  /*video_prefs=*/{4, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/low.m3u8", absl::nullopt);
}

TEST(HlsRenditionSelectorTest, SimpleAudioOnlyStreamSelection) {
  auto rs = GetRenditionSelector(
      "#EXT-X-STREAM-INF:BANDWIDTH=7680000,CODECS=\"audio.codec\"",
      "http://example.com/audio1.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=2560000,CODECS=\"audio.codec\"",
      "http://example.com/audio2.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=1280000,CODECS=\"audio.codec\"",
      "http://example.com/audio3.m3u8");

  // There are no variants that we've detected as having video, so we end up
  // with the fallback to selecting from a bunch of audio-only variants. There
  // aren't any renditions either, so this will count as a primary variant.
  CheckSelections(rs,
                  /*video_prefs=*/{absl::nullopt, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/audio1.m3u8", absl::nullopt);

  // Try the selection again, but this time only have one variant under the
  // bandwidth cap.
  CheckSelections(rs,
                  /*video_prefs=*/{1280001, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/audio3.m3u8", absl::nullopt);

  // Finally, try selecting where the bandwidth cap is too low to select any
  // variant by normal means, and default to the lowest.
  CheckSelections(rs,
                  /*video_prefs=*/{5, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://example.com/audio3.m3u8", absl::nullopt);
}

TEST(HlsRenditionSelectorTest, AlternativeAudioSelection) {
  auto rs = GetRenditionSelector(
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Eng\",DEFAULT=YES,"
      "AUTOSELECT=YES,LANGUAGE=\"en\",URI=\"eng-audio.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Ger\",DEFAULT=NO,"
      "AUTOSELECT=YES,LANGUAGE=\"en\",URI=\"ger-audio.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Com\",DEFAULT=NO,"
      "AUTOSELECT=NO,LANGUAGE=\"en\",URI=\"eng-comments.m3u8\"",
      "#EXT-X-STREAM-INF:BANDWIDTH=1280000,CODECS=\"video.codec\",AUDIO="
      "\"aac\"",
      "low/video-only.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=2560000,CODECS=\"video.codec\",AUDIO="
      "\"aac\"",
      "mid/video-only.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=7680000,CODECS=\"video.codec\",AUDIO="
      "\"aac\"",
      "hi/video-only.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"audio.codec\",AUDIO=\"aac\"",
      "main/english-audio.m3u8");

  // With no preferences for bandwidth, resolution, or language, we pick the
  // highest bandwidth stream. Since that stream has an audio group, we then
  // look at all the matching renditions and select the one tagged as DEFAULT.
  CheckSelections(rs,
                  /*video_prefs=*/{absl::nullopt, absl::nullopt},
                  /*audio_prefs=*/{absl::nullopt, absl::nullopt},
                  "http://localhost/hi/video-only.m3u8",
                  "http://localhost/eng-audio.m3u8");

  // With no preferences for bandwidth, resolution, or language, we pick the
  // highest bandwidth stream. Since that stream has an audio group, we then
  // look at all the matching renditions and see if anything matches our
  // language criteria. First, we try english, which lets us select the DEFAULT
  // one again, but then if we set it to prefer german, we will select german
  // audio.
  CheckSelections(rs,
                  /*video_prefs=*/{absl::nullopt, absl::nullopt},
                  /*audio_prefs=*/{"en", absl::nullopt},
                  "http://localhost/hi/video-only.m3u8",
                  "http://localhost/eng-audio.m3u8");
  CheckSelections(rs,
                  /*video_prefs=*/{absl::nullopt, absl::nullopt},
                  /*audio_prefs=*/{"de", absl::nullopt},
                  "http://localhost/hi/video-only.m3u8",
                  "http://localhost/eng-audio.m3u8");
}

}  // namespace media::hls
