// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_cluster_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/webm/cluster_builder.h"
#include "media/formats/webm/opus_packet_builder.h"
#include "media/formats/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Mock;
using ::testing::_;

namespace media {

// Matchers for verifying common media log entry strings.
MATCHER_P(OpusPacketDurationTooHigh, actual_duration_ms, "") {
  return CONTAINS_STRING(
      arg, "Warning, demuxed Opus packet with encoded duration: " +
               base::NumberToString(static_cast<int64_t>(actual_duration_ms)) +
               "ms. Should be no greater than 120ms.");
}

MATCHER_P2(WebMBlockDurationMismatchesOpusDuration,
           block_duration_ms,
           opus_duration_ms,
           "") {
  return CONTAINS_STRING(
      arg, "BlockDuration (" +
               base::NumberToString(static_cast<int64_t>(block_duration_ms)) +
               "ms) differs significantly from encoded duration (" +
               base::NumberToString(static_cast<int64_t>(opus_duration_ms)) +
               "ms).");
}

namespace {

// Timecode scale for millisecond timestamps.
const int kTimecodeScale = 1000000;

const int kAudioTrackNum = 1;
const int kVideoTrackNum = 2;
const int kTextTrackNum = 3;
constexpr double kTestAudioFrameDefaultDurationInMs = 13;
constexpr double kTestVideoFrameDefaultDurationInMs = 17;

// Test duration defaults must differ from parser estimation defaults to know
// which durations parser used when emitting buffers.
static_assert(
    static_cast<int>(kTestAudioFrameDefaultDurationInMs) !=
        static_cast<int>(WebMClusterParser::kDefaultAudioBufferDurationInMs),
    "test default is the same as estimation fallback audio duration");
static_assert(
    static_cast<int>(kTestVideoFrameDefaultDurationInMs) !=
        static_cast<int>(WebMClusterParser::kDefaultVideoBufferDurationInMs),
    "test default is the same as estimation fallback video duration");

struct BlockInfo {
  int track_num;
  int timestamp;

  // Negative value is allowed only for block groups (not simple blocks) and
  // directs CreateCluster() to exclude BlockDuration entry from the cluster for
  // this BlockGroup. The absolute value is used for parser verification.
  // For simple blocks, this value must be non-negative, and is used only for
  // parser verification.
  double duration;

  bool use_simple_block;

  // Default data will be used if no data given.
  const uint8_t* data;
  int data_length;

