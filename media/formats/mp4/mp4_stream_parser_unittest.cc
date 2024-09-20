// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/mp4_stream_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/fourccs.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {
namespace mp4 {
namespace {

// Useful in single-track test media cases that need to verify
// keyframe/non-keyframe sequence in output of parse.
enum class Keyframeness {
  kKeyframe = 0,
  kNonKeyframe,
};

// Tells gtest how to print our Keyframeness enum values.
std::ostream& operator<<(std::ostream& os, Keyframeness k) {
  return os << (k == Keyframeness::kKeyframe ? "kKeyframe" : "kNonKeyframe");
}

}  // namespace

// Matchers for verifying common media log entry strings.
MATCHER(SampleEncryptionInfoUnavailableLog, "") {
  return CONTAINS_STRING(arg, "Sample encryption info is not available.");
}

MATCHER_P(InfoLog, error_string, "") {
  return CONTAINS_STRING(arg, error_string) && CONTAINS_STRING(arg, "info");
}

MATCHER_P(ErrorLog, error_string, "") {
  return CONTAINS_STRING(arg, error_string) && CONTAINS_STRING(arg, "error");
}

MATCHER_P(DebugLog, debug_string, "") {
  return CONTAINS_STRING(arg, debug_string) && CONTAINS_STRING(arg, "debug");
}

class MP4StreamParserTest : public testing::Test {
 public:
  MP4StreamParserTest()
      : configs_received_(false),
        lower_bound_(kMaxDecodeTimestamp),
        verifying_keyframeness_sequence_(false) {
    base::flat_set<int> audio_object_types;
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(
        new MP4StreamParser(audio_object_types, false, false, false, false));
  }

 protected:
  StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<MP4StreamParser> parser_;
  bool configs_received_;
  std::unique_ptr<MediaTracks> media_tracks_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
  DecodeTimestamp lower_bound_;
  StreamParser::TrackId audio_track_id_;
  StreamParser::TrackId video_track_id_;
  bool verifying_keyframeness_sequence_;
  StrictMock<base::MockRepeatingCallback<void(Keyframeness)>> keyframeness_cb_;

  // Note this is similar to a StreamParserTestBase method, so may benefit from
  // utility method or inheritance if they don't diverge.
  bool AppendAllDataThenParseInPieces(base::span<const uint8_t> data,
                                      size_t piece_size) {
    EXPECT_TRUE(parser_->AppendToParseBuffer(data));

    // Also verify the expected number of pieces is needed to fully parse
    // `data`.
    size_t expected_remaining_data = data.size();
    bool has_more_data = true;

    while (has_more_data) {
      StreamParser::ParseStatus parse_result = parser_->Parse(piece_size);
      if (parse_result == StreamParser::ParseStatus::kFailed) {
        return false;
      }

      has_more_data =
          parse_result == StreamParser::ParseStatus::kSuccessHasMoreData;

      EXPECT_EQ(piece_size < expected_remaining_data, has_more_data);

      if (has_more_data) {
        expected_remaining_data -= piece_size;
      } else {
        EXPECT_EQ(parse_result, StreamParser::ParseStatus::kSuccess);
      }
    }

    return true;
  }

  void InitF(const StreamParser::InitParameters& expected_params,
             const StreamParser::InitParameters& params) {
    DVLOG(1) << "InitF: dur=" << params.duration.InMicroseconds();
    EXPECT_EQ(expected_params.duration, params.duration);
    EXPECT_EQ(expected_params.timeline_offset, params.timeline_offset);
    EXPECT_EQ(expected_params.liveness, params.liveness);
    EXPECT_EQ(expected_params.detected_audio_track_count,
              params.detected_audio_track_count);
    EXPECT_EQ(expected_params.detected_video_track_count,
              params.detected_video_track_count);
  }

