// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/mp4_stream_parser.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "base/big_endian.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/mp4_stream_parser.h"
#include "media/muxers/mp4_muxer.h"
#include "media/muxers/mp4_type_conversion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

namespace {
constexpr int kAudioTrackId = 1;
constexpr int kVideoTrackId = 2;
constexpr int kSampleDuration = 30;
constexpr int kStartAudioTimeticks = 100;
constexpr int kStartVideoTimeticks = 200;
}  // namespace

class Mp4MuxerTest : public testing::Test {
 public:
  Mp4MuxerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void InitF(const StreamParser::InitParameters& expected_params) {}
  bool NewConfigCB(std::unique_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& text_track_map) {
    tracks_ = std::move(tracks);
    return true;
  }
  bool NewBuffersF(const StreamParser::BufferQueueMap& buffer_queue_map) {
    buffer_queue_map_ = buffer_queue_map;
    return true;
  }
  void KeyNeededF(EmeInitDataType type, const std::vector<uint8_t>& init_data) {
    NOTREACHED();
  }
  void NewSegmentF() { ++new_fragment_count_; }

  void EndOfSegmentF() {}

 protected:
  std::string GetAudioSample() { return "audio-sample"; }

  std::string GetVideoKeyFrame() {
    base::MemoryMappedFile mapped_file_2;
    LoadEncodedFile("avc-bitstream-format-0.h264", mapped_file_2);
    return std::string(reinterpret_cast<const char*>(mapped_file_2.data()),
                       mapped_file_2.length());
  }

  std::string GetVideoFrame() {
    base::MemoryMappedFile mapped_file_2;
    LoadEncodedFile("avc-bitstream-format-1.h264", mapped_file_2);
    return std::string(reinterpret_cast<const char*>(mapped_file_2.data()),
                       mapped_file_2.length());
  }

  std::unique_ptr<Mp4Muxer> CreateMp4Muxer(bool has_video,
                                           bool has_audio,
                                           int expected_fragment_count,
                                           bool audio_only = false) {
    constexpr int kNoneFragmentTopBoxCount = 3;
    constexpr int kNoneFragmentTopBoxCountForAudioOnly = 2;
    if (!run_loop_) {
      InitCallbackVariables();
    }

    int expected_called_count = expected_fragment_count;
    expected_called_count += audio_only ? kNoneFragmentTopBoxCountForAudioOnly
                                        : kNoneFragmentTopBoxCount;
    auto mp4_muxer = std::make_unique<Mp4Muxer>(
        AudioCodec::kAAC, has_video, has_audio,
        base::BindLambdaForTesting([=](base::StringPiece mp4_data_string) {
          std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                    std::back_inserter(written_data_));
          ++called_count_;

          if (called_count_ == expected_called_count) {
            if (!audio_only) {
              EXPECT_TRUE(IsMfraBox(mp4_data_string));
            }

            if (run_loop_) {
              run_loop_->Quit();
            }
          }

          constexpr int kMoovBoxIndex = 2;
          if (called_count_ == kMoovBoxIndex) {
            CaptureMoovBox(mp4_data_string);
          }
        }));