  bool is_key_frame;
};

const BlockInfo kDefaultBlockInfo[] = {
    {kAudioTrackNum, 0, 23, true, NULL, 0, true},
    {kAudioTrackNum, 23, 23, true, NULL, 0, true},
    // Assumes not using DefaultDuration
    {kVideoTrackNum, 33, 34, true, NULL, 0, true},
    {kAudioTrackNum, 46, 23, true, NULL, 0, false},
    {kVideoTrackNum, 67, 33, false, NULL, 0, true},
    {kAudioTrackNum, 69, 23, false, NULL, 0, false},
    {kVideoTrackNum, 100, 33, false, NULL, 0, false},
};

const uint8_t kEncryptedFrame[] = {
    // Block is encrypted
    0x01,

    // IV
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

std::unique_ptr<Cluster> CreateCluster(int timecode,
                                       const BlockInfo* block_info,
                                       int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  uint8_t kDefaultBlockData[] = { 0x00 };

  for (int i = 0; i < block_count; i++) {
    const uint8_t* data;
    int data_length;
    if (block_info[i].data != NULL) {
      data = block_info[i].data;
      data_length = block_info[i].data_length;
    } else {
      data = kDefaultBlockData;
      data_length = sizeof(kDefaultBlockData);
    }

    if (block_info[i].use_simple_block) {
      CHECK_GE(block_info[i].duration, 0);
      cb.AddSimpleBlock(block_info[i].track_num, block_info[i].timestamp,
                        block_info[i].is_key_frame ? 0x80 : 0x00, data,
                        data_length);
      continue;
    }

    if (block_info[i].duration < 0) {
      cb.AddBlockGroupWithoutBlockDuration(
          block_info[i].track_num, block_info[i].timestamp, 0,
          block_info[i].is_key_frame, data, data_length);
      continue;
    }

    cb.AddBlockGroup(block_info[i].track_num, block_info[i].timestamp,
                     block_info[i].duration, 0, block_info[i].is_key_frame,
                     data, data_length);
  }

  return cb.Finish();
}

// Creates a Cluster with one encrypted Block. |bytes_to_write| is number of
// bytes of the encrypted frame to write.
std::unique_ptr<Cluster> CreateEncryptedCluster(int bytes_to_write) {
  CHECK_GT(bytes_to_write, 0);
  CHECK_LE(bytes_to_write, static_cast<int>(sizeof(kEncryptedFrame)));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  cb.AddSimpleBlock(kVideoTrackNum, 0, 0, kEncryptedFrame, bytes_to_write);
  return cb.Finish();
}

bool VerifyBuffers(const StreamParser::BufferQueueMap& buffer_queue_map,
                   const BlockInfo* block_info,
                   int block_count) {
  int buffer_count = 0;
  for (const auto& [track_id, buffer_queue] : buffer_queue_map)
    buffer_count += buffer_queue.size();
  if (block_count != buffer_count) {
    DVLOG(1) << __func__ << " : block_count (" << block_count
             << ") mismatches buffer_count (" << buffer_count << ")";
    return false;
  }

  size_t audio_offset = 0;
  size_t video_offset = 0;
  for (int i = 0; i < block_count; i++) {
    const StreamParser::BufferQueue* buffers = NULL;
    size_t* offset;
    StreamParserBuffer::Type expected_type = DemuxerStream::UNKNOWN;

    const auto& it = buffer_queue_map.find(block_info[i].track_num);
    EXPECT_NE(buffer_queue_map.end(), it);
    buffers = &it->second;
    if (block_info[i].track_num == kAudioTrackNum) {
      offset = &audio_offset;
      expected_type = DemuxerStream::AUDIO;
    } else if (block_info[i].track_num == kVideoTrackNum) {
      offset = &video_offset;
      expected_type = DemuxerStream::VIDEO;
    } else {
      LOG(ERROR) << "Unexpected track number " << block_info[i].track_num;
      return false;
    }

    if (*offset >= buffers->size()) {
      DVLOG(1) << __func__ << " : Too few buffers (" << buffers->size()
               << ") for track_num (" << block_info[i].track_num
               << "), expected at least " << *offset + 1 << " buffers";
      return false;
    }

    scoped_refptr<StreamParserBuffer> buffer = (*buffers)[(*offset)++];

    EXPECT_EQ(block_info[i].timestamp, buffer->timestamp().InMilliseconds());
    EXPECT_EQ(std::abs(block_info[i].duration),
              buffer->duration().InMillisecondsF());
    EXPECT_EQ(expected_type, buffer->type());
    EXPECT_EQ(block_info[i].track_num, buffer->track_id());
    EXPECT_EQ(block_info[i].is_key_frame, buffer->is_key_frame());
  }

  return true;
}

bool VerifyBuffers(const std::unique_ptr<WebMClusterParser>& parser,
                   const BlockInfo* block_info,
                   int block_count) {
  StreamParser::BufferQueueMap buffers;
  parser->GetBuffers(&buffers);
  return VerifyBuffers(buffers, block_info, block_count);
}

void VerifyEncryptedBuffer(scoped_refptr<StreamParserBuffer> buffer) {
  EXPECT_TRUE(buffer->decrypt_config());
  EXPECT_EQ(static_cast<unsigned long>(DecryptConfig::kDecryptionKeySize),
            buffer->decrypt_config()->iv().length());
}

void AppendToEnd(const StreamParser::BufferQueue& src,
                 StreamParser::BufferQueue* dest) {
  for (StreamParser::BufferQueue::const_iterator itr = src.begin();
       itr != src.end(); ++itr) {
    dest->push_back(*itr);
  }
}

}  // namespace

class WebMClusterParserTest : public testing::Test {
 public:
  WebMClusterParserTest() : parser_(CreateDefaultParser()) {}

  WebMClusterParserTest(const WebMClusterParserTest&) = delete;
  WebMClusterParserTest& operator=(const WebMClusterParserTest&) = delete;

 protected:
  void ResetParserToHaveDefaultDurations() {
    base::TimeDelta default_audio_duration =
        base::Milliseconds(kTestAudioFrameDefaultDurationInMs);
    base::TimeDelta default_video_duration =
        base::Milliseconds(kTestVideoFrameDefaultDurationInMs);
    ASSERT_GE(default_audio_duration, base::TimeDelta());
    ASSERT_GE(default_video_duration, base::TimeDelta());
    ASSERT_NE(kNoTimestamp, default_audio_duration);
    ASSERT_NE(kNoTimestamp, default_video_duration);

    parser_.reset(CreateParserWithDefaultDurations(default_audio_duration,
                                                   default_video_duration));
  }