  bool NewConfigF(std::unique_ptr<MediaTracks> tracks) {
    size_t audio_config_count = 0;
    size_t video_config_count = 0;
    configs_received_ = true;
    CHECK(tracks.get());
    DVLOG(1) << "NewConfigF: got " << tracks->tracks().size() << " tracks";
    for (const auto& track : tracks->tracks()) {
      const auto& track_id = track->stream_id();
      if (track->type() == MediaTrack::Type::kAudio) {
        audio_track_id_ = track_id;
        audio_decoder_config_ = tracks->getAudioConfig(track_id);
        DVLOG(1) << "track_id=" << track_id << " audio config="
                 << (audio_decoder_config_.IsValidConfig()
                         ? audio_decoder_config_.AsHumanReadableString()
                         : "INVALID");
        audio_config_count++;
      } else if (track->type() == MediaTrack::Type::kVideo) {
        video_track_id_ = track_id;
        video_decoder_config_ = tracks->getVideoConfig(track_id);
        DVLOG(1) << "track_id=" << track_id << " video config="
                 << (video_decoder_config_.IsValidConfig()
                         ? video_decoder_config_.AsHumanReadableString()
                         : "INVALID");
        video_config_count++;
      }
    }
    EXPECT_EQ(tracks->GetAudioConfigs().size(), audio_config_count);
    EXPECT_EQ(tracks->GetVideoConfigs().size(), video_config_count);
    media_tracks_ = std::move(tracks);
    return true;
  }

  bool NewBuffersF(const StreamParser::BufferQueueMap& buffer_queue_map) {
    DecodeTimestamp lowest_end_dts = kNoDecodeTimestamp;
    for (const auto& [track_id, buffer_queue] : buffer_queue_map) {
      DVLOG(3) << "Buffers for track_id=" << track_id;
      DCHECK(!buffer_queue.empty());

      if (lowest_end_dts == kNoDecodeTimestamp ||
          lowest_end_dts > buffer_queue.back()->GetDecodeTimestamp())
        lowest_end_dts = buffer_queue.back()->GetDecodeTimestamp();

      for (const auto& buf : buffer_queue) {
        DVLOG(3) << "  track_id=" << buf->track_id() << ", size=" << buf->size()
                 << ", pts=" << buf->timestamp().InSecondsF()
                 << ", dts=" << buf->GetDecodeTimestamp().InSecondsF()
                 << ", dur=" << buf->duration().InSecondsF();
        // Ensure that track ids are properly assigned on all emitted buffers.
        EXPECT_EQ(track_id, buf->track_id());

        // Let single-track tests verify the sequence of keyframes/nonkeyframes.
        if (verifying_keyframeness_sequence_) {
          keyframeness_cb_.Run(buf->is_key_frame()
                                   ? Keyframeness::kKeyframe
                                   : Keyframeness::kNonKeyframe);
        }
      }
    }

    EXPECT_NE(lowest_end_dts, kNoDecodeTimestamp);

    if (lower_bound_ != kNoDecodeTimestamp && lowest_end_dts < lower_bound_) {
      return false;
    }

    lower_bound_ = lowest_end_dts;
    return true;
  }

  void KeyNeededF(EmeInitDataType type, const std::vector<uint8_t>& init_data) {
    DVLOG(1) << "KeyNeededF: " << init_data.size();
    EXPECT_EQ(EmeInitDataType::CENC, type);
    EXPECT_FALSE(init_data.empty());
  }

  void NewSegmentF() {
    DVLOG(1) << "NewSegmentF";
    lower_bound_ = kNoDecodeTimestamp;
  }

  void EndOfSegmentF() {
    DVLOG(1) << "EndOfSegmentF()";
    lower_bound_ = kMaxDecodeTimestamp;
  }

  void InitializeParserWithInitParametersExpectations(
      StreamParser::InitParameters params) {
    parser_->Init(base::BindOnce(&MP4StreamParserTest::InitF,
                                 base::Unretained(this), params),
                  base::BindRepeating(&MP4StreamParserTest::NewConfigF,
                                      base::Unretained(this)),
                  base::BindRepeating(&MP4StreamParserTest::NewBuffersF,
                                      base::Unretained(this)),
                  base::BindRepeating(&MP4StreamParserTest::KeyNeededF,
                                      base::Unretained(this)),
                  base::BindRepeating(&MP4StreamParserTest::NewSegmentF,
                                      base::Unretained(this)),
                  base::BindRepeating(&MP4StreamParserTest::EndOfSegmentF,
                                      base::Unretained(this)),
                  &media_log_);
  }

