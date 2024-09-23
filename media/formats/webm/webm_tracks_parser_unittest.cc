// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_tracks_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/channel_layout.h"
#include "media/base/mock_media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/webm/tracks_builder.h"
#include "media/formats/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

// WebM muxing commonly uses 1 millisecond resolution.
static const int64_t kOneMsInNs = 1000000;

class WebMTracksParserTest : public testing::Test {
 public:
  WebMTracksParserTest() = default;

 protected:
  StrictMock<MockMediaLog> media_log_;
};

TEST_F(WebMTracksParserTest, IgnoringTextTracks) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTextTrack(1, 1, kWebMCodecSubtitles, "Subtitles", "fre");
  tb.AddTextTrack(2, 2, kWebMCodecSubtitles, "Commentary", "fre");

  const std::vector<uint8_t> buf = tb.Finish();
  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;

  EXPECT_MEDIA_LOG(HasSubstr("Ignoring text track 1"));
  EXPECT_MEDIA_LOG(HasSubstr("Ignoring text track 2"));

  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_GT(result, 0);
  EXPECT_EQ(result, static_cast<int>(buf.size()));

  const std::set<int64_t>& ignored_tracks = parser->ignored_tracks();
  EXPECT_TRUE(ignored_tracks.find(1) != ignored_tracks.end());
  EXPECT_TRUE(ignored_tracks.find(2) != ignored_tracks.end());
}

TEST_F(WebMTracksParserTest, AudioVideoDefaultDurationUnset) {
  // Other audio/video decoder config fields are necessary in the test
  // audio/video TrackEntry configurations. This method does only very minimal
  // verification of their inclusion and parsing; the goal is to confirm
  // TrackEntry DefaultDuration defaults to -1 if not included in audio or
  // video TrackEntry.
  TracksBuilder tb;
  tb.AddAudioTrack(1, 1, "A_VORBIS", "audio", "", -1, 2, 8000);
  tb.AddVideoTrack(2, 2, "V_VP8", "video", "", -1, 320, 240);
  const std::vector<uint8_t> buf = tb.Finish();

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;
  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_LE(0, result);
  EXPECT_EQ(static_cast<int>(buf.size()), result);

  EXPECT_EQ(kNoTimestamp, parser->GetAudioDefaultDuration(kOneMsInNs));
  EXPECT_EQ(kNoTimestamp, parser->GetVideoDefaultDuration(kOneMsInNs));

  const VideoDecoderConfig& video_config = parser->video_decoder_config();
  EXPECT_TRUE(video_config.IsValidConfig());
  EXPECT_EQ(320, video_config.coded_size().width());
  EXPECT_EQ(240, video_config.coded_size().height());

  const AudioDecoderConfig& audio_config = parser->audio_decoder_config();
  EXPECT_TRUE(audio_config.IsValidConfig());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, audio_config.channel_layout());
  EXPECT_EQ(8000, audio_config.samples_per_second());
}

TEST_F(WebMTracksParserTest, AudioVideoDefaultDurationSet) {
  // Confirm audio or video TrackEntry DefaultDuration values are parsed, if
  // present.
  TracksBuilder tb;
  tb.AddAudioTrack(1, 1, "A_VORBIS", "audio", "", 12345678, 2, 8000);
  tb.AddVideoTrack(2, 2, "V_VP8", "video", "", 987654321, 320, 240);
  const std::vector<uint8_t> buf = tb.Finish();

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;
  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_LE(0, result);
  EXPECT_EQ(static_cast<int>(buf.size()), result);

  EXPECT_EQ(base::Microseconds(12000),
            parser->GetAudioDefaultDuration(kOneMsInNs));
  EXPECT_EQ(base::Microseconds(985000),
            parser->GetVideoDefaultDuration(5000000));  // 5 ms resolution
  EXPECT_EQ(kNoTimestamp, parser->GetAudioDefaultDuration(12346000));
  EXPECT_EQ(base::Microseconds(12345),
            parser->GetAudioDefaultDuration(12345000));
  EXPECT_EQ(base::Microseconds(12003),
            parser->GetAudioDefaultDuration(1000300));  // 1.0003 ms resolution
}

