// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <type_traits>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_box_writer.h"
#include "media/muxers/mp4_movie_box_writer.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/mp4_type_conversion.h"
#include "media/muxers/output_position_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr uint32_t kDuration1 = 12345u;
constexpr uint32_t kDuration2 = 12300u;
constexpr uint32_t kDefaultSampleSize = 1024u;
constexpr uint32_t kWidth = 1024u;
constexpr uint32_t kHeight = 780u;
constexpr uint32_t kVideoTimescale = 30000u;
constexpr uint32_t kAudioTimescale = 44100u;
constexpr uint32_t kVideoSampleFlags = 0x112u;
constexpr uint32_t kAudioSampleFlags = 0x113u;
constexpr uint16_t kAudioVolume = 0x0100;
constexpr char kVideoHandlerName[] = "VideoHandler";
constexpr char kAudioHandlerName[] = "SoundHandler";

uint64_t ConvertTo1904TimeInSeconds(base::Time time) {
  base::Time time1904;
  CHECK(base::Time::FromUTCString("1904-01-01 00:00:00 UTC", &time1904));
  uint64_t iso_time = (time - time1904).InSeconds();
  return iso_time;
}

}  // namespace
class Mp4MuxerBoxWriterTest : public testing::Test {
 public:
  Mp4MuxerBoxWriterTest() = default;

 protected:
  void CreateContext(std::unique_ptr<OutputPositionTracker> tracker) {
    context_ = std::make_unique<Mp4MuxerContext>(std::move(tracker));
  }

  Mp4MuxerContext* context() const { return context_.get(); }

  void Reset() { context_ = nullptr; }

  void CreateContext(std::vector<uint8_t>& written_data) {
    auto tracker = std::make_unique<OutputPositionTracker>(base::BindRepeating(
        [&](base::OnceClosure run_loop_quit, std::vector<uint8_t>* written_data,
            base::StringPiece mp4_data_string) {
          // Callback is called per box output.

          std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                    std::back_inserter(*written_data));
          std::move(run_loop_quit).Run();
        },
        run_loop_.QuitClosure(), &written_data));