  StreamParser::InitParameters GetDefaultInitParametersExpectations() {
    // Most unencrypted test mp4 files have zero duration and are treated as
    // live streams.
    StreamParser::InitParameters params(kInfiniteDuration);
    params.liveness = StreamLiveness::kLive;
    params.detected_audio_track_count = 1;
    params.detected_video_track_count = 1;
    return params;
  }

  void InitializeParserAndExpectLiveness(StreamLiveness liveness) {
    auto params = GetDefaultInitParametersExpectations();
    params.liveness = liveness;
    InitializeParserWithInitParametersExpectations(params);
  }

  void InitializeParser() {
    InitializeParserWithInitParametersExpectations(
        GetDefaultInitParametersExpectations());
  }

  // Note this is also similar to a StreamParserTestBase method.
  bool ParseMP4File(const std::string& filename, int append_bytes) {
    CHECK_GE(append_bytes, 0);
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);

    size_t start = 0;
    size_t end = buffer->size();
    do {
      size_t chunk_size = std::min(static_cast<size_t>(append_bytes),
                                   static_cast<size_t>(end - start));
      // Attempt to incrementally parse each appended chunk to test out the
      // parser's internal management of input queue and pending data bytes.
      EXPECT_TRUE(AppendAllDataThenParseInPieces(
          buffer->AsSpan().subspan(start, chunk_size),
          (chunk_size > 7) ? (chunk_size - 7) : chunk_size));
      start += chunk_size;
    } while (start < end);

    return true;
  }
};

TEST_F(MP4StreamParserTest, UnalignedAppend) {
  // Test small, non-segment-aligned appends (small enough to exercise
  // incremental append system)
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, BytewiseAppend) {
  // Ensure no incremental errors occur when parsing
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 1);
}

TEST_F(MP4StreamParserTest, MultiFragmentAppend) {
  // Large size ensures multiple fragments are appended in one call (size is
  // larger than this particular test file)
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 768432);
}

TEST_F(MP4StreamParserTest, Flush) {
  // Flush while reading sample data, then start a new stream.
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(
      AppendAllDataThenParseInPieces(buffer->AsSpan().first(65536u), 512));
  parser_->Flush();
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, Reinitialization) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, UnknownDuration_V0_AllBitsSet) {
  InitializeParser();
  // 32 bit duration field in mvhd box, all bits set.
  ParseMP4File(
      "bear-1280x720-av_frag-initsegment-mvhd_version_0-mvhd_duration_bits_all_"
      "set.mp4",
      512);
}

TEST_F(MP4StreamParserTest, AVC_KeyAndNonKeyframeness_Match_Container) {
  // Both AVC video frames' keyframe-ness metadata matches the MP4:
  // Frame 0: AVC IDR, trun.first_sample_flags: sync sample that doesn't
  //          depend on others.
  // Frame 1: AVC Non-IDR, tfhd.default_sample_flags: not sync sample, depends
  //          on others.
  // This is the base case; see also the "Mismatches" cases, below.
  InSequence s;  // The EXPECT* sequence matters for this test.
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  verifying_keyframeness_sequence_ = true;
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe));
  ParseMP4File("bear-640x360-v-2frames_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, AVC_Keyframeness_Mismatches_Container) {
  // The first AVC video frame's keyframe-ness metadata mismatches the MP4:
  // Frame 0: AVC IDR, trun.first_sample_flags: NOT sync sample, DEPENDS on
  //          others.
  // Frame 1: AVC Non-IDR, tfhd.default_sample_flags: not sync sample, depends
  //          on others.
  InSequence s;  // The EXPECT* sequence matters for this test.
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  verifying_keyframeness_sequence_ = true;
  EXPECT_MEDIA_LOG(DebugLog(
      "ISO-BMFF container metadata for video frame indicates that the frame is "
      "not a keyframe, but the video frame contents indicate the opposite."));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe));
  ParseMP4File("bear-640x360-v-2frames-keyframe-is-non-sync-sample_frag.mp4",
               512);
}

