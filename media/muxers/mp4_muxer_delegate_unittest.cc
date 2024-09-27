// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/muxers/mp4_muxer_delegate.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
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
constexpr uint32_t kAudioSampleRate = 44100u;
constexpr char kVideoHandlerName[] = "VideoHandler";
constexpr char kAudioHandlerName[] = "SoundHandler";
constexpr uint32_t kBoxHeaderSize = 8u;
#endif
}  // namespace
class Mp4MuxerDelegateTest : public testing::Test {
 public:
  Mp4MuxerDelegateTest() = default;

  void InitF(const StreamParser::InitParameters& expected_params) {}

  bool NewConfigCB(std::unique_ptr<MediaTracks> tracks) { return true; }
  bool NewConfigF(std::unique_ptr<MediaTracks> tracks) { return true; }

  bool NewBuffersF(const StreamParser::BufferQueueMap& buffer_queue_map) {
    return true;
  }

  void KeyNeededF(EmeInitDataType type, const std::vector<uint8_t>& init_data) {
  }

  void NewSegmentF() {}

  void EndOfSegmentF() {}

 protected:
  void LoadEncodedFile(std::string_view filename,
                       base::MemoryMappedFile& mapped_stream) {
    base::FilePath file_path = GetTestDataFilePath(filename);

    ASSERT_TRUE(mapped_stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  void PopulateAVCDecoderConfiguration(
      std::vector<uint8_t>& video_codec_description) {
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
    ASSERT_TRUE(avc_config.Serialize(video_codec_description));
  }

  void PopulateAacAdts(std::vector<uint8_t>& audio_video_codec_description) {
    // copied from aac_unittest.cc.
    std::vector<uint8_t> test_data = {0x12, 0x10};
    audio_video_codec_description = test_data;
  }
#endif

  testing::StrictMock<MockMediaLog> media_log_;

 private:
  base::FilePath GetTestDataFilePath(std::string_view name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
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
  // Add video stream only.
  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("bear-320x180-10bit-frame-0.h264", mapped_file_1);
  auto video_stream_1 = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::MemoryMappedFile mapped_file_2;
  LoadEncodedFile("bear-320x180-10bit-frame-1.h264", mapped_file_2);

  auto video_stream_2 = media::DecoderBuffer::CopyFrom(mapped_file_2.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> moov_written_data;
  std::vector<uint8_t> first_moof_written_data;
  std::vector<uint8_t> second_moof_written_data;
  int callback_count = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        switch (++callback_count) {
          case 2:
            base::ranges::copy(mp4_data, std::back_inserter(moov_written_data));
            break;
          case 3:
            base::ranges::copy(mp4_data,
                               std::back_inserter(first_moof_written_data));
            break;
          case 4:
            base::ranges::copy(mp4_data,
                               std::back_inserter(second_moof_written_data));
            run_loop.Quit();
            break;
        }
      }));

  std::vector<uint8_t> video_codec_description;
  PopulateAVCDecoderConfiguration(video_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();

  constexpr uint32_t kSampleDurations[] = {29, 32, 31, 30};
  constexpr uint32_t kSampleDurationsAfterTimescale[] = {870, 960, 930, 999};

  base::TimeDelta delta;

  media::Muxer::VideoParameters params(gfx::Size(kWidth, kHeight), 30,
                                       media::VideoCodec::kH264,
                                       gfx::ColorSpace());
  video_stream_1->set_is_key_frame(true);
  delegate.AddVideoFrame(params, video_stream_1, video_codec_description,
                         base_time_ticks);
  video_stream_2->set_is_key_frame(false);
  for (int i = 0; i < 3; ++i) {
    delta += base::Milliseconds(kSampleDurations[i]);
    delegate.AddVideoFrame(params, video_stream_2, std::nullopt,
                           base_time_ticks + delta);
  }

  delta += base::Milliseconds(kSampleDurations[3]);
  video_stream_1->set_is_key_frame(true);
  delegate.AddVideoFrame(params, video_stream_1, video_codec_description,
                         base_time_ticks + delta);
  video_stream_2->set_is_key_frame(false);
  for (int i = 0; i < 2; ++i) {
    delta += base::Milliseconds(kSampleDurations[i]);
    delegate.AddVideoFrame(params, video_stream_2, std::nullopt,
                           base_time_ticks + delta);
  }

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // Validate MP4 format.
    base::flat_set<int> audio_object_types;
    audio_object_types.insert(mp4::kISO_14496_3);
    mp4::MP4StreamParser mp4_stream_parser(audio_object_types, false, false,
                                           false, false);
    StreamParser::InitParameters stream_params(base::TimeDelta::Max());
    stream_params.detected_video_track_count = 1;
    mp4_stream_parser.Init(
        base::BindOnce(&Mp4MuxerDelegateTest::InitF, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewConfigCB,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewBuffersF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::KeyNeededF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewSegmentF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::EndOfSegmentF,
                            base::Unretained(this)),
        &media_log_);

    bool result = mp4_stream_parser.AppendToParseBuffer(
        base::make_span(total_written_data));
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

    EXPECT_EQ(std::vector<uint32_t>(std::begin(kSampleDurationsAfterTimescale),
                                    std::end(kSampleDurationsAfterTimescale)),
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
    EXPECT_EQ(kSampleDurationsAfterTimescale[0],
              traf_boxes[0].runs[0].sample_durations[0]);
    EXPECT_EQ(kSampleDurationsAfterTimescale[1],
              traf_boxes[0].runs[0].sample_durations[1]);
    // The last sample duration of the last fragment will 1/frame_rate.
    EXPECT_EQ(999u, traf_boxes[0].runs[0].sample_durations[2]);

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

TEST_F(Mp4MuxerDelegateTest, AddAudioFrame) {
  // Add audio stream only.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("aac-44100-packet-0", mapped_file_1);

  auto audio_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> moov_written_data;
  std::vector<uint8_t> first_moof_written_data;

  int callback_count = 0;

  // Default Mp4MuxerDelegate with default max default audio duration of
  // 5 seconds.
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        switch (++callback_count) {
          case 2:
            base::ranges::copy(mp4_data, std::back_inserter(moov_written_data));
            break;
          case 3:
            base::ranges::copy(mp4_data,
                               std::back_inserter(first_moof_written_data));
            // Quit.
            run_loop.Quit();
        }
      }));

  std::vector<uint8_t> audio_codec_description;
  PopulateAacAdts(audio_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  base::TimeDelta delta;

  delegate.AddAudioFrame(params, audio_stream, audio_codec_description,
                         base_time_ticks);
  int incremental_delta = 30;

  // Single fragment.
  constexpr int kAudioAdditionalSampleCount = 29;
  for (int i = 0; i < kAudioAdditionalSampleCount; ++i) {
    delta += base::Milliseconds(incremental_delta);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
    ++incremental_delta;
  }

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // Validate MP4 format.
    base::flat_set<int> audio_object_types;
    audio_object_types.insert(mp4::kISO_14496_3);
    mp4::MP4StreamParser mp4_stream_parser(audio_object_types, false, false,
                                           false, false);
    StreamParser::InitParameters stream_params(base::TimeDelta::Max());
    stream_params.detected_audio_track_count = 1;
    mp4_stream_parser.Init(
        base::BindOnce(&Mp4MuxerDelegateTest::InitF, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewConfigCB,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewBuffersF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::KeyNeededF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::NewSegmentF,
                            base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerDelegateTest::EndOfSegmentF,
                            base::Unretained(this)),
        &media_log_);

    bool result = mp4_stream_parser.AppendToParseBuffer(
        base::make_span(total_written_data));
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
    EXPECT_EQ(track_boxes[0].header.volume, 0x0100);
    EXPECT_EQ(track_boxes[0].header.width, 0u);
    EXPECT_EQ(track_boxes[0].header.height, 0u);

    // Media Header validation.
    EXPECT_NE(track_boxes[0].media.header.creation_time, 0u);
    EXPECT_NE(track_boxes[0].media.header.modification_time, 0u);
    EXPECT_NE(track_boxes[0].media.header.duration, 0u);
    EXPECT_EQ(track_boxes[0].media.header.timescale, kAudioSampleRate);
    EXPECT_EQ(track_boxes[0].media.header.language_code,
              kUndefinedLanguageCode);

    // Media Handler validation.
    EXPECT_EQ(track_boxes[0].media.handler.type, mp4::TrackType::kAudio);
    EXPECT_EQ(track_boxes[0].media.handler.name, kAudioHandlerName);
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

    // `tfhd` test of audio.
    EXPECT_EQ(1u, traf_boxes[0].header.track_id);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_duration);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_size);
    EXPECT_EQ(true, traf_boxes[0].header.has_default_sample_flags);