  // Helper that hard-codes some non-varying constructor parameters.
  WebMClusterParser* CreateParserHelper(
      base::TimeDelta audio_default_duration,
      base::TimeDelta video_default_duration,
      const std::set<int64_t>& ignored_tracks,
      const std::string& audio_encryption_key_id,
      const std::string& video_encryption_key_id,
      const AudioCodec audio_codec) {
    return new WebMClusterParser(
        kTimecodeScale, kAudioTrackNum, audio_default_duration, kVideoTrackNum,
        video_default_duration, ignored_tracks, audio_encryption_key_id,
        video_encryption_key_id, audio_codec, &media_log_);
  }

  // Create a default version of the parser for test.
  WebMClusterParser* CreateDefaultParser() {
    return CreateParserHelper(kNoTimestamp, kNoTimestamp, std::set<int64_t>(),
                              std::string(), std::string(),
                              AudioCodec::kUnknown);
  }

  // Create a parser for test with custom audio and video default durations.
  WebMClusterParser* CreateParserWithDefaultDurations(
      base::TimeDelta audio_default_duration,
      base::TimeDelta video_default_duration) {
    return CreateParserHelper(audio_default_duration, video_default_duration,
                              std::set<int64_t>(), std::string(), std::string(),
                              AudioCodec::kUnknown);
  }

  // Create a parser for test with custom ignored tracks.
  WebMClusterParser* CreateParserWithIgnoredTracks(
      std::set<int64_t>& ignored_tracks) {
    return CreateParserHelper(kNoTimestamp, kNoTimestamp, ignored_tracks,
                              std::string(), std::string(),
                              AudioCodec::kUnknown);
  }

  // Create a parser for test with custom encryption key ids and audio codec.
  WebMClusterParser* CreateParserWithKeyIdsAndAudioCodec(
      const std::string& audio_encryption_key_id,
      const std::string& video_encryption_key_id,
      const AudioCodec audio_codec) {
    return CreateParserHelper(kNoTimestamp, kNoTimestamp, std::set<int64_t>(),
                              audio_encryption_key_id, video_encryption_key_id,
                              audio_codec);
  }

  StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<WebMClusterParser> parser_;
};

TEST_F(WebMClusterParserTest, HeldBackBufferHoldsBackAllTracks) {
  // If a buffer is missing duration and is being held back, then all other
  // tracks' buffers that have same or higher (decode) timestamp should be held
  // back too to keep the timestamps emitted for a cluster monotonically
  // non-decreasing and in same order as parsed.
  InSequence s;

  base::TimeDelta default_audio_duration =
      base::Milliseconds(kTestAudioFrameDefaultDurationInMs);
  ASSERT_GE(default_audio_duration, base::TimeDelta());
  ASSERT_NE(kNoTimestamp, default_audio_duration);
  parser_.reset(
      CreateParserWithDefaultDurations(default_audio_duration, kNoTimestamp));

  constexpr double kExpectedVideoEstimationInMs = 33;

  const BlockInfo kBlockInfo[] = {
      {kVideoTrackNum, 0, 33, true, NULL, 0, false},
      {kAudioTrackNum, 0, 23, false, NULL, 0, false},
      {kAudioTrackNum, 23, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 33, 33, true, NULL, 0, false},
      {kAudioTrackNum, 36, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 66, kExpectedVideoEstimationInMs, true, NULL, 0, false},
      {kAudioTrackNum, 70, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kAudioTrackNum, 83, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
  };

  const int kExpectedBuffersOnPartialCluster[] = {
      0,  // Video simple block without DefaultDuration should be held back
      0,  // Audio buffer ready, but not emitted because its TS >= held back
          // video
      0,  // 2nd audio buffer ready, also not emitted for same reason as first
      3,  // All previous buffers emitted, 2nd video held back with no duration
      3,  // 2nd video still has no duration, 3rd audio ready but not emitted
      5,  // All previous buffers emitted, 3rd video held back with no duration
      5,  // 3rd video still has no duration, 4th audio ready but not emitted
      8,  // Cluster end emits all buffers and 3rd video's duration is estimated
  };

  ASSERT_EQ(std::size(kBlockInfo), std::size(kExpectedBuffersOnPartialCluster));
  int block_count = std::size(kBlockInfo);

  // Iteratively create a cluster containing the first N+1 blocks and parse all
  // but the last byte of the cluster (except when N==|block_count|, just parse
  // the whole cluster). Verify that the corresponding entry in
  // |kExpectedBuffersOnPartialCluster| identifies the exact subset of
  // |kBlockInfo| returned by the parser.
  for (int i = 0; i < block_count; ++i) {
    if (i > 0)
      parser_->Reset();
    // Since we don't know exactly the offsets of each block in the full
    // cluster, build a cluster with exactly one additional block so that
    // parse of all but one byte should deterministically parse all but the
    // last full block. Don't |exceed block_count| blocks though.
    int blocks_in_cluster = std::min(i + 2, block_count);
    std::unique_ptr<Cluster> cluster(
        CreateCluster(0, kBlockInfo, blocks_in_cluster));
    // Parse all but the last byte unless we need to parse the full cluster.
    bool parse_full_cluster = i == (block_count - 1);

    if (parse_full_cluster) {
      EXPECT_MEDIA_LOG(
          WebMSimpleBlockDurationEstimated(kExpectedVideoEstimationInMs));
    }

    int result = parser_->Parse(
        cluster->data(),
        parse_full_cluster ? cluster->bytes_used() : cluster->bytes_used() - 1);
    if (parse_full_cluster) {
      DVLOG(1) << "Verifying parse result of full cluster of "
               << blocks_in_cluster << " blocks";
      EXPECT_EQ(cluster->bytes_used(), result);
    } else {
      DVLOG(1) << "Verifying parse result of cluster of "
               << blocks_in_cluster << " blocks with last block incomplete";
      EXPECT_GT(cluster->bytes_used(), result);
      EXPECT_LT(0, result);
    }

    EXPECT_TRUE(
        VerifyBuffers(parser_, kBlockInfo, kExpectedBuffersOnPartialCluster[i]))
        << i;
  }
}

TEST_F(WebMClusterParserTest, Reset) {
  InSequence s;

  int block_count = std::size(kDefaultBlockInfo);
  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kDefaultBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser_->Parse(cluster->data(), cluster->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->bytes_used());

  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count - 1));
  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithSingleCall) {
  int block_count = std::size(kDefaultBlockInfo);
  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kDefaultBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithMultipleCalls) {
  int block_count = std::size(kDefaultBlockInfo);
  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kDefaultBlockInfo, block_count));

  const uint8_t* data = cluster->data();
  int size = cluster->bytes_used();
  int default_parse_size = 3;
  int parse_size = std::min(default_parse_size, size);

  StreamParser::BufferQueueMap buffers;
  while (size > 0) {
    int result = parser_->Parse(data, parse_size);
    ASSERT_GE(result, 0);
    ASSERT_LE(result, parse_size);

    if (result == 0) {
      // The parser needs more data so increase the parse_size a little.
      parse_size += default_parse_size;
      parse_size = std::min(parse_size, size);
      continue;
    }

    StreamParser::BufferQueueMap bqm;
    parser_->GetBuffers(&bqm);
    for (const auto& it : bqm) {
      AppendToEnd(it.second, &buffers[it.first]);
    }

    parse_size = default_parse_size;

    data += result;
    size -= result;
  }
  ASSERT_TRUE(VerifyBuffers(buffers, kDefaultBlockInfo, block_count));
}