    return mp4_muxer;
  }

  bool IsMfraBox(base::StringPiece mp4_data_string) {
    // Ensure Last box when video samples are added.
    std::unique_ptr<mp4::BoxReader> reader;
    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        reinterpret_cast<const uint8_t*>(mp4_data_string.data()),
        mp4_data_string.size(), nullptr, &reader);
    EXPECT_EQ(result, mp4::ParseResult::kOk);
    return reader->type() == mp4::FOURCC_MFRA;
  }

  bool CaptureMoovBox(base::StringPiece mp4_data_string) {
    // Ensure Last box when video samples are added.
    std::copy(mp4_data_string.begin(), mp4_data_string.end(),
              std::back_inserter(moov_written_data_));

    mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
        reinterpret_cast<const uint8_t*>(moov_written_data_.data()),
        moov_written_data_.size(), nullptr, &moov_reader_);
    EXPECT_EQ(result, mp4::ParseResult::kOk);
    EXPECT_EQ(moov_reader_->type(), mp4::FOURCC_MOOV);
    EXPECT_TRUE(moov_reader_->ScanChildren());
    return true;
  }

  void FlushAndParse(std::unique_ptr<Mp4Muxer> mp4_muxer) {
    // Destruction of the Mp4Muxer is the only way to invoke Flush.
    mp4_muxer.reset();
    run_loop_->Run();
    ASSERT_TRUE(!written_data_.empty());
    ParseWithMp4StreamParser(written_data_);
  }

  void AddAudioSamples(Mp4Muxer* mp4_muxer,
                       int system_timestamp_offset_ms,
                       int sample_count) {
    constexpr uint32_t kAudioSampleRate = 44100u;
    std::vector<uint8_t> code_description;
    PopulateAacAdts(code_description);

    media::AudioParameters audio_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Stereo(), kAudioSampleRate, 1000);
    std::string audio_stream = GetAudioSample();

    base::TimeDelta delta;
    base::TimeTicks timestamp =
        base::TimeTicks() + base::Milliseconds(system_timestamp_offset_ms);

    for (int i = 0; i < sample_count; ++i) {
      mp4_muxer->OnEncodedAudio(audio_params, audio_stream, code_description,
                                timestamp + delta);
      delta += base::Milliseconds(kSampleDuration);
    }
  }

  void AddVideoSamples(Mp4Muxer* mp4_muxer,
                       int system_timestamp_offset_ms,
                       int sample_count) {
    std::vector<uint8_t> video_code_description;
    PopulateAVCDecoderConfiguration(video_code_description);

    media::Muxer::VideoParameters video_params(
        gfx::Size(40, 30), 30, media::VideoCodec::kH264, gfx::ColorSpace());
    std::string video_key_frame = GetVideoKeyFrame();

    base::TimeDelta delta;
    base::TimeTicks timestamp =
        base::TimeTicks() + base::Milliseconds(system_timestamp_offset_ms);
    mp4_muxer->OnEncodedVideo(video_params, video_key_frame, std::string(),
                              video_code_description, timestamp, true);
    delta += base::Milliseconds(kSampleDuration);

    std::string video_frame = GetVideoFrame();
    for (int i = 1; i < sample_count; ++i) {
      mp4_muxer->OnEncodedVideo(video_params, video_frame, std::string(), {},
                                timestamp + delta, false);
      delta += base::Milliseconds(kSampleDuration);
    }
  }

  void AddVideoSamplesForNonKeyFrame(Mp4Muxer* mp4_muxer,
                                     int system_timestamp_offset_ms,
                                     int sample_count) {
    std::vector<uint8_t> video_code_description;
    PopulateAVCDecoderConfiguration(video_code_description);

    media::Muxer::VideoParameters video_params(
        gfx::Size(40, 30), 30, media::VideoCodec::kH264, gfx::ColorSpace());
    std::string video_frame = GetVideoFrame();

    base::TimeDelta delta;
    base::TimeTicks timestamp =
        base::TimeTicks() + base::Milliseconds(system_timestamp_offset_ms);
    for (int i = 0; i < sample_count; ++i) {
      mp4_muxer->OnEncodedVideo(video_params, video_frame, std::string(), {},
                                timestamp + delta, false);
      delta += base::Milliseconds(kSampleDuration);
    }
  }

  void ParseWithMp4StreamParser(std::vector<uint8_t>& written_data) {
    std::set<int> audio_object_types;
    audio_object_types.insert(mp4::kISO_14496_3);
    mp4::MP4StreamParser mp4_stream_parser(audio_object_types, false, false);
    StreamParser::InitParameters stream_params(base::TimeDelta::Max());
    stream_params.detected_video_track_count = 1;

    mp4_stream_parser.Init(
        base::BindOnce(&Mp4MuxerTest::InitF, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerTest::NewConfigCB, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerTest::NewBuffersF, base::Unretained(this)),
        /*ignore_text_tracks=*/false,
        base::BindRepeating(&Mp4MuxerTest::KeyNeededF, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerTest::NewSegmentF, base::Unretained(this)),
        base::BindRepeating(&Mp4MuxerTest::EndOfSegmentF,
                            base::Unretained(this)),
        &media_log_);

    bool result = mp4_stream_parser.AppendToParseBuffer(written_data.data(),
                                                        written_data.size());
    EXPECT_TRUE(result);

    // `MP4StreamParser::Parse` validates the MP4 format.
    StreamParser::ParseStatus parse_result =
        mp4_stream_parser.Parse(written_data.size());
    EXPECT_EQ(StreamParser::ParseStatus::kSuccess, parse_result);
  }

  void ValidateTrackAndSamplesCount(uint32_t fragments,
                                    uint32_t expected_video_sample_count,
                                    uint32_t expected_audio_sample_count,
                                    int video_track_id = 2,
                                    int audio_track_id = 1) {
    // Total tracks are video and audio.
    EXPECT_EQ(parsed_buffer_queue_map().size(), fragments);

    auto video_it = parsed_buffer_queue_map().find(video_track_id);
    if (expected_video_sample_count) {
      EXPECT_TRUE(video_it != parsed_buffer_queue_map().end());
      EXPECT_EQ(video_it->second.size(), expected_video_sample_count);
    } else {
      EXPECT_TRUE(video_it == parsed_buffer_queue_map().end());
    }

    auto audio_it = parsed_buffer_queue_map().find(audio_track_id);
    if (expected_audio_sample_count) {
      EXPECT_TRUE(audio_it != parsed_buffer_queue_map().end());
      EXPECT_EQ(audio_it->second.size(), expected_audio_sample_count);
    } else {
      EXPECT_TRUE(audio_it == parsed_buffer_queue_map().end());
    }
  }

  void ValidateMovieHeaderDuration(uint64_t expected_duration) {
    // `mvhd` test.
    mp4::MovieHeader mvhd_box;
    EXPECT_TRUE(moov_box_reader()->ReadChild(&mvhd_box));
    EXPECT_EQ(mvhd_box.version, 1);

    // Round from microsecond to millisecond could have different value.
    EXPECT_EQ(mvhd_box.duration, expected_duration);
  }
  void InitCallbackVariables() {
    called_count_ = 0;
    run_loop_ = std::make_unique<base::RunLoop>();
    written_data_.clear();
    moov_written_data_.clear();
  }

  MediaTracks* parsed_tracks() const { return tracks_.get(); }

  const StreamParser::BufferQueueMap& parsed_buffer_queue_map() const {
    return buffer_queue_map_;
  }

  mp4::BoxReader* moov_box_reader() const { return moov_reader_.get(); }

  int parsed_num_fragments() const { return new_fragment_count_; }

  base::test::TaskEnvironment task_environment_;
  std::vector<uint8_t> written_data_;
  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  void LoadEncodedFile(base::StringPiece filename,
                       base::MemoryMappedFile& mapped_stream) {
    base::FilePath file_path = GetTestDataFilePath(filename);
    ASSERT_TRUE(mapped_stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  }

  base::FilePath GetTestDataFilePath(base::StringPiece name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .AppendASCII(name);
    return file_path;
  }

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

  void PopulateAacAdts(std::vector<uint8_t>& code_description) {
    // copied from aac_unittest.cc.
    std::vector<uint8_t> test_data = {0x12, 0x10};
    code_description = test_data;
  }

  testing::StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<mp4::BoxReader> moov_reader_;
  std::vector<uint8_t> moov_written_data_;
  std::unique_ptr<MediaTracks> tracks_;
  StreamParser::BufferQueueMap buffer_queue_map_;
  int called_count_ = 0;
  int new_fragment_count_ = 0;
};

