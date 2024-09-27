// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/formats/mp2t/es_parser_adts.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/formats/mpeg/adts_stream_parser.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_box_writer.h"
#include "media/muxers/mp4_fragment_box_writer.h"
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
constexpr uint32_t kTotalSizeLength = 4u;
constexpr uint32_t kFlagsAndVersionLength = 4u;
constexpr uint32_t kEntryCountLength = 4u;
constexpr uint32_t kSampleSizeAndCount = 8u;
constexpr size_t kVideoIndex = 0;
constexpr size_t kAudioIndex = 1;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
constexpr uint8_t kProfileIndicationNoChroma = 77;
constexpr uint8_t kProfileIndication = 122;
constexpr uint8_t kProfileCompatibility = 100;
constexpr uint8_t kLevelIndication = 64;
constexpr uint8_t kNALUUnitLength = 4;
constexpr uint8_t kSPS[] = {0x67, 0x64, 0x00, 0x0C, 0xAC, 0xD9, 0x41,
                            0x41, 0xFB, 0x01, 0x10, 0x00, 0x00, 0x03,
                            0x00, 0x10, 0x00, 0x00, 0x00, 0x03, 0x01,
                            0xE0, 0xF1, 0x42, 0x99, 0x60};
constexpr uint8_t kPPS[] = {0x68, 0xEE, 0xE3, 0xCB, 0x22, 0xC0};
constexpr uint8_t kChromaFormat = 0x3;
constexpr uint8_t kLumaMinus8 = 0x4;
constexpr uint8_t kChromaMinus8 = 0x4;
#endif

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
            base::span<const uint8_t> mp4_data_string) {
          // Callback is called per box output.

          base::ranges::copy(mp4_data_string,
                             std::back_inserter(*written_data));
          std::move(run_loop_quit).Run();
        },
        run_loop_.QuitClosure(), &written_data));

    // Initialize.
    CreateContext(std::move(tracker));
  }

  void AddTrackWithSampleDescriptions(mp4::writable_boxes::Movie& movie_box) {
    context_->SetVideoTrack({kVideoIndex, kVideoTimescale});
    context_->SetAudioTrack({kAudioIndex, kAudioTimescale});

    mp4::writable_boxes::Track video_track(kVideoIndex + 1, false);
    movie_box.tracks.push_back(std::move(video_track));
    mp4::writable_boxes::Track audio_track(kAudioIndex + 1, true);
    movie_box.tracks.push_back(std::move(audio_track));
    AddMediaInformations(movie_box);
  }

  void AddMediaInformations(mp4::writable_boxes::Movie& movie_box) {
    AddVideoMediaInformation(movie_box.tracks[kVideoIndex].media.information);
    AddAudioMediaInformation(movie_box.tracks[kAudioIndex].media.information);
  }

  void AddVideoMediaInformation(
      mp4::writable_boxes::MediaInformation& media_information) {
    AddVideoSampleTable(media_information.sample_table);
  }

  void AddAudioMediaInformation(
      mp4::writable_boxes::MediaInformation& media_information) {
    AddAudioSampleTable(media_information.sample_table);
  }

  void AddVideoSampleTable(mp4::writable_boxes::SampleTable& sample_table) {
    mp4::writable_boxes::SampleDescription video_sample_description;
    mp4::writable_boxes::VisualSampleEntry visual_sample_entry(
        VideoCodec::kVP9);
    visual_sample_entry.coded_size = gfx::Size(kWidth, kHeight);
    visual_sample_entry.compressor_name = "VPC Coding";
    mp4::writable_boxes::VPCodecConfiguration vp_config(
        /*profile*/ VP9PROFILE_PROFILE0, /*level*/ 0,
        /*color_space */ gfx::ColorSpace());
    visual_sample_entry.vp_decoder_configuration = std::move(vp_config);
    video_sample_description.video_sample_entry =
        std::move(visual_sample_entry);
    sample_table.sample_description = std::move(video_sample_description);
  }

  void AddAudioSampleTable(mp4::writable_boxes::SampleTable& sample_table) {
    mp4::writable_boxes::SampleDescription audio_sample_description;
    constexpr uint32_t kSampleRate = 48000u;
    mp4::writable_boxes::AudioSampleEntry audio_sample_entry(AudioCodec::kOpus,
                                                             kSampleRate, 2u);
    mp4::writable_boxes::OpusSpecificBox opus_specific_box;
    opus_specific_box.channel_count = 2u;
    opus_specific_box.sample_rate = 48000u;
    audio_sample_entry.opus_specific_box = std::move(opus_specific_box);
    audio_sample_description.audio_sample_entry = std::move(audio_sample_entry);
    sample_table.sample_description = std::move(audio_sample_description);
  }

  size_t FlushAndWait(Mp4BoxWriter* box_writer) {
    // Flush at requested.
    size_t written_size = box_writer->WriteAndFlush();

    // Wait for finishing flush of all boxes.
    run_loop_.Run();

    return written_size;
  }

  size_t FlushWithBoxWriterAndWait(Mp4BoxWriter* box_writer,
                                   BoxByteStream& box_byte_stream) {
    // Flush at requested.
    size_t written_size = box_writer->WriteAndFlush(box_byte_stream);

    // Wait for finishing flush of all boxes.
    run_loop_.Run();
    return written_size;
  }

  std::unique_ptr<Mp4MuxerContext> context_;

 private:
  base::test::TaskEnvironment task_environment;
  mp4::writable_boxes::Movie mp4_moov_box_;
  base::RunLoop run_loop_;
};

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieAndHeader) {
  // Tests `moov/mvhd` box writer.
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
    mp4_moov_box.header.duration = base::Milliseconds(0);
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
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::Movie mp4_moov_box;
  AddTrackWithSampleDescriptions(mp4_moov_box);

  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  {
    mp4::writable_boxes::TrackExtends video_extends;
    video_extends.track_id = 1u;
    video_extends.default_sample_description_index = 1u;
    video_extends.default_sample_duration = base::Milliseconds(0);
    video_extends.default_sample_size = kDefaultSampleSize;
    video_extends.default_sample_flags = kVideoSampleFlags;
    mp4_moov_box.extends.track_extends.push_back(std::move(video_extends));
  }

  {
    mp4::writable_boxes::TrackExtends audio_extends;
    audio_extends.track_id = 2u;
    audio_extends.default_sample_description_index = 1u;
    audio_extends.default_sample_duration = base::Milliseconds(0);
    audio_extends.default_sample_size = kDefaultSampleSize;
    audio_extends.default_sample_flags = kAudioSampleFlags;
    mp4_moov_box.extends.track_extends.push_back(std::move(audio_extends));
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
  std::vector<uint8_t> written_data;
  CreateContext(written_data);
  context_->SetVideoTrack({kVideoIndex, kVideoTimescale});
  context_->SetAudioTrack({kAudioIndex, kAudioTimescale});

  mp4::writable_boxes::Movie mp4_moov_box;
  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  base::Time creation_time = base::Time::FromTimeT(0x1234567);
  base::Time modification_time = base::Time::FromTimeT(0x2345678);
  {
    mp4::writable_boxes::TrackExtends video_extends;
    mp4_moov_box.extends.track_extends.push_back(std::move(video_extends));

    mp4::writable_boxes::Track video_track(kVideoIndex + 1, false);
    using T = std::underlying_type_t<mp4::writable_boxes::TrackHeaderFlags>;
    video_track.header.flags =
        (static_cast<T>(mp4::writable_boxes::TrackHeaderFlags::kTrackEnabled) |
         static_cast<T>(mp4::writable_boxes::TrackHeaderFlags::kTrackInMovie));
    video_track.header.creation_time = creation_time;
    video_track.header.modification_time = modification_time;
    video_track.header.duration = base::Milliseconds(kDuration1);
    video_track.header.natural_size = gfx::Size(kWidth, kHeight);

    video_track.media.header.creation_time = creation_time;
    video_track.media.header.modification_time = modification_time;
    video_track.media.header.duration = base::Milliseconds(kDuration1);
    video_track.media.header.timescale = kVideoTimescale;
    video_track.media.header.language = "und";
    video_track.media.handler.name = kVideoHandlerName;

    mp4_moov_box.tracks.push_back(std::move(video_track));
  }

  {
    mp4::writable_boxes::TrackExtends audio_extends;
    mp4_moov_box.extends.track_extends.push_back(std::move(audio_extends));

    mp4::writable_boxes::Track audio_track(kAudioIndex + 1, true);
    audio_track.header.creation_time = creation_time;
    audio_track.header.modification_time = modification_time;
    audio_track.header.duration = base::Milliseconds(kDuration2);
    audio_track.header.natural_size = gfx::Size(0, 0);

    audio_track.media.header.creation_time = creation_time;
    audio_track.media.header.modification_time = modification_time;
    audio_track.media.header.duration = base::Milliseconds(kDuration2);
    audio_track.media.header.timescale = kAudioTimescale;
    audio_track.media.header.language = "";
    audio_track.media.handler.name = kAudioHandlerName;

    mp4_moov_box.tracks.push_back(std::move(audio_track));
  }

  // Add `MediaInformation` under `media`.
  AddMediaInformations(mp4_moov_box);

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

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieMediaDataInformation) {
  // Tests `tkhd/mdhd` box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  const std::string kUrl = "";
  mp4::writable_boxes::DataUrlEntry entry;
  mp4::writable_boxes::MediaInformation media_information;
  media_information.video_header = mp4::writable_boxes::VideoMediaHeader();
  media_information.data_information.data_reference.entries.push_back(
      std::move(entry));

  AddVideoMediaInformation(media_information);

  // Flush at requested.
  Mp4MovieMediaInformationBoxWriter box_writer(*context(), media_information);
  FlushAndWait(&box_writer);

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));
  // `minf`.
  uint32_t fourcc;
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_MINF, static_cast<mp4::FourCC>(fourcc));

  // `vmhd`
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_VMHD, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));

  uint16_t value16;

  // graphics_mode.
  EXPECT_TRUE(box_reader->Read2(&value16));
  EXPECT_EQ(0, value16);
  // op_color
  EXPECT_TRUE(box_reader->Read2(&value16));
  EXPECT_EQ(0, value16);
  EXPECT_TRUE(box_reader->Read2(&value16));
  EXPECT_EQ(0, value16);
  EXPECT_TRUE(box_reader->Read2(&value16));
  EXPECT_EQ(0, value16);

  // `dinf`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_DINF, static_cast<mp4::FourCC>(fourcc));

  // `dref`
  uint32_t value;
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_DREF, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));
  EXPECT_TRUE(box_reader->Read4(&value));
  EXPECT_EQ(1u, value);  // entry_count.

  // `url`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_URL, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));

  std::vector<uint8_t> value_bytes;
  EXPECT_TRUE(box_reader->ReadVec(&value_bytes, kUrl.size()));
  std::string location = std::string(value_bytes.begin(), value_bytes.end());
  EXPECT_EQ(kUrl, location);

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieMediaMultipleSampleBoxes) {
  // Tests `dinf` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::SampleTable sample_table;
  AddVideoSampleTable(sample_table);

  Mp4MovieSampleTableBoxWriter box_writer(*context(), sample_table);
  FlushAndWait(&box_writer);

  // MediaInformation will have multiple sample boxes even though they
  // not added exclusively.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));

  // `stbl`.
  uint32_t fourcc;
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_STBL, static_cast<mp4::FourCC>(fourcc));

  // `stsc`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_STSC, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));
  EXPECT_TRUE(box_reader->SkipBytes(kEntryCountLength));

  // `stts`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_STTS, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));
  EXPECT_TRUE(box_reader->SkipBytes(kEntryCountLength));

  // `stsz`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_STSZ, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));
  EXPECT_TRUE(box_reader->SkipBytes(kSampleSizeAndCount));

  // `stco`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_STCO, static_cast<mp4::FourCC>(fourcc));
  EXPECT_TRUE(box_reader->SkipBytes(kFlagsAndVersionLength));
  EXPECT_TRUE(box_reader->SkipBytes(kEntryCountLength));

  // `stsd`.
  EXPECT_TRUE(box_reader->SkipBytes(kTotalSizeLength));
  EXPECT_TRUE(box_reader->Read4(&fourcc));
  EXPECT_EQ(mp4::FOURCC_STSD, static_cast<mp4::FourCC>(fourcc));
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieVisualSampleEntry) {
  // Tests `avc1` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::SampleDescription sample_description;

  mp4::writable_boxes::VisualSampleEntry visual_sample_entry(VideoCodec::kH264);
  visual_sample_entry.coded_size = gfx::Size(kWidth, kHeight);
  visual_sample_entry.compressor_name = "Chromium AVC Coding";

  mp4::writable_boxes::AVCDecoderConfiguration avc = {};
  avc.avc_config_record.version = 1;
  avc.avc_config_record.profile_indication = kProfileIndicationNoChroma;
  avc.avc_config_record.profile_compatibility = kProfileCompatibility;
  avc.avc_config_record.avc_level = kLevelIndication;
  avc.avc_config_record.length_size = kNALUUnitLength;

  std::vector<uint8_t> sps(std::begin(kSPS), std::end(kSPS));
  avc.avc_config_record.sps_list.emplace_back(sps);

  std::vector<uint8_t> pps(std::begin(kPPS), std::end(kPPS));
  avc.avc_config_record.pps_list.emplace_back(pps);

  visual_sample_entry.avc_decoder_configuration = std::move(avc);

  sample_description.video_sample_entry = std::move(visual_sample_entry);

  Mp4MovieSampleDescriptionBoxWriter box_writer(*context(), sample_description);
  FlushAndWait(&box_writer);

  // MediaInformation will have multiple sample boxes even though they
  // not added exclusively.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));

  EXPECT_TRUE(box_reader->ScanChildren());

  mp4::SampleDescription reader_sample_description;
  reader_sample_description.type = mp4::kVideo;

  EXPECT_TRUE(box_reader->ReadChild(&reader_sample_description));
  EXPECT_EQ(1u, reader_sample_description.video_entries.size());

  const auto& video_sample_entry = reader_sample_description.video_entries[0];
  EXPECT_TRUE(video_sample_entry.IsFormatValid());
  EXPECT_EQ(1, video_sample_entry.data_reference_index);
  EXPECT_EQ(static_cast<uint16_t>(kWidth), video_sample_entry.width);
  EXPECT_EQ(static_cast<uint16_t>(kHeight), video_sample_entry.height);
  EXPECT_EQ(VideoCodecProfile::H264PROFILE_MAIN,
            video_sample_entry.video_info.profile);
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieAVCDecoderConfigurationRecord) {
  // Tests `avc1` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::AVCDecoderConfiguration avc = {};
  avc.avc_config_record.version = 1;
  avc.avc_config_record.profile_indication = kProfileIndication;
  avc.avc_config_record.profile_compatibility = kProfileCompatibility;
  avc.avc_config_record.avc_level = kLevelIndication;
  avc.avc_config_record.length_size = kNALUUnitLength;

  std::vector<uint8_t> sps(std::begin(kSPS), std::end(kSPS));
  avc.avc_config_record.sps_list.emplace_back(sps);

  std::vector<uint8_t> pps(std::begin(kPPS), std::end(kPPS));
  avc.avc_config_record.pps_list.emplace_back(pps);

  avc.avc_config_record.chroma_format = kChromaFormat;
  avc.avc_config_record.bit_depth_luma_minus8 = kLumaMinus8;
  avc.avc_config_record.bit_depth_chroma_minus8 = kChromaMinus8;

  Mp4MovieAVCDecoderConfigurationBoxWriter box_writer(*context(), avc);
  FlushAndWait(&box_writer);

  // MediaInformation will have multiple sample boxes even though they
  // not added exclusively.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));

  EXPECT_TRUE(box_reader->ScanChildren());

  mp4::AVCDecoderConfigurationRecord avc_config_reader;
  EXPECT_TRUE(box_reader->ReadChild(&avc_config_reader));

  EXPECT_EQ(kProfileIndication, avc_config_reader.profile_indication);
  EXPECT_EQ(kProfileCompatibility, avc_config_reader.profile_compatibility);
  EXPECT_EQ(kLevelIndication, avc_config_reader.avc_level);
  EXPECT_EQ(kNALUUnitLength, avc_config_reader.length_size);

  EXPECT_EQ(1u, avc_config_reader.sps_list.size());
  EXPECT_EQ(1u, avc_config_reader.pps_list.size());
  std::vector<uint8_t> sps1 = avc_config_reader.sps_list[0];
  std::vector<uint8_t> pps1 = avc_config_reader.pps_list[0];
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kSPS), std::end(kSPS)), sps1);
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kPPS), std::end(kPPS)), pps1);

  EXPECT_EQ((kChromaFormat & 0x3), avc_config_reader.chroma_format);
  EXPECT_EQ((kLumaMinus8 & 0x7), avc_config_reader.bit_depth_luma_minus8);
  EXPECT_EQ((kChromaMinus8 & 0x7), avc_config_reader.bit_depth_chroma_minus8);
  EXPECT_EQ(0u, avc_config_reader.sps_ext_list.size());
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4AacAudioSampleEntry) {
  // Tests `aac` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::SampleDescription sample_description;

  constexpr uint32_t kSampleRate = 48000u;
  mp4::writable_boxes::AudioSampleEntry audio_sample_entry(AudioCodec::kAAC,
                                                           kSampleRate, 2u);

  mp4::writable_boxes::ElementaryStreamDescriptor esds;
  constexpr uint32_t kBitRate = 341000u;
  constexpr int32_t kSampleFrequency = 48000;

  esds.aac_codec_description.push_back(0x11);
  esds.aac_codec_description.push_back(0x90);
  audio_sample_entry.elementary_stream_descriptor = std::move(esds);

  mp4::writable_boxes::BitRate bit_rate;
  bit_rate.max_bit_rate = kBitRate;
  bit_rate.avg_bit_rate = kBitRate;
  audio_sample_entry.bit_rate = std::move(bit_rate);

  sample_description.audio_sample_entry = std::move(audio_sample_entry);

  Mp4MovieSampleDescriptionBoxWriter box_writer(*context(), sample_description);
  FlushAndWait(&box_writer);

  // MediaInformation will have multiple sample boxes even though they
  // not added exclusively.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));

  EXPECT_TRUE(box_reader->ScanChildren());

  mp4::SampleDescription reader_sample_description;
  reader_sample_description.type = mp4::kAudio;

  EXPECT_TRUE(box_reader->ReadChild(&reader_sample_description));
  EXPECT_EQ(1u, reader_sample_description.audio_entries.size());

  const auto& audio_sample = reader_sample_description.audio_entries[0];
  EXPECT_EQ(1, audio_sample.data_reference_index);
  EXPECT_EQ(2, audio_sample.channelcount);
  EXPECT_EQ(16, audio_sample.samplesize);
  EXPECT_EQ(kSampleRate, audio_sample.samplerate);

  const mp4::ElementaryStreamDescriptor& esds_reader = audio_sample.esds;
  EXPECT_EQ(mp4::kISO_14496_3, esds_reader.object_type);

  const mp4::AAC& aac = esds_reader.aac;

  AudioCodecProfile profile = aac.GetProfile();
  EXPECT_EQ(AudioCodecProfile::kUnknown, profile);

  int aac_frequency = aac.GetOutputSamplesPerSecond(false);
  EXPECT_EQ(kSampleFrequency, aac_frequency);

  ChannelLayout channel_layout = aac.GetChannelLayout(false);
  EXPECT_EQ(media::CHANNEL_LAYOUT_STEREO, channel_layout);

  int adts_header_size;
  auto buffer = aac.CreateAdtsFromEsds({}, &adts_header_size);
  EXPECT_FALSE(buffer.empty());

  ADTSStreamParser adts_parser;

  int frame_size = 0, sample_rate = 0, sample_count = 0;
  ChannelLayout adts_channel_layout;
  bool metadata_frame;
  EXPECT_NE(adts_parser.ParseFrameHeader(
                buffer.data(), adts_header_size, &frame_size, &sample_rate,
                &adts_channel_layout, &sample_count, &metadata_frame, nullptr),
            -1);
  EXPECT_EQ(adts_header_size, frame_size);
  EXPECT_EQ(kSampleFrequency, sample_rate);
  EXPECT_EQ(media::CHANNEL_LAYOUT_STEREO, adts_channel_layout);
  EXPECT_EQ(1024, sample_count);
  EXPECT_FALSE(metadata_frame);
}
#endif

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieVPConfigurationRecord) {
  // Tests `vpcC` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::VPCodecConfiguration vp_config(
      VP9PROFILE_MIN, 0u,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT470M,
                      gfx::ColorSpace::TransferID::GAMMA28,
                      gfx::ColorSpace::MatrixID::BT470BG,
                      gfx::ColorSpace::RangeID::FULL));

  Mp4MovieVPCodecConfigurationBoxWriter box_writer(*context(),
                                                   std::move(vp_config));
  FlushAndWait(&box_writer);

  // MediaInformation will have multiple sample boxes even though they
  // not added exclusively.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));

  EXPECT_TRUE(box_reader->ScanChildren());

  mp4::VPCodecConfigurationRecord vp_config_record;
  EXPECT_TRUE(box_reader->ReadChild(&vp_config_record));

  EXPECT_EQ(VP9PROFILE_MIN, vp_config_record.profile);
  EXPECT_EQ(0u, vp_config_record.level);

  EXPECT_EQ(gfx::ColorSpace::RangeID::FULL, vp_config_record.color_space.range);
  EXPECT_EQ(VideoColorSpace::PrimaryID::BT470M,
            vp_config_record.color_space.primaries);
  EXPECT_EQ(VideoColorSpace::TransferID::GAMMA28,
            vp_config_record.color_space.transfer);
  EXPECT_EQ(VideoColorSpace::MatrixID::BT470BG,
            vp_config_record.color_space.matrix);
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4OpusAudioSampleEntry) {
  // Tests `opus` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::SampleDescription sample_description;

  constexpr uint32_t kSampleRate = 48000u;
  mp4::writable_boxes::AudioSampleEntry audio_sample_entry(AudioCodec::kOpus,
                                                           kSampleRate, 2u);

  mp4::writable_boxes::OpusSpecificBox opus_specific_box;
  opus_specific_box.channel_count = 2u;
  opus_specific_box.sample_rate = 48000u;
  audio_sample_entry.opus_specific_box = std::move(opus_specific_box);

  sample_description.audio_sample_entry = std::move(audio_sample_entry);

  Mp4MovieSampleDescriptionBoxWriter box_writer(*context(), sample_description);
  FlushAndWait(&box_writer);

  // MediaInformation will have multiple sample boxes even though they
  // not added exclusively.
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(written_data.data(),
                                             written_data.size(), nullptr));

  EXPECT_TRUE(box_reader->ScanChildren());

  mp4::SampleDescription reader_sample_description;
  reader_sample_description.type = mp4::kAudio;

  EXPECT_TRUE(box_reader->ReadChild(&reader_sample_description));
  EXPECT_EQ(1u, reader_sample_description.audio_entries.size());

  const auto& audio_sample = reader_sample_description.audio_entries[0];
  EXPECT_EQ(1, audio_sample.data_reference_index);
  EXPECT_EQ(2, audio_sample.channelcount);
  EXPECT_EQ(16, audio_sample.samplesize);
  EXPECT_EQ(kSampleRate, audio_sample.samplerate);

  const mp4::OpusSpecificBox& dops_box = audio_sample.dops;
  EXPECT_EQ(2u, dops_box.channel_count);
  EXPECT_EQ(48000u, dops_box.sample_rate);
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4Fragments) {
  // Tests `mvex/trex` box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  context_->SetVideoTrack({0, kVideoTimescale});
  context_->SetAudioTrack({1, kAudioTimescale});

  constexpr uint32_t kSampleDurations[] = {960, 960, 960};
  constexpr uint32_t kVideoSampleDurationsAfterTimescale[] = {28800, 28800,
                                                              28800};
  constexpr uint32_t kAudioSampleDurationsAfterTimescale[] = {42336, 42336,
                                                              42336};

  constexpr uint32_t kSampleSizes[] = {6400, 333, 333};
  constexpr uint32_t kSampleCount = 3u;
  constexpr uint32_t kVideoBaseDecodeTime = 123u;
  constexpr uint32_t kAudioBaseDecodeTime = 345u;
  constexpr uint32_t kVideoDataSize = 4000u;
  constexpr uint32_t kAudioDataSize = 2000u;
  constexpr uint32_t kBoxHeaderSize = 8u;

  using H =
      std::underlying_type_t<mp4::writable_boxes::TrackFragmentHeaderFlags>;
  using R = std::underlying_type_t<mp4::writable_boxes::TrackFragmentRunFlags>;
  using S = std::underlying_type_t<mp4::writable_boxes::FragmentSampleFlags>;

  mp4::writable_boxes::MovieFragment moof(2);

  {  // `video`.
    mp4::writable_boxes::TrackFragment video_fragment;
    video_fragment.header.track_id = 1u;
    video_fragment.header.flags =
        (static_cast<H>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                            kDefaultBaseIsMoof) |
         static_cast<H>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                            kDefaultSampleDurationPresent) |
         static_cast<H>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                            kkDefaultSampleFlagsPresent));

    video_fragment.header.default_sample_duration =
        base::Milliseconds(kDuration1);
    video_fragment.header.default_sample_flags = static_cast<S>(
        mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo);

    video_fragment.decode_time.track_id = 1;
    video_fragment.decode_time.base_media_decode_time =
        base::Milliseconds(kVideoBaseDecodeTime);

    {  // `video, trun`
      mp4::writable_boxes::TrackFragmentRun video_trun;
      video_trun.flags =
          (static_cast<R>(
               mp4::writable_boxes::TrackFragmentRunFlags::kDataOffsetPresent) |
           static_cast<R>(mp4::writable_boxes::TrackFragmentRunFlags::
                              kFirstSampleFlagsPresent) |
           static_cast<R>(mp4::writable_boxes::TrackFragmentRunFlags::
                              kSampleDurationPresent));

      video_trun.sample_count = kSampleCount;
      video_trun.first_sample_flags =
          (static_cast<S>(
               mp4::writable_boxes::FragmentSampleFlags::kSampleFlagIsNonSync) |
           static_cast<S>(mp4::writable_boxes::FragmentSampleFlags::
                              kSampleFlagDependsYes));

      std::vector<base::TimeTicks> time_ticks;
      base::TimeTicks base_time_ticks = base::TimeTicks::Now();
      time_ticks.push_back(base_time_ticks);
      base::TimeDelta delta;
      for (auto* iter = std::begin(kSampleDurations);
           iter != std::end(kSampleDurations); ++iter) {
        delta += base::Milliseconds(*iter);
        time_ticks.push_back(base_time_ticks + delta);
      }
      video_trun.sample_timestamps = std::move(time_ticks);
      video_fragment.run = std::move(video_trun);
    }
    moof.track_fragments.push_back(std::move(video_fragment));
  }

  {  // `audio`.
    mp4::writable_boxes::TrackFragment audio_fragment;
    audio_fragment.header.track_id = 2u;
    audio_fragment.header.flags =
        (static_cast<H>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                            kDefaultBaseIsMoof) |
         static_cast<H>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                            kDefaultSampleSizePresent) |
         static_cast<H>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                            kkDefaultSampleFlagsPresent));

    audio_fragment.header.default_sample_size = kDefaultSampleSize;
    audio_fragment.header.default_sample_flags =
        (static_cast<S>(
             mp4::writable_boxes::FragmentSampleFlags::kSampleFlagIsNonSync) |
         static_cast<S>(
             mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsYes));

    audio_fragment.decode_time.track_id = 2u;
    audio_fragment.decode_time.base_media_decode_time =
        base::Milliseconds(kAudioBaseDecodeTime);

    {  // `audio, trun.
      mp4::writable_boxes::TrackFragmentRun audio_trun;
      audio_trun.flags =
          (static_cast<R>(
               mp4::writable_boxes::TrackFragmentRunFlags::kDataOffsetPresent) |
           static_cast<R>(mp4::writable_boxes::TrackFragmentRunFlags::
                              kSampleDurationPresent) |
           static_cast<R>(
               mp4::writable_boxes::TrackFragmentRunFlags::kSampleSizePresent));

      audio_trun.sample_count = kSampleCount;
      audio_trun.first_sample_flags =
          (static_cast<S>(
               mp4::writable_boxes::FragmentSampleFlags::kSampleFlagIsNonSync) |
           static_cast<S>(mp4::writable_boxes::FragmentSampleFlags::
                              kSampleFlagDependsYes));

      std::vector<base::TimeTicks> time_ticks;
      base::TimeTicks base_time_ticks = base::TimeTicks::Now();
      time_ticks.push_back(base_time_ticks);
      base::TimeDelta delta = base::Milliseconds(0);
      for (auto* iter = std::begin(kSampleDurations);
           iter != std::end(kSampleDurations); ++iter) {
        delta += base::Milliseconds(*iter);
        time_ticks.push_back(base_time_ticks + delta);
      }
      audio_trun.sample_timestamps = std::move(time_ticks);

      std::vector<uint32_t> sizes(std::begin(kSampleSizes),
                                  std::end(kSampleSizes));
      audio_trun.sample_sizes = std::move(sizes);
      audio_fragment.run = std::move(audio_trun);
    }
    moof.track_fragments.push_back(std::move(audio_fragment));
  }

  // Write `mdat` data.
  mp4::writable_boxes::MediaData media_data;
  std::vector<uint8_t> video_data(kVideoDataSize, 0);
  std::vector<uint8_t> audio_data(kAudioDataSize, 1);

  media_data.track_data.push_back(std::move(video_data));
  media_data.track_data.push_back(std::move(audio_data));

  // Write `moof` boxes.
  Mp4MovieFragmentBoxWriter box_writer(*context(), moof);
  BoxByteStream box_byte_stream;
  box_writer.Write(box_byte_stream);

  // Write `mdat` box with `moof` boxes writer object.
  Mp4MediaDataBoxWriter box_mdat_writer(*context(), media_data);
  FlushWithBoxWriterAndWait(&box_mdat_writer, box_byte_stream);

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      written_data.data(), written_data.size(), nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);

  // `moof` test.
  EXPECT_EQ(mp4::FOURCC_MOOF, reader->type());
  EXPECT_TRUE(reader->ScanChildren());

  // `mfhd` test.
  mp4::MovieFragmentHeader mfhd_box;
  EXPECT_TRUE(reader->ReadChild(&mfhd_box));

  EXPECT_EQ(2u, mfhd_box.sequence_number);

  // `traf` test.
  std::vector<mp4::TrackFragment> traf_boxes;
  EXPECT_TRUE(reader->ReadChildren(&traf_boxes));
  ASSERT_EQ(traf_boxes.size(), 2u);

  // `tfhd` test of video.
  EXPECT_EQ(1u, traf_boxes[0].header.track_id);
  EXPECT_EQ(kDuration1, traf_boxes[0].header.default_sample_duration);
  EXPECT_EQ(0u, traf_boxes[0].header.default_sample_size);
  EXPECT_EQ(true, traf_boxes[0].header.has_default_sample_flags);
  EXPECT_EQ(static_cast<S>(
                mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo),
            traf_boxes[0].header.default_sample_flags);

  // `tfdt` test of video.
  EXPECT_EQ(ConvertToTimescale(base::Milliseconds(kVideoBaseDecodeTime),
                               kVideoTimescale),
            traf_boxes[0].decode_time.decode_time);

  // `trun` test of video.
  uint32_t mdat_video_data_offset;

  ASSERT_EQ(1u, traf_boxes[0].runs.size());
  EXPECT_EQ(kSampleCount, traf_boxes[0].runs[0].sample_count);
  EXPECT_EQ(216u, traf_boxes[0].runs[0].data_offset);
  mdat_video_data_offset = traf_boxes[0].runs[0].data_offset;

  ASSERT_EQ(kSampleCount, traf_boxes[0].runs[0].sample_durations.size());
  EXPECT_EQ(
      std::vector<uint32_t>(std::begin(kVideoSampleDurationsAfterTimescale),
                            std::end(kVideoSampleDurationsAfterTimescale)),
      traf_boxes[0].runs[0].sample_durations);
  ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_sizes.size());
  // kFirstSampleFlagsPresent enabled and no sample_flags entry,
  // then sample_flags will have a value of the first sample flags.
  ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_flags.size());
  ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_composition_time_offsets.size());

  // `tfhd` test of audio.
  EXPECT_EQ(2u, traf_boxes[1].header.track_id);
  EXPECT_EQ(0u, traf_boxes[1].header.default_sample_duration);
  EXPECT_EQ(kDefaultSampleSize, traf_boxes[1].header.default_sample_size);
  EXPECT_EQ(true, traf_boxes[1].header.has_default_sample_flags);
  EXPECT_EQ(
      (static_cast<S>(
           mp4::writable_boxes::FragmentSampleFlags::kSampleFlagIsNonSync) |
       static_cast<S>(
           mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsYes)),
      traf_boxes[1].header.default_sample_flags);

  // `tfdt` test of audio.
  EXPECT_EQ(ConvertToTimescale(base::Milliseconds(kAudioBaseDecodeTime),
                               kAudioTimescale),
            traf_boxes[1].decode_time.decode_time);

  // `trun` test of audio.
  ASSERT_EQ(1u, traf_boxes[1].runs.size());
  EXPECT_EQ(kSampleCount, traf_boxes[1].runs[0].sample_count);

  uint32_t audio_data_offset = mdat_video_data_offset + kVideoDataSize;
  EXPECT_EQ(audio_data_offset, traf_boxes[1].runs[0].data_offset);
  ASSERT_EQ(kSampleCount, traf_boxes[1].runs[0].sample_durations.size());
  EXPECT_EQ(
      std::vector<uint32_t>(std::begin(kAudioSampleDurationsAfterTimescale),
                            std::end(kAudioSampleDurationsAfterTimescale)),
      traf_boxes[1].runs[0].sample_durations);

  ASSERT_EQ(kSampleCount, traf_boxes[1].runs[0].sample_sizes.size());
  ASSERT_EQ(0u, traf_boxes[1].runs[0].sample_flags.size());
  EXPECT_EQ(
      std::vector<uint32_t>(std::begin(kSampleSizes), std::end(kSampleSizes)),
      traf_boxes[1].runs[0].sample_sizes);
  ASSERT_EQ(0u, traf_boxes[1].runs[0].sample_composition_time_offsets.size());

  // `mdat` test.
  std::unique_ptr<mp4::BoxReader> mdat_reader;
  mp4::ParseResult result1 = mp4::BoxReader::ReadTopLevelBox(
      written_data.data() + mdat_video_data_offset - kBoxHeaderSize,
      written_data.size() - mdat_video_data_offset + kBoxHeaderSize, nullptr,
      &mdat_reader);

  EXPECT_EQ(result1, mp4::ParseResult::kOk);
  EXPECT_TRUE(mdat_reader);
  EXPECT_EQ(mp4::FOURCC_MDAT, mdat_reader->type());
  EXPECT_EQ(kVideoDataSize + kAudioDataSize + kBoxHeaderSize,
            mdat_reader->box_size());
  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4FtypBox) {
  // Tests `ftyp` box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::FileType mp4_file_type_box(mp4::FOURCC_MP41, 0);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_MP4A);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_AVC1);

  // Flush at requested.
  Mp4FileTypeBoxWriter box_writer(*context(), mp4_file_type_box);
  FlushAndWait(&box_writer);

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      written_data.data(), written_data.size(), nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);
  EXPECT_EQ(mp4::FOURCC_FTYP, reader->type());

  mp4::FileType file_type;
  EXPECT_TRUE(file_type.Parse(reader.get()));
  EXPECT_EQ(file_type.major_brand, mp4::FOURCC_MP41);
  EXPECT_EQ(file_type.minor_version, 0u);

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