// Verify that both BlockGroups with the BlockDuration before the Block
// and BlockGroups with the BlockDuration after the Block are supported
// correctly.
// Note: Raw bytes are use here because ClusterBuilder only generates
// one of these scenarios.
TEST_F(WebMClusterParserTest, ParseBlockGroup) {
  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, 23, false, NULL, 0, true},
      {kVideoTrackNum, 33, 34, false, NULL, 0, true},
  };
  int block_count = std::size(kBlockInfo);

  const uint8_t kClusterData[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x9B,  // Cluster(size=27)
    0xE7, 0x81, 0x00,  // Timecode(size=1, value=0)
    // BlockGroup with BlockDuration before Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0x9B, 0x81, 0x17,  // BlockDuration(size=1, value=23)
    0xA1, 0x85, 0x81, 0x00, 0x00, 0x00, 0xaa,  // Block(size=5, track=1, ts=0)
    // BlockGroup with BlockDuration after Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0xA1, 0x85, 0x82, 0x00, 0x21, 0x00, 0x55,  // Block(size=5, track=2, ts=33)
    0x9B, 0x81, 0x22,  // BlockDuration(size=1, value=34)
  };
  const int kClusterSize = sizeof(kClusterData);

  int result = parser_->Parse(kClusterData, kClusterSize);
  EXPECT_EQ(kClusterSize, result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseSimpleBlockAndBlockGroupMixture) {
  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, false, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, false, NULL, 0, false},
      {kVideoTrackNum, 67, 33, false, NULL, 0, false},
  };
  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, IgnoredTracks) {
  std::set<int64_t> ignored_tracks;
  ignored_tracks.insert(kTextTrackNum);

  parser_.reset(CreateParserWithIgnoredTracks(ignored_tracks));

  const BlockInfo kInputBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kTextTrackNum, 33, 99, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kVideoTrackNum, 67, 34, true, NULL, 0, false},
  };
  int input_block_count = std::size(kInputBlockInfo);

  const BlockInfo kOutputBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kVideoTrackNum, 67, 34, true, NULL, 0, false},
  };
  int output_block_count = std::size(kOutputBlockInfo);

  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(23));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(34));
  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kOutputBlockInfo, output_block_count));
}