TEST_F(Mp4MuxerTest, CreateMp4Blob) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);
  AddAudioSamples(mp4_muxer.get(), 111, 2);
  AddVideoSamples(mp4_muxer.get(), 123, 2);

  FlushAndParse(std::move(mp4_muxer));

  CHECK_NE(parsed_tracks(), nullptr);
  size_t audio_config_count = 0;
  size_t video_config_count = 0;

  AudioDecoderConfig audio_decoder_config;
  VideoDecoderConfig video_decoder_config;

  for (const auto& track : parsed_tracks()->tracks()) {
    const auto& track_id = track->bytestream_track_id();
    if (track->type() == MediaTrack::Type::kAudio) {
      EXPECT_EQ(track_id, 1);
      audio_decoder_config = parsed_tracks()->getAudioConfig(track_id);
      audio_config_count++;
    } else if (track->type() == MediaTrack::Type::kVideo) {
      EXPECT_EQ(track_id, 2);
      video_decoder_config = parsed_tracks()->getVideoConfig(track_id);
      video_config_count++;
    } else {
      NOTREACHED();
    }
  }

  EXPECT_EQ(parsed_tracks()->GetAudioConfigs().size(), audio_config_count);
  EXPECT_EQ(parsed_tracks()->GetVideoConfigs().size(), video_config_count);
  EXPECT_EQ(audio_decoder_config.profile(), AudioCodecProfile::kUnknown);
  EXPECT_EQ(video_decoder_config.codec(), VideoCodec::kH264);
  EXPECT_EQ(video_decoder_config.natural_size(), gfx::Size(40, 30));
  EXPECT_EQ(video_decoder_config.profile(), H264PROFILE_HIGH);

  for (const auto& [track_id, buffer_queue] : parsed_buffer_queue_map()) {
    DCHECK(!buffer_queue.empty());
    // `buffer_queue` has respective sample.
    for (const auto& buf : buffer_queue) {
      EXPECT_EQ(track_id, buf->track_id());
      if (track_id == kAudioTrackId) {
        EXPECT_NE(buf->data_size(), 0u);
        EXPECT_NE(buf->duration().InMicroseconds(), 0u);
        EXPECT_EQ(buf->type(), DemuxerStream::AUDIO);
      } else if (track_id == kVideoTrackId) {
        EXPECT_NE(buf->data_size(), 0u);
        EXPECT_NE(buf->duration().InMicroseconds(), 0u);
        EXPECT_EQ(buf->type(), DemuxerStream::VIDEO);
      } else {
        NOTREACHED();
      }
    }
  }
}