    // Audio:
    EXPECT_EQ(0x02000000u, traf_boxes[0].header.default_sample_flags);

    // `tfdt` test of audio.
    EXPECT_EQ(0u, traf_boxes[0].decode_time.decode_time);

    // `trun` test of audio.
    uint32_t mdat_audio_data_offset;

    ASSERT_EQ(1u, traf_boxes[0].runs.size());
    EXPECT_EQ(30u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(340u, traf_boxes[0].runs[0].data_offset);
    mdat_audio_data_offset = traf_boxes[0].runs[0].data_offset;

    ASSERT_EQ(30u, traf_boxes[0].runs[0].sample_durations.size());
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[0]);
    EXPECT_EQ(1367u, traf_boxes[0].runs[0].sample_durations[1]);
    EXPECT_EQ(2557u, traf_boxes[0].runs[0].sample_durations[28]);
    EXPECT_EQ(1014u, traf_boxes[0].runs[0].sample_durations[29]);

    ASSERT_EQ(30u, traf_boxes[0].runs[0].sample_sizes.size());
    // kFirstSampleFlagsPresent is not enabled.
    ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_flags.size());
    ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_composition_time_offsets.size());

    // `mdat` test.
    std::unique_ptr<mp4::BoxReader> mdat_reader;
    // first_moof_written_data.data() is `moof` box start address.
    mp4::ParseResult result1 = mp4::BoxReader::ReadTopLevelBox(
        first_moof_written_data.data() + mdat_audio_data_offset -
            kBoxHeaderSize,
        first_moof_written_data.size() - mdat_audio_data_offset +
            kBoxHeaderSize,
        nullptr, &mdat_reader);

    EXPECT_EQ(result1, mp4::ParseResult::kOk);
    EXPECT_TRUE(mdat_reader);
    EXPECT_EQ(mp4::FOURCC_MDAT, mdat_reader->type());
  }
}