TEST_F(WebMClusterParserTest, ParseEncryptedBlock) {
  std::unique_ptr<Cluster> cluster(
      CreateEncryptedCluster(sizeof(kEncryptedFrame)));

  parser_.reset(CreateParserWithKeyIdsAndAudioCodec(
      std::string(), "video_key_id", AudioCodec::kUnknown));

  // The encrypted cluster contains just one block, video.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(
      WebMClusterParser::kDefaultVideoBufferDurationInMs));

  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  StreamParser::BufferQueueMap buffers;
  parser_->GetBuffers(&buffers);
  EXPECT_EQ(1UL, buffers[kVideoTrackNum].size());
  scoped_refptr<StreamParserBuffer> buffer = buffers[kVideoTrackNum][0];
  VerifyEncryptedBuffer(buffer);
}

TEST_F(WebMClusterParserTest, ParseBadEncryptedBlock) {
  std::unique_ptr<Cluster> cluster(
      CreateEncryptedCluster(sizeof(kEncryptedFrame) - 1));

  parser_.reset(CreateParserWithKeyIdsAndAudioCodec(
      std::string(), "video_key_id", AudioCodec::kUnknown));

  EXPECT_MEDIA_LOG(HasSubstr("Failed to extract decrypt config"));
  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(-1, result);
}

TEST_F(WebMClusterParserTest, ParseInvalidZeroSizedCluster) {
  const uint8_t kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x80,  // CLUSTER (size = 0)
  };

  EXPECT_EQ(-1, parser_->Parse(kBuffer, sizeof(kBuffer)));
}

TEST_F(WebMClusterParserTest, ParseInvalidUnknownButActuallyZeroSizedCluster) {
  const uint8_t kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0xFF,  // CLUSTER (size = "unknown")
    0x1F, 0x43, 0xB6, 0x75, 0x85,  // CLUSTER (size = 5)
  };

  EXPECT_EQ(-1, parser_->Parse(kBuffer, sizeof(kBuffer)));
}

TEST_F(WebMClusterParserTest, ParseWithDefaultDurationsSimpleBlocks) {
  InSequence s;
  ResetParserToHaveDefaultDurations();

  EXPECT_LT(kTestAudioFrameDefaultDurationInMs, 23);
  EXPECT_LT(kTestVideoFrameDefaultDurationInMs, 33);

  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kAudioTrackNum, 23, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 33, kTestVideoFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kAudioTrackNum, 46, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 67, kTestVideoFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kAudioTrackNum, 69, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 100, kTestVideoFrameDefaultDurationInMs, true, NULL, 0,
       false},
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed. Though all the blocks are simple blocks, none should be held aside
  // for duration estimation prior to end of cluster detection because all the
  // tracks have DefaultDurations.
  int result = parser_->Parse(cluster->data(), cluster->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->bytes_used());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count - 1));

  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseWithoutAnyDurationsSimpleBlocks) {
  InSequence s;

  // Absent DefaultDuration information, SimpleBlock durations are derived from
  // inter-buffer track timestamp delta if within the cluster. Duration for the
  // last block in a cluster is estimated independently for each track in the
  // cluster using the maximum seen so far.

  constexpr double kExpectedAudioEstimationInMs = 23;
  constexpr double kExpectedVideoEstimationInMs = 34;
  const BlockInfo kBlockInfo1[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 22, true, NULL, 0, false},
      {kVideoTrackNum, 33, 33, true, NULL, 0, false},
      {kAudioTrackNum, 45, 23, true, NULL, 0, false},
      {kVideoTrackNum, 66, 34, true, NULL, 0, false},
      {kAudioTrackNum, 68, kExpectedAudioEstimationInMs, true, NULL, 0, false},
      {kVideoTrackNum, 100, kExpectedVideoEstimationInMs, true, NULL, 0, false},
  };

  int block_count1 = std::size(kBlockInfo1);
  std::unique_ptr<Cluster> cluster1(
      CreateCluster(0, kBlockInfo1, block_count1));

  // Send slightly less than the first full cluster so all but the last video
  // block is parsed. Verify the last fully parsed audio and video buffer are
  // both missing from the result (parser should hold them aside for duration
  // estimation prior to end of cluster detection in the absence of
  // DefaultDurations.)
  int result = parser_->Parse(cluster1->data(), cluster1->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster1->bytes_used());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1 - 3));
  StreamParser::BufferQueueMap buffers;
  parser_->GetBuffers(&buffers);
  EXPECT_EQ(3UL, buffers[kAudioTrackNum].size());
  EXPECT_EQ(1UL, buffers[kVideoTrackNum].size());

  parser_->Reset();

  // Now parse the full first cluster and verify all the blocks are parsed.
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedAudioEstimationInMs));
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedVideoEstimationInMs));
  result = parser_->Parse(cluster1->data(), cluster1->bytes_used());
  EXPECT_EQ(cluster1->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1));

  // Verify that the estimated frame duration is tracked across clusters for
  // each track.
  const BlockInfo kBlockInfo2[] = {
      // Estimate carries over across clusters
      {kAudioTrackNum, 200, kExpectedAudioEstimationInMs, true, NULL, 0, false},
      // Estimate carries over across clusters
      {kVideoTrackNum, 201, kExpectedVideoEstimationInMs, true, NULL, 0, false},
  };

  int block_count2 = std::size(kBlockInfo2);
  std::unique_ptr<Cluster> cluster2(
      CreateCluster(0, kBlockInfo2, block_count2));
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedAudioEstimationInMs));
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedVideoEstimationInMs));
  result = parser_->Parse(cluster2->data(), cluster2->bytes_used());
  EXPECT_EQ(cluster2->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo2, block_count2));
}