TEST_F(Mp4MuxerBoxWriterTest, Mp4MfraBox) {
  // Tests `mfra` box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);
  context_->SetVideoTrack({1, kVideoTimescale});
  context_->SetAudioTrack({0, kAudioTimescale});

  mp4::writable_boxes::TrackFragmentRandomAccess video_randome_access;
  video_randome_access.track_id = 2;
  mp4::writable_boxes::TrackFragmentRandomAccessEntry entry1;
  entry1.moof_offset = 200;
  entry1.time = base::Seconds(0);
  entry1.traf_number = 1;
  entry1.trun_number = 1;
  entry1.sample_number = 1;
  video_randome_access.entries.emplace_back(std::move(entry1));

  mp4::writable_boxes::TrackFragmentRandomAccessEntry entry2;
  entry2.moof_offset = 1000;
  entry2.time = base::Seconds(3);
  entry2.trun_number = 1;
  entry2.sample_number = 1;
  video_randome_access.entries.emplace_back(std::move(entry2));

  mp4::writable_boxes::TrackFragmentRandomAccessEntry entry3;
  entry3.moof_offset = 4000;
  entry3.time = base::Seconds(6);
  entry3.traf_number = 1;
  entry3.trun_number = 1;
  entry3.sample_number = 1;
  video_randome_access.entries.emplace_back(std::move(entry3));

  mp4::writable_boxes::FragmentRandomAccess frag_random_access;
  // Add empty audio random access by its index position.
  mp4::writable_boxes::TrackFragmentRandomAccess audio_randome_access;
  frag_random_access.tracks.emplace_back(std::move(audio_randome_access));
  frag_random_access.tracks.emplace_back(std::move(video_randome_access));

  // Flush at requested.
  Mp4FragmentRandomAccessBoxWriter box_writer(*context(), frag_random_access);
  FlushAndWait(&box_writer);

  // Validation of the written boxes.

  // `written_data` test.

  // Read from the `mfro` size value that will lead lead to point at
  // the `mfra` box start offset.
  uint32_t mfra_box_size = 0;
  for (int last_index = written_data.size() - 1, j = 0; j < 4; j++) {
    mfra_box_size += (written_data[last_index - j] << (j * 8));
  }

  uint8_t* last_offset_of_mp4_file = written_data.data() + written_data.size();

  uint8_t* mfra_start_offset = last_offset_of_mp4_file - mfra_box_size;
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      mfra_start_offset, mfra_box_size, nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);
  EXPECT_EQ(mp4::FOURCC_MFRA, reader->type());

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

}  // namespace media