TEST_F(Mp4MuxerDelegateTest, AudioOnlyNewFragmentCreation) {
  // Add audio stream with counts over new fragment threshold..
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("aac-44100-packet-0", mapped_file_1);

  auto audio_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> third_moof_written_data;

  int callback_count = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        switch (++callback_count) {
          case 1:
          case 2:
          case 3:
          case 4:
            // DO Nothing.
            break;
          case 5:
            base::ranges::copy(mp4_data,
                               std::back_inserter(third_moof_written_data));
            run_loop.Quit();
        }
      }),
      10);

  std::vector<uint8_t> audio_codec_description;
  PopulateAacAdts(audio_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  base::TimeDelta delta;

  delegate.AddAudioFrame(params, audio_stream, audio_codec_description,
                         base_time_ticks);
  // Total count is 24, which will have 3 fragments and the last fragment
  // has 4 samples.
  constexpr int kMaxAudioSampleFragment = 23;

  for (int i = 0; i < kMaxAudioSampleFragment; ++i) {
    delta += base::Milliseconds(30);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
  }

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // The third `moof` and `mdat` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        third_moof_written_data.data(), third_moof_written_data.size(), nullptr,
        &moof_reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_TRUE(moof_reader);

    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `mfhd` test.
    mp4::MovieFragmentHeader mfhd_box;
    EXPECT_TRUE(moof_reader->ReadChild(&mfhd_box));
    EXPECT_EQ(3u, mfhd_box.sequence_number);

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 1u);

    // `tfdt` test of audio.
    EXPECT_NE(0u, traf_boxes[0].decode_time.decode_time);

    // `trun` test of audio.
    uint32_t mdat_audio_data_offset;

    ASSERT_EQ(1u, traf_boxes[0].runs.size());
    EXPECT_EQ(4u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(132u, traf_boxes[0].runs[0].data_offset);
    mdat_audio_data_offset = traf_boxes[0].runs[0].data_offset;

    ASSERT_EQ(4u, traf_boxes[0].runs[0].sample_durations.size());
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[0]);
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[1]);

    ASSERT_EQ(4u, traf_boxes[0].runs[0].sample_sizes.size());

    // `mdat` test.
    std::unique_ptr<mp4::BoxReader> mdat_reader;
    mp4::ParseResult result1 = mp4::BoxReader::ReadTopLevelBox(
        third_moof_written_data.data() + mdat_audio_data_offset -
            kBoxHeaderSize,
        third_moof_written_data.size() - mdat_audio_data_offset +
            kBoxHeaderSize,
        nullptr, &mdat_reader);

    EXPECT_EQ(result1, mp4::ParseResult::kOk);
    EXPECT_TRUE(mdat_reader);
    EXPECT_EQ(mp4::FOURCC_MDAT, mdat_reader->type());
  }
}