TEST_F(MP4StreamParserTest, AVC_NonKeyframeness_Mismatches_Container) {
  // The second AVC video frame's keyframe-ness metadata mismatches the MP4:
  // Frame 0: AVC IDR, trun.first_sample_flags: sync sample that doesn't
  //          depend on others.
  // Frame 1: AVC Non-IDR, tfhd.default_sample_flags: SYNC sample, DOES NOT
  //          depend on others.
  InSequence s;  // The EXPECT* sequence matters for this test.
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  verifying_keyframeness_sequence_ = true;
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_MEDIA_LOG(DebugLog(
      "ISO-BMFF container metadata for video frame indicates that the frame is "
      "a keyframe, but the video frame contents indicate the opposite."));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe));
  ParseMP4File("bear-640x360-v-2frames-nonkeyframe-is-sync-sample_frag.mp4",
               512);
}

TEST_F(MP4StreamParserTest, MPEG2_AAC_LC) {
  InSequence s;
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kISO_13818_7_AAC_LC);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));
  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bear-mpeg2-aac-only_frag.mp4", 512);
  EXPECT_EQ(audio_decoder_config_.profile(), AudioCodecProfile::kUnknown);
}

TEST_F(MP4StreamParserTest, ParsingAACLCNoAudioTypeStrictness) {
  InSequence s;
  parser_.reset(new MP4StreamParser(std::nullopt, false, false, false, false));
  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bear-mpeg2-aac-only_frag.mp4", 512);
  EXPECT_EQ(audio_decoder_config_.profile(), AudioCodecProfile::kUnknown);
}

TEST_F(MP4StreamParserTest, MPEG4_XHE_AAC) {
  InSequence s;  // The keyframeness sequence matters for this test.
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kISO_14496_3);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));
  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;

  InitializeParserWithInitParametersExpectations(params);

  // This test file contains a single audio keyframe followed by 23
  // non-keyframes.
  verifying_keyframeness_sequence_ = true;
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe)).Times(23);

  ParseMP4File("noise-xhe-aac.mp4", 512);
  EXPECT_EQ(audio_decoder_config_.profile(), AudioCodecProfile::kXHE_AAC);
}

// Test that a moov box is not always required after Flush() is called.
TEST_F(MP4StreamParserTest, NoMoovAfterFlush) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
  parser_->Flush();

  const int kFirstMoofOffset = 1307;
  EXPECT_TRUE(AppendAllDataThenParseInPieces(
      buffer->AsSpan().subspan(kFirstMoofOffset,
                               buffer->size() - kFirstMoofOffset),
      512));
}

// Test an invalid file where there are encrypted samples, but
// SampleEncryptionBox (senc) and SampleAuxiliaryInformation{Sizes|Offsets}Box
// (saiz|saio) are missing.
// The parser should fail instead of crash. See http://crbug.com/361347
TEST_F(MP4StreamParserTest, MissingSampleEncryptionInfo) {
  InSequence s;

  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(23219);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-a_frag-cenc_missing-saiz-saio.mp4");
  EXPECT_MEDIA_LOG(SampleEncryptionInfoUnavailableLog());
  EXPECT_FALSE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

// Test a file where all video samples start with an Access Unit
// Delimiter (AUD) NALU.
TEST_F(MP4StreamParserTest, VideoSamplesStartWithAUDs) {
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bear-1280x720-av_with-aud-nalus_frag.mp4", 512);
}

TEST_F(MP4StreamParserTest, HEVC_in_MP4_container) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported VisualSampleEntry type hev1"));
#endif
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(1002000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear-hevc-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  EXPECT_EQ(VideoCodec::kHEVC, video_decoder_config_.codec());
  EXPECT_EQ(HEVCPROFILE_MAIN, video_decoder_config_.profile());
#endif
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
TEST_F(MP4StreamParserTest, HEVC_KeyAndNonKeyframeness_Match_Container) {
  // Both HEVC video frames' keyframe-ness metadata matches the MP4:
  // Frame 0: HEVC IDR, trun.first_sample_flags: sync sample that doesn't
  //          depend on others.
  // Frame 1: HEVC Non-IDR, tfhd.default_sample_flags: not sync sample, depends
  //          on others.
  // This is the base case; see also the "Mismatches" cases, below.
  InSequence s;  // The EXPECT* sequence matters for this test.
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  verifying_keyframeness_sequence_ = true;
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe));
  ParseMP4File("bear-320x240-v-2frames_frag-hevc.mp4", 256);
}

