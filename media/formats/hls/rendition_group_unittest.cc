// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/multivariant_playlist_test_builder.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/quirks.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

template <typename... Strings>
scoped_refptr<MultivariantPlaylist> ParseMultivariantPlaylist(
    Strings... strings) {
  MultivariantPlaylistTestBuilder builder;
  ([&] { builder.AppendLine(strings); }(), ...);
  return builder.Parse();
}

RenditionGroup::RenditionTrack MakeRenditionTrack(Rendition* rendition,
                                                  int stream_id) {
  return std::make_tuple(
      MediaTrack::CreateAudioTrack(
          rendition->GetName(), MediaTrack::AudioKind::kMain,
          rendition->GetName(), rendition->GetLanguage().value(), true,
          stream_id, true),
      rendition);
}

TEST(RenditionGroupUnittest, MayNotAutoSelectIfNothingTagged) {
  const auto mvp = ParseMultivariantPlaylist(
      "#EXTM3U",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"G\",NAME=\"A\",URI=\"A.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"G\",NAME=\"B\",URI=\"B.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"G\",NAME=\"C\",URI=\"C.m3u8\"",
      "#EXT-X-STREAM-INF:BANDWIDTH=100,CODECS=\"audio.codec\",AUDIO=\"G\"",
      "100.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=200,CODECS=\"audio.codec\",AUDIO=\"G\"",
      "200.m3u8");
  ASSERT_EQ(mvp->GetVariants().size(), 2u);
  auto g = mvp->GetVariants()[0].GetAudioRenditionGroup();
  auto lookup = g->MostSimilar(std::nullopt);
  ASSERT_FALSE(lookup.has_value());
}

TEST(RenditionGroupUnittest, XStreamInfTag) {
  const auto mvp = ParseMultivariantPlaylist(
      "#EXTM3U",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_1\",LANGUAGE=\"es\",NAME="
      "\"Spanish\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_es.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_1\",LANGUAGE=\"en\",NAME="
      "\"English\",AUTOSELECT=YES,DEFAULT=YES,URI=\"audio_en.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_1\",LANGUAGE=\"fr\",NAME="
      "\"French\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_fr.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_1\",LANGUAGE=\"de\",NAME="
      "\"German\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_de.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_2\",LANGUAGE=\"en\",NAME="
      "\"English\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_en_480p.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_2\",LANGUAGE=\"es\",NAME="
      "\"Spanish\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_es_480p.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_2\",LANGUAGE=\"it\",NAME="
      "\"Italian\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_it_480p.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_3\",LANGUAGE=\"es\",NAME="
      "\"Spanish\",AUTOSELECT=NO,DEFAULT=NO,URI=\"audio_es.m3u8\"",
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_group_3\",LANGUAGE=\"en\",NAME="
      "\"English\",AUTOSELECT=YES,DEFAULT=NO,URI=\"audio_en.m3u8\"",
      "#EXT-X-STREAM-INF:BANDWIDTH=1500000,RESOLUTION=1280x720,AUDIO=\"audio_"
      "group_1\"",
      "variant1_720p.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=2500000,RESOLUTION=1920x1080,AUDIO=\"audio_"
      "group_2\"",
      "variant2_1080p.m3u8",
      "#EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=854x480,AUDIO=\"audio_"
      "group_3\"",
      "variant3_480p.m3u8");
  ASSERT_EQ(mvp->GetVariants().size(), 3u);

  auto group1 = mvp->GetVariants()[0].GetAudioRenditionGroup();
  auto group2 = mvp->GetVariants()[1].GetAudioRenditionGroup();
  auto group3 = mvp->GetVariants()[2].GetAudioRenditionGroup();

  // uses DEFAULT=YES rendition even if not first.
  {
    auto lookup = group1->MostSimilar(std::nullopt);
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "English");
    EXPECT_EQ(*lookup_track.language(), "en");
  }

  // uses first if no default.
  {
    auto lookup = group2->MostSimilar(std::nullopt);
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "English");
    EXPECT_EQ(*lookup_track.language(), "en");
  }

  // uses omits non-autoselectable
  {
    auto lookup = group3->MostSimilar(std::nullopt);
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "English");
    EXPECT_EQ(*lookup_track.language(), "en");
  }

  // find the exact track match, if it exists:
  {
    Rendition rendition = Rendition::CreateRenditionForTesting({
        .uri = GURL(""),
        .name = "French",
        .language = "fr",
        .associated_language = std::nullopt,
        .stable_rendition_id = std::nullopt,
        .channels = std::nullopt,
    });
    auto exact = MakeRenditionTrack(&rendition, 3);
    auto lookup = group1->MostSimilar(std::move(exact));
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "French");
    EXPECT_EQ(*lookup_track.language(), "fr");
  }

  // Something unmatchable
  {
    Rendition rendition = Rendition::CreateRenditionForTesting({
        .uri = GURL(""),
        .name = "Esperanto",
        .language = "ep",
        .associated_language = std::nullopt,
        .stable_rendition_id = std::nullopt,
        .channels = std::nullopt,
    });
    auto exact = MakeRenditionTrack(&rendition, 99);
    auto lookup = group1->MostSimilar(std::move(exact));
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "English");
    EXPECT_EQ(*lookup_track.language(), "en");
  }

  // Exists in group1, but not in group2
  {
    Rendition rendition = Rendition::CreateRenditionForTesting({
        .uri = GURL(""),
        .name = "French",
        .language = "fr",
        .associated_language = std::nullopt,
        .stable_rendition_id = std::nullopt,
        .channels = std::nullopt,
    });
    auto exact = MakeRenditionTrack(&rendition, 3);
    auto lookup = group2->MostSimilar(std::move(exact));
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "English");
    EXPECT_EQ(*lookup_track.language(), "en");
  }

  // use spanish from group2, it will match spanish from group1
  {
    Rendition rendition = Rendition::CreateRenditionForTesting({
        .uri = GURL(""),
        .name = "Spanish",
        .language = "es",
        .associated_language = std::nullopt,
        .stable_rendition_id = std::nullopt,
        .channels = std::nullopt,
    });
    auto exact = MakeRenditionTrack(&rendition, 6);
    auto lookup = group1->MostSimilar(std::move(exact));
    ASSERT_TRUE(lookup.has_value());
    auto lookup_track = std::get<0>(*lookup);
    EXPECT_EQ(*lookup_track.track_id(), "Spanish");
    EXPECT_EQ(*lookup_track.language(), "es");
    EXPECT_EQ(lookup_track.stream_id(), 1);
  }
}

}  // namespace media::hls