TEST_F(Mp4MuxerDelegateTest, AudioAndVideoAddition) {
  // Add stream audio first and video.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("aac-44100-packet-0", mapped_file_1);

  auto audio_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::MemoryMappedFile mapped_file_2;
  LoadEncodedFile("bear-320x180-10bit-frame-0.h264", mapped_file_2);

  auto video_stream = media::DecoderBuffer::CopyFrom(mapped_file_2.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> third_moof_written_data;
  std::vector<uint8_t> fourth_moof_written_data;

  int callback_count = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        ++callback_count;
        switch (callback_count) {
          case 1:
          case 2:
          case 3:
          case 4:
            // DO Nothing.
            break;
          case 5:
            base::ranges::copy(mp4_data,
                               std::back_inserter(third_moof_written_data));
            break;
          case 6:
            base::ranges::copy(mp4_data,
                               std::back_inserter(fourth_moof_written_data));
            // Quit.
            run_loop.Quit();
        }
      }),
      10);

  std::vector<uint8_t> code_description;
  PopulateAacAdts(code_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  base::TimeDelta delta;

  delegate.AddAudioFrame(params, audio_stream, code_description,
                         base_time_ticks);
  // Total count is 24, which will have 3 fragments and the last fragment
  // has 4 samples.
  constexpr int kMaxAudioSampleFragment = 23;

  for (int i = 0; i < kMaxAudioSampleFragment; ++i) {
    delta += base::Milliseconds(30);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
  }

  // video stream, third fragment.
  std::vector<uint8_t> video_code_description;
  PopulateAVCDecoderConfiguration(video_code_description);

  media::Muxer::VideoParameters video_params(gfx::Size(kWidth, kHeight), 30,
                                             media::VideoCodec::kH264,
                                             gfx::ColorSpace());
  video_stream->set_is_key_frame(true);
  delegate.AddVideoFrame(video_params, video_stream, video_code_description,
                         base_time_ticks);
  for (int i = 0; i < kMaxAudioSampleFragment; ++i) {
    delta += base::Milliseconds(30);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
  }

  video_stream->set_is_key_frame(true);
  // video stream, fourth fragment.
  delegate.AddVideoFrame(video_params, video_stream, video_code_description,
                         base_time_ticks + base::Milliseconds(50));
  delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                         base_time_ticks + delta + base::Milliseconds(30));

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // The third `moof` and `mdat` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        third_moof_written_data.data(), third_moof_written_data.size(), nullptr,
        &moof_reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_TRUE(moof_reader);

    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `mfhd` test.
    mp4::MovieFragmentHeader mfhd_box;
    EXPECT_TRUE(moof_reader->ReadChild(&mfhd_box));
    EXPECT_EQ(3u, mfhd_box.sequence_number);

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 2u);

    // `tfhd` test of audio.
    EXPECT_EQ(1u, traf_boxes[0].header.track_id);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_duration);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_size);
    EXPECT_EQ(true, traf_boxes[0].header.has_default_sample_flags);
    EXPECT_EQ(0x02000000u, traf_boxes[0].header.default_sample_flags);

    // `tfdt` test of audio.
    EXPECT_NE(0u, traf_boxes[0].decode_time.decode_time);

    // `trun` test of audio.
    ASSERT_EQ(1u, traf_boxes[0].runs.size());
    EXPECT_EQ(27u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(396u, traf_boxes[0].runs[0].data_offset);
    ASSERT_EQ(27u, traf_boxes[0].runs[0].sample_durations.size());
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[0]);
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[1]);
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[2]);
    EXPECT_EQ(1323u, traf_boxes[0].runs[0].sample_durations[3]);

    ASSERT_EQ(27u, traf_boxes[0].runs[0].sample_sizes.size());

    // video track.

    // `tfdt` test of video.
    EXPECT_EQ(0u, traf_boxes[1].decode_time.decode_time);

    // `trun` test of video.
    ASSERT_EQ(1u, traf_boxes[1].runs.size());
    EXPECT_EQ(1u, traf_boxes[1].runs[0].sample_count);
    EXPECT_EQ(10413u, traf_boxes[1].runs[0].data_offset);

    ASSERT_EQ(1u, traf_boxes[1].runs[0].sample_durations.size());

    // The first and last item.
    EXPECT_EQ(999u, traf_boxes[1].runs[0].sample_durations[0]);
  }

  {
    // The fourth `moof` and `mdat` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        fourth_moof_written_data.data(), fourth_moof_written_data.size(),
        nullptr, &moof_reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_TRUE(moof_reader);

    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `mfhd` test.
    mp4::MovieFragmentHeader mfhd_box;
    EXPECT_TRUE(moof_reader->ReadChild(&mfhd_box));
    EXPECT_EQ(4u, mfhd_box.sequence_number);

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 2u);

    // `trun` test of audio.
    EXPECT_EQ(1u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(188u, traf_boxes[0].runs[0].data_offset);
    ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_durations.size());

    // video track.
    // `tfdt` test of video.
    EXPECT_EQ(ConvertToTimescale(base::Milliseconds(50), 30000),
              traf_boxes[1].decode_time.decode_time);

    // `trun` test of video.
    ASSERT_EQ(1u, traf_boxes[1].runs.size());
    EXPECT_EQ(1u, traf_boxes[1].runs[0].sample_count);
    EXPECT_EQ(559u, traf_boxes[1].runs[0].data_offset);

    ASSERT_EQ(1u, traf_boxes[1].runs[0].sample_durations.size());

    // The first and last item.
    EXPECT_EQ(999u, traf_boxes[1].runs[0].sample_durations[0]);
  }
}

