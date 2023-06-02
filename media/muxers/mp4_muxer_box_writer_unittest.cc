// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <type_traits>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/subsample_entry.h"
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

  void FlushWithBoxWriterAndWait(Mp4BoxWriter* box_writer,
                                 BoxByteStream& box_byte_stream) {
    // Flush at requested.
    box_writer->WriteAndFlush(box_byte_stream);

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

  mp4::writable_boxes::VisualSampleEntry visual_sample_entry;
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

  sample_description.visual_sample_entry = std::move(visual_sample_entry);

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
            video_sample_entry.video_codec_profile);
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

TEST_F(Mp4MuxerBoxWriterTest, Mp4AudioSampleEntryAndESDS) {
  // Tests `avc1` and its children box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  mp4::writable_boxes::SampleDescription sample_description;

  mp4::writable_boxes::AudioSampleEntry audio_sample_entry;
  constexpr uint32_t kSampleRate = 48000u;
  audio_sample_entry.sample_rate = kSampleRate;

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

  std::vector<uint8_t> buffer;
  int adts_header_size;
  EXPECT_TRUE(aac.ConvertEsdsToADTS(&buffer, &adts_header_size));

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

TEST_F(Mp4MuxerBoxWriterTest, Mp4Fragments) {
  // Tests `mvex/trex` box writer.
  std::vector<uint8_t> written_data;
  CreateContext(written_data);

  constexpr uint32_t kSampleDurations[] = {960, 960, 960};
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

  mp4::writable_boxes::MovieFragment moof;

  moof.header.sequence_number = 2u;

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

    video_fragment.header.default_sample_duration = base::Seconds(kDuration1);
    video_fragment.header.default_sample_flags = static_cast<S>(
        mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo);

    video_fragment.decode_time.base_media_decode_time =
        base::Seconds(kVideoBaseDecodeTime);

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

      std::vector<base::TimeDelta> durations;
      for (auto* iter = std::begin(kSampleDurations);
           iter != std::end(kSampleDurations); ++iter) {
        durations.push_back(base::Seconds(*iter));
      }
      video_trun.sample_durations = std::move(durations);
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

    audio_fragment.decode_time.base_media_decode_time =
        base::Seconds(kAudioBaseDecodeTime);

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

      std::vector<base::TimeDelta> durations;
      for (auto* iter = std::begin(kSampleDurations);
           iter != std::end(kSampleDurations); ++iter) {
        durations.push_back(base::Seconds(*iter));
      }
      audio_trun.sample_durations = std::move(durations);

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

  base::span<uint8_t> video_span(video_data);
  base::span<uint8_t> audio_span(audio_data);

  media_data.data.push_back(std::move(video_span));
  media_data.data.push_back(std::move(audio_span));

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
  EXPECT_EQ(kVideoBaseDecodeTime, traf_boxes[0].decode_time.decode_time);

  // `trun` test of video.
  uint32_t mdat_video_data_offset;

  ASSERT_EQ(1u, traf_boxes[0].runs.size());
  EXPECT_EQ(kSampleCount, traf_boxes[0].runs[0].sample_count);
  EXPECT_EQ(208u, traf_boxes[0].runs[0].data_offset);
  mdat_video_data_offset = traf_boxes[0].runs[0].data_offset;

  ASSERT_EQ(kSampleCount, traf_boxes[0].runs[0].sample_durations.size());
  EXPECT_EQ(std::vector<uint32_t>(std::begin(kSampleDurations),
                                  std::end(kSampleDurations)),
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
  EXPECT_EQ(kAudioBaseDecodeTime, traf_boxes[1].decode_time.decode_time);

  // `trun` test of audio.
  ASSERT_EQ(1u, traf_boxes[1].runs.size());
  EXPECT_EQ(kSampleCount, traf_boxes[1].runs[0].sample_count);

  uint32_t audio_data_offset = mdat_video_data_offset + kVideoDataSize;
  EXPECT_EQ(audio_data_offset, traf_boxes[1].runs[0].data_offset);
  ASSERT_EQ(kSampleCount, traf_boxes[1].runs[0].sample_durations.size());
  EXPECT_EQ(std::vector<uint32_t>(std::begin(kSampleDurations),
                                  std::end(kSampleDurations)),
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

}  // namespace media