TEST_F(MP4StreamParserTest, HEVC_Keyframeness_Mismatches_Container) {
  // The first HEVC video frame's keyframe-ness metadata mismatches the MP4:
  // Frame 0: HEVC IDR, trun.first_sample_flags: NOT sync sample, DEPENDS on
  //          others.
  // Frame 1: HEVC Non-IDR, tfhd.default_sample_flags: not sync sample, depends
  //          on others.
  InSequence s;  // The EXPECT* sequence matters for this test.
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  verifying_keyframeness_sequence_ = true;
  EXPECT_MEDIA_LOG(DebugLog(
      "ISO-BMFF container metadata for video frame indicates that the frame is "
      "not a keyframe, but the video frame contents indicate the opposite."));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe));
  ParseMP4File(
      "bear-320x240-v-2frames-keyframe-is-non-sync-sample_frag-hevc.mp4", 256);
}

TEST_F(MP4StreamParserTest, HEVC_NonKeyframeness_Mismatches_Container) {
  // The second HEVC video frame's keyframe-ness metadata mismatches the MP4:
  // Frame 0: HEVC IDR, trun.first_sample_flags: sync sample that doesn't
  //          depend on others.
  // Frame 1: HEVC Non-IDR, tfhd.default_sample_flags: SYNC sample, DOES NOT
  //          depend on others.
  InSequence s;  // The EXPECT* sequence matters for this test.
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);
  verifying_keyframeness_sequence_ = true;
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kKeyframe));
  EXPECT_MEDIA_LOG(DebugLog(
      "ISO-BMFF container metadata for video frame indicates that the frame is "
      "a keyframe, but the video frame contents indicate the opposite."));
  EXPECT_CALL(keyframeness_cb_, Run(Keyframeness::kNonKeyframe));
  ParseMP4File(
      "bear-320x240-v-2frames-nonkeyframe-is-sync-sample_frag-hevc.mp4", 256);
}
#endif

// Sample encryption information is stored as CencSampleAuxiliaryDataFormat
// (ISO/IEC 23001-7:2015 8) inside 'mdat' box. No SampleEncryption ('senc') box.
TEST_F(MP4StreamParserTest, CencWithEncryptionInfoStoredAsAuxDataInMdat) {
  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2736066);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, CencWithSampleEncryptionBox) {
  // Encrypted test mp4 files have non-zero duration and are treated as
  // recorded streams.
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2736066);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-v_frag-cenc-senc.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, NaturalSizeWithoutPASP) {
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(1000966);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-non_square_pixel-without_pasp.mp4");

  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
  EXPECT_EQ(gfx::Size(639, 360), video_decoder_config_.natural_size());
}

TEST_F(MP4StreamParserTest, NaturalSizeWithPASP) {
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(1000966);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-640x360-non_square_pixel-with_pasp.mp4");

  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
  EXPECT_EQ(gfx::Size(639, 360), video_decoder_config_.natural_size());
}

TEST_F(MP4StreamParserTest, DemuxingDVProfile5WithDVMimeTypeSourceBuffer) {
  base::flat_set<int> audio_object_types;
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, true));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported VisualSampleEntry type dvh1"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2000000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("glass-blowing2-dolby-vision-profile-5-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  EXPECT_EQ(VideoCodec::kDolbyVision, video_decoder_config_.codec());
  EXPECT_EQ(DOLBYVISION_PROFILE5, video_decoder_config_.profile());
#endif
}

TEST_F(MP4StreamParserTest, DemuxingDVProfile5WithHEVCMimeTypeSourceBuffer) {
  base::flat_set<int> audio_object_types;
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported VisualSampleEntry type dvh1"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2000000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("glass-blowing2-dolby-vision-profile-5-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  EXPECT_EQ(VideoCodec::kDolbyVision, video_decoder_config_.codec());
  EXPECT_EQ(DOLBYVISION_PROFILE5, video_decoder_config_.profile());
#endif
}