TEST_F(Mp4MuxerTest, PauseAndResumeWithVideo) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  AddVideoSamples(mp4_muxer.get(), kStartVideoTimeticks, 1);

  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(500));
  mp4_muxer->Resume();

  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), kStartVideoTimeticks + 540, 1);

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(1u, 2u, 0u, /*video_track_id=*/1, 2);

  // (200 + 540) - 500) + 33 - 200 = 73.
  ValidateMovieHeaderDuration(73u);
}

TEST_F(Mp4MuxerTest, PauseAndResumeWithAudio) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1, true);

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks, 1);

  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(500));
  mp4_muxer->Resume();

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks + 540, 1);

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(1u, 0u, 2u);

  // (200 + 540) - 500) + 23 - 200 = 63.
  ValidateMovieHeaderDuration(63u);
}

TEST_F(Mp4MuxerTest, DoublePauseAndResumeWithAudio) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1, true);

  AddAudioSamples(mp4_muxer.get(), kStartVideoTimeticks, 1);

  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(300));
  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  mp4_muxer->Resume();

  AddAudioSamples(mp4_muxer.get(), kStartVideoTimeticks + 540, 1);

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(1u, 0u, 2u);

  // (200 + 540) - (300+200))) + 23 - 200 = 63.
  ValidateMovieHeaderDuration(63u);
}

TEST_F(Mp4MuxerTest, PauseWithoutResumeVideoAudio) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks, 1);
  AddVideoSamples(mp4_muxer.get(), kStartVideoTimeticks, 1);

  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(500));

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(2u, 1u, 1u);
  // single sample.
  ValidateMovieHeaderDuration(33u);
}

TEST_F(Mp4MuxerTest, PauseAndResumeWithVideoAudio) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks, 1);
  AddVideoSamples(mp4_muxer.get(), kStartVideoTimeticks, 1);

  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(500));
  mp4_muxer->Resume();

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks + 550, 2);
  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), kStartVideoTimeticks + 600, 2);

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(2u, 3u, 3u);

  // video track: (200 + 630) - 500) + 33 - 200 = 163.
  ValidateMovieHeaderDuration(163u);
}

TEST_F(Mp4MuxerTest, OutOfOrderSampleForVideoAudio) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks, 1);
  AddVideoSamples(mp4_muxer.get(), kStartVideoTimeticks, 1);

  mp4_muxer->Pause();
  task_environment_.FastForwardBy(base::Milliseconds(500));
  mp4_muxer->Resume();

  AddAudioSamples(mp4_muxer.get(), kStartAudioTimeticks + 400, 2);
  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), kStartVideoTimeticks + 400, 2);

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(2u, 3u, 3u);
}

TEST_F(Mp4MuxerTest, OutOfOrderDifferentVideoAudio) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  AddAudioSamples(mp4_muxer.get(), 100, 1);
  AddVideoSamples(mp4_muxer.get(), 100, 1);

  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), 190, 1);
  AddAudioSamples(mp4_muxer.get(), 150, 1);                // added.
  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), 140, 1);  // dropped.

  FlushAndParse(std::move(mp4_muxer));

  ValidateTrackAndSamplesCount(2u, 3u, 2u);
}

TEST_F(Mp4MuxerTest, ZeroDurationForOutOfOrderFrame) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  AddAudioSamples(mp4_muxer.get(), 100, 1);
  AddVideoSamples(mp4_muxer.get(), 100, 1);

  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), 90, 1);
  AddAudioSamples(mp4_muxer.get(), 80, 1);
  AddAudioSamples(mp4_muxer.get(), 90, 1);

  FlushAndParse(std::move(mp4_muxer));
  ValidateMovieHeaderDuration(33u);
}

TEST_F(Mp4MuxerTest, MaximumDurationWithInterval) {
  std::unique_ptr<Mp4Muxer> mp4_muxer = CreateMp4Muxer(true, true, 1);

  mp4_muxer->SetMaximumDurationToForceDataOutput(base::Milliseconds(1000));
  AddAudioSamples(mp4_muxer.get(), 100, 1);
  AddVideoSamples(mp4_muxer.get(), 100, 1);
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  // Will call Flush as the time is over maximum interval.
  AddVideoSamplesForNonKeyFrame(mp4_muxer.get(), 200, 1);
  run_loop_->Run();
  ParseWithMp4StreamParser(written_data_);
  ValidateMovieHeaderDuration(133u);

  AddAudioSamples(mp4_muxer.get(), 200, 1);
  AddVideoSamples(mp4_muxer.get(), 100, 1);
  AddAudioSamples(mp4_muxer.get(), 400, 1);

  InitCallbackVariables();
  FlushAndParse(std::move(mp4_muxer));
  ValidateMovieHeaderDuration(223u);
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace media