TEST_F(Mp4MuxerDelegateTest, MfraBoxOnAudioAndVideoAddition) {
  // Add stream audio first and video.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("aac-44100-packet-0", mapped_file_1);

  auto audio_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::MemoryMappedFile mapped_file_2;
  LoadEncodedFile("bear-320x180-10bit-frame-0.h264", mapped_file_2);

  auto video_stream = media::DecoderBuffer::CopyFrom(mapped_file_2.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> third_moof_written_data;
  std::vector<uint8_t> fourth_moof_written_data;
  std::vector<uint8_t> mfra_written_data;

  int callback_count = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        switch (++callback_count) {
          case 1:
          case 2:
          case 3:
          case 4:
            // DO Nothing.
            break;
          case 5:
            base::ranges::copy(mp4_data,
                               std::back_inserter(third_moof_written_data));
            break;
          case 6:
            base::ranges::copy(mp4_data,
                               std::back_inserter(fourth_moof_written_data));
            break;
          case 7:
            base::ranges::copy(mp4_data, std::back_inserter(mfra_written_data));
            run_loop.Quit();
        }
      }),
      10);

  std::vector<uint8_t> audio_codec_description;
  PopulateAacAdts(audio_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  base::TimeDelta delta;

  delegate.AddAudioFrame(params, audio_stream, audio_codec_description,
                         base_time_ticks);
  // Total count is 24, which will have 3 fragments and the last fragment
  // has 4 samples.
  constexpr int kMaxAudioSampleFragment = 23;

  for (int i = 0; i < kMaxAudioSampleFragment; ++i) {
    delta += base::Milliseconds(30);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
  }

  // video stream, third fragment.
  std::vector<uint8_t> video_codec_description;
  PopulateAVCDecoderConfiguration(video_codec_description);

  media::Muxer::VideoParameters video_params(gfx::Size(kWidth, kHeight), 30,
                                             media::VideoCodec::kH264,
                                             gfx::ColorSpace());
  video_stream->set_is_key_frame(true);
  delegate.AddVideoFrame(video_params, video_stream, video_codec_description,
                         base_time_ticks);
  for (int i = 0; i < kMaxAudioSampleFragment; ++i) {
    delta += base::Milliseconds(30);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
  }
  video_stream->set_is_key_frame(true);
  // video stream, fourth fragment.
  delegate.AddVideoFrame(video_params, video_stream, video_codec_description,
                         base_time_ticks + base::Milliseconds(50));
  delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                         base_time_ticks + delta + base::Milliseconds(30));

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  // Locates `mfra` box.
  uint32_t mfra_box_size = 0;
  for (int last_index = total_written_data.size() - 1, j = 0; j < 4; j++) {
    mfra_box_size += (total_written_data[last_index - j] << (j * 8));
  }

  size_t last_offset_of_mp4_file = total_written_data.size();
  size_t mfra_start_offset = last_offset_of_mp4_file - mfra_box_size;

  // Locates the first and fourth `moof` boxes from the `mfra` box.
  // compare the data.
  constexpr uint32_t kMfraBoxSize = 8u;
  constexpr uint32_t kFullBoxSize = 12u;
  constexpr uint32_t kTfraBoxSize = 24u;
  constexpr uint32_t kTfraEntrySize = 28u;

  size_t tfra_box_offset = mfra_start_offset + kMfraBoxSize;
  size_t track_id = tfra_box_offset + kFullBoxSize;
  auto reader =
      base::SpanReader(base::span(total_written_data).subspan(track_id, 12u));

  uint32_t value;
  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 2u);  // video track id.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 63u);  // entry number size. 63 == 0x3f.

  reader.ReadU32BigEndian(value);
  // The file has 4 fragments, but video will also have 4 fragments
  // and the first two fragments have empty video samples.
  EXPECT_EQ(value, 4u);  // number of entries.

  // First entry.
  size_t first_tfra_entry = tfra_box_offset + kTfraBoxSize;
  reader =
      base::SpanReader(base::span(total_written_data)
                           .subspan(first_tfra_entry, kTfraEntrySize * 4u));
  uint64_t time;
  reader.ReadU64BigEndian(time);
  EXPECT_EQ(time, 0u);  // time.

  uint64_t moof_offset;
  reader.ReadU64BigEndian(moof_offset);

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // traf number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // trun number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // sample number.

  // Second entry.
  reader.ReadU64BigEndian(time);
  EXPECT_EQ(time, 0u);  // time.

  reader.ReadU64BigEndian(moof_offset);

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // traf number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // trun number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // sample number.

  // Third entry.
  reader.ReadU64BigEndian(time);

  // first fragment that holds video frames, so it should have
  // a 0 that is video frame start time.
  EXPECT_EQ(time, 0u);  // time.

  reader.ReadU64BigEndian(moof_offset);

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // traf number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // trun number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // sample number.
  EXPECT_EQ(base::span(total_written_data)
                .subspan(moof_offset, third_moof_written_data.size()),
            third_moof_written_data);

  // Fourth entry.
  reader.ReadU64BigEndian(time);
  EXPECT_NE(time, 0u);  // time.

  uint64_t fourth_moof_offset;
  reader.ReadU64BigEndian(fourth_moof_offset);

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // traf number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // trun number.

  reader.ReadU32BigEndian(value);
  EXPECT_EQ(value, 1u);  // sample number.
  EXPECT_EQ(base::span(total_written_data)
                .subspan(fourth_moof_offset, fourth_moof_written_data.size()),
            fourth_moof_written_data);
}