TEST_F(MP4StreamParserTest, DemuxingDVProfile8WithDVMimeTypeSourceBuffer) {
  base::flat_set<int> audio_object_types;
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, true));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported VisualSampleEntry type hvc1"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2000000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("glass-blowing2-dolby-vision-profile-8-1-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  EXPECT_EQ(VideoCodec::kDolbyVision, video_decoder_config_.codec());
  EXPECT_EQ(DOLBYVISION_PROFILE8, video_decoder_config_.profile());
#endif
}

TEST_F(MP4StreamParserTest, DemuxingDVProfile8WithHEVCMimeTypeSourceBuffer) {
  base::flat_set<int> audio_object_types;
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  bool expect_success = true;
  EXPECT_MEDIA_LOG(InfoLog(
      "Dolby Vision video track with track_id=1 is using cross-compatible "
      "codec: hevc. To prevent this, where Dolby Vision is supported, use a "
      "Dolby Vision codec string when constructing the SourceBuffer."));
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported VisualSampleEntry type hvc1"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2000000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("glass-blowing2-dolby-vision-profile-8-1-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  EXPECT_EQ(VideoCodec::kHEVC, video_decoder_config_.codec());
  EXPECT_EQ(HEVCPROFILE_MAIN10, video_decoder_config_.profile());
#endif
}

TEST_F(MP4StreamParserTest, DemuxingAC3) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kAC3);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x61632d33 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(1045000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-ac3-only-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingEAC3) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kEAC3);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x65632d33 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(1045000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-eac3-only-frag.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingAc4Ims) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kAC4);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  constexpr bool kExpectSuccess = true;
#else
  constexpr bool kExpectSuccess = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x61632d34 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2432000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("ac4-only-ims-frag.mp4");
  EXPECT_EQ(kExpectSuccess,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingAc4AJoc) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kAC4);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  constexpr bool kExpectSuccess = true;
#else
  constexpr bool kExpectSuccess = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x61632d34 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(2135000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("ac4-only-ajoc-frag.mp4");
  EXPECT_EQ(kExpectSuccess,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingAc4ChannelBasedCoding) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kAC4);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  constexpr bool kExpectSuccess = true;
#else
  constexpr bool kExpectSuccess = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x61632d34 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(4087000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("ac4-only-channel-based-coding-frag.mp4");
  EXPECT_EQ(kExpectSuccess,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingDTS) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kDTS);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x64747363 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(3222000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear_dtsc.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingDTSE) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kDTSE);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x64747365 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(3243000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear_dtse.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, DemuxingDTSX) {
  base::flat_set<int> audio_object_types;
  audio_object_types.insert(kDTSX);
  parser_.reset(
      new MP4StreamParser(audio_object_types, false, false, false, false));

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  bool expect_success = true;
#else
  bool expect_success = false;
  EXPECT_MEDIA_LOG(ErrorLog("Unsupported audio format 0x64747378 in stsd box"));
#endif

  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Microseconds(3222000);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear_dtsx.mp4");
  EXPECT_EQ(expect_success,
            AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, Flac) {
  parser_.reset(
      new MP4StreamParser(base::flat_set<int>(), false, true, false, false));

  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("bear-flac_frag.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, Flac192kHz) {
  parser_.reset(
      new MP4StreamParser(base::flat_set<int>(), false, true, false, false));

  auto params = GetDefaultInitParametersExpectations();
  params.detected_video_track_count = 0;

  // 192kHz exceeds the range of AudioSampleEntry samplerate. The correct
  // samplerate should be applied from the dfLa STREAMINFO metadata block.
  EXPECT_MEDIA_LOG(FlacAudioSampleRateOverriddenByStreaminfo("0", "192000"));
  InitializeParserWithInitParametersExpectations(params);

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear-flac-192kHz_frag.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));
}

TEST_F(MP4StreamParserTest, VideoColorSpaceInvalidValues) {
  ColorParameterInformation invalid;
  invalid.colour_primaries = 1234;
  invalid.transfer_characteristics = 42;
  invalid.matrix_coefficients = 999;
  invalid.full_range = true;
  invalid.fully_parsed = true;
  MediaSerialize(
      VideoSampleEntry::ConvertColorParameterInformationToColorSpace(invalid));
}