TEST_F(WebMClusterParserTest, ParseWithoutAnyDurationsBlockGroups) {
  InSequence s;

  // Absent DefaultDuration and BlockDuration information, BlockGroup block
  // durations are derived from inter-buffer track timestamp delta if within the
  // cluster. Duration for the last block in a cluster is estimated
  // independently for each track in the cluster using the maximum seen so far.

  constexpr double kExpectedAudioEstimationInMs = 23;
  constexpr double kExpectedVideoEstimationInMs = 34;
  const BlockInfo kBlockInfo1[] = {
      {kAudioTrackNum, 0, -23, false, NULL, 0, false},
      {kAudioTrackNum, 23, -22, false, NULL, 0, false},
      {kVideoTrackNum, 33, -33, false, NULL, 0, false},
      {kAudioTrackNum, 45, -23, false, NULL, 0, false},
      {kVideoTrackNum, 66, -34, false, NULL, 0, false},
      {kAudioTrackNum, 68, -kExpectedAudioEstimationInMs, false, NULL, 0,
       false},
      {kVideoTrackNum, 100, -kExpectedVideoEstimationInMs, false, NULL, 0,
       false},
  };

  int block_count1 = std::size(kBlockInfo1);
  std::unique_ptr<Cluster> cluster1(
      CreateCluster(0, kBlockInfo1, block_count1));

  // Send slightly less than the first full cluster so all but the last video
  // block is parsed. Verify the last fully parsed audio and video buffer are
  // both missing from the result (parser should hold them aside for duration
  // estimation prior to end of cluster detection in the absence of
  // DefaultDurations.)
  int result = parser_->Parse(cluster1->data(), cluster1->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster1->bytes_used());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1 - 3));
  StreamParser::BufferQueueMap buffers;
  parser_->GetBuffers(&buffers);
  EXPECT_EQ(3UL, buffers[kAudioTrackNum].size());
  EXPECT_EQ(1UL, buffers[kVideoTrackNum].size());

  parser_->Reset();

  // Now parse the full first cluster and verify all the blocks are parsed.
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedAudioEstimationInMs));
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedVideoEstimationInMs));
  result = parser_->Parse(cluster1->data(), cluster1->bytes_used());
  EXPECT_EQ(cluster1->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1));

  // Verify that the estimated frame duration is tracked across clusters for
  // each track.
  const BlockInfo kBlockInfo2[] = {
      {kAudioTrackNum, 200, -kExpectedAudioEstimationInMs, false, NULL, 0,
       false},
      {kVideoTrackNum, 201, -kExpectedVideoEstimationInMs, false, NULL, 0,
       false},
  };

  int block_count2 = std::size(kBlockInfo2);
  std::unique_ptr<Cluster> cluster2(
      CreateCluster(0, kBlockInfo2, block_count2));
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedAudioEstimationInMs));
  EXPECT_MEDIA_LOG(
      WebMSimpleBlockDurationEstimated(kExpectedVideoEstimationInMs));
  result = parser_->Parse(cluster2->data(), cluster2->bytes_used());
  EXPECT_EQ(cluster2->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo2, block_count2));
}