TEST_F(Mp4MuxerDelegateTest, VideoAndAudioAddition) {
  // Add stream with video first, and audio.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("aac-44100-packet-0", mapped_file_1);

  auto audio_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::MemoryMappedFile mapped_file_2;
  LoadEncodedFile("bear-320x180-10bit-frame-0.h264", mapped_file_2);

  auto video_stream = media::DecoderBuffer::CopyFrom(mapped_file_2.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> first_moof_written_data;

  int callback_count = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        switch (++callback_count) {
          case 1:
          case 2:
            // Do nothing.
            break;
          case 3:
            base::ranges::copy(mp4_data,
                               std::back_inserter(first_moof_written_data));
            run_loop.Quit();
            break;
        }
      }),
      10);

  std::vector<uint8_t> audio_codec_description;
  PopulateAacAdts(audio_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  base::TimeDelta delta;

  // video stream.
  std::vector<uint8_t> video_codec_description;
  PopulateAVCDecoderConfiguration(video_codec_description);

  media::Muxer::VideoParameters video_params(gfx::Size(kWidth, kHeight), 30,
                                             media::VideoCodec::kH264,
                                             gfx::ColorSpace());
  video_stream->set_is_key_frame(true);
  delegate.AddVideoFrame(video_params, video_stream, video_codec_description,
                         base_time_ticks);

  // audio stream.
  delegate.AddAudioFrame(params, audio_stream, audio_codec_description,
                         base_time_ticks);
  // Total count is 24, which will
  // have 3 fragments and the last
  // fragment has 4 samples.
  constexpr int kMaxAudioSampleFragment = 23;

  for (int i = 0; i < kMaxAudioSampleFragment; ++i) {
    delta += base::Milliseconds(30);
    delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                           base_time_ticks + delta);
  }

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // The first `moof` and `mdat` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        first_moof_written_data.data(), first_moof_written_data.size(), nullptr,
        &moof_reader);

    EXPECT_EQ(result, mp4::ParseResult::kOk);

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
    ASSERT_EQ(traf_boxes.size(), 2u);

    // `tfhd` test of audio.
    EXPECT_EQ(1u, traf_boxes[0].header.track_id);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_duration);
    EXPECT_EQ(0u, traf_boxes[0].header.default_sample_size);
    EXPECT_EQ(true, traf_boxes[0].header.has_default_sample_flags);
    EXPECT_EQ(0x1010000u, traf_boxes[0].header.default_sample_flags);

    // `tfdt` test of audio.
    EXPECT_DOUBLE_EQ(0u, traf_boxes[0].decode_time.decode_time);

    // `trun` test of audio.
    ASSERT_EQ(1u, traf_boxes[0].runs.size());
    EXPECT_EQ(1u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(372u, traf_boxes[0].runs[0].data_offset);
    ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_durations.size());
    EXPECT_EQ(999u, traf_boxes[0].runs[0].sample_durations[0]);

    ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_sizes.size());
    // kFirstSampleFlagsPresent enabled and no sample_flags entry,
    // then sample_flags will have a value of the first sample flags.
    ASSERT_EQ(1u, traf_boxes[0].runs[0].sample_flags.size());
    ASSERT_EQ(0u, traf_boxes[0].runs[0].sample_composition_time_offsets.size());

    // audio track.
    // `tfhd` test of video.
    EXPECT_EQ(2u, traf_boxes[1].header.track_id);

    // `tfdt` test of video.
    EXPECT_EQ(0u, traf_boxes[1].decode_time.decode_time);

    // `trun` test of video.
    ASSERT_EQ(1u, traf_boxes[1].runs.size());
    EXPECT_EQ(24u, traf_boxes[1].runs[0].sample_count);
    EXPECT_EQ(5859u, traf_boxes[1].runs[0].data_offset);

    ASSERT_EQ(24u, traf_boxes[1].runs[0].sample_durations.size());

    // The first and last item.
    EXPECT_EQ(1323u, traf_boxes[1].runs[0].sample_durations[0]);
    EXPECT_EQ(1014u, traf_boxes[1].runs[0].sample_durations[23]);
  }
}