TEST_F(MP4StreamParserTest, Vp9) {
  auto params = GetDefaultInitParametersExpectations();
  params.detected_audio_track_count = 0;
  InitializeParserWithInitParametersExpectations(params);

  auto buffer = ReadTestDataFile("vp9-hdr-init-segment.mp4");
  EXPECT_TRUE(AppendAllDataThenParseInPieces(buffer->AsSpan(), 512));

  EXPECT_EQ(video_decoder_config_.profile(), VP9PROFILE_PROFILE2);
  EXPECT_EQ(video_decoder_config_.level(), 31u);
  EXPECT_EQ(video_decoder_config_.color_space_info(),
            VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                            VideoColorSpace::TransferID::SMPTEST2084,
                            VideoColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::LIMITED));

  ASSERT_TRUE(video_decoder_config_.hdr_metadata().has_value());

  const auto& hdr_metadata = *video_decoder_config_.hdr_metadata();
  EXPECT_EQ(hdr_metadata.cta_861_3->max_content_light_level, 1000u);
  EXPECT_EQ(hdr_metadata.cta_861_3->max_frame_average_light_level, 640u);

  const auto& smpte_st_2086 = hdr_metadata.smpte_st_2086.value();
  const auto& primaries = smpte_st_2086.primaries;

  constexpr float kColorCoordinateUnit = 1 / 16.0f;
  EXPECT_NEAR(primaries.fRX, 0.68, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fRY, 0.31998, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fGX, 0.26496, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fGY, 0.68998, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fBX, 0.15, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fBY, 0.05998, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fWX, 0.314, kColorCoordinateUnit);
  EXPECT_NEAR(primaries.fWY, 0.351, kColorCoordinateUnit);

  constexpr float kLuminanceMaxUnit = 1 / 8.0f;
  EXPECT_NEAR(smpte_st_2086.luminance_max, 1000.0f, kLuminanceMaxUnit);

  constexpr float kLuminanceMinUnit = 1 / 14.0;
  EXPECT_NEAR(smpte_st_2086.luminance_min, 0.01f, kLuminanceMinUnit);
}

TEST_F(MP4StreamParserTest, FourCCToString) {
  // A real FOURCC should print.
  EXPECT_EQ("mvex", FourCCToString(FOURCC_MVEX));

  // Invalid FOURCC should also print whenever ASCII values are printable.
  EXPECT_EQ("fake", FourCCToString(static_cast<FourCC>(0x66616b65)));

  // Invalid FORCC with non-printable values should not give error message.
  EXPECT_EQ("0x66616b00", FourCCToString(static_cast<FourCC>(0x66616b00)));
}

TEST_F(MP4StreamParserTest, MediaTrackInfoSourcing) {
  InitializeParser();
  ParseMP4File("bear-1280x720-av_frag.mp4", 4096);

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);
  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Type::kVideo);
  EXPECT_EQ(video_track.stream_id(), 1);
  EXPECT_EQ(video_track.kind().value(), "main");
  EXPECT_EQ(video_track.label().value(), "VideoHandler");
  EXPECT_EQ(video_track.language().value(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Type::kAudio);
  EXPECT_EQ(audio_track.stream_id(), 2);
  EXPECT_EQ(audio_track.kind().value(), "main");
  EXPECT_EQ(audio_track.label().value(), "SoundHandler");
  EXPECT_EQ(audio_track.language().value(), "und");
}