    // Initialize.
    CreateContext(std::move(tracker));
  }

  void FlushAndWait(Mp4BoxWriter* box_writer) {
    // Flush at requested.
    box_writer->WriteAndFlush();

    // Wait for finishing flush of all boxes.
    run_loop_.Run();
  }

 private:
  base::test::TaskEnvironment task_environment;
  mp4::writable_boxes::Movie mp4_moov_box_;
  std::unique_ptr<Mp4MuxerContext> context_;
  base::RunLoop run_loop_;
};

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieAndHeader) {
  // Tests `moov/mvhd` box writer.
  base::RunLoop run_loop;

  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  mp4::writable_boxes::Movie mp4_moov_box;
  base::Time creation_time = base::Time::FromTimeT(0x1234567);
  base::Time modification_time = base::Time::FromTimeT(0x2345678);
  {
    mp4_moov_box.header.creation_time = creation_time;
    mp4_moov_box.header.modification_time = modification_time;
    mp4_moov_box.header.timescale = kVideoTimescale;
    mp4_moov_box.header.duration = base::Seconds(0);
    mp4_moov_box.header.next_track_id = 1u;
  }

  // Flush at requested.
  Mp4MovieBoxWriter box_writer(*context(), mp4_moov_box);
  FlushAndWait(&box_writer);

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      written_data.data(), written_data.size(), nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);
  EXPECT_EQ(mp4::FOURCC_MOOV, reader->type());
  EXPECT_TRUE(reader->ScanChildren());

  // `mvhd` test.
  mp4::MovieHeader mvhd_box;
  EXPECT_TRUE(reader->ReadChild(&mvhd_box));
  EXPECT_EQ(mvhd_box.version, 1);

  EXPECT_EQ(mvhd_box.creation_time, ConvertTo1904TimeInSeconds(creation_time));
  EXPECT_EQ(mvhd_box.modification_time,
            ConvertTo1904TimeInSeconds(modification_time));
  EXPECT_EQ(mvhd_box.timescale, kVideoTimescale);
  EXPECT_EQ(mvhd_box.duration, 0u);
  EXPECT_EQ(mvhd_box.next_track_id, 1u);

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieExtends) {
  // Tests `mvex/trex` box writer.
  base::RunLoop run_loop;

  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  mp4::writable_boxes::Movie mp4_moov_box;
  {
    mp4::writable_boxes::TrackExtends video_extends;
    video_extends.track_id = 1u;
    video_extends.default_sample_description_index = 1u;
    video_extends.default_sample_duration = base::Seconds(0);
    video_extends.default_sample_size = kDefaultSampleSize;
    video_extends.default_sample_flags = kVideoSampleFlags;
    mp4_moov_box.extends.track_extends.push_back(std::move(video_extends));
    context()->SetVideoIndex(0);

    mp4::writable_boxes::Track video_track = {};
    // Minimum value.
    mp4_moov_box.tracks.push_back(std::move(video_track));
  }

  {
    mp4::writable_boxes::TrackExtends audio_extends;
    audio_extends.track_id = 2u;
    audio_extends.default_sample_description_index = 1u;
    audio_extends.default_sample_duration = base::Seconds(0);
    audio_extends.default_sample_size = kDefaultSampleSize;
    audio_extends.default_sample_flags = kAudioSampleFlags;
    mp4_moov_box.extends.track_extends.push_back(std::move(audio_extends));
    context()->SetAudioIndex(1);

    mp4::writable_boxes::Track audio_track = {};

    // Minimum value.
    mp4_moov_box.tracks.push_back(std::move(audio_track));
  }

  // Flush at requested.
  Mp4MovieBoxWriter box_writer(*context(), mp4_moov_box);
  FlushAndWait(&box_writer);

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      written_data.data(), written_data.size(), nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);
  EXPECT_EQ(mp4::FOURCC_MOOV, reader->type());
  EXPECT_TRUE(reader->ScanChildren());

  // `mvex` test.
  mp4::MovieExtends mvex_box;
  EXPECT_TRUE(reader->ReadChild(&mvex_box));

  // mp4::MovieExtends mvex_box = mvex_boxes[0];
  EXPECT_EQ(mvex_box.tracks.size(), 2u);

  EXPECT_EQ(mvex_box.tracks[0].track_id, 1u);
  EXPECT_EQ(mvex_box.tracks[0].default_sample_description_index, 1u);
  EXPECT_EQ(mvex_box.tracks[0].default_sample_duration, 0u);
  EXPECT_EQ(mvex_box.tracks[0].default_sample_size, kDefaultSampleSize);
  EXPECT_EQ(mvex_box.tracks[0].default_sample_flags, kVideoSampleFlags);

  EXPECT_EQ(mvex_box.tracks[1].track_id, 2u);
  EXPECT_EQ(mvex_box.tracks[1].default_sample_description_index, 1u);
  EXPECT_EQ(mvex_box.tracks[1].default_sample_duration, 0u);
  EXPECT_EQ(mvex_box.tracks[1].default_sample_size, kDefaultSampleSize);
  EXPECT_EQ(mvex_box.tracks[1].default_sample_flags, kAudioSampleFlags);

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieTrackAndMediaHeader) {
  // Tests `tkhd/mdhd` box writer.
  base::RunLoop run_loop;

  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  constexpr size_t kVideoIndex = 0;
  constexpr size_t kAudioIndex = 1;

  mp4::writable_boxes::Movie mp4_moov_box;
  base::Time creation_time = base::Time::FromTimeT(0x1234567);
  base::Time modification_time = base::Time::FromTimeT(0x2345678);
  {
    mp4::writable_boxes::TrackExtends video_extends;
    mp4_moov_box.extends.track_extends.push_back(std::move(video_extends));

    mp4::writable_boxes::Track video_track = {};
    using T = std::underlying_type_t<mp4::writable_boxes::TrackHeaderFlags>;
    video_track.header.flags =
        (static_cast<T>(mp4::writable_boxes::TrackHeaderFlags::kTrackEnabled) |
         static_cast<T>(mp4::writable_boxes::TrackHeaderFlags::kTrackInMovie));
    video_track.header.track_id = 1u;
    video_track.header.creation_time = creation_time;
    video_track.header.modification_time = modification_time;
    video_track.header.duration = base::Seconds(kDuration1);
    video_track.header.is_audio = false;
    video_track.header.natural_size = gfx::Size(kWidth, kHeight);

    video_track.media.header.creation_time = creation_time;
    video_track.media.header.modification_time = modification_time;
    video_track.media.header.duration = base::Seconds(kDuration1);
    video_track.media.header.timescale = kVideoTimescale;
    video_track.media.header.language = "und";

    video_track.media.handler.handler_type = mp4::FOURCC_VIDE;
    video_track.media.handler.name = kVideoHandlerName;

    mp4_moov_box.tracks.push_back(std::move(video_track));
    context()->SetVideoIndex(kVideoIndex);
  }

  {
    mp4::writable_boxes::TrackExtends audio_extends;
    mp4_moov_box.extends.track_extends.push_back(std::move(audio_extends));

    mp4::writable_boxes::Track audio_track = {};
    audio_track.header.track_id = 2u;
    audio_track.header.creation_time = creation_time;
    audio_track.header.modification_time = modification_time;
    audio_track.header.duration = base::Seconds(kDuration2);
    audio_track.header.is_audio = true;
    audio_track.header.natural_size = gfx::Size(0, 0);

    audio_track.media.header.creation_time = creation_time;
    audio_track.media.header.modification_time = modification_time;
    audio_track.media.header.duration = base::Seconds(kDuration2);
    audio_track.media.header.timescale = kAudioTimescale;
    audio_track.media.header.language = "";

    audio_track.media.handler.handler_type = mp4::FOURCC_SOUN;
    audio_track.media.handler.name = kAudioHandlerName;

    mp4_moov_box.tracks.push_back(std::move(audio_track));
    context()->SetAudioIndex(kAudioIndex);
  }

  // Flush at requested.
  Mp4MovieBoxWriter box_writer(*context(), mp4_moov_box);
  FlushAndWait(&box_writer);

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      written_data.data(), written_data.size(), nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);
  EXPECT_EQ(mp4::FOURCC_MOOV, reader->type());
  EXPECT_TRUE(reader->ScanChildren());

  // Track test.
  std::vector<mp4::Track> track_boxes;
  EXPECT_TRUE(reader->ReadChildren(&track_boxes));

  // mp4::MovieExtends mvex_box = mvex_boxes[0];
  EXPECT_EQ(track_boxes.size(), 2u);

  // Track header validation.

  EXPECT_EQ(track_boxes[kVideoIndex].header.track_id, 1u);
  EXPECT_EQ(track_boxes[kVideoIndex].header.creation_time,
            ConvertTo1904TimeInSeconds(creation_time));
  EXPECT_EQ(track_boxes[kVideoIndex].header.modification_time,
            ConvertTo1904TimeInSeconds(modification_time));
  EXPECT_EQ(track_boxes[kVideoIndex].header.duration, kDuration1);
  EXPECT_EQ(track_boxes[kVideoIndex].header.volume, 0);
  EXPECT_EQ(track_boxes[kVideoIndex].header.width, kWidth);
  EXPECT_EQ(track_boxes[kVideoIndex].header.height, kHeight);

  EXPECT_EQ(track_boxes[kAudioIndex].header.track_id, 2u);
  EXPECT_EQ(track_boxes[kAudioIndex].header.creation_time,
            ConvertTo1904TimeInSeconds(creation_time));
  EXPECT_EQ(track_boxes[kAudioIndex].header.modification_time,
            ConvertTo1904TimeInSeconds(modification_time));
  EXPECT_EQ(track_boxes[kAudioIndex].header.duration, kDuration2);
  EXPECT_EQ(track_boxes[kAudioIndex].header.volume, kAudioVolume);
  EXPECT_EQ(track_boxes[kAudioIndex].header.width, 0u);
  EXPECT_EQ(track_boxes[kAudioIndex].header.height, 0u);

  // Media Header validation.
  EXPECT_EQ(track_boxes[kAudioIndex].media.header.creation_time,
            ConvertTo1904TimeInSeconds(creation_time));
  EXPECT_EQ(track_boxes[kAudioIndex].media.header.modification_time,
            ConvertTo1904TimeInSeconds(modification_time));
  EXPECT_EQ(track_boxes[kAudioIndex].media.header.duration, kDuration2);
  EXPECT_EQ(track_boxes[kAudioIndex].media.header.timescale, kAudioTimescale);
  EXPECT_EQ(track_boxes[kAudioIndex].media.header.language_code,
            kUndefinedLanguageCode);

  EXPECT_EQ(track_boxes[kVideoIndex].media.header.creation_time,
            ConvertTo1904TimeInSeconds(creation_time));
  EXPECT_EQ(track_boxes[kVideoIndex].media.header.modification_time,
            ConvertTo1904TimeInSeconds(modification_time));
  EXPECT_EQ(track_boxes[kVideoIndex].media.header.duration, kDuration1);
  EXPECT_EQ(track_boxes[kVideoIndex].media.header.timescale, kVideoTimescale);
  EXPECT_EQ(track_boxes[kVideoIndex].media.header.language_code,
            kUndefinedLanguageCode);

  // Media Handler validation.
  EXPECT_EQ(track_boxes[kVideoIndex].media.handler.type,
            mp4::TrackType::kVideo);
  EXPECT_EQ(track_boxes[kVideoIndex].media.handler.name, kVideoHandlerName);

  EXPECT_EQ(track_boxes[kAudioIndex].media.handler.type,
            mp4::TrackType::kAudio);
  EXPECT_EQ(track_boxes[kAudioIndex].media.handler.name, kAudioHandlerName);

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}
}  // namespace media