TEST_F(Mp4MuxerDelegateTest, AudioVideoAndAudioVideoFragment) {
  // Add audio and video the first fragment, but video and audio
  // on the second segment.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("aac-44100-packet-0", mapped_file_1);

  auto audio_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::MemoryMappedFile mapped_file_2;
  LoadEncodedFile("bear-320x180-10bit-frame-0.h264", mapped_file_2);

  auto video_stream = media::DecoderBuffer::CopyFrom(mapped_file_2.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> first_moof_written_data;
  std::vector<uint8_t> second_moof_written_data;

  int callback_count = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        base::ranges::copy(mp4_data, std::back_inserter(total_written_data));

        switch (++callback_count) {
          case 1:
          case 2:
            // DO Nothing.
            break;
          case 3:
            base::ranges::copy(mp4_data,
                               std::back_inserter(first_moof_written_data));
            break;
          case 4:
            base::ranges::copy(mp4_data,
                               std::back_inserter(second_moof_written_data));
            run_loop.Quit();
            break;
        }
      }),
      10);

  std::vector<uint8_t> audio_codec_description;
  PopulateAacAdts(audio_codec_description);

  // video stream.
  std::vector<uint8_t> video_codec_description;
  PopulateAVCDecoderConfiguration(video_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  constexpr base::TimeDelta kDelta = base::Milliseconds(30);
  media::Muxer::VideoParameters video_params(gfx::Size(kWidth, kHeight), 30,
                                             media::VideoCodec::kH264,
                                             gfx::ColorSpace());

  // The first fragment; audio (1 sample) -> video (2 samples) track.
  delegate.AddAudioFrame(params, audio_stream, audio_codec_description,
                         base_time_ticks);
  video_stream->set_is_key_frame(true);
  delegate.AddVideoFrame(video_params, video_stream, video_codec_description,
                         base_time_ticks);
  video_stream->set_is_key_frame(false);
  delegate.AddVideoFrame(video_params, video_stream, std::nullopt,
                         base_time_ticks + kDelta);

  video_stream->set_is_key_frame(true);
  // The second fragment; video (1 sample) -> audio (2
  // samples) track.
  delegate.AddVideoFrame(video_params, video_stream, video_codec_description,
                         base_time_ticks + kDelta * 2);
  delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                         base_time_ticks + kDelta);
  delegate.AddAudioFrame(params, audio_stream, std::nullopt,
                         base_time_ticks + kDelta * 2);
  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  {
    // The first `moof`validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        first_moof_written_data.data(), first_moof_written_data.size(), nullptr,
        &moof_reader);
    EXPECT_EQ(result, mp4::ParseResult::kOk);
    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 2u);

    // The first framgment: `trun`.
    // The first track is audio, the second is video.
    EXPECT_EQ(1u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(2u, traf_boxes[1].runs[0].sample_count);
  }
  {
    // The second `moof` validation.
    std::unique_ptr<mp4::BoxReader> moof_reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        second_moof_written_data.data(), second_moof_written_data.size(),
        nullptr, &moof_reader);
    EXPECT_EQ(result, mp4::ParseResult::kOk);
    // `moof` test.
    EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
    EXPECT_TRUE(moof_reader->ScanChildren());

    // `traf` test.
    std::vector<mp4::TrackFragment> traf_boxes;
    EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));
    ASSERT_EQ(traf_boxes.size(), 2u);

    // The first framgment: `trun`.

    // The track order should be same:
    // first track is audio, the second is video.
    EXPECT_EQ(2u, traf_boxes[0].runs[0].sample_count);
    EXPECT_EQ(1u, traf_boxes[1].runs[0].sample_count);
  }
}