TEST_F(MP4StreamParserTest, MultiTrackFile) {
  auto params = GetDefaultInitParametersExpectations();
  params.duration = base::Milliseconds(4248);
  params.liveness = StreamLiveness::kRecorded;
  params.detected_audio_track_count = 2;
  params.detected_video_track_count = 2;
  InitializeParserWithInitParametersExpectations(params);
  ParseMP4File("bbb-320x240-2video-2audio.mp4", 4096);

  EXPECT_EQ(media_tracks_->tracks().size(), 4u);

  const MediaTrack& video_track1 = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track1.type(), MediaTrack::Type::kVideo);
  EXPECT_EQ(video_track1.stream_id(), 1);
  EXPECT_EQ(video_track1.kind().value(), "main");
  EXPECT_EQ(video_track1.label().value(), "VideoHandler");
  EXPECT_EQ(video_track1.language().value(), "und");

  const MediaTrack& audio_track1 = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track1.type(), MediaTrack::Type::kAudio);
  EXPECT_EQ(audio_track1.stream_id(), 2);
  EXPECT_EQ(audio_track1.kind().value(), "main");
  EXPECT_EQ(audio_track1.label().value(), "SoundHandler");
  EXPECT_EQ(audio_track1.language().value(), "und");

  const MediaTrack& video_track2 = *(media_tracks_->tracks()[2]);
  EXPECT_EQ(video_track2.type(), MediaTrack::Type::kVideo);
  EXPECT_EQ(video_track2.stream_id(), 3);
  EXPECT_EQ(video_track2.kind().value(), "");
  EXPECT_EQ(video_track2.label().value(), "VideoHandler");
  EXPECT_EQ(video_track2.language().value(), "und");

  const MediaTrack& audio_track2 = *(media_tracks_->tracks()[3]);
  EXPECT_EQ(audio_track2.type(), MediaTrack::Type::kAudio);
  EXPECT_EQ(audio_track2.stream_id(), 4);
  EXPECT_EQ(audio_track2.kind().value(), "");
  EXPECT_EQ(audio_track2.label().value(), "SoundHandler");
  EXPECT_EQ(audio_track2.language().value(), "und");
}

// <cos(θ), sin(θ), θ expressed as a rotation Enum>
using MatrixRotationTestCaseParam =
    std::tuple<double, double, VideoTransformation>;

class MP4StreamParserRotationMatrixEvaluatorTest
    : public ::testing::TestWithParam<MatrixRotationTestCaseParam> {
 public:
  MP4StreamParserRotationMatrixEvaluatorTest() {
    base::flat_set<int> audio_object_types;
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(
        new MP4StreamParser(audio_object_types, false, false, false, false));
  }

 protected:
  std::unique_ptr<MP4StreamParser> parser_;
};

TEST_P(MP4StreamParserRotationMatrixEvaluatorTest, RotationCalculation) {
  TrackHeader track_header;
  MovieHeader movie_header;

  // Identity matrix, with 16.16 and 2.30 fixed points.
  uint32_t identity_matrix[9] = {1 << 16, 0, 0, 0, 1 << 16, 0, 0, 0, 1 << 30};

  memcpy(movie_header.display_matrix, identity_matrix, sizeof(identity_matrix));
  memcpy(track_header.display_matrix, identity_matrix, sizeof(identity_matrix));

  MatrixRotationTestCaseParam data = GetParam();

  // Insert fixed point decimal data into the rotation matrix.
  track_header.display_matrix[0] = std::get<0>(data) * (1 << 16);
  track_header.display_matrix[4] = std::get<0>(data) * (1 << 16);
  track_header.display_matrix[1] = -(std::get<1>(data) * (1 << 16));
  track_header.display_matrix[3] = std::get<1>(data) * (1 << 16);

  VideoTransformation expected = std::get<2>(data);
  VideoTransformation actual =
      parser_->CalculateRotation(track_header, movie_header);
  EXPECT_EQ(actual.rotation, expected.rotation);
  EXPECT_EQ(actual.mirrored, expected.mirrored);
}

MatrixRotationTestCaseParam rotation_test_cases[6] = {
    {1, 0, VideoTransformation(VIDEO_ROTATION_0)},  // cos(0)  = 1, sin(0)  = 0
    {0, -1,
     VideoTransformation(VIDEO_ROTATION_90)},  // cos(90) = 0, sin(90) =-1
    {-1, 0,
     VideoTransformation(VIDEO_ROTATION_180)},  // cos(180)=-1, sin(180)= 0
    {0, 1,
     VideoTransformation(VIDEO_ROTATION_270)},      // cos(270)= 0, sin(270)= 1
    {1, 1, VideoTransformation(VIDEO_ROTATION_0)},  // Error case
    {5, 5, VideoTransformation(VIDEO_ROTATION_0)},  // Error case
};
INSTANTIATE_TEST_SUITE_P(CheckMath,
                         MP4StreamParserRotationMatrixEvaluatorTest,
                         testing::ValuesIn(rotation_test_cases));

}  // namespace mp4
}  // namespace media