// TODO(wolenetz): Is parser behavior correct? See http://crbug.com/363433.
TEST_F(WebMClusterParserTest,
       ParseWithDefaultDurationsBlockGroupsWithoutDurations) {
  InSequence s;
  ResetParserToHaveDefaultDurations();

  EXPECT_LT(kTestAudioFrameDefaultDurationInMs, 23);
  EXPECT_LT(kTestVideoFrameDefaultDurationInMs, 33);

  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, -kTestAudioFrameDefaultDurationInMs, false, NULL, 0,
       false},
      {kAudioTrackNum, 23, -kTestAudioFrameDefaultDurationInMs, false, NULL, 0,
       false},
      {kVideoTrackNum, 33, -kTestVideoFrameDefaultDurationInMs, false, NULL, 0,
       false},
      {kAudioTrackNum, 46, -kTestAudioFrameDefaultDurationInMs, false, NULL, 0,
       false},
      {kVideoTrackNum, 67, -kTestVideoFrameDefaultDurationInMs, false, NULL, 0,
       false},
      {kAudioTrackNum, 69, -kTestAudioFrameDefaultDurationInMs, false, NULL, 0,
       false},
      {kVideoTrackNum, 100, -kTestVideoFrameDefaultDurationInMs, false, NULL, 0,
       false},
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed. None should be held aside for duration estimation prior to end of
  // cluster detection because all the tracks have DefaultDurations.
  int result = parser_->Parse(cluster->data(), cluster->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->bytes_used());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count - 1));

  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

// Verify the parser can handle block timestamps that are negative
// relative-to-cluster and to absolute time. With BlockDurations provided, there
// is no buffer duration estimation, and the ready-buffer extraction bounds are
// always maximal, for both a partial cluster and a full cluster parse.
TEST_F(WebMClusterParserTest,
       ParseClusterWithNegativeBlockTimestampsAndWithBlockDurations) {
  InSequence s;

  EXPECT_LT(kTestAudioFrameDefaultDurationInMs, 23);
  EXPECT_LT(kTestVideoFrameDefaultDurationInMs, 33);

  const BlockInfo kBlockInfo[] = {
      {kVideoTrackNum, -33, 10, false, NULL, 0, false},
      {kAudioTrackNum, -23, 5, false, NULL, 0, false},
  };

  int block_count = std::size(kBlockInfo);
  // Using 0 for cluster timecode will make each of the blocks, above, use a
  // negative relative timecode to achieve the desired negative block
  // timestamps.
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed. None should be held aside for duration estimation prior to end of
  // cluster detection because all blocks have BlockDurations.
  int result = parser_->Parse(cluster->data(), cluster->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->bytes_used());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count - 1));

  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

// Verify the parser can handle block timestamps that are negative
// relative-to-cluster and to absolute time. With neither BlockDurations nor
// DefaultDuration provided, all blocks' durations are derived from interblock
// timestamps in the track or estimated for the last block in the each track in
// the cluster, requiring holding back the last block in each track (unless the
// cluster is fully parsed to completion) so it can get estimated duration based
// on the next block in that track in the cluster. The ready-buffer extraction
// methods are driven by the test block to have negative upper-bounds in the
// partial-block parse in this case.
TEST_F(WebMClusterParserTest,
       ParseClusterWithNegativeBlockTimestampsAndWithoutDurations) {
  InSequence s;

  // Simple blocks, used here, include no block duration information.
  const BlockInfo kBlockInfo[] = {
      {kVideoTrackNum, -68, 33, true, NULL, 0, false},
      {kAudioTrackNum, -48, 23, true, NULL, 0, false},
      {kVideoTrackNum, -35, 33, true, NULL, 0, false},
      {kAudioTrackNum, -25, 23, true, NULL, 0, false},
  };

  int block_count = std::size(kBlockInfo);
  // Using 0 for cluster timecode will make each of the blocks, above, use a
  // negative relative timecode to achieve the desired negative block
  // timestamps.
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed. Only the first video block should be readable from the parser since
  // the second video is held back still (not yet at end of cluster) and the
  // first audio is held back still (no second block parsed fully yet and not
  // yet at end of cluster).
  int result = parser_->Parse(cluster->data(), cluster->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->bytes_used());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count - 3));

  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed and
  // have estimated durations applied correctly. Implementation applies audio
  // block estimations before video block estimations upon reaching the end of
  // the cluster, hence the expected order of MEDIA_LOGs here.
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(23));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(33));
  result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest,
       ParseDegenerateClusterYieldsHardcodedEstimatedDurations) {
  const BlockInfo kBlockInfo[] = {
    {
      kAudioTrackNum,
      0,
      WebMClusterParser::kDefaultAudioBufferDurationInMs,
      true
    }, {
      kVideoTrackNum,
      0,
      WebMClusterParser::kDefaultVideoBufferDurationInMs,
      true
    },
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(
      WebMClusterParser::kDefaultAudioBufferDurationInMs));
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimated(
      WebMClusterParser::kDefaultVideoBufferDurationInMs));
  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest,
       ParseDegenerateClusterWithDefaultDurationsYieldsDefaultDurations) {
  ResetParserToHaveDefaultDurations();

  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 0, kTestVideoFrameDefaultDurationInMs, true },
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ReadOpusDurationsSimpleBlockAtEndOfCluster) {
  int loop_count = 0;
  for (const auto& packet_ptr : BuildAllOpusPackets()) {
    InSequence s;

    // Get a new parser each iteration to prevent exceeding the media log cap.
    parser_.reset(CreateParserWithKeyIdsAndAudioCodec(
        std::string(), std::string(), AudioCodec::kOpus));

    const BlockInfo kBlockInfo[] = {{kAudioTrackNum,
                                     0,
                                     packet_ptr->duration_ms(),
                                     true,  // Make it a SimpleBlock.
                                     packet_ptr->data(),
                                     packet_ptr->size()}};

    int block_count = std::size(kBlockInfo);
    std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
    int duration_ms = packet_ptr->duration_ms();  // Casts from double.
    if (duration_ms > 120) {
      EXPECT_MEDIA_LOG(OpusPacketDurationTooHigh(duration_ms));
    }

    int result = parser_->Parse(cluster->data(), cluster->bytes_used());
    EXPECT_EQ(cluster->bytes_used(), result);
    ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));

    // Fail early if any iteration fails to meet the logging expectations.
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&media_log_));

    loop_count++;
  }

  // Test should minimally cover all the combinations of config and frame count.
  ASSERT_GE(loop_count, kNumPossibleOpusConfigs * kMaxOpusPacketFrameCount);
}