TEST_F(Mp4MuxerDelegateTest, ConvertedEncodedDataOnAvc1) {
  // Add audio and video the first fragment, but video and audio
  // on the second fragment.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kAudioSampleRate, 1000);

  base::MemoryMappedFile mapped_file_1;
  LoadEncodedFile("bear.h264", mapped_file_1);

  auto video_stream = media::DecoderBuffer::CopyFrom(mapped_file_1.bytes());

  base::RunLoop run_loop;

  std::vector<uint8_t> total_written_data;
  std::vector<uint8_t> moof_and_mdat_written_data;

  int callback_count = 0;
  int moof_box_start_offset = 0;
  Mp4MuxerDelegate delegate(
      media::AudioCodec::kAAC, std::nullopt, std::nullopt,
      base::BindLambdaForTesting([&](base::span<const uint8_t> mp4_data) {
        switch (++callback_count) {
          case 1:
            // 'ftyp' box.
          case 2:
            // 'moov' box.
            base::ranges::copy(mp4_data,
                               std::back_inserter(total_written_data));
            break;
          case 3:
            // 'moof' box.
            moof_box_start_offset = total_written_data.size();

            base::ranges::copy(mp4_data,
                               std::back_inserter(total_written_data));

            base::ranges::copy(mp4_data,
                               std::back_inserter(moof_and_mdat_written_data));
            run_loop.Quit();
            break;
          case 4:
            // MFRA data
            run_loop.Quit();
            break;
        }
      }),
      10);

  std::vector<uint8_t> audio_codec_description;
  PopulateAacAdts(audio_codec_description);

  // video stream.
  std::vector<uint8_t> video_codec_description;
  PopulateAVCDecoderConfiguration(video_codec_description);

  base::TimeTicks base_time_ticks = base::TimeTicks::Now();
  media::Muxer::VideoParameters video_params(gfx::Size(kWidth, kHeight), 30,
                                             media::VideoCodec::kH264,
                                             gfx::ColorSpace());

  video_stream->set_is_key_frame(true);
  delegate.AddVideoFrame(video_params, video_stream, video_codec_description,
                         base_time_ticks);

  // Write box data to the callback.
  delegate.Flush();

  run_loop.Run();

  std::unique_ptr<mp4::BoxReader> moof_reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      moof_and_mdat_written_data.data(), moof_and_mdat_written_data.size(),
      nullptr, &moof_reader);
  EXPECT_EQ(result, mp4::ParseResult::kOk);

  // `moof`box read.
  EXPECT_EQ(mp4::FOURCC_MOOF, moof_reader->type());
  EXPECT_TRUE(moof_reader->ScanChildren());

  // `traf` box read.
  std::vector<mp4::TrackFragment> traf_boxes;
  EXPECT_TRUE(moof_reader->ReadChildren(&traf_boxes));

  uint32_t mdat_video_data_offset;
  EXPECT_EQ(112u, traf_boxes[0].runs[0].data_offset);
  mdat_video_data_offset = traf_boxes[0].runs[0].data_offset;

  // `moof` offset + `mdat_video_data_offset` is the start of the video data,
  // which should be NAL size instead of the start code.
  uint32_t nal_size_offset = moof_box_start_offset + mdat_video_data_offset;

  constexpr int kNaluLength = 4;
  uint32_t nalsize = 0;
  for (int i = 0; i < kNaluLength; i++) {
    nalsize = (nalsize << 8) | total_written_data[nal_size_offset++];
  }

  EXPECT_EQ(nalsize, 578u);
}

#endif

}  // namespace media
