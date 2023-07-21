// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/mp4_stream_parser.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/mp4_type_conversion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
constexpr uint32_t kWidth = 1024u;
constexpr uint32_t kHeight = 780u;
constexpr uint32_t kMovieHeaderTimescale = 1000u;
constexpr uint32_t kVideoTimescale = 30000u;
constexpr char kVideoHandlerName[] = "VideoHandler";
constexpr uint32_t kBoxHeaderSize = 8u;
#endif
}  // namespace
class Mp4MuxerDelegateTest : public testing::Test {
 public:
  Mp4MuxerDelegateTest() = default;

  void InitF(const StreamParser::InitParameters& expected_params) {}

  bool NewConfigCB(std::unique_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& text_track_map) {
    return true;
  }
  bool NewConfigF(std::unique_ptr<MediaTracks> tracks,
                  const StreamParser::TextTrackConfigMap& tc) {
    return true;
  }

  bool NewBuffersF(const StreamParser::BufferQueueMap& buffer_queue_map) {
    return true;
  }

  void KeyNeededF(EmeInitDataType type, const std::vector<uint8_t>& init_data) {
  }

  void NewSegmentF() {}

  void EndOfSegmentF() {}

 protected:
  void LoadVideo(base::StringPiece filename,
                 base::MemoryMappedFile& video_stream) {
    base::FilePath file_path = GetTestDataFilePath(filename);

    ASSERT_TRUE(video_stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  void PopulateAVCDecoderConfiguration(std::vector<uint8_t>& code_description) {
    // copied from box_reader_unittest.cc.
    std::vector<uint8_t> test_data{
        0x1,        // configurationVersion = 1
        0x64,       // AVCProfileIndication = 100
        0x0,        // profile_compatibility = 0
        0xc,        // AVCLevelIndication = 10
        0xff,       // lengthSizeMinusOne = 3
        0xe1,       // numOfSequenceParameterSets = 1
        0x0, 0x19,  // sequenceParameterSetLength = 25

        // sequenceParameterSet
        0x67, 0x64, 0x0, 0xc, 0xac, 0xd9, 0x41, 0x41, 0xfb, 0x1, 0x10, 0x0, 0x0,
        0x3, 0x0, 0x10, 0x0, 0x0, 0x3, 0x1, 0x40, 0xf1, 0x42, 0x99, 0x60,

        0x1,       // numOfPictureParameterSets
        0x0, 0x6,  // pictureParameterSetLength = 6
        0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0,

        0xfd,  // chroma_format = 1
        0xf8,  // bit_depth_luma_minus8 = 0
        0xf8,  // bit_depth_chroma_minus8 = 0
        0x0,   // numOfSequanceParameterSetExt = 0
    };
    mp4::AVCDecoderConfigurationRecord avc_config;
    ASSERT_TRUE(avc_config.Parse(test_data.data(), test_data.size()));
    ASSERT_TRUE(avc_config.Serialize(code_description));
  }
#endif

  testing::StrictMock<MockMediaLog> media_log_;

 private:
  base::FilePath GetTestDataFilePath(base::StringPiece name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .AppendASCII(name);
    return file_path;
  }

  base::test::TaskEnvironment task_environment;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(Mp4MuxerDelegateTest, AddVideoFrame) {
  media::Muxer::VideoParameters params(gfx::Size(kWidth, kHeight), 30,
                                       media::VideoCodec::kH264,
                                       gfx::ColorSpace());

  base::MemoryMappedFile mapped_file_1;
  LoadVideo("avc-bitstream-format-0.h264", mapped_file_1);
  base::StringPiece video_stream_1(
      reinterpret_cast<const char*>(mapped_file_1.data()),
      mapped_file_1.length());

  base::MemoryMappedFile mapped_file_2;
  LoadVideo("avc-bitstream-format-1.h264", mapped_file_2);
  base::StringPiece video_stream_2(
      reinterpret_cast<const char*>(mapped_file_2.data()),
      mapped_file_2.length());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> moov_written_data;
  std::vector<uint8_t> first_moof_written_data;
  std::vector<uint8_t> second_moof_written_data;

  int callback_count = 0;
  Mp4MuxerDelegate delegate(base::BindRepeating(
      [](base::OnceClosure run_loop_quit,
         std::vector<uint8_t>* total_written_data,
         std::vector<uint8_t>* moov_written_data,
         std::vector<uint8_t>* first_moof_written_data,
         std::vector<uint8_t>* second_moof_written_data, int* callback_count,
         base::StringPiece mp4_data_string) {
        std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                  std::back_inserter(*total_written_data));

        ++(*callback_count);
        switch (*callback_count) {
          case 1:
            std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                      std::back_inserter(*moov_written_data));
            break;
          case 2:
            std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                      std::back_inserter(*first_moof_written_data));
            break;
          case 3:
            std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                      std::back_inserter(*second_moof_written_data));

            // Quit.
            std::move(run_loop_quit).Run();
        }
      },
      run_loop.QuitClosure(), &total_written_data, &moov_written_data,
      &first_moof_written_data, &second_moof_written_data, &callback_count));

  std::vector<uint8_t> code_description;
  PopulateAVCDecoderConfiguration(code_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();

  constexpr uint32_t kSampleDurations[] = {1020, 960, 900, 950};
  base::TimeDelta delta;

  delegate.AddVideoFrame(params, video_stream_1, code_description,
                         base_time_ticks, true);
  for (int i = 0; i < 3; ++i) {
    delta += base::Milliseconds(kSampleDurations[i]);
    delegate.AddVideoFrame(params, video_stream_2, absl::nullopt,
                           base_time_ticks + delta, false);
  }

  delta += base::Milliseconds(kSampleDurations[3]);
  delegate.AddVideoFrame(params, video_stream_1, code_description,
                         base_time_ticks + delta, true);
  for (int i = 0; i < 2; ++i) {
    delta += base::Milliseconds(kSampleDurations[i]);
    delegate.AddVideoFrame(params, video_stream_2, absl::nullopt,
                           base_time_ticks + delta, false);
  }

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // Validate MP4 format.
    std::set<int> audio_object_types;
    audio_object_types.insert(mp4::kISO_14496_3);
    mp4::MP4StreamParser mp4_stream_parser(audio_object_types, false, false);
    StreamParser::InitParameters stream_params(base::TimeDelta::Max());
    stream_params.detected_video_track_count = 1;
    mp4_stream_parser.Init(
        base::BindOnce(&Mp4MuxerDelegateTest::InitF, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewConfigCB,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewBuffersF,
                            base::Unretained(this)),
        true,
        base::BindRepeating(&Mp4MuxerDelegateTest::KeyNeededF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewSegmentF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::EndOfSegmentF,
                            base::Unretained(this)),
        &media_log_);

    bool result = mp4_stream_parser.AppendToParseBuffer(
        total_written_data.data(), total_written_data.size());
    EXPECT_TRUE(result);

    // `MP4StreamParser::Parse` validates the MP4 format.
    StreamParser::ParseStatus parse_result =
        mp4_stream_parser.Parse(total_written_data.size());
    EXPECT_EQ(StreamParser::ParseStatus::kSuccess, parse_result);
  }

  // Validates the MP4 boxes.
  {
    // `moov` validation.
    std::unique_ptr<mp4::BoxReader> reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        moov_written_data.data(), moov_written_data.size(), nullptr, &reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_TRUE(reader);
    EXPECT_EQ(mp4::FOURCC_MOOV, reader->type());
    EXPECT_TRUE(reader->ScanChildren());

    // `mvhd` test.
    mp4::MovieHeader mvhd_box;
    EXPECT_TRUE(reader->ReadChild(&mvhd_box));
    EXPECT_EQ(mvhd_box.version, 1);

    EXPECT_NE(mvhd_box.creation_time, 0u);
    EXPECT_NE(mvhd_box.modification_time, 0u);
    EXPECT_EQ(mvhd_box.timescale, kMovieHeaderTimescale);
    EXPECT_NE(mvhd_box.duration, 0u);
    EXPECT_EQ(mvhd_box.next_track_id, 2u);

    // `mvex` test.
    mp4::MovieExtends mvex_box;
    EXPECT_TRUE(reader->ReadChild(&mvex_box));

    // mp4::MovieExtends mvex_box = mvex_boxes[0];
    EXPECT_EQ(mvex_box.tracks.size(), 1u);

    EXPECT_EQ(mvex_box.tracks[0].track_id, 1u);
    EXPECT_EQ(mvex_box.tracks[0].default_sample_description_index, 1u);
    EXPECT_EQ(mvex_box.tracks[0].default_sample_duration, 0u);
    EXPECT_EQ(mvex_box.tracks[0].default_sample_size, 0u);
    EXPECT_EQ(mvex_box.tracks[0].default_sample_flags, 0u);

    // Track header validation.
    std::vector<mp4::Track> track_boxes;
    EXPECT_TRUE(reader->ReadChildren(&track_boxes));
    EXPECT_EQ(track_boxes.size(), 1u);

    EXPECT_EQ(track_boxes[0].header.track_id, 1u);
    EXPECT_NE(track_boxes[0].header.creation_time, 0u);
    EXPECT_NE(track_boxes[0].header.modification_time, 0u);
    EXPECT_NE(track_boxes[0].header.duration, 0u);
    EXPECT_EQ(track_boxes[0].header.volume, 0);
    EXPECT_EQ(track_boxes[0].header.width, kWidth);
    EXPECT_EQ(track_boxes[0].header.height, kHeight);

    // Media Header validation.
    EXPECT_NE(track_boxes[0].media.header.creation_time, 0u);
    EXPECT_NE(track_boxes[0].media.header.modification_time, 0u);
    EXPECT_NE(track_boxes[0].media.header.duration, 0u);
    EXPECT_EQ(track_boxes[0].media.header.timescale, kVideoTimescale);
    EXPECT_EQ(track_boxes[0].media.header.language_code,
              kUndefinedLanguageCode);

    // Media Handler validation.
    EXPECT_EQ(track_boxes[0].media.handler.type, mp4::TrackType::kVideo);
    EXPECT_EQ(track_boxes[0].media.handler.name, kVideoHandlerName);
  }

  {
    // The first `moof` and `mdat` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        first_moof_written_data.data(), first_moof_written_data.size(), nullptr,
        &moof_reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_TRUE(moof_reader);

    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `mfhd` test.
    mp4::MovieFragmentHeader mfhd_box;
    EXPECT_TRUE(moof_reader->ReadChild(&mfhd_box));
    EXPECT_EQ(1u, mfhd_box.sequence_number);

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 1u);

    // `tfhd` test of video.
    EXPECT_EQ(1u, traf_boxes[0].header.track_id);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_duration);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_size);
    EXPECT_EQ(true, traf_boxes[0].header.has_default_sample_flags);
    EXPECT_EQ(0x1010000u, traf_boxes[0].header.default_sample_flags);

    // `tfdt` test of video.
    EXPECT_EQ(0u, traf_boxes[0].decode_time.decode_time);

    // `trun` test of video.
    uint32_t mdat_video_data_offset;

    ASSERT_EQ(1u, traf_boxes[0].runs.size());
    EXPECT_EQ(4u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(136u, traf_boxes[0].runs[0].data_offset);
    mdat_video_data_offset = traf_boxes[0].runs[0].data_offset;

    ASSERT_EQ(4u, traf_boxes[0].runs[0].sample_durations.size());
    EXPECT_EQ(std::vector<uint32_t>(std::begin(kSampleDurations),
                                    std::end(kSampleDurations)),
              traf_boxes[0].runs[0].sample_durations);

    ASSERT_EQ(4u, traf_boxes[0].runs[0].sample_sizes.size());
    // kFirstSampleFlagsPresent enabled and no sample_flags entry,
    // then sample_flags will have a value of the first sample flags.
    ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_flags.size());
    ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_composition_time_offsets.size());

    // `mdat` test.
    std::unique_ptr<mp4::BoxReader> mdat_reader;
    // first_moof_written_data.data() is `moof` box start address.
    mp4::ParseResult result1 = mp4::BoxReader::ReadTopLevelBox(
        first_moof_written_data.data() + mdat_video_data_offset -
            kBoxHeaderSize,
        first_moof_written_data.size() - mdat_video_data_offset +
            kBoxHeaderSize,
        nullptr, &mdat_reader);

    EXPECT_EQ(result1, mp4::ParseResult::kOk);
    EXPECT_TRUE(mdat_reader);
    EXPECT_EQ(mp4::FOURCC_MDAT, mdat_reader->type());
  }

  {
    // The second `moof` and `mdat` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        second_moof_written_data.data(), second_moof_written_data.size(),
        nullptr, &moof_reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_TRUE(moof_reader);

    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `mfhd` test.
    mp4::MovieFragmentHeader mfhd_box;
    EXPECT_TRUE(moof_reader->ReadChild(&mfhd_box));
    EXPECT_EQ(2u, mfhd_box.sequence_number);

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 1u);

    // `tfhd` test of video.
    EXPECT_EQ(1u, traf_boxes[0].header.track_id);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_duration);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_size);
    EXPECT_EQ(true, traf_boxes[0].header.has_default_sample_flags);
    EXPECT_EQ(0x1010000u, traf_boxes[0].header.default_sample_flags);

    // `tfdt` test of video.
    EXPECT_NE(0u, traf_boxes[0].decode_time.decode_time);

    // `trun` test of video.
    uint32_t mdat_video_data_offset;

    ASSERT_EQ(1u, traf_boxes[0].runs.size());
    EXPECT_EQ(3u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(128u, traf_boxes[0].runs[0].data_offset);
    mdat_video_data_offset = traf_boxes[0].runs[0].data_offset;

    ASSERT_EQ(3u, traf_boxes[0].runs[0].sample_durations.size());
    EXPECT_EQ(kSampleDurations[0], traf_boxes[0].runs[0].sample_durations[0]);
    EXPECT_EQ(kSampleDurations[1], traf_boxes[0].runs[0].sample_durations[1]);
    // The last sample duration of the last fragment will 1/frame_rate.
    EXPECT_EQ(33u, traf_boxes[0].runs[0].sample_durations[2]);

    ASSERT_EQ(3u, traf_boxes[0].runs[0].sample_sizes.size());
    // kFirstSampleFlagsPresent enabled and no sample_flags entry,
    // then sample_flags will have a value of the first sample flags.
    ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_flags.size());
    ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_composition_time_offsets.size());

    // `mdat` test.
    std::unique_ptr<mp4::BoxReader> mdat_reader;
    // second_moof_written_data.data() is `moof` box start address.
    mp4::ParseResult result1 = mp4::BoxReader::ReadTopLevelBox(
        second_moof_written_data.data() + mdat_video_data_offset -
            kBoxHeaderSize,
        second_moof_written_data.size() - mdat_video_data_offset +
            kBoxHeaderSize,
        nullptr, &mdat_reader);

    EXPECT_EQ(result1, mp4::ParseResult::kOk);
    EXPECT_TRUE(mdat_reader);
    EXPECT_EQ(mp4::FOURCC_MDAT, mdat_reader->type());
  }
}
#endif

}  // namespace media