TEST_F(WebMClusterParserTest, PreferOpusDurationsOverBlockDurations) {
  int loop_count = 0;
  for (const auto& packet_ptr : BuildAllOpusPackets()) {
    InSequence s;

    // Get a new parser each iteration to prevent exceeding the media log cap.
    parser_.reset(CreateParserWithKeyIdsAndAudioCodec(
        std::string(), std::string(), AudioCodec::kOpus));

    // Setting BlockDuration != Opus duration to see which one the parser uses.
    double block_duration_ms = packet_ptr->duration_ms() + 10;
    if (packet_ptr->duration_ms() > 120) {
      EXPECT_MEDIA_LOG(OpusPacketDurationTooHigh(packet_ptr->duration_ms()));
    }

    EXPECT_MEDIA_LOG(WebMBlockDurationMismatchesOpusDuration(
        block_duration_ms, packet_ptr->duration_ms()));

    BlockInfo block_infos[] = {{kAudioTrackNum,
                                0,
                                block_duration_ms,
                                false,  // Not a SimpleBlock.
                                packet_ptr->data(),
                                packet_ptr->size()}};

    int block_count = std::size(block_infos);
    std::unique_ptr<Cluster> cluster(
        CreateCluster(0, block_infos, block_count));
    int result = parser_->Parse(cluster->data(), cluster->bytes_used());
    EXPECT_EQ(cluster->bytes_used(), result);

    // BlockInfo duration will be used to verify buffer duration, so changing
    // duration to be that of the Opus packet to verify it was preferred.
    block_infos[0].duration = packet_ptr->duration_ms();

    ASSERT_TRUE(VerifyBuffers(parser_, block_infos, block_count));

    // Fail early if any iteration fails to meet the logging expectations.
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&media_log_));

    loop_count++;
  }

  // Test should minimally cover all the combinations of config and frame count.
  ASSERT_GE(loop_count, kNumPossibleOpusConfigs * kMaxOpusPacketFrameCount);
}

// Tests that BlockDuration is used to set duration on buffer rather than
// encoded duration in Opus packet (or hard coded duration estimates). Encoded
// Opus duration is usually preferred but cannot be known when encrypted.
TEST_F(WebMClusterParserTest, DontReadEncodedDurationWhenEncrypted) {
  // Non-empty dummy value signals encryption is active for audio.
  std::string audio_encryption_id("audio_key_id");

  // Reset parser to expect Opus codec audio and use audio encryption key id.
  parser_.reset(CreateParserWithKeyIdsAndAudioCodec(
      audio_encryption_id, std::string(), AudioCodec::kOpus));

  // Single Block with BlockDuration and encrypted data.
  const BlockInfo kBlockInfo[] = {{kAudioTrackNum, 0,
                                   kTestAudioFrameDefaultDurationInMs,
                                   false,            // Not a SimpleBlock
                                   kEncryptedFrame,  // Encrypted frame data
                                   std::size(kEncryptedFrame)}};

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->bytes_used());
  EXPECT_EQ(cluster->bytes_used(), result);

  // Will verify that duration of buffer matches that of BlockDuration.
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

}  // namespace media