TEST_F(WebMTracksParserTest, InvalidZeroDefaultDurationSet) {
  // Confirm parse error if TrackEntry DefaultDuration is present, but is 0ns.
  TracksBuilder tb(true);
  tb.AddAudioTrack(1, 1, "A_VORBIS", "audio", "", 0, 2, 8000);
  const std::vector<uint8_t> buf = tb.Finish();

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;

  EXPECT_MEDIA_LOG(HasSubstr("Illegal 0ns audio TrackEntry DefaultDuration"));

  EXPECT_EQ(-1, parser->Parse(&buf[0], buf.size()));
}

TEST_F(WebMTracksParserTest, InvalidTracksCodecIdFormat) {
  // Inexhaustively confirms parse error if Tracks CodecID element value
  // contains a character outside of 0x01 - 0x7F.
  TracksBuilder tb(true);
  tb.AddAudioTrack(1, 1, "A_VORB\xA1S", "audio", "", -1, 2, 8000);
  const std::vector<uint8_t> buf = tb.Finish();

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;

  EXPECT_MEDIA_LOG(
      HasSubstr("Tracks CodecID element value must be an ASCII string"));

  EXPECT_EQ(-1, parser->Parse(&buf[0], buf.size()));
}

TEST_F(WebMTracksParserTest, InvalidTracksNameFormat) {
  // Inexhaustively confirms parse error if Tracks Name element value
  // contains a character outside of 0x01 - 0x7F.
  TracksBuilder tb(true);
  tb.AddAudioTrack(1, 1, "A_VORBIS", "aud\x80o", "", -1, 2, 8000);
  const std::vector<uint8_t> buf = tb.Finish();

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;

  EXPECT_MEDIA_LOG(
      HasSubstr("Tracks Name element value must be an ASCII string"));

  EXPECT_EQ(-1, parser->Parse(&buf[0], buf.size()));
}

TEST_F(WebMTracksParserTest, HighTrackUID) {
  // Confirm no parse error if TrackEntry TrackUID has MSb set
  // (http://crbug.com/397067).
  TracksBuilder tb(true);
  tb.AddAudioTrack(1, 1ULL << 31, "A_VORBIS", "audio", "", 40, 2, 8000);
  const std::vector<uint8_t> buf = tb.Finish();

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;
  EXPECT_GT(parser->Parse(&buf[0], buf.size()),0);
}

TEST_F(WebMTracksParserTest, PrecisionCapping) {
  struct CappingCases {
    int64_t scale_ns;
    int64_t duration_ns;
    base::TimeDelta expected_result;
  };

  const CappingCases kCappingCases[] = {
      {kOneMsInNs, -1, kNoTimestamp},
      {kOneMsInNs, 0, kNoTimestamp},
      {kOneMsInNs, 1, kNoTimestamp},
      {kOneMsInNs, 999999, kNoTimestamp},
      {kOneMsInNs, 1000000, base::Milliseconds(1)},
      {kOneMsInNs, 1000001, base::Milliseconds(1)},
      {kOneMsInNs, 1999999, base::Milliseconds(1)},
      {kOneMsInNs, 2000000, base::Milliseconds(2)},
      {1, -1, kNoTimestamp},
      {1, 0, kNoTimestamp},

      // Result < 1us, so kNoTimestamp
      {1, 1, kNoTimestamp},
      {1, 999, kNoTimestamp},

      {1, 1000, base::Microseconds(1)},
      {1, 1999, base::Microseconds(1)},
      {1, 2000, base::Microseconds(2)},

      {64, 1792, base::Microseconds(1)},
  };

  auto parser = std::make_unique<WebMTracksParser>(&media_log_);
  ;

  for (size_t i = 0; i < std::size(kCappingCases); ++i) {
    InSequence s;
    int64_t scale_ns = kCappingCases[i].scale_ns;
    int64_t duration_ns = kCappingCases[i].duration_ns;
    base::TimeDelta expected_result = kCappingCases[i].expected_result;

    EXPECT_EQ(parser->PrecisionCappedDefaultDuration(scale_ns, duration_ns),
              expected_result)
        << i << ": " << scale_ns << ", " << duration_ns;
  }
}

}  // namespace media
