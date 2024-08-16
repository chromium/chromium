// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/base/webvtt_util.h"
#include "media/filters/source_buffer_range.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace {

enum class TimeGranularity { kMicrosecond, kMillisecond };

}  // namespace

namespace media {

typedef StreamParser::BufferQueue BufferQueue;

static const int kDefaultFramesPerSecond = 30;
static const int kDefaultKeyframesPerSecond = 6;
static const uint8_t kDataA = 0x11;
static const uint8_t kDataB = 0x33;
static const size_t kDataSize = 1u;

// Matchers for verifying common media log entry strings.
MATCHER_P(ContainsTrackBufferExhaustionSkipLog, skip_milliseconds, "") {
  return CONTAINS_STRING(arg,
                         "Media append that overlapped current playback "
                         "position may cause time gap in playing VIDEO stream "
                         "because the next keyframe is " +
                             base::NumberToString(skip_milliseconds) +
                             "ms beyond last overlapped frame. Media may "
                             "appear temporarily frozen.");
}

#define EXPECT_STATUS_FOR_STREAM_OP(status_suffix, operation) \
  { EXPECT_EQ(SourceBufferStreamStatus::status_suffix, stream_->operation); }

class SourceBufferStreamTest : public testing::Test {
 public:
  SourceBufferStreamTest(const SourceBufferStreamTest&) = delete;
  SourceBufferStreamTest& operator=(const SourceBufferStreamTest&) = delete;

 protected:
  SourceBufferStreamTest() {
    video_config_ = TestVideoConfig::Normal();
    SetStreamInfo(kDefaultFramesPerSecond, kDefaultKeyframesPerSecond);
    ResetStream<>(video_config_);
  }

  template <typename ConfigT>
  void ResetStream(const ConfigT& config) {
    stream_ = std::make_unique<SourceBufferStream>(config, &media_log_);
  }

  void SetMemoryLimit(size_t buffers_of_data) {
    stream_->set_memory_limit(buffers_of_data * GetMemoryUsagePerBuffer());
  }

  void SetStreamInfo(int frames_per_second, int keyframes_per_second) {
    frames_per_second_ = frames_per_second;
    keyframes_per_second_ = keyframes_per_second;
    frame_duration_ = ConvertToFrameDuration(frames_per_second);
  }

  void SetAudioStream() {
    video_config_ = TestVideoConfig::Invalid();
    audio_config_.Initialize(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                             CHANNEL_LAYOUT_STEREO, 1000, EmptyExtraData(),
                             EncryptionScheme::kUnencrypted, base::TimeDelta(),
                             0);
    ResetStream<>(audio_config_);

    // Equivalent to 2ms per frame.
    SetStreamInfo(500, 500);
  }

  void NewCodedFrameGroupAppend(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, true, base::TimeDelta(),
                  &kDataA, kDataSize);
  }

  void NewCodedFrameGroupAppend(int starting_position,
                                int number_of_buffers,
                                const uint8_t* data) {
    AppendBuffers(starting_position, number_of_buffers, true, base::TimeDelta(),
                  data, kDataSize);
  }

  void NewCodedFrameGroupAppend_OffsetFirstBuffer(
      int starting_position,
      int number_of_buffers,
      base::TimeDelta first_buffer_offset) {
    AppendBuffers(starting_position, number_of_buffers, true,
                  first_buffer_offset, &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position, int number_of_buffers) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), &kDataA, kDataSize);
  }

  void AppendBuffers(int starting_position,
                     int number_of_buffers,
                     const uint8_t* data) {
    AppendBuffers(starting_position, number_of_buffers, false,
                  base::TimeDelta(), data, kDataSize);
  }

  void NewCodedFrameGroupAppend(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp, false);
  }

  void NewCodedFrameGroupAppend(base::TimeDelta start_timestamp,
                                const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, start_timestamp, false);
  }

  void AppendBuffers(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp, false);
  }

  void NewCodedFrameGroupAppendOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, true, kNoTimestamp, true);
  }

  void AppendBuffersOneByOne(const std::string& buffers_to_append) {
    AppendBuffers(buffers_to_append, false, kNoTimestamp, true);
  }

  void Seek(int position) { stream_->Seek(position * frame_duration_); }

  void SeekToTimestampMs(int64_t timestamp_ms) {
    stream_->Seek(base::Milliseconds(timestamp_ms));
  }

  bool GarbageCollect(base::TimeDelta media_time, int new_data_size) {
    return stream_->GarbageCollectIfNeeded(media_time, new_data_size);
  }

  bool GarbageCollectWithPlaybackAtBuffer(int position, int new_data_buffers) {
    return GarbageCollect(position * frame_duration_,
                          new_data_buffers * kDataSize);
  }

  void RemoveInMs(int start, int end, int duration) {
    Remove(base::Milliseconds(start), base::Milliseconds(end),
           base::Milliseconds(duration));
  }

  void Remove(base::TimeDelta start, base::TimeDelta end,
              base::TimeDelta duration) {
    stream_->Remove(start, end, duration);
  }

  void SignalStartOfCodedFrameGroup(base::TimeDelta start_timestamp) {
    stream_->OnStartOfCodedFrameGroup(start_timestamp);
  }

  int GetRemovalRangeInMs(int start, int end, int bytes_to_free,
                          int* removal_end) {
    base::TimeDelta removal_end_timestamp = base::Milliseconds(*removal_end);
    int bytes_removed = stream_->GetRemovalRange(
        base::Milliseconds(start), base::Milliseconds(end), bytes_to_free,
        &removal_end_timestamp);
    *removal_end = removal_end_timestamp.InMilliseconds();
    return bytes_removed;
  }

  void CheckExpectedRanges(const std::string& expected) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64_t start = r.start(i).IntDiv(frame_duration_);
      int64_t end = r.end(i).IntDiv(frame_duration_) - 1;
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  void CheckExpectedRangesByTimestamp(
      const std::string& expected,
      TimeGranularity granularity = TimeGranularity::kMillisecond) {
    Ranges<base::TimeDelta> r = stream_->GetBufferedTime();

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      auto conversion = (granularity == TimeGranularity::kMillisecond)
                            ? &base::TimeDelta::InMilliseconds
                            : &base::TimeDelta::InMicroseconds;
      int64_t start = (r.start(i).*conversion)();
      int64_t end = (r.end(i).*conversion)();
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  void CheckExpectedRangeEndTimes(const std::string& expected) {
    std::stringstream ss;
    ss << "{ ";
    for (const auto& r : stream_->ranges_) {
      base::TimeDelta highest_pts = r->GetEndTimestamp();
      base::TimeDelta end_time = r->GetBufferedEndTimestamp();
      ss << "<" << highest_pts.InMilliseconds() << ","
         << end_time.InMilliseconds() << "> ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  void CheckIsNextInPTSSequenceWithFirstRange(int64_t pts_in_ms,
                                              bool expectation) {
    ASSERT_GE(stream_->ranges_.size(), 1u);
    const auto& range_ptr = *(stream_->ranges_.begin());
    EXPECT_EQ(expectation, range_ptr->IsNextInPresentationSequence(
                               base::Milliseconds(pts_in_ms)));
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position) {
    CheckExpectedBuffers(starting_position, ending_position, false,
                         std::nullopt);
  }

  void CheckExpectedBuffers(
      int starting_position, int ending_position, bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         std::nullopt);
  }

  void CheckExpectedBuffers(int starting_position,
                            int ending_position,
                            const uint8_t* data) {
    CheckExpectedBuffers(starting_position, ending_position, false,
                         UNSAFE_TODO(base::span(data, kDataSize)));
  }

  void CheckExpectedBuffers(int starting_position,
                            int ending_position,
                            const uint8_t* data,
                            bool expect_keyframe) {
    CheckExpectedBuffers(starting_position, ending_position, expect_keyframe,
                         UNSAFE_TODO(base::span(data, kDataSize)));
  }

  void CheckExpectedBuffers(
      int starting_position,
      int ending_position,
      bool expect_keyframe,
      std::optional<base::span<const uint8_t>> expected_data) {
    int current_position = starting_position;
    for (; current_position <= ending_position; current_position++) {
      scoped_refptr<StreamParserBuffer> buffer;
      SourceBufferStreamStatus status = stream_->GetNextBuffer(&buffer);
      EXPECT_NE(status, SourceBufferStreamStatus::kConfigChange);
      if (status != SourceBufferStreamStatus::kSuccess)
        break;

      if (expect_keyframe && current_position == starting_position)
        EXPECT_TRUE(buffer->is_key_frame());

      if (expected_data) {
        EXPECT_EQ(base::span(*expected_data), base::span(*buffer));
      }

      EXPECT_EQ(
          base::ClampFloor(buffer->GetDecodeTimestamp() / frame_duration_),
          current_position);
    }

    EXPECT_EQ(ending_position + 1, current_position);
  }

  void CheckExpectedBuffers(
      const std::string& expected,
      TimeGranularity granularity = TimeGranularity::kMillisecond) {
    std::vector<std::string> timestamps = base::SplitString(
        expected, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::stringstream ss;
    const SourceBufferStreamType type = stream_->GetType();
    for (size_t i = 0; i < timestamps.size(); i++) {
      scoped_refptr<StreamParserBuffer> buffer;
      SourceBufferStreamStatus status = stream_->GetNextBuffer(&buffer);

      if (i > 0)
        ss << " ";

      if (status == SourceBufferStreamStatus::kConfigChange) {
        switch (type) {
          case SourceBufferStreamType::kVideo:
            stream_->GetCurrentVideoDecoderConfig();
            break;
          case SourceBufferStreamType::kAudio:
            stream_->GetCurrentAudioDecoderConfig();
            break;
        }

        EXPECT_EQ("C", timestamps[i]);
        ss << "C";
        continue;
      }

      EXPECT_EQ(SourceBufferStreamStatus::kSuccess, status);
      if (status != SourceBufferStreamStatus::kSuccess)
        break;

      if (granularity == TimeGranularity::kMillisecond)
        ss << buffer->timestamp().InMilliseconds();
      else
        ss << buffer->timestamp().InMicroseconds();

      if (buffer->GetDecodeTimestamp() !=
          DecodeTimestamp::FromPresentationTime(buffer->timestamp())) {
        if (granularity == TimeGranularity::kMillisecond)
          ss << "|" << buffer->GetDecodeTimestamp().InMilliseconds();
        else
          ss << "|" << buffer->GetDecodeTimestamp().InMicroseconds();
      }

      // Check duration if expected timestamp contains it.
      if (timestamps[i].find('D') != std::string::npos) {
        ss << "D" << buffer->duration().InMilliseconds();
      }

      // Check duration estimation if expected timestamp contains it.
      if (timestamps[i].find('E') != std::string::npos &&
          buffer->is_duration_estimated()) {
        ss << "E";
      }

      // Handle preroll buffers.
      if (base::EndsWith(timestamps[i], "P", base::CompareCase::SENSITIVE)) {
        ASSERT_TRUE(buffer->is_key_frame());
        scoped_refptr<StreamParserBuffer> preroll_buffer;
        preroll_buffer.swap(buffer);

        // When a preroll buffer is encountered we should be able to request one
        // more buffer.  The first buffer should match the timestamp and config
        // of the second buffer, except that its discard_padding() should be its
        // duration.
        EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));
        ASSERT_EQ(buffer->GetConfigId(), preroll_buffer->GetConfigId());
        ASSERT_EQ(buffer->track_id(), preroll_buffer->track_id());
        ASSERT_EQ(buffer->timestamp(), preroll_buffer->timestamp());
        ASSERT_EQ(buffer->GetDecodeTimestamp(),
                  preroll_buffer->GetDecodeTimestamp());
        ASSERT_EQ(kInfiniteDuration, preroll_buffer->discard_padding().first);
        ASSERT_EQ(base::TimeDelta(), preroll_buffer->discard_padding().second);
        ASSERT_TRUE(buffer->is_key_frame());

        ss << "P";
      } else if (buffer->is_key_frame()) {
        ss << "K";
      }
    }
    EXPECT_EQ(expected, ss.str());
  }

  void CheckNoNextBuffer() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_STATUS_FOR_STREAM_OP(kNeedBuffer, GetNextBuffer(&buffer));
  }

  void CheckEOSReached() {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_STATUS_FOR_STREAM_OP(kEndOfStream, GetNextBuffer(&buffer));
  }

  void CheckVideoConfig(const VideoDecoderConfig& config) {
    const VideoDecoderConfig& actual = stream_->GetCurrentVideoDecoderConfig();
    EXPECT_TRUE(actual.Matches(config))
        << "Expected: " << config.AsHumanReadableString()
        << "\nActual: " << actual.AsHumanReadableString();
  }

  void CheckAudioConfig(const AudioDecoderConfig& config) {
    const AudioDecoderConfig& actual = stream_->GetCurrentAudioDecoderConfig();
    EXPECT_TRUE(actual.Matches(config))
        << "Expected: " << config.AsHumanReadableString()
        << "\nActual: " << actual.AsHumanReadableString();
  }

  int GetMemoryUsagePerBuffer() const {
    return kDataSize + sizeof(StreamParserBuffer);
  }

  base::TimeDelta frame_duration() const { return frame_duration_; }

  StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<SourceBufferStream> stream_;

  VideoDecoderConfig video_config_;
  AudioDecoderConfig audio_config_;

 private:
  DemuxerStream::Type GetStreamType() {
    switch (stream_->GetType()) {
      case SourceBufferStreamType::kAudio:
        return DemuxerStream::AUDIO;
      case SourceBufferStreamType::kVideo:
        return DemuxerStream::VIDEO;
    }
    NOTREACHED();
  }

  base::TimeDelta ConvertToFrameDuration(int frames_per_second) {
    return base::Seconds(1) / frames_per_second;
  }

  void AppendBuffers(int starting_position,
                     int number_of_buffers,
                     bool begin_coded_frame_group,
                     base::TimeDelta first_buffer_offset,
                     const uint8_t* data,
                     int size) {
    if (begin_coded_frame_group) {
      stream_->OnStartOfCodedFrameGroup(starting_position * frame_duration_);
    }

    int keyframe_interval = frames_per_second_ / keyframes_per_second_;

    BufferQueue queue;
    for (int i = 0; i < number_of_buffers; i++) {
      int position = starting_position + i;
      bool is_keyframe = position % keyframe_interval == 0;
      // Track ID is meaningless to these tests.
      scoped_refptr<StreamParserBuffer> buffer = StreamParserBuffer::CopyFrom(
          data, size, is_keyframe, GetStreamType(), 0);
      base::TimeDelta timestamp = frame_duration_ * position;

      if (i == 0)
        timestamp += first_buffer_offset;
      buffer->SetDecodeTimestamp(
          DecodeTimestamp::FromPresentationTime(timestamp));

      // Simulate an IBB...BBP pattern in which all B-frames reference both
      // the I- and P-frames. For a GOP with playback order 12345, this would
      // result in a decode timestamp order of 15234.
      base::TimeDelta presentation_timestamp;
      if (is_keyframe) {
        presentation_timestamp = timestamp;
      } else if ((position - 1) % keyframe_interval == 0) {
        // This is the P-frame (first frame following the I-frame)
        presentation_timestamp =
            (timestamp + frame_duration_ * (keyframe_interval - 2));
      } else {
        presentation_timestamp = timestamp - frame_duration_;
      }
      buffer->set_timestamp(presentation_timestamp);
      buffer->set_duration(frame_duration_);

      queue.push_back(buffer);
    }
    if (!queue.empty())
      stream_->Append(queue);
  }

  void UpdateLastBufferDuration(DecodeTimestamp current_dts,
                                BufferQueue* buffers) {
    if (buffers->empty() || buffers->back()->duration().is_positive())
      return;

    DecodeTimestamp last_dts = buffers->back()->GetDecodeTimestamp();
    DCHECK(current_dts >= last_dts);
    buffers->back()->set_duration(current_dts - last_dts);
  }

  // StringToBufferQueue() allows for the generation of StreamParserBuffers from
  // coded strings of timestamps separated by spaces.
  //
  // Supported syntax (options must be in this order):
  // pp[u][|dd[u]][Dzz][E][P][K]
  //
  // pp:
  // Generates a StreamParserBuffer with decode and presentation timestamp xx.
  // E.g., "0 1 2 3".
  // pp is interpreted as milliseconds, unless suffixed with "u", in which case
  // pp is interpreted as microseconds.
  //
  // pp|dd:
  // Generates a StreamParserBuffer with presentation timestamp pp and decode
  // timestamp dd. E.g., "0|0 3|1 1|2 2|3". dd is interpreted as milliseconds,
  // unless suffixed with "u", in which case dd is interpreted as microseconds.
  //
  // Dzz[u]
  // Explicitly describe the duration of the buffer. zz specifies the duration
  // in milliseconds (or in microseconds if suffixed with "u"). If the duration
  // isn't specified with this syntax, the duration is derived using the
  // timestamp delta between this buffer and the next buffer. If not specified,
  // the final buffer will simply copy the duration of the previous buffer. If
  // the queue only contains 1 buffer then the duration must be explicitly
  // specified with this format.
  // E.g. "0D10 10D20"
  //
  // E:
  // Indicates that the buffer should be marked as containing an *estimated*
  // duration. E.g., "0D20E 20 25E 30"
  //
  // P:
  // Indicates the buffer with will also have a preroll buffer
  // associated with it. The preroll buffer will just be dummy data.
  // E.g. "0P 5 10"
  //
  // K:
  // Indicates the buffer is a keyframe. E.g., "0K 1|2K 2|4D2K 6 8".
  BufferQueue StringToBufferQueue(const std::string& buffers_to_append) {
    std::vector<std::string> timestamps = base::SplitString(
        buffers_to_append, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    CHECK_GT(timestamps.size(), 0u);

    BufferQueue buffers;
    for (size_t i = 0; i < timestamps.size(); i++) {
      bool is_keyframe = false;
      bool has_preroll = false;
      bool is_duration_estimated = false;

      if (base::EndsWith(timestamps[i], "K", base::CompareCase::SENSITIVE)) {
        is_keyframe = true;
        // Remove the "K" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }
      // Handle preroll buffers.
      if (base::EndsWith(timestamps[i], "P", base::CompareCase::SENSITIVE)) {
        is_keyframe = true;
        has_preroll = true;
        // Remove the "P" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      if (base::EndsWith(timestamps[i], "E", base::CompareCase::SENSITIVE)) {
        is_duration_estimated = true;
        // Remove the "E" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      int duration_in_us = -1;
      size_t duration_pos = timestamps[i].find('D');
      if (duration_pos != std::string::npos) {
        bool is_duration_us = false;  // Default to millisecond interpretation.
        if (base::EndsWith(timestamps[i], "u", base::CompareCase::SENSITIVE)) {
          is_duration_us = true;
          timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
        }
        CHECK(base::StringToInt(timestamps[i].substr(duration_pos + 1),
                                &duration_in_us));
        if (!is_duration_us)
          duration_in_us *= base::Time::kMicrosecondsPerMillisecond;
        timestamps[i] = timestamps[i].substr(0, duration_pos);
      }

      std::vector<std::string> buffer_timestamp_strings = base::SplitString(
          timestamps[i], "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

      if (buffer_timestamp_strings.size() == 1)
        buffer_timestamp_strings.push_back(buffer_timestamp_strings[0]);

      CHECK_EQ(2u, buffer_timestamp_strings.size());

      std::vector<base::TimeDelta> buffer_timestamps;

      // Parse PTS, then DTS into TimeDeltas.
      for (size_t j = 0; j < 2; ++j) {
        int us = 0;
        bool is_us = false;  // Default to millisecond interpretation.

        if (base::EndsWith(buffer_timestamp_strings[j], "u",
                           base::CompareCase::SENSITIVE)) {
          is_us = true;
          buffer_timestamp_strings[j] = buffer_timestamp_strings[j].substr(
              0, buffer_timestamp_strings[j].length() - 1);
        }
        CHECK(base::StringToInt(buffer_timestamp_strings[j], &us));
        if (!is_us)
          us *= base::Time::kMicrosecondsPerMillisecond;

        buffer_timestamps.push_back(base::Microseconds(us));
      }

      // Create buffer. Track ID is meaningless to these tests
      scoped_refptr<StreamParserBuffer> buffer = StreamParserBuffer::CopyFrom(
          &kDataA, kDataSize, is_keyframe, GetStreamType(), 0);
      buffer->set_timestamp(buffer_timestamps[0]);
      if (is_duration_estimated)
        buffer->set_is_duration_estimated(true);

      if (buffer_timestamps[1] != buffer_timestamps[0]) {
        buffer->SetDecodeTimestamp(
            DecodeTimestamp::FromPresentationTime(buffer_timestamps[1]));
      }

      if (duration_in_us >= 0)
        buffer->set_duration(base::Microseconds(duration_in_us));

      // Simulate preroll buffers by just generating another buffer and sticking
      // it as the preroll.
      if (has_preroll) {
        scoped_refptr<StreamParserBuffer> preroll_buffer =
            StreamParserBuffer::CopyFrom(&kDataA, kDataSize, is_keyframe,
                                         GetStreamType(), 0);
        preroll_buffer->set_duration(frame_duration_);
        buffer->SetPrerollBuffer(preroll_buffer);
      }

      UpdateLastBufferDuration(buffer->GetDecodeTimestamp(), &buffers);
      buffers.push_back(buffer);
    }

    // If the last buffer doesn't have a duration, assume it is the
    // same as the second to last buffer.
    if (buffers.size() >= 2 && buffers.back()->duration() == kNoTimestamp) {
      buffers.back()->set_duration(
          buffers[buffers.size() - 2]->duration());
    }

    return buffers;
  }

  void AppendBuffers(const std::string& buffers_to_append,
                     bool start_new_coded_frame_group,
                     base::TimeDelta coded_frame_group_start_timestamp,
                     bool one_by_one) {
    BufferQueue buffers = StringToBufferQueue(buffers_to_append);

    if (start_new_coded_frame_group) {
      base::TimeDelta start_timestamp = coded_frame_group_start_timestamp;

      base::TimeDelta buffers_start_timestamp = buffers[0]->timestamp();

      if (start_timestamp == kNoTimestamp)
        start_timestamp = buffers_start_timestamp;
      else
        ASSERT_TRUE(start_timestamp <= buffers_start_timestamp);

      stream_->OnStartOfCodedFrameGroup(start_timestamp);
    }

    if (!one_by_one) {
      stream_->Append(buffers);
      return;
    }

    // Append each buffer one by one.
    for (size_t i = 0; i < buffers.size(); i++) {
      BufferQueue wrapper;
      wrapper.push_back(buffers[i]);
      stream_->Append(wrapper);
    }
  }

  int frames_per_second_;
  int keyframes_per_second_;
  base::TimeDelta frame_duration_;
};

TEST_F(SourceBufferStreamTest, Append_SingleRange) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_SingleRange_OneBufferAtATime) {
  // Append 15 buffers starting at position 0, one buffer at a time.
  NewCodedFrameGroupAppend(0, 1);
  for (int i = 1; i < 15; i++)
    AppendBuffers(i, 1);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest, Append_DisjointRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5);

  // Append 10 buffers at positions 15 through 24.
  NewCodedFrameGroupAppend(15, 10);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [15,24) }");
  // Check buffers in ranges.
  Seek(0);
  CheckExpectedBuffers(0, 4);
  Seek(15);
  CheckExpectedBuffers(15, 24);
}

TEST_F(SourceBufferStreamTest, Append_AdjacentRanges) {
  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10);

  // Append 11 buffers at positions 15 through 25.
  NewCodedFrameGroupAppend(15, 11);

  // Append 5 buffers at positions 10 through 14 to bridge the gap.
  NewCodedFrameGroupAppend(10, 5);

  // Check expected range.
  CheckExpectedRanges("{ [0,25) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 25);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap) {
  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5);

  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 14);
}

TEST_F(SourceBufferStreamTest,
       Complete_Overlap_AfterGroupTimestampAndBeforeFirstBufferTimestamp) {
  // Append a coded frame group with a start timestamp of 0, but the first
  // buffer starts at 30ms. This can happen in muxed content where the
  // audio starts before the first frame.
  NewCodedFrameGroupAppend(base::Milliseconds(0), "30K 60K 90K 120K");

  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Completely overlap the old buffers, with a coded frame group that starts
  // after the old coded frame group start timestamp, but before the timestamp
  // of the first buffer in the coded frame group.
  NewCodedFrameGroupAppend("20K 50K 80K 110D10K");

  // Verify that the buffered ranges are updated properly and we don't crash.
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  SeekToTimestampMs(0);
  CheckExpectedBuffers("20K 50K 80K 110K 120K");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_EdgeCase) {
  // Make each frame a keyframe so that it's okay to overlap frames at any point
  // (instead of needing to respect keyframe boundaries).
  SetStreamInfo(30, 30);

  // Append 6 buffers at positions 6 through 11.
  NewCodedFrameGroupAppend(6, 6);

  // Append 8 buffers at positions 5 through 12.
  NewCodedFrameGroupAppend(5, 8);

  // Check expected range.
  CheckExpectedRanges("{ [5,12) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12);
}

TEST_F(SourceBufferStreamTest, Start_Overlap) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 5);

  // Append 6 buffers at positions 10 through 15.
  NewCodedFrameGroupAppend(10, 6);

  // Check expected range.
  CheckExpectedRanges("{ [5,15) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 15);
}

TEST_F(SourceBufferStreamTest, End_Overlap) {
  // Append 10 buffers at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10);

  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 19);
}

// Using position based test API:
// DTS  :  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
// PTS  :  0  4  1  2  3  5  9  6  7  8 10 14 11 12 13 15 19 16 17 18 20
// old  :                                A  a  a  a  a  A  a  a  a  a
// new  :                 B  b  b  b  b  B  b  b
// after:                 B  b  b  b  b  B  b  b        A  a  a  a  a
TEST_F(SourceBufferStreamTest, End_Overlap_Several) {
  // Append 10 buffers at positions 10 through 19 (DTS and PTS).
  NewCodedFrameGroupAppend(10, 10, &kDataA);

  // Append 8 buffers at positions 5 through 12 (DTS); 5 through 14 (PTS) with
  // partial second GOP.
  NewCodedFrameGroupAppend(5, 8, &kDataB);

  // Check expected ranges: stream should not have kept buffers at DTS 13,14;
  // PTS 12,13 because the keyframe on which they depended (10, PTS=DTS) was
  // overwritten. Note that partial second GOP of B includes PTS [10,14), DTS
  // [10,12). These are continuous with the overlapped original range's next GOP
  // at (15, PTS=DTS).
  // Unlike the rest of the position based test API used in this case, these
  // range expectation strings are the actual timestamps (divided by
  // frame_duration_).
  CheckExpectedRanges("{ [5,19) }");

  // Check buffers in range.
  Seek(5);
  CheckExpectedBuffers(5, 12, &kDataB);
  // No seek is necessary (1 continuous range).
  CheckExpectedBuffers(15, 19, &kDataA);
  CheckNoNextBuffer();
}

// Test an end overlap edge case where a single buffer overlaps the
// beginning of a range.
// old  : 0K   30   60   90   120K  150
// new  : 0K
// after: 0K                  120K  150
// track:
TEST_F(SourceBufferStreamTest, End_Overlap_SingleBuffer) {
  // Seek to start of stream.
  SeekToTimestampMs(0);

  NewCodedFrameGroupAppend("0K 30 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  NewCodedFrameGroupAppend("0D30K");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  CheckExpectedBuffers("0K 120K 150");
  CheckNoNextBuffer();
}

// Using position based test API:
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// PTS  :  0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0
// old  :            A a                 A a                 A a
// new  :  B b b b b B b b b b B b b b b B b b b b B b b b b B b b b b
// after == new
TEST_F(SourceBufferStreamTest, Complete_Overlap_Several) {
  // Append 2 buffers at positions 5 through 6 (DTS); 5 through 9 (PTS) partial
  // GOP.
  NewCodedFrameGroupAppend(5, 2, &kDataA);

  // Append 2 buffers at positions 15 through 16 (DTS); 15 through 19 (PTS)
  // partial GOP.
  NewCodedFrameGroupAppend(15, 2, &kDataA);

  // Append 2 buffers at positions 25 through 26 (DTS); 25 through 29 (PTS)
  // partial GOP.
  NewCodedFrameGroupAppend(25, 2, &kDataA);

  // Check expected ranges.  Unlike the rest of the position based test API used
  // in this case, these range expectation strings are the actual timestamps
  // (divided by frame_duration_).
  CheckExpectedRanges("{ [5,9) [15,19) [25,29) }");

  // Append buffers at positions 0 through 29 (DTS and PTS).
  NewCodedFrameGroupAppend(0, 30, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,29) }");
  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 29, &kDataB);
}

// Using position based test API:
// DTS:0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6
// PTS:0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9
// old:          A a                 A a                 A a                 A a
// new:B b b b b B b b b b B b b b b B b b b b B b b b b B b b b b B b b b b
TEST_F(SourceBufferStreamTest, Complete_Overlap_Several_Then_Merge) {
  // Append 2 buffers at positions 5 through 6 (DTS); 5 through 9 (PTS) partial
  // GOP.
  NewCodedFrameGroupAppend(5, 2, &kDataA);

  // Append 2 buffers at positions 10 through 11 (DTS); 15 through 19 (PTS)
  // partial GOP.
  NewCodedFrameGroupAppend(15, 2, &kDataA);

  // Append 2 buffers at positions 25 through 26 (DTS); 25 through 29 (PTS)
  // partial GOP.
  NewCodedFrameGroupAppend(25, 2, &kDataA);

  // Append 2 buffers at positions 35 through 36 (DTS); 35 through 39 (PTS)
  // partial GOP.
  NewCodedFrameGroupAppend(35, 2, &kDataA);

  // Append buffers at positions 0 through 34 (DTS and PTS).
  NewCodedFrameGroupAppend(0, 35, &kDataB);

  // Check expected ranges.  Unlike the rest of the position based test API used
  // in this case, these range expectation strings are the actual timestamps
  // (divided by frame_duration_).
  CheckExpectedRanges("{ [0,39) }");

  // Check buffers in range.
  Seek(0);
  CheckExpectedBuffers(0, 34, &kDataB);
  CheckExpectedBuffers(35, 36, &kDataA);
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5.
  Seek(5);

  // Replace old data with new data.
  NewCodedFrameGroupAppend(5, 10, &kDataB);

  // Check ranges are correct.
  CheckExpectedRanges("{ [5,14) }");

  // Check that data has been replaced with new data.
  CheckExpectedBuffers(5, 14, &kDataB);
}

// This test is testing that a client can append data to SourceBufferStream that
// overlaps the range from which the client is currently grabbing buffers. We
// would expect that the SourceBufferStream would return old data until it hits
// the keyframe of the new data, after which it will return the new data.
TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_TrackBuffer) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Expect rest of data to be new.
  CheckExpectedBuffers(11, 19, &kDataB);

  // Seek back to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_EdgeCase) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewCodedFrameGroupAppend(5, 10, &kDataB);

  // Check ranges are correct.
  CheckExpectedRanges("{ [5,14) }");

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Expect rest of data to be new.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Seek back to beginning; all data should be new.
  Seek(5);
  CheckExpectedBuffers(5, 14, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [5,14) }");
}

TEST_F(SourceBufferStreamTest, Complete_Overlap_Selected_Multiple) {
  static const uint8_t kDataC = 0x55;
  static const uint8_t kDataD = 0x77;

  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Then replace it again with different data.
  NewCodedFrameGroupAppend(5, 5, &kDataC);

  // Now append 5 new buffers at positions 10 through 14.
  NewCodedFrameGroupAppend(10, 5, &kDataC);

  // Now replace all the data entirely.
  NewCodedFrameGroupAppend(5, 10, &kDataD);

  // Expect buffers 6 through 9 to be DataA, and the remaining
  // buffers to be kDataD.
  CheckExpectedBuffers(6, 9, &kDataA);
  CheckExpectedBuffers(10, 14, &kDataD);

  // At this point we cannot fulfill request.
  CheckNoNextBuffer();

  // Seek back to beginning; all data should be new.
  Seek(5);
  CheckExpectedBuffers(5, 14, &kDataD);
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected) {
  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataA);

  // Seek to position 5, then add buffers to overlap data at that position.
  Seek(5);
  NewCodedFrameGroupAppend(5, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Because we seeked to a keyframe, the next buffers should all be new data.
  CheckExpectedBuffers(5, 14, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected_TrackBuffer) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to 10 and get buffer.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 10 buffers of new data at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");

  // The next 4 buffers should be a from the old buffer, followed by a keyframe
  // from the new data.
  CheckExpectedBuffers(11, 14, &kDataA);
  CheckExpectedBuffers(15, 15, &kDataB, true);

  // The rest of the buffers should be new data.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Now seek to the beginning; positions 0 through 9 should be the original
  // data, positions 10 through 19 should be the new data.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckExpectedBuffers(10, 19, &kDataB);

  // Make sure range is still correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Start_Overlap_Selected_EdgeCase) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now replace the last 5 buffers with new data.
  NewCodedFrameGroupAppend(10, 5, &kDataB);

  // The next 4 buffers should be the original data, held in the track buffer.
  CheckExpectedBuffers(11, 14, &kDataA);

  // The next buffer is at position 15, so we should fail to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now append data at 15 through 19 and check to make sure it's correct.
  NewCodedFrameGroupAppend(15, 5, &kDataB);
  CheckExpectedBuffers(15, 19, &kDataB);

  // Seek to beginning of buffered range and check buffers.
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
  CheckExpectedBuffers(10, 19, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [5,19) }");
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer is a keyframe that's being overlapped by new
// buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           *A*a a a a A a a a a
// new  :  B b b b b B b b b b
// after:  B b b b b*B*b b b b A a a a a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Because we seeked to a keyframe, the next buffers should be new.
  CheckExpectedBuffers(5, 9, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a a a A a a*a*a|
// new  :  B b b b b B b b b b
// after: |B b b b b B b b b b A a a*a*a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_1) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure rest of data is as expected.
  CheckExpectedBuffers(13, 14, &kDataA);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// Using position based test API:
// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
//
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// PTS  :  0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0
// old  :            A a a a a A a a*a*a
// new  :  B b b b b B b b
// after:  B b b b b B b b     A a a*a*a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_2) {
  // Append 10 buffers at positions 5 through 14 (DTS and PTS, 2 full GOPs)
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 10, then move to position 13.
  Seek(10);
  CheckExpectedBuffers(10, 12, &kDataA);

  // Now append 8 buffers at positions 0 through 7 (DTS); 0 through 9 (PTS) with
  // partial second GOP.
  NewCodedFrameGroupAppend(0, 8, &kDataB);

  // Check expected ranges: stream should not have kept buffers at DTS 8,9;
  // PTS 7,8 because the keyframe on which they depended (5, PTS=DTS) was
  // overwritten. Note that partial second GOP of B includes PTS [5,9), DTS
  // [5,7). These are continuous with the overlapped original range's next GOP
  // at (10, PTS=DTS).
  // Unlike the rest of the position based test API used in this case, these
  // range expectation strings are the actual timestamps (divided by
  // frame_duration_).
  CheckExpectedRanges("{ [0,14) }");

  // Make sure rest of data is as expected.
  CheckExpectedBuffers(13, 14, &kDataA);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 7, &kDataB);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

// Using position based test API:
// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is after the newly appended buffers.
//
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// PTS  :  0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0
// old  :            A a a*a*a A a a a a
// new  :  B b b b b B b b
// after:  B b b b b B b b     A a a a a
// track:                  a a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_AfterEndOfNew_3) {
  // Append 10 buffers at positions 5 through 14 (DTS and PTS, 2 full GOPs)
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 8 buffers at positions 0 through 7 (DTS); 0 through 9 (PTS) with
  // partial second GOP.
  NewCodedFrameGroupAppend(0, 8, &kDataB);

  // Check expected ranges: stream should not have kept buffers at DTS 8,9;
  // PTS 7,8 because the keyframe on which they depended (5, PTS=DTS) was
  // overwritten. However, they were in the GOP being read from, so were put
  // into the track buffer. Note that partial second GOP of B includes PTS
  // [5,9), DTS [5,7). These are continuous with the overlapped original range's
  // next GOP at (10, PTS=DTS).
  // Unlike the rest of the position based test API used in this case, these
  // range expectation strings are the actual timestamps (divided by
  // frame_duration_).
  CheckExpectedRanges("{ [0,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 7, &kDataB);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
//
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A a a*a*a A a a a a|
// new  :  B b b b b B b b b b
// after: |B b b b b B b b b b A a a a a|
// track:                 |a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_1) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
}

// Using position based test API:
// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
//
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// PTS  :  0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0
// old  :            A*a*a a a A a a a a
// new  :  B b b b b B b
// after:  B b b b b B b       A a a a a
// track:              a a a a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_2) {
  // Append 10 buffers at positions 5 through 14 (PTS and DTS, 2 full GOPs).
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 7 buffers at positions 0 through 6 (DTS); 0 through 9 (PTS) with
  // partial second GOP.
  NewCodedFrameGroupAppend(0, 7, &kDataB);

  // Check expected ranges: stream should not have kept buffers at DTS 7,8,9;
  // PTS 6,7,8 because the keyframe on which they depended (5, PTS=DTS) was
  // overwritten. However, they were in the GOP being read from, so were put
  // into the track buffer. Note that partial second GOP of B includes PTS
  // [5,9), DTS [5,6). These are continuous with the overlapped original range's
  // next GOP at (10, PTS=DTS).
  // Unlike the rest of the position based test API used in this case, these
  // range expectation strings are the actual timestamps (divided by
  // frame_duration_).
  CheckExpectedRanges("{ [0,14) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 6, &kDataB);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

// Using position based test API:
// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next buffer in the range is overlapped by the new buffers.
// In this particular case, the next keyframe after the track buffer is in the
// range with the new buffers.
//
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// PTS  :  0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0
// old  :            A*a*a a a A a a a a A a a a a
// new  :  B b b b b B b b b b B
// after:  B b b b b B b b b b B         A a a a a
// track:              a a a a
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_OverlappedByNew_3) {
  // Append 15 buffers at positions 5 through 19 (PTS and DTS, 3 full GOPs).
  NewCodedFrameGroupAppend(5, 15, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 11 buffers at positions 0 through 10 (PTS and DTS, 2 full GOPs
  // and just the keyframe of a third GOP).
  NewCodedFrameGroupAppend(0, 11, &kDataB);

  // Check expected ranges: stream should not have kept buffers at 11-14 (DTS
  // and PTS) because the keyframe on which they depended (10, PTS=DTS) was
  // overwritten. The GOP being read from was overwritten, so track buffer
  // should contain DTS 6-9 (PTS 9,6,7,8). Note that the partial third GOP of B
  // includes (10, PTS=DTS). This partial GOP is continuous with the overlapped
  // original range's next GOP at (15, PTS=DTS).
  // Unlike the rest of the position based test API used in this case, these
  // range expectation strings are the actual timestamps (divided by
  // frame_duration_).
  CheckExpectedRanges("{ [0,19) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);
  // The buffer immediately after the track buffer should be a keyframe
  // from the new data.
  CheckExpectedBuffers(10, 10, &kDataB, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 10, &kDataB);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(15, 19, &kDataA);
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and there is no keyframe after the end of the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a|
// new  :  B b b b b B
// after: |B b b b b B|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew) {
  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewCodedFrameGroupAppend(0, 6, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,5) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Let's fill in the gap, buffers 6 through 10.
  AppendBuffers(6, 5, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(10, 10, &kDataB);
}

// Using position based test API:
// This test covers the case where new buffers end-overlap an existing, selected
// range, and there is no keyframe after the end of the new buffers, then more
// buffers end-overlap the beginning.
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// PTS :   0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0
// old  :                      A a a a a A*a*
// new  :            B b b b b B b b b b B
// after:            B b b b b B b b b b B
// new  :  A a a a a A
// after:  A a a a a A         B b b b b B
// track:                                  a
// new  :                                B b b b b B
// after:  A a a a a A         B b b b b B b b b b B
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew2) {
  // Append 7 buffers at positions 10 through 16 (DTS); 10 through 19 (PTS) with
  // a partial second GOP.
  NewCodedFrameGroupAppend(10, 7, &kDataA);

  // Seek to position 15, then move to position 16.
  Seek(15);
  CheckExpectedBuffers(15, 15, &kDataA);

  // Now append 11 buffers at positions 5 through 15 (PTS and DTS), 2 full GOPs
  // and just the keyframe of a third GOP.
  NewCodedFrameGroupAppend(5, 11, &kDataB);

  // Check expected ranges: stream should not have kept buffer at DTS 16, PTS 19
  // because the keyframe it depended on (15, PTS=DTS) was overwritten.
  // The GOP being read from was overwritten, so track buffer
  // should contain DTS 16, PTS 19.
  // Unlike the rest of the position based test API used in this case,
  // CheckExpectedRanges() uses expectation strings containing actual timestamps
  // (divided by frame_duration_).
  CheckExpectedRanges("{ [5,15) }");

  // Now do another end-overlap. Append one full GOP plus keyframe of 2nd. Note
  // that this new keyframe at (5, PTS=DTS) is continuous with the overlapped
  // range's next GOP (B) at (10, PTS=DTS).
  NewCodedFrameGroupAppend(0, 6, &kDataA);
  CheckExpectedRanges("{ [0,15) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(16, 16, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Add data to the end of the range in the position just read from the track
  // buffer. The stream should not be able to fulfill the next read
  // until we've added a keyframe continuous beyond this point.
  NewCodedFrameGroupAppend(15, 1, &kDataB);
  CheckNoNextBuffer();
  for (int i = 16; i <= 19; i++) {
    AppendBuffers(i, 1, &kDataB);
    CheckNoNextBuffer();
  }

  // Now append a keyframe at PTS=DTS=20.
  AppendBuffers(20, 1, &kDataB);

  // The buffer at position 16 (PTS 19) in track buffer is adjacent to the next
  // keyframe, so no warning should be emitted on that track buffer exhaustion.
  // We should be able to get the next buffer (no longer from the track buffer).
  CheckExpectedBuffers(20, 20, &kDataB, true);
  CheckNoNextBuffer();

  // Make sure all data is correct.
  CheckExpectedRanges("{ [0,20) }");
  Seek(0);
  CheckExpectedBuffers(0, 5, &kDataA);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(10, 20, &kDataB);
  CheckNoNextBuffer();
}

// This test covers the case where new buffers end-overlap an existing, selected
// range, and the next keyframe in a separate range.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :           |A*a*a a a|         |A a a a a|
// new  :  B b b b b B
// after: |B b b b b B|                 |A a a a a|
// track:             |a a a a|
TEST_F(SourceBufferStreamTest, End_Overlap_Selected_NoKeyframeAfterNew3) {
  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Append 5 buffers at positions 15 through 19.
  NewCodedFrameGroupAppend(15, 5, &kDataA);

  // Check expected range.
  CheckExpectedRanges("{ [5,9) [15,19) }");

  // Seek to position 5, then move to position 6.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Now append 6 buffers at positions 0 through 5.
  NewCodedFrameGroupAppend(0, 6, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,5) [15,19) }");

  // Check for data in the track buffer.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Now there's no data to fulfill the request.
  CheckNoNextBuffer();

  // Let's fill in the gap, buffers 6 through 14.
  AppendBuffers(6, 9, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,19) }");

  // We should be able to get the next buffer.
  CheckExpectedBuffers(10, 14, &kDataB);

  // We should be able to get the next buffer.
  CheckExpectedBuffers(15, 19, &kDataA);
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is precise overlap of an existing GOP,
// and the next buffer is a keyframe.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a*A*a a a a A a a a a
// new  :            B b b b b
// after:  A a a a a*B*b b b b A a a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_1) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to position 5.
  Seek(5);

  // Now append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Check for next data; should be new data.
  CheckExpectedBuffers(5, 9, &kDataB);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when there is precise overlap of an existing GOP,
// and the next buffer is a non-keyframe in a GOP after the new buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a A a a a a A*a*a a a
// new  :            B b b b b
// after:  A a a a a B b b b b A*a*a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_2) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to 10 then move to position 11.
  Seek(10);
  CheckExpectedBuffers(10, 10, &kDataA);

  // Now append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure data is correct.
  CheckExpectedBuffers(11, 14, &kDataA);
  CheckNoNextBuffer();
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 9, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when only a partial GOP is appended, that append
// is merged into the overlapped range, and the next buffer is before the new
// buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a*a*a a A a a a a A a a a a
// new  :            B
// after:  A a*a*a a B         A a a a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_3) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to beginning then move to position 2.
  Seek(0);
  CheckExpectedBuffers(0, 1, &kDataA);

  // Now append 1 buffer at position 5 (just the keyframe of a GOP).
  NewCodedFrameGroupAppend(5, 1, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Make sure data is correct.
  CheckExpectedBuffers(2, 4, &kDataA);
  CheckExpectedBuffers(5, 5, &kDataB);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();

  // Seek to the beginning and recheck data in case track buffer erroneously
  // became involved.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 5, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

// This test covers the case when new buffers overlap the middle of a selected
// range. This tests the case when only a partial GOP is appended, and the next
// buffer is after the new buffers, and comes from the track buffer until the
// next GOP in the original buffers.
// index:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
// old  :  A a a a a A a a*a*a A a a a a
// new  :            B
// after:  A a a a a B         A a a a a
// track:                  a a
TEST_F(SourceBufferStreamTest, Middle_Overlap_Selected_4) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15, &kDataA);

  // Seek to 5 then move to position 8.
  Seek(5);
  CheckExpectedBuffers(5, 7, &kDataA);

  // Now append 1 buffer at position 5.
  NewCodedFrameGroupAppend(5, 1, &kDataB);

  // Check expected range.
  CheckExpectedRanges("{ [0,14) }");

  // Buffers 8 and 9 should be in the track buffer.
  CheckExpectedBuffers(8, 9, &kDataA);

  // The buffer immediately after the track buffer should be a keyframe.
  CheckExpectedBuffers(10, 10, &kDataA, true);

  // Make sure all data is correct.
  Seek(0);
  CheckExpectedBuffers(0, 4, &kDataA);
  CheckExpectedBuffers(5, 5, &kDataB);
  // No seek should be necessary (1 continuous range).
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne) {
  // Append 5 buffers starting at 10ms, 30ms apart.
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100 130");

  // The range ends at 160, accounting for the last buffer's duration.
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Overlap with 10 buffers starting at the beginning, appended one at a
  // time.
  NewCodedFrameGroupAppend(0, 1, &kDataB);
  for (int i = 1; i < 10; i++)
    AppendBuffers(i, 1, &kDataB);

  // All data should be replaced.
  Seek(0);
  CheckExpectedRanges("{ [0,9) }");
  CheckExpectedBuffers(0, 9, &kDataB);
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_DeleteGroup) {
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 130ms.
  SeekToTimestampMs(130);

  // Overlap with a new coded frame group from 0 to 130ms.
  NewCodedFrameGroupAppendOneByOne("0K 120D10");

  // Next buffer should still be 130ms.
  CheckExpectedBuffers("130K");

  // Check the final buffers is correct.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 120 130K");
}

TEST_F(SourceBufferStreamTest, Overlap_OneByOne_BetweenCodedFrameGroups) {
  // Append 5 buffers starting at 110ms, 30ms apart.
  NewCodedFrameGroupAppendOneByOne("110K 140 170 200 230");
  CheckExpectedRangesByTimestamp("{ [110,260) }");

  // Now append 2 coded frame groups from 0ms to 210ms, 30ms apart. Note that
  // the
  // old keyframe 110ms falls in between these two groups.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90");
  NewCodedFrameGroupAppendOneByOne("120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  // Check the final buffers is correct; the keyframe at 110ms should be
  // deleted.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 120K 150 180 210");
}

// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(50));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 130ms.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Should return frame 70ms from the track buffer, then switch
  // to the new data at 120K, then switch back to the old data at 130K. The
  // frame at 125ms that depended on keyframe 100ms should have been deleted.
  CheckExpectedBuffers("70 120K 130K");

  // Check the final result: should not include data from the track buffer.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
}

// Overlap the next keyframe after the end of the track buffer with a new
// keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70
// new  :                     110K    130
// after: 0K   30   60   90  *110K*   130
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer2) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(40));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms and 100ms go in
  // track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms.
  NewCodedFrameGroupAppendOneByOne("110K 130");

  // Should return frame 70ms from the track buffer. Then it should
  // return the keyframe after the track buffer, which is at 110ms.
  CheckExpectedBuffers("70 110K 130");
}

// Overlap the next keyframe after the end of the track buffer without a
// new keyframe.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70
// new  :        50K   80   110          140
// after: 0K   30   50K   80   110   140 * (waiting for keyframe)
// track:             70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer3) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(80));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms goes in track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now overlap the keyframe at 120ms and 130ms.
  NewCodedFrameGroupAppendOneByOne("50K 80 110 140");
  CheckExpectedRangesByTimestamp("{ [0,170) }");

  // Should have all the buffers from the track buffer, then stall.
  CheckExpectedBuffers("70");
  CheckNoNextBuffer();

  // Appending a keyframe should fulfill the read.
  AppendBuffersOneByOne("150D30K");
  CheckExpectedBuffers("150K");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer.
// old  :   10K  40  *70*  100K  125  130K
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K
// track:             70
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer4) {
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms and 100ms go in
  // track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) }");

  // Now append a keyframe at 80ms.
  NewCodedFrameGroupAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Overlap the next keyframe after the end of the track buffer with a keyframe
// that comes before the end of the track buffer, when the selected stream was
// waiting for the next keyframe.
// old  :   10K  40  *70*  100K
// new  : 0K   30   60   90   120
// after: 0K   30   60   90   120 * (waiting for keyframe)
// track:             70
// new  :              80K  110          140
// after: 0K   30   60   *80K*  110   140
// track:               70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer5) {
  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K");
  CheckExpectedRangesByTimestamp("{ [10,130) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms; 70ms goes in track
  // buffer.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Now append a keyframe at 80ms.
  NewCodedFrameGroupAppendOneByOne("80K 110 140");

  CheckExpectedBuffers("70 80K 110 140");
  CheckNoNextBuffer();
}

// Test that appending to a different range while there is data in
// the track buffer doesn't affect the selected range or track buffer state.
// old  :   10K  40  *70*  100K  125  130K ... 200K 230
// new  : 0K   30   60   90   120K
// after: 0K   30   60   90  *120K*   130K ... 200K 230
// track:             70
// old  : 0K   30   60   90  *120K*   130K ... 200K 230
// new  :                                               260K 290
// after: 0K   30   60   90  *120K*   130K ... 200K 230 260K 290
// track:             70
TEST_F(SourceBufferStreamTest, Overlap_OneByOne_TrackBuffer6) {
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(50));

  NewCodedFrameGroupAppendOneByOne("10K 40 70 100K 125 130D30K");
  NewCodedFrameGroupAppendOneByOne("200K 230");
  CheckExpectedRangesByTimestamp("{ [10,160) [200,260) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms.
  NewCodedFrameGroupAppendOneByOne("0K 30 60 90 120D10K");
  CheckExpectedRangesByTimestamp("{ [0,160) [200,260) }");

  // Verify that 70 gets read out of the track buffer.
  CheckExpectedBuffers("70");

  // Append more data to the unselected range.
  NewCodedFrameGroupAppendOneByOne("260K 290");
  CheckExpectedRangesByTimestamp("{ [0,160) [200,320) }");

  CheckExpectedBuffers("120K 130K");
  CheckNoNextBuffer();

  // Check the final result: should not include data from the track buffer.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 120K 130K");
  CheckNoNextBuffer();
}

// Test that overlap-appending with a GOP that begins with time of next track
// buffer frame drops that track buffer frame and buffers the new GOP correctly.
// append :    10K   40    70     100
// read the first two buffers
// after  :    10K   40   *70*    100
//
// append : 0K    30    60    90    120
// after  : 0K    30    60    90    120
// track  :               *70*    100
//
// read the buffer at 70ms from track
// after  : 0K    30    60    90    120
// track  :                      *100*
//
// append :                       100K   130
// after  : 0K    30    60    90 *100K*  130
// track  : (empty)
// 100K, not 100, should be the next buffer read.
TEST_F(SourceBufferStreamTest,
       Overlap_That_Prunes_All_of_Previous_TrackBuffer) {
  NewCodedFrameGroupAppend("10K 40 70 100");
  CheckExpectedRangesByTimestamp("{ [10,130) }");

  // Seek to 70ms.
  SeekToTimestampMs(70);
  CheckExpectedBuffers("10K 40");

  // Overlap with a new coded frame group from 0 to 120ms, leaving the original
  // nonkeyframes at 70ms and 100ms in the track buffer.
  NewCodedFrameGroupAppend("0K 30 60 90 120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Verify that 70 gets read out of the track buffer, leaving the nonkeyframe
  // at 100ms in the track buffer.
  CheckExpectedBuffers("70");

  // Overlap with a coded frame group having a keyframe at 100ms. This should
  // clear the track buffer and serve that keyframe, not the original
  // nonkeyframe at time 100ms on the next read call.
  NewCodedFrameGroupAppend("100K 130");
  CheckExpectedRangesByTimestamp("{ [0,160) }");
  CheckExpectedBuffers("100K 130");
  CheckNoNextBuffer();

  // Check the final result: should not include data from the track buffer.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90 100K 130");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_Keyframe) {
  // Append 6 buffers at positions 0 through 5.
  NewCodedFrameGroupAppend(0, 6);

  // Seek to beginning.
  Seek(0);
  CheckExpectedBuffers(0, 5, true);
}

TEST_F(SourceBufferStreamTest, Seek_NonKeyframe) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

  // Seek to buffer at position 13.
  Seek(13);

  // Expect seeking back to the nearest keyframe.
  CheckExpectedBuffers(10, 14, true);

  // Seek to buffer at position 3.
  Seek(3);

  // Expect seeking back to the nearest keyframe.
  CheckExpectedBuffers(0, 3, true);
}

TEST_F(SourceBufferStreamTest, Seek_NotBuffered) {
  // Seek to beginning.
  SeekToTimestampMs(0);

  // Try to get buffer; nothing's appended.
  CheckNoNextBuffer();

  // Append 1 buffer at time 0, duration 10ms.
  NewCodedFrameGroupAppend("0D10K");

  // Confirm we can read it back.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K");

  // Try to get buffer out of range.
  SeekToTimestampMs(10);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Seek_InBetweenTimestamps) {
  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10);

  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump.is_positive());

  // Seek to buffer a little after position 5.
  stream_->Seek(5 * frame_duration() + bump);
  CheckExpectedBuffers(5, 5, true);

  // Seek to buffer a little before position 5.
  stream_->Seek(5 * frame_duration() - bump);
  CheckExpectedBuffers(0, 0, true);
}

// This test will do a complete overlap of an existing range in order to add
// buffers to the track buffers. Then the test does a seek to another part of
// the stream. The SourceBufferStream should clear its internal track buffer in
// response to the Seek().
TEST_F(SourceBufferStreamTest, Seek_After_TrackBuffer_Filled) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Do a complete overlap by appending 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20, &kDataB);

  // Check range is correct.
  CheckExpectedRanges("{ [0,19) }");

  // Seek to beginning; all data should be new.
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataB);

  // Check range continues to be correct.
  CheckExpectedRanges("{ [0,19) }");
}

TEST_F(SourceBufferStreamTest, Seek_StartOfGroup) {
  base::TimeDelta bump = frame_duration() / 4;
  CHECK(bump.is_positive());

  // Append 5 buffers at position (5 + |bump|) through 9, where the coded frame
  // group begins at position 5.
  Seek(5);
  NewCodedFrameGroupAppend_OffsetFirstBuffer(5, 5, bump);
  scoped_refptr<StreamParserBuffer> buffer;

  // GetNextBuffer() should return the next buffer at position (5 + |bump|).
  EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));
  EXPECT_EQ(buffer->GetDecodeTimestamp(),
            DecodeTimestamp::FromPresentationTime(5 * frame_duration() + bump));

  // Check rest of buffers.
  CheckExpectedBuffers(6, 9);

  // Seek to position 15.
  Seek(15);

  // Append 5 buffers at positions (15 + |bump|) through 19, where the coded
  // frame group begins at 15.
  NewCodedFrameGroupAppend_OffsetFirstBuffer(15, 5, bump);

  // GetNextBuffer() should return the next buffer at position (15 + |bump|).
  EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));
  EXPECT_EQ(buffer->GetDecodeTimestamp(), DecodeTimestamp::FromPresentationTime(
      15 * frame_duration() + bump));

  // Check rest of buffers.
  CheckExpectedBuffers(16, 19);
}

TEST_F(SourceBufferStreamTest, Seek_BeforeStartOfGroup) {
  // Append 10 buffers at positions 5 through 14.
  NewCodedFrameGroupAppend(5, 10);

  // Seek to a time before the first buffer in the range.
  Seek(0);

  // Should return buffers from the beginning of the range.
  CheckExpectedBuffers(5, 14);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 4);

  // Append 5 buffers at positions 10 through 14, and seek to the beginning of
  // this range.
  NewCodedFrameGroupAppend(10, 5);
  Seek(10);

  // Now seek to the beginning of the first range.
  Seek(0);

  // Completely overlap the old seek point.
  NewCodedFrameGroupAppend(5, 15);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_CompleteOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 5 buffers at positions 15 through 19 and seek to beginning of the
  // range.
  NewCodedFrameGroupAppend(15, 5);
  Seek(15);

  // Now seek position 5.
  Seek(5);

  // Completely overlap the old seek point.
  NewCodedFrameGroupAppend(10, 15);

  // The seek at position 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap) {
  // Append 1 buffer at position 0, duration 10ms.
  NewCodedFrameGroupAppend("0D10K");

  // Append 3 IPBBB GOPs starting at 50ms.
  NewCodedFrameGroupAppend(
      "50K 90|60 60|70 70|80 80|90 "
      "100K 140|110 110|120 120|130 130|140 "
      "150K 190|160 160|170 170|180 180|190");
  SeekToTimestampMs(150);

  // Now seek to the beginning of the stream.
  SeekToTimestampMs(0);

  // Overlap the middle of the last range with a partial GOP, just a keyframe.
  NewCodedFrameGroupAppend("100D10K");
  CheckExpectedRangesByTimestamp("{ [0,10) [50,200) }");

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();

  // Check the data in the second range.
  SeekToTimestampMs(50);
  CheckExpectedBuffers(
      "50K 90|60 60|70 70|80 80|90 100K 150K 190|160 160|170 170|180 180|190");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_MiddleOverlap_Pending) {
  // Append 1 buffer at position 0, duration 10ms.
  NewCodedFrameGroupAppend("0D10K");

  // Append 3 IPBBB GOPs starting at 50ms. Then seek to 150ms.
  NewCodedFrameGroupAppend(
      "50K 90|60 60|70 70|80 80|90 "
      "100K 140|110 110|120 120|130 130|140 "
      "150K 190|160 160|170 170|180 180|190");
  SeekToTimestampMs(150);

  // Now seek to unbuffered time 20ms.
  SeekToTimestampMs(20);

  // Overlap the middle of the last range with a partial GOP, just a keyframe.
  NewCodedFrameGroupAppend("100D10K");
  CheckExpectedRangesByTimestamp("{ [0,10) [50,200) }");

  // The seek to 20ms should still be pending.
  CheckNoNextBuffer();

  // Check the data in both ranges.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();
  SeekToTimestampMs(50);
  CheckExpectedBuffers(
      "50K 90|60 60|70 70|80 80|90 100K 150K 190|160 160|170 170|180 180|190");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 5 through 19 and seek to position 15.
  NewCodedFrameGroupAppend(5, 15);
  Seek(15);

  // Now seek to the beginning of the stream.
  Seek(0);

  // Start overlap the old seek point.
  NewCodedFrameGroupAppend(10, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_StartOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 10 through 24 and seek to position 20.
  NewCodedFrameGroupAppend(10, 15);
  Seek(20);

  // Now seek to position 5.
  Seek(5);

  // Start overlap the old seek point.
  NewCodedFrameGroupAppend(15, 10);

  // The seek at time 0 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 4);

  // Append 15 buffers at positions 10 through 24 and seek to start of range.
  NewCodedFrameGroupAppend(10, 15);
  Seek(10);

  // Now seek to the beginning of the stream.
  Seek(0);

  // End overlap the old seek point.
  NewCodedFrameGroupAppend(5, 10);

  // The GetNextBuffer() call should respect the 2nd seek point.
  CheckExpectedBuffers(0, 0);
}

TEST_F(SourceBufferStreamTest, OldSeekPoint_EndOverlap_Pending) {
  // Append 2 buffers at positions 0 through 1.
  NewCodedFrameGroupAppend(0, 2);

  // Append 15 buffers at positions 15 through 29 and seek to start of range.
  NewCodedFrameGroupAppend(15, 15);
  Seek(15);

  // Now seek to position 5
  Seek(5);

  // End overlap the old seek point.
  NewCodedFrameGroupAppend(10, 10);

  // The seek at time 5 should still be pending.
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_AfterMerges) {
  // Append 5 buffers at positions 10 through 14.
  NewCodedFrameGroupAppend(10, 5);

  // Seek to buffer at position 12.
  Seek(12);

  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5);

  // Make sure ranges are merged.
  CheckExpectedRanges("{ [5,14) }");

  // Make sure the next buffer is correct.
  CheckExpectedBuffers(10, 10);

  // Append 5 buffers at positions 15 through 19.
  NewCodedFrameGroupAppend(15, 5);
  CheckExpectedRanges("{ [5,19) }");

  // Make sure the remaining next buffers are correct.
  CheckExpectedBuffers(11, 14);
}

TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenAppend) {
  // Append 4 buffers at positions 0 through 3.
  NewCodedFrameGroupAppend(0, 4);

  // Seek to buffer at position 0 and get all buffers.
  Seek(0);
  CheckExpectedBuffers(0, 3);

  // Next buffer is at position 4, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 2 buffers at positions 4 through 5.
  AppendBuffers(4, 2);
  CheckExpectedBuffers(4, 5);
}

// This test covers the case where new buffers start-overlap a range whose next
// buffer is not buffered.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenStartOverlap) {
  // Append 10 buffers at positions 0 through 9 and exhaust the buffers.
  NewCodedFrameGroupAppend(0, 10, &kDataA);
  Seek(0);
  CheckExpectedBuffers(0, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Append 6 buffers at positions 5 through 10. This is to test that doing a
  // start-overlap successfully fulfills the read at position 10, even though
  // position 10 was unbuffered.
  NewCodedFrameGroupAppend(5, 6, &kDataB);
  CheckExpectedBuffers(10, 10, &kDataB);

  // Then add 5 buffers from positions 11 though 15.
  AppendBuffers(11, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 15.
  CheckExpectedBuffers(11, 14, &kDataB);

  // Replace the next buffer at position 15 with another start overlap.
  NewCodedFrameGroupAppend(15, 2, &kDataA);
  CheckExpectedBuffers(15, 16, &kDataA);
}

// Tests a start overlap that occurs right at the timestamp of the last output
// buffer that was returned by GetNextBuffer(). This test verifies that
// GetNextBuffer() skips to second GOP in the newly appended data instead
// of returning two buffers with the same timestamp.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenStartOverlap2) {
  NewCodedFrameGroupAppend("0K 30 60 90 120");

  Seek(0);
  CheckExpectedBuffers("0K 30 60 90 120");
  CheckNoNextBuffer();

  // Append a keyframe with the same timestamp as the last buffer output.
  NewCodedFrameGroupAppend("120D30K");
  CheckNoNextBuffer();

  // Append the rest of the coded frame group and make sure that buffers are
  // returned from the first GOP after 120.
  AppendBuffers("150 180 210K 240");
  CheckExpectedBuffers("210K 240");

  // Seek to the beginning and verify the contents of the source buffer.
  Seek(0);
  CheckExpectedBuffers("0K 30 60 90 120K 150 180 210K 240");
  CheckNoNextBuffer();
}

// This test covers the case where new buffers completely overlap a range
// whose next buffer is not buffered.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenCompleteOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewCodedFrameGroupAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do a complete overlap and test that this successfully fulfills the read
  // at position 15.
  NewCodedFrameGroupAppend(5, 11, &kDataB);
  CheckExpectedBuffers(15, 15, &kDataB);

  // Then add 5 buffers from positions 16 though 20.
  AppendBuffers(16, 5, &kDataB);

  // Check the next 4 buffers are correct, which also effectively seeks to
  // position 20.
  CheckExpectedBuffers(16, 19, &kDataB);

  // Do a complete overlap and replace the buffer at position 20.
  NewCodedFrameGroupAppend(0, 21, &kDataA);
  CheckExpectedBuffers(20, 20, &kDataA);
}

// This test covers the case where a range is stalled waiting for its next
// buffer, then an end-overlap causes the end of the range to be deleted.
TEST_F(SourceBufferStreamTest, GetNextBuffer_ExhaustThenEndOverlap) {
  // Append 5 buffers at positions 10 through 14 and exhaust the buffers.
  NewCodedFrameGroupAppend(10, 5, &kDataA);
  Seek(10);
  CheckExpectedBuffers(10, 14, &kDataA);
  CheckExpectedRanges("{ [10,14) }");

  // Next buffer is at position 15, so should not be able to fulfill request.
  CheckNoNextBuffer();

  // Do an end overlap that causes the latter half of the range to be deleted.
  NewCodedFrameGroupAppend(5, 6, &kDataB);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [5,10) }");

  // Fill in the gap. Getting the next buffer should still stall at position 15.
  for (int i = 11; i <= 14; i++) {
    AppendBuffers(i, 1, &kDataB);
    CheckNoNextBuffer();
  }

  // Append the buffer at position 15 and check to make sure all is correct.
  AppendBuffers(15, 1);
  CheckExpectedBuffers(15, 15);
  CheckExpectedRanges("{ [5,15) }");
}

// This test is testing the "next buffer" logic after a complete overlap. In
// this scenario, when the track buffer is exhausted, there is no buffered data
// to fulfill the request. The SourceBufferStream should be able to fulfill the
// request when the data is later appended, and should not lose track of the
// "next buffer" position.
TEST_F(SourceBufferStreamTest, GetNextBuffer_Overlap_Selected_Complete) {
  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataA);

  // Seek to buffer at position 5 and get next buffer.
  Seek(5);
  CheckExpectedBuffers(5, 5, &kDataA);

  // Replace existing data with new data.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Expect old data up until next keyframe in new data.
  CheckExpectedBuffers(6, 9, &kDataA);

  // Next buffer is at position 10, so should not be able to fulfill the
  // request.
  CheckNoNextBuffer();

  // Now add 5 new buffers at positions 10 through 14.
  AppendBuffers(10, 5, &kDataB);
  CheckExpectedBuffers(10, 14, &kDataB);
}

TEST_F(SourceBufferStreamTest, PresentationTimestampIndependence) {
  // Append 20 buffers at position 0.
  NewCodedFrameGroupAppend(0, 20);
  Seek(0);

  int last_keyframe_idx = -1;
  base::TimeDelta last_keyframe_presentation_timestamp;
  base::TimeDelta last_p_frame_presentation_timestamp;

  // Check for IBB...BBP pattern.
  for (int i = 0; i < 20; i++) {
    scoped_refptr<StreamParserBuffer> buffer;
    EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));

    if (buffer->is_key_frame()) {
      EXPECT_EQ(DecodeTimestamp::FromPresentationTime(buffer->timestamp()),
                buffer->GetDecodeTimestamp());
      last_keyframe_idx = i;
      last_keyframe_presentation_timestamp = buffer->timestamp();
    } else if (i == last_keyframe_idx + 1) {
      ASSERT_NE(last_keyframe_idx, -1);
      last_p_frame_presentation_timestamp = buffer->timestamp();
      EXPECT_LT(last_keyframe_presentation_timestamp,
                last_p_frame_presentation_timestamp);
    } else {
      EXPECT_GT(buffer->timestamp(), last_keyframe_presentation_timestamp);
      EXPECT_LT(buffer->timestamp(), last_p_frame_presentation_timestamp);
      EXPECT_LT(DecodeTimestamp::FromPresentationTime(buffer->timestamp()),
                buffer->GetDecodeTimestamp());
    }
  }
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFront) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 1, &kDataA);
  for (int i = 1; i < 20; i++)
    AppendBuffers(i, 1, &kDataA);

  // GC should be a no-op, since we are just under memory limit.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRanges("{ [0,19) }");
  Seek(0);
  CheckExpectedBuffers(0, 19, &kDataA);

  // Seek to the middle of the stream.
  Seek(10);

  // We are about to append 5 new buffers and current playback position is 10,
  // so the GC algorithm should be able to delete some old data from the front.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(10, 5));
  CheckExpectedRanges("{ [5,19) }");

  // Append 5 buffers to the end of the stream.
  AppendBuffers(20, 5, &kDataA);
  CheckExpectedRanges("{ [5,24) }");

  CheckExpectedBuffers(10, 24, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest,
       GarbageCollection_DeleteFront_PreserveSeekedGOP) {
  // Set memory limit to 15 buffers.
  SetMemoryLimit(15);

  NewCodedFrameGroupAppend("0K 10 20 30 40 50K 60 70 80 90");
  NewCodedFrameGroupAppend("1000K 1010 1020 1030 1040");

  // GC should be a no-op, since we are just under memory limit.
  EXPECT_TRUE(GarbageCollect(base::TimeDelta(), 0));
  CheckExpectedRangesByTimestamp("{ [0,100) [1000,1050) }");

  // Seek to the near the end of the first range
  SeekToTimestampMs(95);

  // We are about to append 7 new buffers and current playback position is at
  // the end of the last GOP in the first range, so the GC algorithm should be
  // able to delete some old data from the front, but must not collect the last
  // GOP in that first range. Neither can it collect the last appended GOP
  // (which is the entire second range), so GC should return false since it
  // couldn't collect enough.
  EXPECT_FALSE(
      GarbageCollect(base::Milliseconds(95), 7 * GetMemoryUsagePerBuffer()));
  CheckExpectedRangesByTimestamp("{ [50,100) [1000,1050) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontGOPsAtATime) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 20 buffers at positions 0 through 19.
  NewCodedFrameGroupAppend(0, 20, &kDataA);

  // Seek to position 10.
  Seek(10);
  CheckExpectedRanges("{ [0,19) }");

  // Add one buffer to put the memory over the cap.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(10, 1));
  AppendBuffers(20, 1, &kDataA);

  // GC should have deleted the first 5 buffers so that the range still begins
  // with a keyframe.
  CheckExpectedRanges("{ [5,20) }");
  CheckExpectedBuffers(10, 20, &kDataA);
  Seek(5);
  CheckExpectedBuffers(5, 9, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteBack) {
  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Append 5 buffers at positions 15 through 19.
  NewCodedFrameGroupAppend(15, 5, &kDataA);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));

  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5, &kDataA);
  CheckExpectedRanges("{ [0,4) [15,19) }");

  // Seek to position 0.
  Seek(0);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  // Should leave the first 5 buffers from 0 to 4.
  CheckExpectedRanges("{ [0,4) }");
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteFrontAndBack) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 40 buffers at positions 0 through 39.
  NewCodedFrameGroupAppend(0, 40, &kDataA);
  // GC will try to keep data between current playback position and last append
  // position. This will ensure that the last append position is 19 and will
  // allow GC algorithm to collect data outside of the range [15,19)
  NewCodedFrameGroupAppend(15, 5, &kDataA);
  CheckExpectedRanges("{ [0,39) }");

  // Should leave the GOP containing the current playback position 15 and the
  // last append position 19. GC returns false, since we are still above limit.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(15, 0));
  CheckExpectedRanges("{ [15,19) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteSeveralRanges) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5);

  // Append 5 buffers at positions 10 through 14.
  NewCodedFrameGroupAppend(10, 5);

  // Append 5 buffers at positions 20 through 24.
  NewCodedFrameGroupAppend(20, 5);

  // Append 5 buffers at positions 40 through 44.
  NewCodedFrameGroupAppend(40, 5);

  CheckExpectedRanges("{ [0,4) [10,14) [20,24) [40,44) }");

  // Seek to position 20.
  Seek(20);
  CheckExpectedBuffers(20, 20);

  // Set memory limit to 1 buffer.
  SetMemoryLimit(1);

  // Append 5 buffers at positions 30 through 34.
  NewCodedFrameGroupAppend(30, 5);

  // We will have more than 1 buffer left, GC will fail
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(20, 0));

  // Should have deleted all buffer ranges before the current buffer and after
  // last GOP
  CheckExpectedRanges("{ [20,24) [30,34) }");
  CheckExpectedBuffers(21, 24);
  CheckNoNextBuffer();

  // Continue appending into the last range to make sure it didn't break.
  AppendBuffers(35, 10);
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(20, 0));
  // Should save everything between read head and last appended
  CheckExpectedRanges("{ [20,24) [30,44) }");

  // Make sure appending before and after the ranges didn't somehow break.
  SetMemoryLimit(100);
  NewCodedFrameGroupAppend(0, 10);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(20, 0));
  CheckExpectedRanges("{ [0,9) [20,24) [30,44) }");
  Seek(0);
  CheckExpectedBuffers(0, 9);

  NewCodedFrameGroupAppend(90, 10);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRanges("{ [0,9) [20,24) [30,44) [90,99) }");
  Seek(30);
  CheckExpectedBuffers(30, 44);
  CheckNoNextBuffer();
  Seek(90);
  CheckExpectedBuffers(90, 99);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteAfterLastAppend) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 1 GOP starting at 310ms, 30ms apart.
  NewCodedFrameGroupAppend("310K 340 370");

  // Append 2 GOPs starting at 490ms, 30ms apart.
  NewCodedFrameGroupAppend("490K 520 550 580K 610 640");

  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));

  CheckExpectedRangesByTimestamp("{ [310,400) [490,670) }");

  // Seek to the GOP at 580ms.
  SeekToTimestampMs(580);

  // Append 2 GOPs before the existing ranges.
  // So the ranges before GC are "{ [100,280) [310,400) [490,670) }".
  NewCodedFrameGroupAppend("100K 130 160 190K 220 250K");

  EXPECT_TRUE(GarbageCollect(base::Milliseconds(580), 0));

  // Should save the newly appended GOPs.
  CheckExpectedRangesByTimestamp("{ [100,280) [580,670) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_DeleteAfterLastAppendMerged) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 3 GOPs starting at 400ms, 30ms apart.
  NewCodedFrameGroupAppend("400K 430 460 490K 520 550 580K 610 640");

  // Seek to the GOP at 580ms.
  SeekToTimestampMs(580);

  // Append 2 GOPs starting at 220ms, and they will be merged with the existing
  // range.  So the range before GC is "{ [220,670) }".
  NewCodedFrameGroupAppend("220K 250 280 310K 340 370");

  EXPECT_TRUE(GarbageCollect(base::Milliseconds(580), 0));

  // Should save the newly appended GOPs.
  CheckExpectedRangesByTimestamp("{ [220,400) [580,670) }");
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NoSeek) {
  // Set memory limit to 20 buffers.
  SetMemoryLimit(20);

  // Append 25 buffers at positions 0 through 24.
  NewCodedFrameGroupAppend(0, 25, &kDataA);

  // If playback is still in the first GOP (starting at 0), GC should fail.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(2, 0));
  CheckExpectedRanges("{ [0,24) }");

  // As soon as playback position moves past the first GOP, it should be removed
  // and after removing the first GOP we are under memory limit.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(5, 0));
  CheckExpectedRanges("{ [5,24) }");
  CheckNoNextBuffer();
  Seek(5);
  CheckExpectedBuffers(5, 24, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_PendingSeek) {
  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataA);

  // Append 5 buffers at positions 25 through 29.
  NewCodedFrameGroupAppend(25, 5, &kDataA);

  // Seek to position 15.
  Seek(15);
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,9) [25,29) }");

  // Set memory limit to 5 buffers.
  SetMemoryLimit(5);

  // Append 5 buffers as positions 30 to 34 to trigger GC.
  AppendBuffers(30, 5, &kDataA);

  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(30, 0));

  // The current algorithm will delete from the beginning until the memory is
  // under cap.
  CheckExpectedRanges("{ [30,34) }");

  // Expand memory limit again so that GC won't be triggered.
  SetMemoryLimit(100);

  // Append data to fulfill seek.
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(30, 5));
  NewCodedFrameGroupAppend(15, 5, &kDataA);

  // Check to make sure all is well.
  CheckExpectedRanges("{ [15,19) [30,34) }");
  CheckExpectedBuffers(15, 19, &kDataA);
  Seek(30);
  CheckExpectedBuffers(30, 34, &kDataA);
}

TEST_F(SourceBufferStreamTest, GarbageCollection_NeedsMoreData) {
  // Set memory limit to 15 buffers.
  SetMemoryLimit(15);

  // Append 10 buffers at positions 0 through 9.
  NewCodedFrameGroupAppend(0, 10, &kDataA);

  // Advance next buffer position to 10.
  Seek(0);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedBuffers(0, 9, &kDataA);
  CheckNoNextBuffer();

  // Append 20 buffers at positions 15 through 34.
  NewCodedFrameGroupAppend(15, 20, &kDataA);
  CheckExpectedRanges("{ [0,9) [15,34) }");

  // GC should save the keyframe before the next buffer position and the data
  // closest to the next buffer position. It will also save all buffers from
  // next buffer to the last GOP appended, which overflows limit and leads to
  // failure.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(5, 0));
  CheckExpectedRanges("{ [5,9) [15,34) }");

  // Now fulfill the seek at position 10. This will make GC delete the data
  // before position 10 to keep it within cap.
  NewCodedFrameGroupAppend(10, 5, &kDataA);
  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(10, 0));
  CheckExpectedRanges("{ [10,24) }");
  CheckExpectedBuffers(10, 24, &kDataA);
}

// Using position based test API:
// DTS  :  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4
// PTS  :  0 4 1 2 3 5 9 6 7 8 0 4 1 2 3 5 9 6 7 8 0 4 1 2 3
// old  :  A a a a a A a a a a A a a a a*A*a a
//       -- Garbage Collect --
// after:                                A a a
//       -- Read one buffer --
// after:                                A*a*a
// new  :  B b b b b B b b b b B b b b b B b b b b
// track:                                 *a*a
//       -- Garbage Collect --
// after:                                B b b b b
// track:                                 *a*a
//       -- Read 2 buffers -> exhausts track buffer
// after:                                B b b b b
//       (awaiting next keyframe after GOP at position 15)
// new  :                                         *B*b b b b
// after:                                B b b b b*B*b b b b
//       -- Garbage Collect --
// after:                                         *B*b b b b
TEST_F(SourceBufferStreamTest, GarbageCollection_TrackBuffer) {
  // Set memory limit to 3 buffers.
  SetMemoryLimit(3);

  // Seek to position 15.
  Seek(15);

  // Append 18 buffers at positions 0 through 17 (DTS), 0 through 19 (PTS) with
  // partial 4th GOP.
  NewCodedFrameGroupAppend(0, 18, &kDataA);

  EXPECT_TRUE(GarbageCollectWithPlaybackAtBuffer(15, 0));

  // GC should leave GOP containing seek position (15,16,17 DTS; 15,19,16 PTS).
  // Unlike the rest of the position based test API used in this case,
  // CheckExpectedRanges() uses expectation strings containing actual timestamps
  // (divided by frame_duration_).
  CheckExpectedRanges("{ [15,19) }");

  // Move next buffer position to 16.
  CheckExpectedBuffers(15, 15, &kDataA);

  // Completely overlap the existing buffers with 4 full GOPs (0-19, PTS and
  // DTS).
  NewCodedFrameGroupAppend(0, 20, &kDataB);

  // Final GOP [15,19) contains 5 buffers, which is more than memory limit of
  // 3 buffers set at the beginning of the test, so GC will fail.
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(15, 0));

  // Because buffers 16,17 (DTS), 19,16 (PTS) are not keyframes, they are moved
  // to the track buffer upon overlap. The source buffer (i.e. not the track
  // buffer) is now waiting for the next keyframe beyond GOP that survived GC.
  CheckExpectedRanges("{ [15,19) }");     // Source buffer
  CheckExpectedBuffers(16, 17, &kDataA);  // Exhaust the track buffer
  CheckNoNextBuffer();  // Confirms the source buffer is awaiting next keyframe.

  // Now add a 5-frame GOP  at position 20-24 (PTS and DTS).
  AppendBuffers(20, 5, &kDataB);

  // 5 buffers in final GOP, GC will fail
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(20, 0));

  // Should garbage collect such that there are 5 frames remaining, starting at
  // the keyframe.
  CheckExpectedRanges("{ [20,24) }");

  // The buffer at position 16 (PTS 19) in track buffer was adjacent
  // to the next keyframe (PTS=DTS=20), so no warning should be emitted on that
  // track buffer exhaustion even though the last frame read out of track buffer
  // before exhaustion was position 17 (PTS 16).
  CheckExpectedBuffers(20, 24, &kDataB);
  CheckNoNextBuffer();
}

// Test GC preserves data starting at first GOP containing playback position.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveDataAtPlaybackPosition) {
  // Set memory limit to 30 buffers = 1 second of data.
  SetMemoryLimit(30);
  // And append 300 buffers = 10 seconds of data.
  NewCodedFrameGroupAppend(0, 300, &kDataA);
  CheckExpectedRanges("{ [0,299) }");

  // Playback position at 0, all data must be preserved.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(0), 0));
  CheckExpectedRanges("{ [0,299) }");

  // Playback position at 1 sec, the first second of data [0,29) should be
  // collected, since we are way over memory limit.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(1000), 0));
  CheckExpectedRanges("{ [30,299) }");

  // Playback position at 1.1 sec, no new data can be collected, since the
  // playback position is still in the first GOP of buffered data.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(1100), 0));
  CheckExpectedRanges("{ [30,299) }");

  // Playback position at 5.166 sec, just at the very end of GOP corresponding
  // to buffer range 150-155, which should be preserved.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(5166), 0));
  CheckExpectedRanges("{ [150,299) }");

  // Playback position at 5.167 sec, just past the end of GOP corresponding to
  // buffer range 150-155, it should be garbage collected now.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(5167), 0));
  CheckExpectedRanges("{ [155,299) }");

  // Playback at 9.0 sec, we can now successfully collect all data except the
  // last second and we are back under memory limit of 30 buffers, so GCIfNeeded
  // should return true.
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(9000), 0));
  CheckExpectedRanges("{ [270,299) }");

  // Playback at 9.999 sec, GC succeeds, since we are under memory limit even
  // without removing any data.
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(9999), 0));
  CheckExpectedRanges("{ [270,299) }");

  // Playback at 15 sec, this should never happen during regular playback in
  // browser, since this position has no data buffered, but it should still
  // cause no problems to GC algorithm, so test it just in case.
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(15000), 0));
  CheckExpectedRanges("{ [270,299) }");
}

// Test saving the last GOP appended when this GOP is the only GOP in its range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP) {
  // Set memory limit to 3 and make sure the 4-byte GOP is not garbage
  // collected.
  SetMemoryLimit(3);
  NewCodedFrameGroupAppend("0K 30 60 90");
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Make sure you can continue appending data to this GOP; again, GC should not
  // wipe out anything.
  AppendBuffers("120D30");
  EXPECT_FALSE(GarbageCollectWithPlaybackAtBuffer(0, 0));
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Append a 2nd range after this without triggering GC.
  NewCodedFrameGroupAppend("200K 230 260 290K 320 350");
  CheckExpectedRangesByTimestamp("{ [0,150) [200,380) }");

  // Seek to 290ms.
  SeekToTimestampMs(290);

  // Now append a GOP in a separate range after the selected range and trigger
  // GC. Because it is after 290ms, this tests that the GOP is saved when
  // deleting from the back.
  NewCodedFrameGroupAppend("500K 530 560 590");
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(290), 0));

  // Should save GOPs between 290ms and the last GOP appended.
  CheckExpectedRangesByTimestamp("{ [290,380) [500,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620D30");
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(290), 0));
  CheckExpectedRangesByTimestamp("{ [290,380) [500,650) }");
}

// Test saving the last GOP appended when this GOP is in the middle of a
// non-selected range.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Middle) {
  // Append 3 GOPs starting at 0ms, 30ms apart.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Now set the memory limit to 1 and overlap the middle of the range with a
  // new GOP.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("80K 110 140");

  // This whole GOP should be saved after GC, which will fail due to GOP being
  // larger than 1 buffer
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(80), 0));
  CheckExpectedRangesByTimestamp("{ [80,170) }");
  // We should still be able to continue appending data to GOP
  AppendBuffers("170D30");
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(80), 0));
  CheckExpectedRangesByTimestamp("{ [80,200) }");

  // Append a 2nd range after this range, without triggering GC.
  NewCodedFrameGroupAppend("400K 430 460 490K 520 550 580K 610 640");
  CheckExpectedRangesByTimestamp("{ [80,200) [400,670) }");

  // Seek to 80ms to make the first range the selected range.
  SeekToTimestampMs(80);

  // Now append a GOP in the middle of the second range and trigger GC. Because
  // it is after the selected range, this tests that the GOP is saved when
  // deleting from the back.
  NewCodedFrameGroupAppend("500K 530 560 590");
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(80), 0));

  // Should save the GOPs between the seek point and GOP that was last appended
  CheckExpectedRangesByTimestamp("{ [80,200) [400,620) }");

  // Continue appending to this GOP after GC.
  AppendBuffers("620D30");
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(80), 0));
  CheckExpectedRangesByTimestamp("{ [80,200) [400,650) }");
}

// Test saving the last GOP appended when the GOP containing the next buffer is
// adjacent to the last GOP appended.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected1) {
  // Append 3 GOPs at 0ms, 90ms, and 180ms.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Seek to the GOP at 90ms.
  SeekToTimestampMs(90);

  // Set the memory limit to 1, then overlap the GOP at 0.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("0K 30 60");

  // GC should save the GOP at 0ms and 90ms, and will fail since GOP larger
  // than 1 buffer
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(90), 0));
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Seek to 0 and check all buffers.
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 30 60 90K 120 150");
  CheckNoNextBuffer();

  // Now seek back to 90ms and append a GOP at 180ms.
  SeekToTimestampMs(90);
  NewCodedFrameGroupAppend("180K 210 240");

  // Should save the GOP at 90ms and the GOP at 180ms.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(90), 0));
  CheckExpectedRangesByTimestamp("{ [90,270) }");
  CheckExpectedBuffers("90K 120 150 180K 210 240");
  CheckNoNextBuffer();
}

// Test saving the last GOP appended when it is at the beginning or end of the
// selected range. This tests when the last GOP appended is before or after the
// GOP containing the next buffer, but not directly adjacent to this GOP.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected2) {
  // Append 4 GOPs starting at positions 0ms, 90ms, 180ms, 270ms.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Seek to the last GOP at 270ms.
  SeekToTimestampMs(270);

  // Set the memory limit to 1, then overlap the GOP at 90ms.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("90K 120 150");

  // GC will save data in the range where the most recent append has happened
  // [0; 180) and the range where the next read position is [270;360)
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(270), 0));
  CheckExpectedRangesByTimestamp("{ [0,180) [270,360) }");

  // Add 3 GOPs to the end of the selected range at 360ms, 450ms, and 540ms.
  NewCodedFrameGroupAppend("360K 390 420 450K 480 510 540K 570 600");
  CheckExpectedRangesByTimestamp("{ [0,180) [270,630) }");

  // Overlap the GOP at 450ms and garbage collect to test deleting from the
  // back.
  NewCodedFrameGroupAppend("450K 480 510");
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(270), 0));

  // Should save GOPs from GOP at 270ms to GOP at 450ms.
  CheckExpectedRangesByTimestamp("{ [270,540) }");
}

// Test saving the last GOP appended when it is the same as the GOP containing
// the next buffer.
TEST_F(SourceBufferStreamTest, GarbageCollection_SaveAppendGOP_Selected3) {
  // Seek to start of stream.
  SeekToTimestampMs(0);

  // Append 3 GOPs starting at 0ms, 90ms, 180ms.
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240");
  CheckExpectedRangesByTimestamp("{ [0,270) }");

  // Set the memory limit to 1 then begin appending the start of a GOP starting
  // at 0ms.
  SetMemoryLimit(1);
  NewCodedFrameGroupAppend("0K 30");

  // GC should save the newly appended GOP, which is also the next GOP that
  // will be returned from the seek request.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(0), 0));
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  // Check the buffers in the range.
  CheckExpectedBuffers("0K 30");
  CheckNoNextBuffer();

  // Continue appending to this buffer.
  AppendBuffers("60 90");

  // GC should still save the rest of this GOP and should be able to fulfill
  // the read.
  EXPECT_FALSE(GarbageCollect(base::Milliseconds(0), 0));
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("60 90");
  CheckNoNextBuffer();
}

// Test the performance of garbage collection.
TEST_F(SourceBufferStreamTest, GarbageCollection_Performance) {
  // Force |keyframes_per_second_| to be equal to kDefaultFramesPerSecond.
  SetStreamInfo(kDefaultFramesPerSecond, kDefaultFramesPerSecond);

  const int kBuffersToKeep = 1000;
  SetMemoryLimit(kBuffersToKeep);

  int buffers_appended = 0;

  NewCodedFrameGroupAppend(0, kBuffersToKeep);
  buffers_appended += kBuffersToKeep;

  const int kBuffersToAppend = 1000;
  const int kGarbageCollections = 3;
  for (int i = 0; i < kGarbageCollections; ++i) {
    AppendBuffers(buffers_appended, kBuffersToAppend);
    buffers_appended += kBuffersToAppend;
  }
}

TEST_F(SourceBufferStreamTest, GarbageCollection_MediaTimeAfterLastAppendTime) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 12 buffers. The duration of the last buffer is 30
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180 210K 240 270 300K 330D30");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Do a garbage collection with the media time higher than the timestamp of
  // the last appended buffer (330), but still within buffered ranges, taking
  // into account the duration of the last frame (timestamp of the last frame is
  // 330, duration is 30, so the latest valid buffered position is 330+30=360).
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(360), 0));

  // GC should collect one GOP from the front to bring us back under memory
  // limit of 10 buffers.
  CheckExpectedRangesByTimestamp("{ [120,360) }");
}

TEST_F(SourceBufferStreamTest,
       GarbageCollection_MediaTimeOutsideOfStreamBufferedRange) {
  // Set memory limit to 10 buffers.
  SetMemoryLimit(10);

  // Append 12 buffers.
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180 210K 240 270 300K 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Seek in order to set the stream read position to 330 an ensure that the
  // stream selects the buffered range.
  SeekToTimestampMs(330);

  // Do a garbage collection with the media time outside the buffered ranges
  // (this might happen when there's both audio and video streams, audio stream
  // buffered range is longer than the video stream buffered range, since
  // media::Pipeline uses audio stream as a time source in that case, it might
  // return a media_time that is slightly outside of video buffered range). In
  // those cases the GC algorithm should clamp the media_time value to the
  // buffered ranges to work correctly (see crbug.com/563292).
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(361), 0));

  // GC should collect one GOP from the front to bring us back under memory
  // limit of 10 buffers.
  CheckExpectedRangesByTimestamp("{ [120,360) }");
}

TEST_F(SourceBufferStreamTest, GetRemovalRange_BytesToFree) {
  // Append 2 GOPs starting at 300ms, 30ms apart.
  NewCodedFrameGroupAppend("300K 330 360 390K 420 450");

  // Append 2 GOPs starting at 600ms, 30ms apart.
  NewCodedFrameGroupAppend("600K 630 660 690K 720 750");

  // Append 2 GOPs starting at 900ms, 30ms apart.
  NewCodedFrameGroupAppend("900K 930 960 990K 1020 1050");

  CheckExpectedRangesByTimestamp("{ [300,480) [600,780) [900,1080) }");

  int remove_range_end = -1;
  int bytes_removed = -1;

  // Size 0.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 0, &remove_range_end);
  EXPECT_EQ(-1, remove_range_end);
  EXPECT_EQ(0, bytes_removed);

  // Smaller than the size of GOP.
  bytes_removed = GetRemovalRangeInMs(300, 1080, GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(390, remove_range_end);
  // Remove as the size of GOP.
  EXPECT_EQ(3 * GetMemoryUsagePerBuffer(), bytes_removed);

  // The same size with a GOP.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 3 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(390, remove_range_end);
  EXPECT_EQ(3 * GetMemoryUsagePerBuffer(), bytes_removed);

  // The same size with a range.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 6 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(480, remove_range_end);
  EXPECT_EQ(6 * GetMemoryUsagePerBuffer(), bytes_removed);

  // A frame larger than a range.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 7 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(690, remove_range_end);
  EXPECT_EQ(9 * GetMemoryUsagePerBuffer(), bytes_removed);

  // The same size with two ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 12 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(12 * GetMemoryUsagePerBuffer(), bytes_removed);

  // Larger than two ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 14 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(990, remove_range_end);
  EXPECT_EQ(15 * GetMemoryUsagePerBuffer(), bytes_removed);

  // The same size with the whole ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 18 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18 * GetMemoryUsagePerBuffer(), bytes_removed);

  // Larger than the whole ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18 * GetMemoryUsagePerBuffer(), bytes_removed);
}

TEST_F(SourceBufferStreamTest, GetRemovalRange_Range) {
  // Append 2 GOPs starting at 300ms, 30ms apart.
  NewCodedFrameGroupAppend("300K 330 360 390K 420 450");

  // Append 2 GOPs starting at 600ms, 30ms apart.
  NewCodedFrameGroupAppend("600K 630 660 690K 720 750");

  // Append 2 GOPs starting at 900ms, 30ms apart.
  NewCodedFrameGroupAppend("900K 930 960 990K 1020 1050");

  CheckExpectedRangesByTimestamp("{ [300,480) [600,780) [900,1080) }");

  int remove_range_end = -1;
  int bytes_removed = -1;

  // Within a GOP and no keyframe.
  bytes_removed = GetRemovalRangeInMs(630, 660, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(-1, remove_range_end);
  EXPECT_EQ(0, bytes_removed);

  // Across a GOP and no keyframe.
  bytes_removed = GetRemovalRangeInMs(630, 750, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(-1, remove_range_end);
  EXPECT_EQ(0, bytes_removed);

  // The same size with a range.
  bytes_removed = GetRemovalRangeInMs(600, 780, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6 * GetMemoryUsagePerBuffer(), bytes_removed);

  // One frame larger than a range.
  bytes_removed = GetRemovalRangeInMs(570, 810, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6 * GetMemoryUsagePerBuffer(), bytes_removed);

  // Facing the other ranges.
  bytes_removed = GetRemovalRangeInMs(480, 900, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6 * GetMemoryUsagePerBuffer(), bytes_removed);

  // In the midle of the other ranges, but not including any GOP.
  bytes_removed = GetRemovalRangeInMs(420, 960, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(780, remove_range_end);
  EXPECT_EQ(6 * GetMemoryUsagePerBuffer(), bytes_removed);

  // In the middle of the other ranges.
  bytes_removed = GetRemovalRangeInMs(390, 990, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(990, remove_range_end);
  EXPECT_EQ(12 * GetMemoryUsagePerBuffer(), bytes_removed);

  // A frame smaller than the whole ranges.
  bytes_removed = GetRemovalRangeInMs(330, 1050, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(990, remove_range_end);
  EXPECT_EQ(12 * GetMemoryUsagePerBuffer(), bytes_removed);

  // The same with the whole ranges.
  bytes_removed = GetRemovalRangeInMs(300, 1080, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18 * GetMemoryUsagePerBuffer(), bytes_removed);

  // Larger than the whole ranges.
  bytes_removed = GetRemovalRangeInMs(270, 1110, 20 * GetMemoryUsagePerBuffer(),
                                      &remove_range_end);
  EXPECT_EQ(1080, remove_range_end);
  EXPECT_EQ(18 * GetMemoryUsagePerBuffer(), bytes_removed);
}

TEST_F(SourceBufferStreamTest, IsNextBufferConfigChanged) {
  // selected_range_ is nullptr, so return false
  EXPECT_FALSE(stream_->IsNextBufferConfigChanged());
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  ASSERT_FALSE(new_config.Matches(video_config_));

  // read all buffers
  NewCodedFrameGroupAppend("0K 10 20");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,30) }");
  CheckExpectedBuffers("0K 10 20");
  EXPECT_FALSE(stream_->IsNextBufferConfigChanged());

  // Signal a config change.
  stream_->UpdateVideoConfig(new_config, false);
  NewCodedFrameGroupAppend("30K 40");
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());

  scoped_refptr<StreamParserBuffer> buffer;
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));
  CheckVideoConfig(new_config);

  // Overlap-append
  NewCodedFrameGroupAppend(
      "21K 41 51 61 71 81 91 101 111 121 "
      "131K 141");
  CheckExpectedRangesByTimestamp("{ [0,151) }");

  // track_buffer has the buffers with timestamp 30 and 40
  EXPECT_FALSE(stream_->IsNextBufferConfigChanged());
}

TEST_F(SourceBufferStreamTest, ConfigChange_Basic) {
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  ASSERT_FALSE(new_config.Matches(video_config_));
  Seek(0);
  CheckVideoConfig(video_config_);

  // Append 5 buffers at positions 0 through 4
  NewCodedFrameGroupAppend(0, 5, &kDataA);

  CheckVideoConfig(video_config_);

  // Signal a config change.
  stream_->UpdateVideoConfig(new_config, false);

  // Make sure updating the config doesn't change anything since new_config
  // should not be associated with the buffer GetNextBuffer() will return.
  CheckVideoConfig(video_config_);

  // Append 5 buffers at positions 5 through 9.
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Consume the buffers associated with the initial config.
  scoped_refptr<StreamParserBuffer> buffer;
  for (int i = 0; i < 5; i++) {
    EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));
    CheckVideoConfig(video_config_);
  }

  // Verify the next attempt to get a buffer will signal that a config change
  // has happened.
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));

  // Verify that the new config is now returned.
  CheckVideoConfig(new_config);

  // Consume the remaining buffers associated with the new config.
  for (int i = 0; i < 5; i++) {
    CheckVideoConfig(new_config);
    EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));
  }
}

TEST_F(SourceBufferStreamTest, ConfigChange_Seek) {
  scoped_refptr<StreamParserBuffer> buffer;
  VideoDecoderConfig new_config = TestVideoConfig::Large();

  Seek(0);
  NewCodedFrameGroupAppend(0, 5, &kDataA);
  stream_->UpdateVideoConfig(new_config, false);
  NewCodedFrameGroupAppend(5, 5, &kDataB);

  // Seek to the start of the buffers with the new config and make sure a
  // config change is signalled.
  CheckVideoConfig(video_config_);
  Seek(5);
  CheckVideoConfig(video_config_);
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));
  CheckVideoConfig(new_config);
  CheckExpectedBuffers(5, 9, &kDataB);


  // Seek to the start which has a different config. Don't fetch any buffers and
  // seek back to buffers with the current config. Make sure a config change
  // isn't signalled in this case.
  CheckVideoConfig(new_config);
  Seek(0);
  Seek(7);
  CheckExpectedBuffers(5, 9, &kDataB);


  // Seek to the start and make sure a config change is signalled.
  CheckVideoConfig(new_config);
  Seek(0);
  CheckVideoConfig(new_config);
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers(0, 4, &kDataA);
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration) {
  // Append 3 discontinuous partial GOPs.
  NewCodedFrameGroupAppend("50K 90|60");
  NewCodedFrameGroupAppend("150K 190|160");
  NewCodedFrameGroupAppend("250K 290|260");

  CheckExpectedRangesByTimestamp("{ [50,100) [150,200) [250,300) }");

  // Set duration to be 80ms. Truncates the buffered data after 80ms.
  stream_->OnSetDuration(base::Milliseconds(80));

  // The simulated P-frame at PTS 90ms should have been
  // removed by the duration truncation. Only the frame at PTS 50ms should
  // remain.
  CheckExpectedRangesByTimestamp("{ [50,60) }");

  // Adding data past the previous duration should still work.
  NewCodedFrameGroupAppend("0D50K 50 100K");
  CheckExpectedRangesByTimestamp("{ [0,150) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_EdgeCase) {
  // Append 10 buffers at positions 10 through 19.
  NewCodedFrameGroupAppend(10, 10);

  // Append 5 buffers at positions 25 through 29.
  NewCodedFrameGroupAppend(25, 5);

  // Check expected ranges.
  CheckExpectedRanges("{ [10,19) [25,29) }");

  // Set duration to be right before buffer 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the last range.
  CheckExpectedRanges("{ [10,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_EdgeCase2) {
  // This test requires specific relative proportions for fudge room, append
  // size, and duration truncation amounts. See details at:
  // https://codereview.chromium.org/2385423002

  // Append buffers with first buffer establishing max_inter_buffer_distance
  // of 5 ms. This translates to a fudge room (2 x max_interbuffer_distance) of
  // 10 ms.
  NewCodedFrameGroupAppend("0K 5K 9D4K");
  CheckExpectedRangesByTimestamp("{ [0,13) }");

  // Trim off last 2 buffers, totaling 8 ms. Notably less than the current fudge
  // room of 10 ms.
  stream_->OnSetDuration(base::Milliseconds(5));

  // Verify truncation.
  CheckExpectedRangesByTimestamp("{ [0,5) }");

  // Append new buffers just beyond the fudge-room allowance of 10ms.
  AppendBuffers("11K 15K");

  // Verify new append creates a gap.
  CheckExpectedRangesByTimestamp("{ [0,5) [11,19) }");
}

TEST_F(SourceBufferStreamTest, RemoveWithinFudgeRoom) {
  // This test requires specific relative proportions for fudge room, append
  // size, and removal amounts. See details at:
  // https://codereview.chromium.org/2385423002

  // Append buffers with first buffer establishing max_inter_buffer_distance
  // of 5 ms. This translates to a fudge room (2 x max_interbuffer_distance) of
  // 10 ms.
  NewCodedFrameGroupAppend("0K 5K 9D4K");
  CheckExpectedRangesByTimestamp("{ [0,13) }");

  // Trim off last 2 buffers, totaling 8 ms. Notably less than the current fudge
  // room of 10 ms.
  RemoveInMs(5, 13, 13);

  // Verify removal.
  CheckExpectedRangesByTimestamp("{ [0,5) }");

  // Append new buffers just beyond the fudge-room allowance of 10ms.
  AppendBuffers("11K 15K");

  // Verify new append creates a gap.
  CheckExpectedRangesByTimestamp("{ [0,5) [11,19) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialRange) {
  // Append IPBBB GOPs into 3 discontinuous ranges.
  NewCodedFrameGroupAppend("0K 40|10 10|20 20|30 30|40");
  NewCodedFrameGroupAppend(
      "100K 140|110 110|120 120|130 130|140 "
      "150K 190|160 160|170 170|180 180|190");
  NewCodedFrameGroupAppend("250K 290|260 260|270 270|280 280|290");

  // Check expected ranges.
  CheckExpectedRangesByTimestamp("{ [0,50) [100,200) [250,300) }");

  stream_->OnSetDuration(base::Milliseconds(140));

  // The B-frames at PTS 110-130 were in the GOP in decode order after
  // the simulated P-frame at PTS 140 which was truncated, so those B-frames
  // are also removed.
  CheckExpectedRangesByTimestamp("{ [0,50) [100,110) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeleteSelectedRange) {
  // Append 3 discontinuous partial GOPs.
  NewCodedFrameGroupAppend("50K 90|60");
  NewCodedFrameGroupAppend("150K 190|160");
  NewCodedFrameGroupAppend("250K 290|260");

  CheckExpectedRangesByTimestamp("{ [50,100) [150,200) [250,300) }");

  SeekToTimestampMs(150);

  // Set duration to 50ms.
  stream_->OnSetDuration(base::Milliseconds(50));

  // Expect everything to be deleted, and should not have next buffer anymore.
  CheckNoNextBuffer();
  CheckExpectedRangesByTimestamp("{ }");

  // Appending data 0ms through 250ms should not fulfill the seek.
  // (If the duration is set to be something smaller than the current seek
  // point, which had been 150ms, then the seek point is reset and the
  // SourceBufferStream waits for a new seek request. Therefore even if the data
  // is re-appended, it should not fulfill the old seek.)
  NewCodedFrameGroupAppend("0K 50K 100K 150K 200K");
  CheckNoNextBuffer();
  CheckExpectedRangesByTimestamp("{ [0,250) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_DeletePartialSelectedRange) {
  // Append 5 buffers at positions 0 through 4.
  NewCodedFrameGroupAppend(0, 5);

  // Append 20 buffers at positions 10 through 29.
  NewCodedFrameGroupAppend(10, 20);

  // Check expected ranges.
  CheckExpectedRanges("{ [0,4) [10,29) }");

  // Seek to position 10.
  Seek(10);

  // Set duration to be between buffers 24 and 25.
  stream_->OnSetDuration(frame_duration() * 25);

  // Should truncate the data after 24.
  CheckExpectedRanges("{ [0,4) [10,24) }");

  // The seek position should not be lost.
  CheckExpectedBuffers(10, 10);

  // Now set the duration immediately after buffer 10.
  stream_->OnSetDuration(frame_duration() * 11);

  // Seek position should be reset.
  CheckNoNextBuffer();
  CheckExpectedRanges("{ [0,4) [10,10) }");
}

// Test the case where duration is set while the stream parser buffers
// already start passing the data to decoding pipeline. Selected range,
// when invalidated by getting truncated, should be updated to NULL
// accordingly so that successive append operations keep working.
TEST_F(SourceBufferStreamTest, SetExplicitDuration_UpdateSelectedRange) {
  // Seek to start of stream.
  SeekToTimestampMs(0);

  NewCodedFrameGroupAppend("0K 30 60 90");

  // Read out the first few buffers.
  CheckExpectedBuffers("0K 30");

  // Set duration to be right before buffer 1.
  stream_->OnSetDuration(base::Milliseconds(60));

  // Verify that there is no next buffer.
  CheckNoNextBuffer();

  // We should be able to append new buffers at this point.
  NewCodedFrameGroupAppend("120K 150");

  CheckExpectedRangesByTimestamp("{ [0,60) [120,180) }");
}

TEST_F(SourceBufferStreamTest,
       SetExplicitDuration_AfterGroupTimestampAndBeforeFirstBufferTimestamp) {
  NewCodedFrameGroupAppend("0K 30K 60K");

  // Append a coded frame group with a start timestamp of 200, but the first
  // buffer starts at 230ms. This can happen in muxed content where the
  // audio starts before the first frame.
  NewCodedFrameGroupAppend(base::Milliseconds(200), "230K 260K 290K 320K");

  NewCodedFrameGroupAppend("400K 430K 460K");

  CheckExpectedRangesByTimestamp("{ [0,90) [200,350) [400,490) }");

  stream_->OnSetDuration(base::Milliseconds(120));

  // Verify that the buffered ranges are updated properly and we don't crash.
  CheckExpectedRangesByTimestamp("{ [0,90) }");
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_MarkEOS) {
  // Append 1 full and 1 partial GOP: IPBBBIPBB
  NewCodedFrameGroupAppend(
      "0K 40|10 10|20 20|30 30|40 "
      "50K 90|60 60|70 70|80");

  CheckExpectedRangesByTimestamp("{ [0,100) }");

  SeekToTimestampMs(50);

  // Set duration to be before the seeked to position.
  // This will result in truncation of the selected range and a reset
  // of NextBufferPosition.
  stream_->OnSetDuration(base::Milliseconds(40));

  // The P-frame at PTS 40ms was removed, so its dependent B-frames at PTS 10-30
  // were also removed.
  CheckExpectedRangesByTimestamp("{ [0,10) }");

  // Mark EOS reached.
  stream_->MarkEndOfStream();

  // Expect EOS to be reached.
  CheckEOSReached();
}

TEST_F(SourceBufferStreamTest, SetExplicitDuration_MarkEOS_IsSeekPending) {
  // Append 1 full and 1 partial GOP: IPBBBIPBB
  NewCodedFrameGroupAppend(
      "0K 40|10 10|20 20|30 30|40 "
      "50K 90|60 60|70 70|80");

  CheckExpectedRangesByTimestamp("{ [0,100) }");

  // Seek to 100ms will result in a pending seek.
  SeekToTimestampMs(100);

  // Set duration to be before the seeked to position.
  // This will result in truncation of the selected range and a reset
  // of NextBufferPosition.
  stream_->OnSetDuration(base::Milliseconds(40));

  // The P-frame at PTS 40ms was removed, so its dependent B-frames at PTS 10-30
  // were also removed.
  CheckExpectedRangesByTimestamp("{ [0,10) }");

  EXPECT_TRUE(stream_->IsSeekPending());
  // Mark EOS reached.
  stream_->MarkEndOfStream();
  EXPECT_FALSE(stream_->IsSeekPending());
}

// Test the case were the current playback position is at the end of the
// buffered data and several overlaps occur.
TEST_F(SourceBufferStreamTest, OverlapWhileWaitingForMoreData) {
  // Seek to start of stream.
  SeekToTimestampMs(0);

  NewCodedFrameGroupAppend("0K 30 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Read all the buffered data.
  CheckExpectedBuffers("0K 30 60 90 120K 150");
  CheckNoNextBuffer();

  // Append data over the current GOP so that a keyframe is needed before
  // playback can continue from the current position.
  NewCodedFrameGroupAppend("120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Append buffers that replace the first GOP with a partial GOP.
  NewCodedFrameGroupAppend("0K 30");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Append buffers that complete that partial GOP.
  AppendBuffers("60 90");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Verify that we still don't have a next buffer.
  CheckNoNextBuffer();

  // Add more data to the end and verify that this new data is read correctly.
  NewCodedFrameGroupAppend("180K 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");
  CheckExpectedBuffers("180K 210");
  CheckNoNextBuffer();
}

// Verify that a single coded frame at the current read position unblocks the
// read even if the frame is buffered after the previously read position is
// removed.
TEST_F(SourceBufferStreamTest, AfterRemove_SingleFrameRange_Unblocks_Read) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("120D30K");
  CheckExpectedRangesByTimestamp("{ [120,150) }");
  CheckExpectedBuffers("120K");
  CheckNoNextBuffer();
}

// Verify that multiple short (relative to max-inter-buffer-distance * 2) coded
// frames at the current read position unblock the read even if the frames are
// buffered after the previously read position is removed.
TEST_F(SourceBufferStreamTest, AfterRemove_TinyFrames_Unblock_Read_1) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("120D1K 121D1");
  CheckExpectedRangesByTimestamp("{ [120,122) }");
  CheckExpectedBuffers("120K 121");
  CheckNoNextBuffer();
}

// Verify that multiple short (relative to max-inter-buffer-distance * 2) coded
// frames starting at the fudge room boundary unblock the read even if the
// frames are buffered after the previously read position is removed.
TEST_F(SourceBufferStreamTest, AfterRemove_TinyFrames_Unblock_Read_2) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("150D1K 151D1");
  CheckExpectedRangesByTimestamp("{ [150,152) }");
  CheckExpectedBuffers("150K 151");
  CheckNoNextBuffer();
}

// Verify that coded frames starting after the fudge room boundary do not
// unblock the read when buffered after the previously read position is removed.
TEST_F(SourceBufferStreamTest, AfterRemove_BeyondFudge_Stalled) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90D30");
  CheckExpectedRangesByTimestamp("{ [0,120) }");
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();

  RemoveInMs(0, 120, 120);
  CheckExpectedRangesByTimestamp("{ }");
  NewCodedFrameGroupAppend("151D1K 152D1");
  CheckExpectedRangesByTimestamp("{ [151,153) }");
  CheckNoNextBuffer();
}

// Verify that non-keyframes with the same timestamp in the same
// append are handled correctly.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_SingleAppend) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 30 60 90 120K 150");
  CheckExpectedBuffers("0K 30 30 60 90 120K 150");
}

// Verify that a non-keyframe followed by a keyframe with the same timestamp
// is allowed.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_SingleAppend2) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 30K 60");
  CheckExpectedBuffers("0K 30 30K 60");
}

// Verify that non-keyframes with the same timestamp can occur
// in different appends.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_TwoAppends) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30D0");
  AppendBuffers("30 60 90 120K 150");
  CheckExpectedBuffers("0K 30 30 60 90 120K 150");
}

// Verify that a non-keyframe followed by a keyframe with the same timestamp
// is allowed.
TEST_F(SourceBufferStreamTest, SameTimestamp_Video_TwoAppends2) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30D0");
  AppendBuffers("30K 60");
  CheckExpectedBuffers("0K 30 30K 60");
}

// Verify that a keyframe followed by a non-keyframe with the same timestamp
// is allowed.
TEST_F(SourceBufferStreamTest, SameTimestamp_VideoKeyFrame_TwoAppends) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30D0K");
  AppendBuffers("30 60");
  CheckExpectedBuffers("0K 30K 30 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_VideoKeyFrame_SingleAppend) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30K 30 60");
  CheckExpectedBuffers("0K 30K 30 60");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_1) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 60 90 120K 150");

  NewCodedFrameGroupAppend("60K 91 121K 151");
  CheckExpectedBuffers("0K 30 60K 91 121K 151");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_2) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 60 90 120K 150");
  NewCodedFrameGroupAppend("0K 30 61");
  CheckExpectedBuffers("0K 30 61 120K 150");
}

TEST_F(SourceBufferStreamTest, SameTimestamp_Video_Overlap_3) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 20 40 60 80 100K 101 102 103K");
  NewCodedFrameGroupAppend("0K 20 40 60 80 90D0");
  CheckExpectedBuffers("0K 20 40 60 80 90 100K 101 102 103K");
  AppendBuffers("90 110K 150");
  Seek(0);
  CheckExpectedBuffers("0K 20 40 60 80 90 90 110K 150");
  CheckNoNextBuffer();
  CheckExpectedRangesByTimestamp("{ [0,190) }");
}

// Test all the valid same timestamp cases for audio.
TEST_F(SourceBufferStreamTest, SameTimestamp_Audio) {
  AudioDecoderConfig config(AudioCodec::kMP3, kSampleFormatF32,
                            CHANNEL_LAYOUT_STEREO, 44100, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
  ResetStream<>(config);
  Seek(0);
  NewCodedFrameGroupAppend("0K 0K 30K 30K");
  CheckExpectedBuffers("0K 0K 30K 30K");
}

// If seeking past any existing range and the seek is pending
// because no data has been provided for that position,
// the stream position can be considered as the end of stream.
TEST_F(SourceBufferStreamTest, EndSelected_During_PendingSeek) {
  // Append 15 buffers at positions 0 through 14.
  NewCodedFrameGroupAppend(0, 15);

  Seek(20);
  EXPECT_TRUE(stream_->IsSeekPending());
  stream_->MarkEndOfStream();
  EXPECT_FALSE(stream_->IsSeekPending());
}

// If there is a pending seek between 2 existing ranges,
// the end of the stream has not been reached.
TEST_F(SourceBufferStreamTest, EndNotSelected_During_PendingSeek) {
  // Append:
  // - 10 buffers at positions 0 through 9.
  // - 10 buffers at positions 30 through 39
  NewCodedFrameGroupAppend(0, 10);
  NewCodedFrameGroupAppend(30, 10);

  Seek(20);
  EXPECT_TRUE(stream_->IsSeekPending());
  stream_->MarkEndOfStream();
  EXPECT_TRUE(stream_->IsSeekPending());
}

// Removing exact start & end of a range.
TEST_F(SourceBufferStreamTest, Remove_WholeRange1) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(10, 160, 160);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts before range and ends exactly at end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange2) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(0, 160, 160);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts at the start of a range and ends beyond the
// range end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange3) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(10, 200, 200);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts before range start and ends after the range end.
TEST_F(SourceBufferStreamTest, Remove_WholeRange4) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  CheckExpectedRangesByTimestamp("{ [10,160) }");
  RemoveInMs(0, 200, 200);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removes multiple ranges.
TEST_F(SourceBufferStreamTest, Remove_WholeRange5) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  NewCodedFrameGroupAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  RemoveInMs(10, 3000, 3000);
  CheckExpectedRangesByTimestamp("{ }");
}

// Verifies a [0-infinity) range removes everything.
TEST_F(SourceBufferStreamTest, Remove_ZeroToInfinity) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  NewCodedFrameGroupAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  Remove(base::TimeDelta(), kInfiniteDuration, kInfiniteDuration);
  CheckExpectedRangesByTimestamp("{ }");
}

// Removal range starts at the beginning of the range and ends in the
// middle of the range. This test verifies that full GOPs are removed.
TEST_F(SourceBufferStreamTest, Remove_Partial1) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(0, 80, 2200);
  CheckExpectedRangesByTimestamp("{ [130,160) [1000,1150) }");
}

// Removal range starts in the middle of a range and ends at the exact
// end of the range.
TEST_F(SourceBufferStreamTest, Remove_Partial2) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(40, 160, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [1000,1150) }");
}

// Removal range starts and ends within a range.
TEST_F(SourceBufferStreamTest, Remove_Partial3) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) }");
  RemoveInMs(40, 120, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [130,160) [1000,1150) }");
}

// Removal range starts in the middle of one range and ends in the
// middle of another range.
TEST_F(SourceBufferStreamTest, Remove_Partial4) {
  Seek(0);
  NewCodedFrameGroupAppend("10K 40 70K 100 130K");
  NewCodedFrameGroupAppend("1000K 1030 1060K 1090 1120K");
  NewCodedFrameGroupAppend("2000K 2030 2060K 2090 2120K");
  CheckExpectedRangesByTimestamp("{ [10,160) [1000,1150) [2000,2150) }");
  RemoveInMs(40, 2030, 2200);
  CheckExpectedRangesByTimestamp("{ [10,40) [2060,2150) }");
}

// Test behavior when the current position is removed and new buffers
// are appended over the removal range.
TEST_F(SourceBufferStreamTest, Remove_CurrentPosition) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");
  CheckExpectedBuffers("0K 30 60 90K 120");

  // Remove a range that includes the next buffer (i.e., 150).
  RemoveInMs(150, 210, 360);
  CheckExpectedRangesByTimestamp("{ [0,150) [270,360) }");

  // Verify that no next buffer is returned.
  CheckNoNextBuffer();

  // Append some buffers to fill the gap that was created.
  NewCodedFrameGroupAppend("120K 150 180 210K 240");
  CheckExpectedRangesByTimestamp("{ [0,360) }");

  // Verify that buffers resume at the next keyframe after the
  // current position.
  CheckExpectedBuffers("210K 240 270K 300 330");
}

// Test behavior when buffers in the selected range before the current position
// are removed.
TEST_F(SourceBufferStreamTest, Remove_BeforeCurrentPosition) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90K 120 150 180K 210 240 270K 300 330");
  CheckExpectedRangesByTimestamp("{ [0,360) }");
  CheckExpectedBuffers("0K 30 60 90K 120");

  // Remove a range that is before the current playback position.
  RemoveInMs(0, 90, 360);
  CheckExpectedRangesByTimestamp("{ [90,360) }");

  CheckExpectedBuffers("150 180K 210 240 270K 300 330");
}

// Test removing the preliminary portion for the current coded frame group being
// appended.
TEST_F(SourceBufferStreamTest, Remove_MidGroup) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180 210");
  CheckExpectedRangesByTimestamp("{ [0,240) }");

  // Partially replace the first GOP, then read its keyframe.
  NewCodedFrameGroupAppend("0K 30");
  CheckExpectedBuffers("0K");

  CheckExpectedRangesByTimestamp("{ [0,240) }");

  // Remove the partial GOP that we're in the middle of reading.
  RemoveInMs(0, 60, 240);

  // Verify that there is no next buffer since it was removed and the remaining
  // buffered range is beyond the current position.
  CheckNoNextBuffer();
  CheckExpectedRangesByTimestamp("{ [120,240) }");

  // Continue appending frames for the current GOP.
  AppendBuffers("60 90");

  // Verify that the non-keyframes are not added.
  CheckExpectedRangesByTimestamp("{ [120,240) }");

  // Finish the previous GOP and start the next one.
  AppendBuffers("120 150K 180");

  // Verify that new GOP replaces the existing GOP.
  CheckExpectedRangesByTimestamp("{ [150,210) }");
  SeekToTimestampMs(150);
  CheckExpectedBuffers("150K 180");
  CheckNoNextBuffer();
}

// Test removing the current GOP being appended, while not removing
// the entire range the GOP belongs to.
TEST_F(SourceBufferStreamTest, Remove_GOPBeingAppended) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 30 60 90 120K 150 180");
  CheckExpectedRangesByTimestamp("{ [0,210) }");

  // Remove the current GOP being appended.
  RemoveInMs(120, 150, 240);
  CheckExpectedRangesByTimestamp("{ [0,120) }");

  // Continue appending the current GOP and the next one.
  AppendBuffers("210 240K 270 300");

  // Verify that the non-keyframe in the previous GOP does
  // not effect any existing ranges and a new range is started at the
  // beginning of the next GOP.
  CheckExpectedRangesByTimestamp("{ [0,120) [240,330) }");

  // Verify the buffers in the ranges.
  CheckExpectedBuffers("0K 30 60 90");
  CheckNoNextBuffer();
  SeekToTimestampMs(240);
  CheckExpectedBuffers("240K 270 300");
}

TEST_F(SourceBufferStreamTest, Remove_WholeGOPBeingAppended) {
  SeekToTimestampMs(1000);
  NewCodedFrameGroupAppend("1000K 1030 1060 1090");
  CheckExpectedRangesByTimestamp("{ [1000,1120) }");

  // Remove the keyframe of the current GOP being appended.
  RemoveInMs(1000, 1030, 1120);
  CheckExpectedRangesByTimestamp("{ }");

  // Continue appending the current GOP.
  AppendBuffers("1210 1240");

  CheckExpectedRangesByTimestamp("{ }");

  // Append the beginning of the next GOP.
  AppendBuffers("1270K 1300");

  // Verify that the new range is started at the
  // beginning of the next GOP.
  CheckExpectedRangesByTimestamp("{ [1270,1330) }");

  // Verify the buffers in the ranges.
  CheckNoNextBuffer();
  SeekToTimestampMs(1270);
  CheckExpectedBuffers("1270K 1300");
}

TEST_F(SourceBufferStreamTest,
       Remove_PreviousAppendDestroyedAndOverwriteExistingRange) {
  SeekToTimestampMs(90);

  NewCodedFrameGroupAppend("90K 120 150");
  CheckExpectedRangesByTimestamp("{ [90,180) }");

  // Append a coded frame group before the previously appended data.
  NewCodedFrameGroupAppend("0K 30 60");

  // Verify that the ranges get merged.
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Remove the data from the last append.
  RemoveInMs(0, 90, 360);
  CheckExpectedRangesByTimestamp("{ [90,180) }");

  // Append a new coded frame group that follows the removed group and
  // starts at the beginning of the range left over from the
  // remove.
  NewCodedFrameGroupAppend("90K 121 151");
  CheckExpectedBuffers("90K 121 151");
}

TEST_F(SourceBufferStreamTest, Remove_GapAtBeginningOfGroup) {
  Seek(0);

  // Append a coded frame group that has a gap at the beginning of it.
  NewCodedFrameGroupAppend(base::Milliseconds(0), "30K 60 90 120K 150");
  CheckExpectedRangesByTimestamp("{ [0,180) }");

  // Remove the gap that doesn't contain any buffers.
  RemoveInMs(0, 10, 180);
  CheckExpectedRangesByTimestamp("{ [10,180) }");

  // Verify we still get the first buffer still since only part of
  // the gap was removed.
  // TODO(acolwell/wolenetz): Consider not returning a buffer at this
  // point since the current seek position has been explicitly
  // removed but didn't happen to remove any buffers.
  // http://crbug.com/384016
  CheckExpectedBuffers("30K");

  // Remove a range that includes the first GOP.
  RemoveInMs(0, 60, 180);

  // Verify that no buffer is returned because the current buffer
  // position has been removed.
  CheckNoNextBuffer();

  CheckExpectedRangesByTimestamp("{ [120,180) }");
}

TEST_F(SourceBufferStreamTest,
       OverlappingAppendRangeMembership_OneMicrosecond_Video) {
  NewCodedFrameGroupAppend("10D20K");
  CheckExpectedRangesByTimestamp("{ [10000,30000) }",
                                 TimeGranularity::kMicrosecond);

  // Append a buffer 1 microsecond earlier, with estimated duration.
  NewCodedFrameGroupAppend("9999uD20EK");
  CheckExpectedRangesByTimestamp("{ [9999,30000) }",
                                 TimeGranularity::kMicrosecond);

  // Append that same buffer again, but without any discontinuity signalled / no
  // new coded frame group.
  AppendBuffers("9999uD20EK");
  CheckExpectedRangesByTimestamp("{ [9999,30000) }",
                                 TimeGranularity::kMicrosecond);

  Seek(0);
  CheckExpectedBuffers("9999K 9999K 10000K", TimeGranularity::kMicrosecond);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       OverlappingAppendRangeMembership_TwoMicroseconds_Video) {
  NewCodedFrameGroupAppend("10D20K");
  CheckExpectedRangesByTimestamp("{ [10000,30000) }",
                                 TimeGranularity::kMicrosecond);

  // Append an exactly abutting buffer 2us earlier.
  NewCodedFrameGroupAppend("9998uD20EK");
  CheckExpectedRangesByTimestamp("{ [9998,30000) }",
                                 TimeGranularity::kMicrosecond);

  // Append that same buffer again, but without any discontinuity signalled / no
  // new coded frame group.
  AppendBuffers("9998uD20EK");
  CheckExpectedRangesByTimestamp("{ [9998,30000) }",
                                 TimeGranularity::kMicrosecond);

  Seek(0);
  CheckExpectedBuffers("9998K 9998K 10000K", TimeGranularity::kMicrosecond);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceTrimmingForOverlap) {
  SetAudioStream();
  Seek(0);
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K 12K");
  CheckExpectedRangesByTimestamp("{ [0,14) }");
  // Note that duration  of frame at time 10 is verified to be 2 ms.
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10D2K 12K");
  CheckNoNextBuffer();

  // Append new group with front slightly overlapping existing buffer at 10ms.
  EXPECT_MEDIA_LOG(TrimmedSpliceOverlap(11000, 10000, 1000));
  NewCodedFrameGroupAppend("11K 13K 15K 17K");

  // Cross-fade splicing is no longer implemented. Instead we should expect
  // wholly overlapped buffers to be removed (12K). If a buffer is partially
  // overlapped (e.g. last millisecond of 10K), the existing buffer should be
  // trimmed to perfectly abut the newly appended buffers.
  Seek(0);

  CheckExpectedRangesByTimestamp("{ [0,19) }");
  CheckExpectedBuffers("0K 2K 4K 6K 8K 10D1K 11D2K 13K 15K 17K");
  CheckNoNextBuffer();
}

// Test that a splice is not created if an end timestamp and start timestamp
// perfectly overlap.
TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoSplice) {
  SetAudioStream();
  Seek(0);

  // Add 10 frames across 2 *non-overlapping* appends.
  NewCodedFrameGroupAppend("0K 2K 4K 6K 8K 10K");
  NewCodedFrameGroupAppend("12K 14K 16K 18K");

  // Manually inspect the buffers at the no-splice boundary to verify duration
  // and lack of discard padding (set when splicing).
  scoped_refptr<StreamParserBuffer> buffer;
  const DecoderBuffer::DiscardPadding kEmptyDiscardPadding;
  for (int i = 0; i < 10; i++) {
    // Verify buffer timestamps and durations are preserved and no buffers have
    // discard padding (indicating no splice trimming).
    EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&buffer));
    EXPECT_EQ(base::Milliseconds(i * 2), buffer->timestamp());
    EXPECT_EQ(base::Milliseconds(2), buffer->duration());
    EXPECT_EQ(kEmptyDiscardPadding, buffer->discard_padding());
  }

  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_NoSpliceForBadOverlap) {
  SetAudioStream();
  Seek(0);

  // Add 2 frames with matching PTS and ov, where the duration of the first
  // frame suggests that it overlaps the second frame. The overlap is within a
  // coded frame group (bad content), so no splicing is expected.
  NewCodedFrameGroupAppend("0D10K 0D10K");
  CheckExpectedRangesByTimestamp("{ [0,10) }");
  CheckExpectedBuffers("0D10K 0D10K");
  CheckNoNextBuffer();

  Seek(0);

  // Add a new frame in a separate coded frame group that falls into the
  // overlap of the two existing frames. Splicing should not be performed since
  // the content is poorly muxed. We can't know which frame to splice when the
  // content is already messed up.
  EXPECT_MEDIA_LOG(NoSpliceForBadMux(2, 2000));
  NewCodedFrameGroupAppend("2D10K");
  CheckExpectedRangesByTimestamp("{ [0,12) }");
  CheckExpectedBuffers("0D10K 0D10K 2D10K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_NoSpliceForEstimatedDuration) {
  SetAudioStream();
  Seek(0);

  // Append two buffers, the latter having estimated duration.
  NewCodedFrameGroupAppend("0D10K 10D10EK");
  CheckExpectedRangesByTimestamp("{ [0,20) }");
  CheckExpectedBuffers("0D10K 10D10EK");
  CheckNoNextBuffer();

  Seek(0);

  // Add a new frame in a separate coded frame group that falls in the middle of
  // the second buffer. In spite of the overlap, no splice should be performed
  // due to the overlapped buffer having estimated duration.
  NewCodedFrameGroupAppend("15D10K");
  CheckExpectedRangesByTimestamp("{ [0,25) }");
  CheckExpectedBuffers("0D10K 10D10EK 15D10K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceTrimming_ExistingTrimming) {
  const base::TimeDelta kDuration = base::Milliseconds(4);
  const base::TimeDelta kNoDiscard = base::TimeDelta();
  const bool is_keyframe = true;

  SetAudioStream();
  Seek(0);

  // Make two BufferQueues with a mix of buffers containing start/end discard.
  // Buffer PTS and duration have been adjusted to reflect discard. A_buffers
  // will be appended first, then B_buffers. The start of B will overlap A
  // to generate a splice.
  BufferQueue A_buffers;
  BufferQueue B_buffers;

  // Buffer A1: PTS = 0, front discard = 2ms, duration = 2ms.
  scoped_refptr<StreamParserBuffer> bufferA1 = StreamParserBuffer::CopyFrom(
      &kDataA, kDataSize, is_keyframe, DemuxerStream::AUDIO, 0);
  bufferA1->set_timestamp(base::Milliseconds(0));
  bufferA1->set_duration(kDuration / 2);
  const DecoderBuffer::DiscardPadding discardA1 =
      std::make_pair(kDuration / 2, kNoDiscard);
  bufferA1->set_discard_padding(discardA1);
  A_buffers.push_back(bufferA1);

  // Buffer A2: PTS = 2, end discard = 2ms, duration = 2ms.
  scoped_refptr<StreamParserBuffer> bufferA2 = StreamParserBuffer::CopyFrom(
      &kDataA, kDataSize, is_keyframe, DemuxerStream::AUDIO, 0);
  bufferA2->set_timestamp(base::Milliseconds(2));
  bufferA2->set_duration(kDuration / 2);
  const DecoderBuffer::DiscardPadding discardA2 =
      std::make_pair(kNoDiscard, kDuration / 2);
  bufferA2->set_discard_padding(discardA2);
  A_buffers.push_back(bufferA2);

  // Buffer B1: PTS = 3, front discard = 2ms, duration = 2ms.
  scoped_refptr<StreamParserBuffer> bufferB1 = StreamParserBuffer::CopyFrom(
      &kDataA, kDataSize, is_keyframe, DemuxerStream::AUDIO, 0);
  bufferB1->set_timestamp(base::Milliseconds(3));
  bufferB1->set_duration(kDuration / 2);
  const DecoderBuffer::DiscardPadding discardB1 =
      std::make_pair(kDuration / 2, kNoDiscard);
  bufferB1->set_discard_padding(discardB1);
  B_buffers.push_back(bufferB1);

  // Buffer B2: PTS = 5, no discard padding, duration = 4ms.
  scoped_refptr<StreamParserBuffer> bufferB2 = StreamParserBuffer::CopyFrom(
      &kDataA, kDataSize, is_keyframe, DemuxerStream::AUDIO, 0);
  bufferB2->set_timestamp(base::Milliseconds(5));
  bufferB2->set_duration(kDuration);
  B_buffers.push_back(bufferB2);

  // Append buffers, trigger splice trimming.
  stream_->OnStartOfCodedFrameGroup(bufferA1->timestamp());
  stream_->Append(A_buffers);
  EXPECT_MEDIA_LOG(TrimmedSpliceOverlap(3000, 2000, 1000));
  stream_->Append(B_buffers);

  // Verify buffers.
  scoped_refptr<StreamParserBuffer> read_buffer;

  // Buffer A1 was not spliced, should be unchanged.
  EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&read_buffer));
  EXPECT_EQ(base::Milliseconds(0), read_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, read_buffer->duration());
  EXPECT_EQ(discardA1, read_buffer->discard_padding());

  // Buffer A2 was overlapped by buffer B1 1ms. Splice trimming should trim A2's
  // duration and increase its discard padding by 1ms.
  const base::TimeDelta overlap = base::Milliseconds(1);
  EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&read_buffer));
  EXPECT_EQ(base::Milliseconds(2), read_buffer->timestamp());
  EXPECT_EQ((kDuration / 2) - overlap, read_buffer->duration());
  const DecoderBuffer::DiscardPadding overlap_discard =
      std::make_pair(discardA2.first, discardA2.second + overlap);
  EXPECT_EQ(overlap_discard, read_buffer->discard_padding());

  // Buffer B1 is overlapping A2, but B1 should be unchanged - splice trimming
  // only modifies the earlier buffer (A1).
  EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&read_buffer));
  EXPECT_EQ(base::Milliseconds(3), read_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, read_buffer->duration());
  EXPECT_EQ(discardB1, read_buffer->discard_padding());

  // Buffer B2 is not spliced, should be unchanged.
  EXPECT_STATUS_FOR_STREAM_OP(kSuccess, GetNextBuffer(&read_buffer));
  EXPECT_EQ(base::Milliseconds(5), read_buffer->timestamp());
  EXPECT_EQ(kDuration, read_buffer->duration());
  EXPECT_EQ(std::make_pair(kNoDiscard, kNoDiscard),
            read_buffer->discard_padding());

  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_SpliceFrame_NoMillisecondSplices) {
  EXPECT_MEDIA_LOG(SkippingSpliceTooLittleOverlap(1250, 250));

  video_config_ = TestVideoConfig::Invalid();
  audio_config_.Initialize(
      AudioCodec::kVorbis, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 4000,
      EmptyExtraData(), EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
  ResetStream<>(audio_config_);
  // Equivalent to 0.5ms per frame.
  SetStreamInfo(2000, 2000);
  Seek(0);

  // Append four buffers with a 0.5ms duration each.
  NewCodedFrameGroupAppend(0, 4);
  CheckExpectedRangesByTimestamp("{ [0,2) }");

  // Overlap the range [0, 2) with [1.25, 2); this results in an overlap of
  // 0.25ms between the original buffer at time 1.0 and the new buffer at time
  // 1.25.
  NewCodedFrameGroupAppend_OffsetFirstBuffer(2, 2, base::Milliseconds(0.25));
  CheckExpectedRangesByTimestamp("{ [0,2) }");

  // A splice frame should not be generated since it requires at least 1ms of
  // data to crossfade.
  CheckExpectedBuffers("0K 0K 1K 1K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_PrerollFrame) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 3P 6K");
  CheckExpectedBuffers("0K 3P 6K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_ConfigChangeWithPreroll) {
  AudioDecoderConfig new_config(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                                CHANNEL_LAYOUT_MONO, 2000, EmptyExtraData(),
                                EncryptionScheme::kUnencrypted);
  SetAudioStream();
  Seek(0);

  // Append some audio using the default configuration.
  CheckAudioConfig(audio_config_);
  NewCodedFrameGroupAppend("0K 3K 6K");

  // Update the configuration.
  stream_->UpdateAudioConfig(new_config, false);

  // We haven't read any buffers at this point, so the config for the next
  // buffer at time 0 should still be the original config.
  CheckAudioConfig(audio_config_);

  // Append new audio containing preroll and using the new config.
  EXPECT_MEDIA_LOG(TrimmedSpliceOverlap(7000, 6000, 2000));
  NewCodedFrameGroupAppend("7P 8K");

  // Check buffers from the first append.
  CheckExpectedBuffers("0K 3K 6K");

  // Verify the next attempt to get a buffer will signal that a config change
  // has happened.
  scoped_refptr<StreamParserBuffer> buffer;
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));

  // Verify upcoming buffers will use the new config.
  CheckAudioConfig(new_config);

  // Check buffers from the second append, including preroll.
  // CheckExpectedBuffers("6P 7K 8K");
  CheckExpectedBuffers("7P 8K");

  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, Audio_Opus_SeekToJustBeforeRangeStart) {
  // Seek to a time within the fudge room of seekability to a buffered Opus
  // audio frame's range, but before the range's start. Use small seek_preroll
  // in case the associated logic to check same config in the preroll time
  // interval requires a nonzero seek_preroll value.
  video_config_ = TestVideoConfig::Invalid();
  audio_config_.Initialize(AudioCodec::kOpus, kSampleFormatPlanarF32,
                           CHANNEL_LAYOUT_STEREO, 1000, EmptyExtraData(),
                           EncryptionScheme::kUnencrypted,
                           base::Milliseconds(10), 0);
  ResetStream<>(audio_config_);

  // Equivalent to 1s per frame.
  SetStreamInfo(1, 1);
  Seek(0);

  // Append a buffer at 1.5 seconds, with duration 1 second, increasing the
  // fudge room to 2 * 1 seconds. The pending seek to time 0 should be satisfied
  // with this buffer's range, because that seek time is within the fudge room
  // of 2.
  NewCodedFrameGroupAppend("1500D1000K");
  CheckExpectedRangesByTimestamp("{ [1500,2500) }");
  CheckExpectedBuffers("1500K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, BFrames) {
  Seek(0);
  NewCodedFrameGroupAppend("0K 120|30 30|60 60|90 90|120");
  CheckExpectedRangesByTimestamp("{ [0,150) }");

  CheckExpectedBuffers("0K 120|30 30|60 60|90 90|120");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RemoveShouldAlwaysExcludeEnd) {
  NewCodedFrameGroupAppend("10D2K 12D2 14D2");
  CheckExpectedRangesByTimestamp("{ [10,16) }");

  // Start new coded frame group, appending KF to abut the start of previous
  // group.
  NewCodedFrameGroupAppend("0D10K");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,16) }");
  CheckExpectedBuffers("0K 10K 12 14");
  CheckNoNextBuffer();

  // Append another buffer with the same timestamp as the last KF. This triggers
  // special logic that allows two buffers to have the same timestamp. When
  // preparing for this new append, there is no reason to remove the later GOP
  // starting at timestamp 10. This verifies the fix for http://crbug.com/469325
  // where the decision *not* to remove the start of the overlapped range was
  // erroneously triggering buffers with a timestamp matching the end
  // of the append (and any later dependent frames) to be removed.
  AppendBuffers("0D10");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,16) }");
  CheckExpectedBuffers("0K 0 10K 12 14");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RefinedDurationEstimates_BackOverlap) {
  // Append a few buffers, the last one having estimated duration.
  NewCodedFrameGroupAppend("0K 5 10 20D10E");
  CheckExpectedRangesByTimestamp("{ [0,30) }");
  Seek(0);
  CheckExpectedBuffers("0K 5 10 20D10E");
  CheckNoNextBuffer();

  // Append a buffer to the end that overlaps the *back* of the existing range.
  // This should trigger the estimated duration to be recomputed as a timestamp
  // delta.
  AppendBuffers("25D10");
  CheckExpectedRangesByTimestamp("{ [0,35) }");
  Seek(0);
  // The duration of the buffer at time 20 has changed from 10ms to 5ms.
  CheckExpectedBuffers("0K 5 10 20D5E 25");
  CheckNoNextBuffer();

  // If the last buffer is removed, the adjusted duration should remain at 5ms.
  RemoveInMs(25, 35, 35);
  CheckExpectedRangesByTimestamp("{ [0,25) }");
  Seek(0);
  CheckExpectedBuffers("0K 5 10 20D5E");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RefinedDurationEstimates_FrontOverlap) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("10K 15 20D5");
  CheckExpectedRangesByTimestamp("{ [10,25) }");
  SeekToTimestampMs(10);
  CheckExpectedBuffers("10K 15 20");
  CheckNoNextBuffer();

  // Append new buffers, where the last has estimated duration that overlaps the
  // *front* of the existing range. The overlap should trigger refinement of the
  // estimated duration from 7ms to 5ms.
  NewCodedFrameGroupAppend("0K 5D7E");
  CheckExpectedRangesByTimestamp("{ [0,25) }");
  Seek(0);
  CheckExpectedBuffers("0K 5D5E 10K 15 20");
  CheckNoNextBuffer();

  // If the overlapped buffer at timestamp 10 is removed, the adjusted duration
  // should remain adjusted.
  RemoveInMs(10, 20, 25);
  CheckExpectedRangesByTimestamp("{ [0,10) }");
  Seek(0);
  CheckExpectedBuffers("0K 5D5E");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SeekToStartSatisfiedUpToThreshold) {
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");

  SeekToTimestampMs(0);
  CheckExpectedBuffers("999K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SeekToStartUnsatisfiedBeyondThreshold) {
  NewCodedFrameGroupAppend("1000K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [1000,1030) }");

  SeekToTimestampMs(0);
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       ReSeekToStartSatisfiedUpToThreshold_SameTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(0);
  RemoveInMs(999, 1030, 1030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers at the original timestamps and verify no stall.
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");
  CheckExpectedBuffers("999K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       ReSeekToStartSatisfiedUpToThreshold_EarlierTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(0);
  RemoveInMs(999, 1030, 1030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers before the original timestamps and verify no stall (the
  // re-seek to time 0 should still be satisfied with the new buffers).
  NewCodedFrameGroupAppend("500K 510 520D10");
  CheckExpectedRangesByTimestamp("{ [500,530) }");
  CheckExpectedBuffers("500K 510 520D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       ReSeekToStartSatisfiedUpToThreshold_LaterTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("500K 510 520D10");
  CheckExpectedRangesByTimestamp("{ [500,530) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(0);
  RemoveInMs(500, 530, 530);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers beginning after original timestamps, but still below the
  // start threshold, and verify no stall (the re-seek to time 0 should still be
  // satisfied with the new buffers).
  NewCodedFrameGroupAppend("999K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [999,1030) }");
  CheckExpectedBuffers("999K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ReSeekBeyondStartThreshold_SameTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("1000K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [1000,1030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(1000);
  RemoveInMs(1000, 1030, 1030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers at the original timestamps and verify no stall.
  NewCodedFrameGroupAppend("1000K 1010 1020D10");
  CheckExpectedRangesByTimestamp("{ [1000,1030) }");
  CheckExpectedBuffers("1000K 1010 1020D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ReSeekBeyondThreshold_EarlierTimestamps) {
  // Append a few buffers.
  NewCodedFrameGroupAppend("2000K 2010 2020D10");
  CheckExpectedRangesByTimestamp("{ [2000,2030) }");

  // Don't read any buffers between Seek and Remove.
  SeekToTimestampMs(2000);
  RemoveInMs(2000, 2030, 2030);
  CheckExpectedRangesByTimestamp("{ }");
  CheckNoNextBuffer();

  // Append buffers before the original timestamps and verify no stall (the
  // re-seek to time 2 seconds should still be satisfied with the new buffers
  // and should emit preroll from last keyframe).
  NewCodedFrameGroupAppend("1080K 1090 2000D10");
  CheckExpectedRangesByTimestamp("{ [1080,2010) }");
  CheckExpectedBuffers("1080K 1090 2000D10");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ConfigChange_ReSeek) {
  // Append a few buffers, with a config change in the middle.
  VideoDecoderConfig new_config = TestVideoConfig::Large();
  NewCodedFrameGroupAppend("2000K 2010 2020D10");
  stream_->UpdateVideoConfig(new_config, false);
  NewCodedFrameGroupAppend("2030K 2040 2050D10");
  CheckExpectedRangesByTimestamp("{ [2000,2060) }");

  // Read the config change, but don't read any non-config-change buffer between
  // Seek and Remove.
  scoped_refptr<StreamParserBuffer> buffer;
  CheckVideoConfig(video_config_);
  SeekToTimestampMs(2030);
  CheckVideoConfig(video_config_);
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));
  CheckVideoConfig(new_config);

  // Trigger the re-seek.
  RemoveInMs(2030, 2060, 2060);
  CheckExpectedRangesByTimestamp("{ [2000,2030) }");
  CheckNoNextBuffer();

  // Append buffers at the original timestamps and verify no stall or redundant
  // signalling of config change.
  NewCodedFrameGroupAppend("2030K 2040 2050D10");
  CheckVideoConfig(new_config);
  CheckExpectedRangesByTimestamp("{ [2000,2060) }");
  CheckExpectedBuffers("2030K 2040 2050D10");
  CheckNoNextBuffer();
  CheckVideoConfig(new_config);

  // Seek to the start of buffered and verify config changes and buffers.
  SeekToTimestampMs(2000);
  CheckVideoConfig(new_config);
  ASSERT_FALSE(new_config.Matches(video_config_));
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));
  CheckVideoConfig(video_config_);
  CheckExpectedBuffers("2000K 2010 2020D10");
  CheckVideoConfig(video_config_);
  EXPECT_TRUE(stream_->IsNextBufferConfigChanged());
  EXPECT_STATUS_FOR_STREAM_OP(kConfigChange, GetNextBuffer(&buffer));
  CheckVideoConfig(new_config);
  CheckExpectedBuffers("2030K 2040 2050D10");
  CheckNoNextBuffer();
  CheckVideoConfig(new_config);
}

TEST_F(SourceBufferStreamTest, TrackBuffer_ExhaustionWithSkipForward) {
  NewCodedFrameGroupAppend("0K 10 20 30 40");

  // Read the first 4 buffers, so next buffer is at time 40.
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,50) }");
  CheckExpectedBuffers("0K 10 20 30");

  // Overlap-append, populating track buffer with timestamp 40 from original
  // append. Confirm there could be a large jump in time until the next key
  // frame after exhausting the track buffer.
  NewCodedFrameGroupAppend(
      "31K 41 51 61 71 81 91 101 111 121 "
      "131K 141");
  CheckExpectedRangesByTimestamp("{ [0,151) }");

  // Confirm the large jump occurs and warning log is generated.
  // If this test is changed, update
  // TrackBufferExhaustion_ImmediateNewTrackBuffer accordingly.
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(91));

  CheckExpectedBuffers("40 131K 141");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       TrackBuffer_ExhaustionAndImmediateNewTrackBuffer) {
  NewCodedFrameGroupAppend("0K 10 20 30 40");

  // Read the first 4 buffers, so next buffer is at time 40.
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,50) }");
  CheckExpectedBuffers("0K 10 20 30");

  // Overlap-append
  NewCodedFrameGroupAppend(
      "31K 41 51 61 71 81 91 101 111 121 "
      "131K 141");
  CheckExpectedRangesByTimestamp("{ [0,151) }");

  // Exhaust the track buffer, but don't read any of the overlapping append yet.
  CheckExpectedBuffers("40");

  // Selected range's next buffer is now the 131K buffer from the overlapping
  // append. (See TrackBuffer_ExhaustionWithSkipForward for that verification.)
  // Do another overlap-append to immediately create another track buffer and
  // verify both track buffer exhaustions skip forward and emit log warnings.
  NewCodedFrameGroupAppend(
      "22K 32 42 52 62 72 82 92 102 112 122K 132 142 152K 162");
  CheckExpectedRangesByTimestamp("{ [0,172) }");

  InSequence s;
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(91));
  EXPECT_MEDIA_LOG(ContainsTrackBufferExhaustionSkipLog(11));

  CheckExpectedBuffers("131K 141 152K 162");
  CheckNoNextBuffer();
}

TEST_F(
    SourceBufferStreamTest,
    AdjacentCodedFrameGroupContinuation_NoGapCreatedByTinyGapInGroupContinuation) {
  NewCodedFrameGroupAppend("0K 10 20K 30 40K 50D10");
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  // Continue appending to the previously started coded frame group, albeit with
  // a tiny (1ms) gap. This gap should *NOT* produce a buffered range gap.
  AppendBuffers("61K 71D10");
  CheckExpectedRangesByTimestamp("{ [0,81) }");
}

TEST_F(SourceBufferStreamTest,
       AdjacentCodedFrameGroupContinuation_NoGapCreatedPrefixRemoved) {
  NewCodedFrameGroupAppend("0K 10 20K 30 40K 50D10");
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  RemoveInMs(0, 35, 60);
  CheckExpectedRangesByTimestamp("{ [40,60) }");

  // Continue appending to the previously started coded frame group, albeit with
  // a tiny (1ms) gap. This gap should *NOT* produce a buffered range gap.
  AppendBuffers("61K 71D10");
  CheckExpectedRangesByTimestamp("{ [40,81) }");
}

TEST_F(SourceBufferStreamTest,
       AdjacentNewCodedFrameGroupContinuation_NoGapCreatedPrefixRemoved) {
  NewCodedFrameGroupAppend("0K 10 20K 30 40K 50D10");
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  RemoveInMs(0, 35, 60);
  CheckExpectedRangesByTimestamp("{ [40,60) }");

  // Continue appending, with a new coded frame group, albeit with
  // a tiny (1ms) gap. This gap should *NOT* produce a buffered range gap.
  // This test demonstrates the "pre-relaxation" behavior, where a new "media
  // segment" (now a new "coded frame group") was signaled at every media
  // segment boundary.
  NewCodedFrameGroupAppend("61K 71D10");
  CheckExpectedRangesByTimestamp("{ [40,81) }");
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_RemoveThenAppendMoreMuchLater) {
  NewCodedFrameGroupAppend("1000K 1010 1020 1030K 1040 1050 1060K 1070 1080");
  NewCodedFrameGroupAppend("0K 10 20");
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1090) }");

  SignalStartOfCodedFrameGroup(base::Milliseconds(1070));
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1090) }");

  RemoveInMs(1030, 1050, 1090);
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1030) [1060,1090) }");

  // We've signalled that we're about to do some appends to a coded frame group
  // which starts at time 1070ms. Note that the first frame, if any ever,
  // appended to this SourceBufferStream for that coded frame group must have a
  // decode timestamp >= 1070ms (it can be significantly in the future).
  // Regardless, that appended frame must be buffered into the same existing
  // range as current [1060,1090), since the new coded frame group's start of
  // 1070ms is within that range.
  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,30) [1000,1030) [1060,2020) }");
  SeekToTimestampMs(1060);
  CheckExpectedBuffers("1060K 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_AppendMuchLater) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::Milliseconds(45));
  CheckExpectedRangesByTimestamp("{ [0,60) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 30K 40 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveGOP_ThenAppend_1) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::Milliseconds(30));
  RemoveInMs(30, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,30) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveGOP_ThenAppend_2) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  // Though we signal 45ms, it's adjusted internally (due to detected overlap)
  // to be 40.001ms (which is just beyond the highest buffered timestamp at or
  // before 45ms) to help prevent potential discontinuity across the front of
  // the overlapping append.
  SignalStartOfCodedFrameGroup(base::Milliseconds(45));
  RemoveInMs(30, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,30) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,30) [40,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20");
  CheckNoNextBuffer();
  SeekToTimestampMs(40);
  CheckExpectedBuffers("2000K 2010");
  CheckNoNextBuffer();
  SeekToTimestampMs(1000);
  CheckExpectedBuffers("2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveMostRecentAppend_ThenAppend_1) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::Milliseconds(45));
  RemoveInMs(50, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,50) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 30K 40 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       StartCodedFrameGroup_InExisting_RemoveMostRecentAppend_ThenAppend_2) {
  NewCodedFrameGroupAppend("0K 10 20 30K 40 50");
  SignalStartOfCodedFrameGroup(base::Milliseconds(50));
  RemoveInMs(50, 60, 60);
  CheckExpectedRangesByTimestamp("{ [0,50) }");

  AppendBuffers("2000K 2010");
  CheckExpectedRangesByTimestamp("{ [0,2020) }");
  Seek(0);
  CheckExpectedBuffers("0K 10 20 30K 40 2000K 2010");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, GetLowestPresentationTimestamp_NonMuxed) {
  EXPECT_EQ(base::TimeDelta(), stream_->GetLowestPresentationTimestamp());

  NewCodedFrameGroupAppend("100K 110K");
  EXPECT_EQ(base::Milliseconds(100), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(110, 120, 120);
  EXPECT_EQ(base::Milliseconds(100), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(100, 110, 120);
  EXPECT_EQ(base::TimeDelta(), stream_->GetLowestPresentationTimestamp());

  NewCodedFrameGroupAppend("100K 110K");
  EXPECT_EQ(base::Milliseconds(100), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(100, 110, 120);
  EXPECT_EQ(base::Milliseconds(110), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(110, 120, 120);
  EXPECT_EQ(base::TimeDelta(), stream_->GetLowestPresentationTimestamp());
}

TEST_F(SourceBufferStreamTest, GetLowestPresentationTimestamp_Muxed) {
  // Simulate `stream_` being one of multiple resulting from parsing and
  // buffering a muxed bytestream. In this case, it is common for range start
  // times across the streams in the same muxed segment to not precisely align.
  // The frame processing algorithm indicates the segment's "coded frame group
  // start time" to the SourceBufferStream, and the underlying range remembers
  // this even if the corresponding actual start time in the underlying range is
  // later than that start time. However, if the start of that range is removed,
  // then the underlying range no longer attempts to maintain the original
  // "coded frame group start time" as the lowest timestamp. This impacts
  // GetLowestPresentationTimestamp(), since the underlying range start time of
  // the first range is involved and is conditional. See also
  // SourceBufferRange::GetStartTimestamp().
  EXPECT_EQ(base::TimeDelta(), stream_->GetLowestPresentationTimestamp());

  NewCodedFrameGroupAppend(base::Milliseconds(50), "100K 110K");
  EXPECT_EQ(base::Milliseconds(50), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(110, 120, 120);
  EXPECT_EQ(base::Milliseconds(50), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(100, 110, 120);
  EXPECT_EQ(base::TimeDelta(), stream_->GetLowestPresentationTimestamp());

  NewCodedFrameGroupAppend(base::Milliseconds(50), "100K 110K");
  EXPECT_EQ(base::Milliseconds(50), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(100, 110, 120);
  EXPECT_EQ(base::Milliseconds(110), stream_->GetLowestPresentationTimestamp());

  RemoveInMs(110, 120, 120);
  EXPECT_EQ(base::TimeDelta(), stream_->GetLowestPresentationTimestamp());
}

TEST_F(SourceBufferStreamTest, GetHighestPresentationTimestamp) {
  EXPECT_EQ(base::TimeDelta(), stream_->GetHighestPresentationTimestamp());

  NewCodedFrameGroupAppend("0K 10K");
  EXPECT_EQ(base::Milliseconds(10), stream_->GetHighestPresentationTimestamp());

  RemoveInMs(0, 10, 20);
  EXPECT_EQ(base::Milliseconds(10), stream_->GetHighestPresentationTimestamp());

  RemoveInMs(10, 20, 20);
  EXPECT_EQ(base::TimeDelta(), stream_->GetHighestPresentationTimestamp());

  NewCodedFrameGroupAppend("0K 10K");
  EXPECT_EQ(base::Milliseconds(10), stream_->GetHighestPresentationTimestamp());

  RemoveInMs(10, 20, 20);
  EXPECT_EQ(base::TimeDelta(), stream_->GetHighestPresentationTimestamp());
}

TEST_F(SourceBufferStreamTest, GarbageCollectionUnderMemoryPressure) {
  SetMemoryLimit(16);
  NewCodedFrameGroupAppend("0K 1 2 3K 4 5 6K 7 8 9K 10 11 12K 13 14 15K");
  CheckExpectedRangesByTimestamp("{ [0,16) }");

  // This feature is disabled by default, so by default memory pressure
  // notification takes no effect and the memory limits and won't remove
  // anything from buffered ranges, since we are under the limit of 20 bytes.
  stream_->OnMemoryPressure(
      base::Milliseconds(0),
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, false);
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(8), 0));
  CheckExpectedRangesByTimestamp("{ [0,16) }");

  // Now enable the feature (on top of any overrides already in
  // |scoped_feature_list_|.)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kMemoryPressureBasedSourceBufferGC);

  // Verify that effective MSE memory limit is reduced under memory pressure.
  stream_->OnMemoryPressure(
      base::Milliseconds(0),
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, false);

  // Effective memory limit is now 8 buffers, but we still will not collect any
  // data between the current playback position 3 and last append position 15.
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(4), 0));
  CheckExpectedRangesByTimestamp("{ [3,16) }");

  // As playback proceeds further to time 9 we should be able to collect
  // enough data to bring us back under memory limit of 8 buffers.
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(9), 0));
  CheckExpectedRangesByTimestamp("{ [9,16) }");

  // If memory pressure becomes critical, the garbage collection algorithm
  // becomes even more aggressive and collects everything up to the current
  // playback position.
  stream_->OnMemoryPressure(
      base::Milliseconds(0),
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, false);
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(13), 0));
  CheckExpectedRangesByTimestamp("{ [12,16) }");

  // But even under critical memory pressure the MSE memory limit imposed by the
  // memory pressure is soft, i.e. we should be able to append more data
  // successfully up to the hard limit of 16 bytes.
  NewCodedFrameGroupAppend("16K 17 18 19 20 21 22 23 24 25 26 27");
  CheckExpectedRangesByTimestamp("{ [12,28) }");
  EXPECT_TRUE(GarbageCollect(base::Milliseconds(13), 0));
  CheckExpectedRangesByTimestamp("{ [12,28) }");
}

TEST_F(SourceBufferStreamTest, InstantGarbageCollectionUnderMemoryPressure) {
  SetMemoryLimit(16);
  NewCodedFrameGroupAppend("0K 1 2 3K 4 5 6K 7 8 9K 10 11 12K 13 14 15K");
  CheckExpectedRangesByTimestamp("{ [0,16) }");

  // Verify that garbage collection happens immediately on critical memory
  // pressure notification, even without explicit GarbageCollect invocation,
  // when the immediate GC is allowed.
  // First, enable the feature (on top of any overrides already in
  // |scoped_feature_list_|.)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kMemoryPressureBasedSourceBufferGC);
  stream_->OnMemoryPressure(
      base::Milliseconds(7),
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, true);
  CheckExpectedRangesByTimestamp("{ [6,16) }");
  stream_->OnMemoryPressure(
      base::Milliseconds(9),
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, true);
  CheckExpectedRangesByTimestamp("{ [9,16) }");
}

TEST_F(SourceBufferStreamTest, GCFromFrontThenExplicitRemoveFromMiddleToEnd) {
  // Attempts to exercise SourceBufferRange::GetBufferIndexAt() after its
  // |keyframe_map_index_base_| has been increased, and when there is a GOP
  // following the search timestamp.  GC followed by an explicit remove may
  // trigger that code path.
  SetMemoryLimit(10);

  // Append 3 IBPPP GOPs in one continuous range.
  NewCodedFrameGroupAppend(
      "0K 40|10 10|20 20|30 30|40 "
      "50K 90|60 60|70 70|80 80|90 "
      "100K 140|110 110|120 120|130 130|140");

  CheckExpectedRangesByTimestamp("{ [0,150) }");

  // Seek to the second GOP's keyframe to allow GC to collect all of the first
  // GOP (ostensibly increasing SourceBufferRange's |keyframe_map_index_base_|).
  SeekToTimestampMs(50);
  GarbageCollect(base::Milliseconds(50), 0);
  CheckExpectedRangesByTimestamp("{ [50,150) }");

  // Remove from the middle of the first remaining GOP to the end of the range.
  RemoveInMs(60, 150, 150);
  CheckExpectedRangesByTimestamp("{ [50,60) }");
}

TEST_F(SourceBufferStreamTest, BFrames_WithoutEditList) {
  // Simulates B-frame content where MP4 edit lists are not used to shift PTS so
  // it matches DTS. From acolwell@chromium.org in https://crbug.com/398130
  Seek(0);
  NewCodedFrameGroupAppend(base::Milliseconds(60),
                           "60|0K 180|30 90|60 120|90 150|120");
  CheckExpectedRangesByTimestamp("{ [60,210) }");
  CheckExpectedBuffers("60|0K 180|30 90|60 120|90 150|120");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, OverlapSameTimestampWithinSameGOP) {
  // We use distinct appends here to make sure the intended frame durations
  // are respected by the test helpers (which the OneByOne helper doesn't
  // respect always). We need granular appends of this GOP for at least the
  // append of PTS=DTS=30, below.
  NewCodedFrameGroupAppend("0|0D10K");
  AppendBuffers("30|10D0");
  AppendBuffers("20|20D10");

  // The following should *not* remove the PTS frame 30, above.
  AppendBuffers("30|30 40|40");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,50) }");
  CheckExpectedBuffers("0K 30|10 20 30 40");
}

struct VideoEndTimeCase {
  // Times in Milliseconds
  int64_t new_frame_pts;
  int64_t new_frame_duration;
  int64_t expected_highest_pts;
  int64_t expected_end_time;
};

TEST_F(SourceBufferStreamTest, VideoRangeEndTimeCases) {
  // With a basic range containing just a single keyframe [10,20), verify
  // various keyframe overlap append cases' results on the range end time.
  const VideoEndTimeCase kCases[] = {
      {0, 10, 10, 20},
      {20, 1, 20, 21},
      {15, 3, 15, 18},
      {15, 5, 15, 20},
      {15, 8, 15, 23},

      // Cases where the new frame removes the previous frame:
      {10, 3, 10, 13},
      {10, 10, 10, 20},
      {10, 13, 10, 23},
      {5, 8, 5, 13},
      {5, 15, 5, 20},
      {5, 20, 5, 25}};

  for (const auto& c : kCases) {
    RemoveInMs(0, 100, 100);
    NewCodedFrameGroupAppend("10D10K");
    CheckExpectedRangesByTimestamp("{ [10,20) }");
    CheckExpectedRangeEndTimes("{ <10,20> }");

    std::stringstream ss;
    ss << c.new_frame_pts << "D" << c.new_frame_duration << "K";
    DVLOG(1) << "Appending " << ss.str();
    NewCodedFrameGroupAppend(ss.str());

    std::stringstream expected;
    expected << "{ <" << c.expected_highest_pts << "," << c.expected_end_time
             << "> }";
    CheckExpectedRangeEndTimes(expected.str());
  }
}

struct AudioEndTimeCase {
  // Times in Milliseconds
  int64_t new_frame_pts;
  int64_t new_frame_duration;
  int64_t expected_highest_pts;
  int64_t expected_end_time;
  bool expect_splice;
};

TEST_F(SourceBufferStreamTest, AudioRangeEndTimeCases) {
  // With a basic range containing just a single keyframe [10,20), verify
  // various keyframe overlap append cases' results on the range end time.
  const AudioEndTimeCase kCases[] = {
      {0, 10, 10, 20, false},
      {20, 1, 20, 21, false},
      {15, 3, 15, 18, true},
      {15, 5, 15, 20, true},
      {15, 8, 15, 23, true},

      // Cases where the new frame removes the previous frame:
      {10, 3, 10, 13, false},
      {10, 10, 10, 20, false},
      {10, 13, 10, 23, false},
      {5, 8, 5, 13, false},
      {5, 15, 5, 20, false},
      {5, 20, 5, 25, false}};

  SetAudioStream();
  for (const auto& c : kCases) {
    InSequence s;

    RemoveInMs(0, 100, 100);
    NewCodedFrameGroupAppend("10D10K");
    CheckExpectedRangesByTimestamp("{ [10,20) }");
    CheckExpectedRangeEndTimes("{ <10,20> }");

    std::stringstream ss;
    ss << c.new_frame_pts << "D" << c.new_frame_duration << "K";
    if (c.expect_splice) {
      EXPECT_MEDIA_LOG(TrimmedSpliceOverlap(c.new_frame_pts * 1000, 10000,
                                            (20 - c.new_frame_pts) * 1000));
    }
    DVLOG(1) << "Appending " << ss.str();
    NewCodedFrameGroupAppend(ss.str());

    std::stringstream expected;
    expected << "{ <" << c.expected_highest_pts << "," << c.expected_end_time
             << "> }";
    CheckExpectedRangeEndTimes(expected.str());
  }
}

TEST_F(SourceBufferStreamTest, SameTimestampEstimatedDurations_Video) {
  // Start a coded frame group with a frame having a non-estimated duration.
  NewCodedFrameGroupAppend("10D10K");

  // In the same coded frame group, append a same-timestamp frame with estimated
  // duration smaller than the first frame. (This can happen at least if there
  // was an intervening init segment resetting the estimation logic.) This
  // second frame need not be a keyframe. We use a non-keyframe here to
  // differentiate the buffers in CheckExpectedBuffers(), below.
  AppendBuffers("10D9E");

  // The next append, which triggered https://crbug.com/761567, didn't need to
  // be with same timestamp as the earlier ones; it just needs to be in the same
  // buffered range.  Also, it doesn't need to be a keyframe, have an estimated
  // duration, nor be in the same coded frame group to trigger that issue.
  NewCodedFrameGroupAppend("11D10K");

  Seek(0);
  CheckExpectedRangesByTimestamp("{ [10,21) }");
  CheckExpectedRangeEndTimes("{ <11,21> }");
  CheckExpectedBuffers("10K 10 11K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RangeIsNextInPTS_Simple) {
  // Append a simple GOP where DTS==PTS, perform basic PTS continuity checks.
  NewCodedFrameGroupAppend("10D10K");
  CheckIsNextInPTSSequenceWithFirstRange(9, false);
  CheckIsNextInPTSSequenceWithFirstRange(10, true);
  CheckIsNextInPTSSequenceWithFirstRange(20, true);
  CheckIsNextInPTSSequenceWithFirstRange(30, true);
  CheckIsNextInPTSSequenceWithFirstRange(31, false);
}

TEST_F(SourceBufferStreamTest, RangeIsNextInPTS_OutOfOrder) {
  // Append a GOP where DTS != PTS such that a timestamp used as DTS would not
  // be continuous, but used as PTS is, and verify PTS continuity.
  NewCodedFrameGroupAppend("1000|0K 1120|30 1030|60 1060|90 1090|120");
  CheckIsNextInPTSSequenceWithFirstRange(0, false);
  CheckIsNextInPTSSequenceWithFirstRange(30, false);
  CheckIsNextInPTSSequenceWithFirstRange(60, false);
  CheckIsNextInPTSSequenceWithFirstRange(90, false);
  CheckIsNextInPTSSequenceWithFirstRange(120, false);
  CheckIsNextInPTSSequenceWithFirstRange(150, false);
  CheckIsNextInPTSSequenceWithFirstRange(1000, false);
  CheckIsNextInPTSSequenceWithFirstRange(1030, false);
  CheckIsNextInPTSSequenceWithFirstRange(1060, false);
  CheckIsNextInPTSSequenceWithFirstRange(1090, false);
  CheckIsNextInPTSSequenceWithFirstRange(1119, false);
  CheckIsNextInPTSSequenceWithFirstRange(1120, true);
  CheckIsNextInPTSSequenceWithFirstRange(1150, true);
  CheckIsNextInPTSSequenceWithFirstRange(1180, true);
  CheckIsNextInPTSSequenceWithFirstRange(1181, false);
}

TEST_F(SourceBufferStreamTest, RangeCoalescenceOnFudgeRoomIncrease_1) {
  // Change the fudge room (by increasing frame duration) and verify coalescence
  // behavior.
  NewCodedFrameGroupAppend("0K 10K");
  NewCodedFrameGroupAppend("100K 110K");
  NewCodedFrameGroupAppend("500K 510K");
  CheckExpectedRangesByTimestamp("{ [0,20) [100,120) [500,520) }");

  // Increase the fudge room almost enough to merge the first two buffered
  // ranges.
  NewCodedFrameGroupAppend("1000D44K");
  CheckExpectedRangesByTimestamp("{ [0,20) [100,120) [500,520) [1000,1044) }");

  // Increase the fudge room again to merge the first two buffered ranges.
  NewCodedFrameGroupAppend("2000D45K");
  CheckExpectedRangesByTimestamp(
      "{ [0,120) [500,520) [1000,1044) [2000,2045) }");

  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 10K 100K 110K");
  CheckNoNextBuffer();
  SeekToTimestampMs(500);
  CheckExpectedBuffers("500K 510K");
  CheckNoNextBuffer();
  SeekToTimestampMs(1000);
  CheckExpectedBuffers("1000K");
  CheckNoNextBuffer();
  SeekToTimestampMs(2000);
  CheckExpectedBuffers("2000K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, RangeCoalescenceOnFudgeRoomIncrease_2) {
  // Change the fudge room (by increasing frame duration) and verify coalescence
  // behavior.
  NewCodedFrameGroupAppend("0K 10K");
  NewCodedFrameGroupAppend("40K 50K 60K");
  CheckExpectedRangesByTimestamp("{ [0,20) [40,70) }");

  // Increase the fudge room to merge the first two buffered ranges.
  NewCodedFrameGroupAppend("1000D20K");
  CheckExpectedRangesByTimestamp("{ [0,70) [1000,1020) }");

  // Try to trigger unsorted ranges, as might occur if the first two buffered
  // ranges were not correctly coalesced.
  NewCodedFrameGroupAppend("45D10K");

  CheckExpectedRangesByTimestamp("{ [0,70) [1000,1020) }");
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 10K 40K 45K 60K");
  CheckNoNextBuffer();
  SeekToTimestampMs(1000);
  CheckExpectedBuffers("1000K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, NoRangeGapWhenIncrementallyOverlapped) {
  // Append 2 SAP-Type-1 GOPs continuous in DTS and PTS interval and with frame
  // durations and number of frames per GOP such that the first keyframe by
  // itself would not be considered "adjacent" to the second GOP by our fudge
  // room logic alone, but we now adjust the range start times occurring during
  // an overlap to enable overlap appends to remain continuous with the
  // remainder of the overlapped range, if any.  Then incrementally reappend
  // each frame of the first GOP.
  NewCodedFrameGroupAppend("0K 10 20 30 40 50K 60 70 80 90");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,100) }");
  CheckExpectedRangeEndTimes("{ <90,100> }");
  CheckExpectedBuffers("0K 10 20 30 40 50K 60 70 80 90");
  CheckNoNextBuffer();

  NewCodedFrameGroupAppend("0D10K");  // Replaces first GOP with 1 frame.
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,100) }");
  CheckExpectedRangeEndTimes("{ <90,100> }");
  CheckExpectedBuffers("0K 50K 60 70 80 90");
  CheckNoNextBuffer();

  // Add more of the replacement GOP.
  AppendBuffers("10 20");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,100) }");
  CheckExpectedRangeEndTimes("{ <90,100> }");
  CheckExpectedBuffers("0K 10 20 50K 60 70 80 90");
  CheckNoNextBuffer();

  // Add more of the replacement GOP.
  AppendBuffers("30D10");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,100) }");
  CheckExpectedRangeEndTimes("{ <90,100> }");
  CheckExpectedBuffers("0K 10 20 30 50K 60 70 80 90");
  CheckNoNextBuffer();

  // Complete the replacement GOP.
  AppendBuffers("40D10");
  Seek(0);
  CheckExpectedRangesByTimestamp("{ [0,100) }");
  CheckExpectedRangeEndTimes("{ <90,100> }");
  CheckExpectedBuffers("0K 10 20 30 40 50K 60 70 80 90");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, AllowIncrementalAppendsToCoalesceRangeGap) {
  // Append a SAP-Type-1 GOP with a coded frame group start time far before the
  // timestamp of the first GOP (beyond any fudge room possible in this test).
  // This simulates one of multiple muxed tracks with jagged start times
  // following a discontinuity.
  // Then incrementally append a preceding SAP-Type-1 GOP with frames that
  // eventually are adjacent within fudge room of the first appended GOP's group
  // start time and observe the buffered range and demux gap coalesces. Finally,
  // incrementally append more frames of that preceding GOP to fill in the
  // timeline to abut the first appended GOP's keyframe timestamp and observe no
  // further buffered range change or discontinuity.
  NewCodedFrameGroupAppend(base::Milliseconds(100), "150K 160");
  SeekToTimestampMs(100);
  CheckExpectedRangesByTimestamp("{ [100,170) }");
  CheckExpectedRangeEndTimes("{ <160,170> }");
  CheckExpectedBuffers("150K 160");
  CheckNoNextBuffer();

  NewCodedFrameGroupAppend("70D10K");
  SeekToTimestampMs(70);
  CheckExpectedRangesByTimestamp("{ [70,80) [100,170) }");
  CheckExpectedRangeEndTimes("{ <70,80> <160,170> }");
  CheckExpectedBuffers("70K");
  CheckNoNextBuffer();
  SeekToTimestampMs(100);
  CheckExpectedBuffers("150K 160");
  CheckNoNextBuffer();

  AppendBuffers("80D10");  // 80ms is just close enough to 100ms to coalesce.
  SeekToTimestampMs(70);
  CheckExpectedRangesByTimestamp("{ [70,170) }");
  CheckExpectedRangeEndTimes("{ <160,170> }");
  CheckExpectedBuffers("70K 80 150K 160");
  CheckNoNextBuffer();

  AppendBuffers("90D10");
  SeekToTimestampMs(70);
  CheckExpectedRangesByTimestamp("{ [70,170) }");
  CheckExpectedRangeEndTimes("{ <160,170> }");
  CheckExpectedBuffers("70K 80 90 150K 160");
  CheckNoNextBuffer();

  AppendBuffers("100 110 120");
  SeekToTimestampMs(70);
  CheckExpectedRangesByTimestamp("{ [70,170) }");
  CheckExpectedRangeEndTimes("{ <160,170> }");
  CheckExpectedBuffers("70K 80 90 100 110 120 150K 160");
  CheckNoNextBuffer();

  AppendBuffers("130D10");
  SeekToTimestampMs(70);
  CheckExpectedRangesByTimestamp("{ [70,170) }");
  CheckExpectedRangeEndTimes("{ <160,170> }");
  CheckExpectedBuffers("70K 80 90 100 110 120 130 150K 160");
  CheckNoNextBuffer();

  AppendBuffers("140D10");
  SeekToTimestampMs(70);
  CheckExpectedRangesByTimestamp("{ [70,170) }");
  CheckExpectedRangeEndTimes("{ <160,170> }");
  CheckExpectedBuffers("70K 80 90 100 110 120 130 140 150K 160");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, PreciselyOverlapLastAudioFrameAppended_1) {
  // Appends an audio frame, A, which is then immediately followed by a
  // subsequent frame, B. Then appends a new frame, C, which precisely overlaps
  // frame B, and verifies that there is exactly 1 buffered range resulting.
  SetAudioStream();

  // Frame A
  NewCodedFrameGroupAppend("0D10K");
  SeekToTimestampMs(0);
  CheckExpectedRangesByTimestamp("{ [0,10) }");
  CheckExpectedRangeEndTimes("{ <0,10> }");
  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();

  // Frame B
  NewCodedFrameGroupAppend("10D10K");
  SeekToTimestampMs(0);
  CheckExpectedRangesByTimestamp("{ [0,20) }");
  CheckExpectedRangeEndTimes("{ <10,20> }");
  CheckExpectedBuffers("0K 10K");
  CheckNoNextBuffer();

  // Frame C.
  // Though DTS is continuous per MSE spec, FrameProcessor signals new CFG more
  // granularly, including in this case.
  NewCodedFrameGroupAppend("10D10K");

  SeekToTimestampMs(0);
  CheckExpectedRangesByTimestamp("{ [0,20) }");
  CheckExpectedRangeEndTimes("{ <10,20> }");
  CheckExpectedBuffers("0K 10K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, PreciselyOverlapLastAudioFrameAppended_2) {
  // Appends an audio frame, A, which is then splice-trim-truncated by a
  // subsequent frame, B. Then appends a new frame, C, which precisely overlaps
  // frame B, and verifies that there is exactly 1 buffered range resulting.
  SetAudioStream();

  // Frame A
  NewCodedFrameGroupAppend("0D100K");
  SeekToTimestampMs(0);
  CheckExpectedRangesByTimestamp("{ [0,100) }");
  CheckExpectedRangeEndTimes("{ <0,100> }");
  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();

  // Frame B
  EXPECT_MEDIA_LOG(TrimmedSpliceOverlap(60000, 0, 40000));
  NewCodedFrameGroupAppend("60D10K");
  SeekToTimestampMs(0);
  CheckExpectedRangesByTimestamp("{ [0,70) }");
  CheckExpectedRangeEndTimes("{ <60,70> }");
  CheckExpectedBuffers("0K 60K");
  CheckNoNextBuffer();

  // Frame C.
  // Though DTS is continuous per MSE spec, FrameProcessor signals new CFG more
  // granularly, including in this case.
  NewCodedFrameGroupAppend("60D10K");

  SeekToTimestampMs(0);
  CheckExpectedRangesByTimestamp("{ [0,70) }");
  CheckExpectedRangeEndTimes("{ <60,70> }");
  CheckExpectedBuffers("0K 60K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, ZeroDurationBuffersThenIncreasingFudgeRoom) {
  // Appends some zero duration buffers to result in disjoint buffered ranges.
  // Verifies that increasing the fudge room allows those that become within
  // adjacency threshold to merge, including those for which the new fudge room
  // is well more than sufficient to let them be adjacent.
  SetAudioStream();

  NewCodedFrameGroupAppend("0uD0K");
  CheckExpectedRangesByTimestamp("{ [0,1) }", TimeGranularity::kMicrosecond);

  NewCodedFrameGroupAppend("1uD0K");
  CheckExpectedRangesByTimestamp("{ [0,2) }", TimeGranularity::kMicrosecond);

  // Initial fudge room allows for up to 2ms gap to coalesce.
  NewCodedFrameGroupAppend("5000uD0K");
  CheckExpectedRangesByTimestamp("{ [0,2) [5000,5001) }",
                                 TimeGranularity::kMicrosecond);
  NewCodedFrameGroupAppend("2002uD0K");
  CheckExpectedRangesByTimestamp("{ [0,2) [2002,2003) [5000,5001) }",
                                 TimeGranularity::kMicrosecond);

  // Grow the fudge room enough to coalesce the first two ranges.
  NewCodedFrameGroupAppend("8000uD1001uK");
  CheckExpectedRangesByTimestamp("{ [0,2003) [5000,5001) [8000,9001) }",
                                 TimeGranularity::kMicrosecond);

  // Append a buffer with duration 4ms, much larger than previous buffers. This
  // grows the fudge room to 8ms (2 * 4ms). Expect that the first three ranges
  // are retroactively merged due to being adjacent per the new, larger fudge
  // room.
  NewCodedFrameGroupAppend("100D4K");
  CheckExpectedRangesByTimestamp("{ [0,9001) [100000,104000) }",
                                 TimeGranularity::kMicrosecond);
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 1K 2002K 5000K 8000K",
                       TimeGranularity::kMicrosecond);
  CheckNoNextBuffer();
  SeekToTimestampMs(100);
  CheckExpectedBuffers("100K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, NonZeroDurationBuffersThenIncreasingFudgeRoom) {
  // Verifies that a single fudge room increase which merges more than 2
  // previously disjoint ranges in a row performs the merging correctly.
  NewCodedFrameGroupAppend("0D10K");
  NewCodedFrameGroupAppend("50D10K");
  NewCodedFrameGroupAppend("100D10K");
  NewCodedFrameGroupAppend("150D10K");
  NewCodedFrameGroupAppend("500D10K");
  CheckExpectedRangesByTimestamp(
      "{ [0,10) [50,60) [100,110) [150,160) [500,510) }");

  NewCodedFrameGroupAppend("600D30K");
  CheckExpectedRangesByTimestamp("{ [0,160) [500,510) [600,630) }");
  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K 50K 100K 150K");
  CheckNoNextBuffer();
  SeekToTimestampMs(500);
  CheckExpectedBuffers("500K");
  CheckNoNextBuffer();
  SeekToTimestampMs(600);
  CheckExpectedBuffers("600K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest, SapType2WithNonkeyframePtsInEarlierRange) {
  // Buffer a standalone GOP [0,10).
  NewCodedFrameGroupAppend("0D10K");
  CheckExpectedRangesByTimestamp("{ [0,10) }");

  // Following discontinuity (simulated by DTS gap, signalled by new coded frame
  // group with time beyond fudge room of [0,10)), buffer 2 new GOPs in a later
  // range: a SAP-2 GOP with a nonkeyframe with PTS belonging to the first
  // range, and a subsequent minimal GOP.
  NewCodedFrameGroupAppend("30D10K 1|40D10");
  CheckExpectedRangesByTimestamp("{ [0,10) [30,40) }");
  NewCodedFrameGroupAppend("40|50D10K");

  // Verify that there are two distinct ranges, and that the SAP-2 nonkeyframe
  // is buffered as part of the second range's first GOP.
  CheckExpectedRangesByTimestamp("{ [0,10) [30,50) }");

  SeekToTimestampMs(0);
  CheckExpectedBuffers("0K");
  CheckNoNextBuffer();
  SeekToTimestampMs(30);
  CheckExpectedBuffers("30K 1|40 40|50K");
  CheckNoNextBuffer();
}

TEST_F(SourceBufferStreamTest,
       MergeAllowedIfRangeEndTimeWithEstimatedDurationMatchesNextRangeStart) {
  // Tests the edge case where fudge room is not increased when an estimated
  // duration is increased due to overlap appends, causing two ranges to not be
  // within fudge room of each other (nor merged), yet abutting each other.
  // Append a GOP that has fudge room as its interval (e.g. 2 frames of same
  // duration >= minimum 1ms).
  NewCodedFrameGroupAppend("0D10K 10D10");
  CheckExpectedRangesByTimestamp("{ [0,20) }");

  // Trigger a DTS discontinuity so later 21ms append also is discontinuous and
  // retains 10ms*2 fudge room.
  NewCodedFrameGroupAppend("100D10K");
  CheckExpectedRangesByTimestamp("{ [0,20) [100,110) }");

  // Append another keyframe that starts within fudge room distance of the
  // non-keyframe in the GOP appended, above.
  NewCodedFrameGroupAppend("21D10K");
  CheckExpectedRangesByTimestamp("{ [0,31) [100,110) }");

  // Overlap-append the original GOP with a single estimated-duration keyframe.
  // Though its timestamp is not within fudge room of the next keyframe, that
  // next keyframe at time 21ms was in the overlapped range and is retained in
  // the result of the overlap append's range.
  NewCodedFrameGroupAppend("0D10EK");
  CheckExpectedRangesByTimestamp("{ [0,31) [100,110) }");

  // That new keyframe at time 0 now has derived estimated duration 21ms.  That
  // increased estimated duration did *not* increase the fudge room (which is
  // still 2 * 10ms = 20ms.) So the next line, which splices in a new frame at
  // time 21 causes the estimated keyframe at time 0 to not have a timestamp
  // within fudge room of the new range that starts right at 21ms, the same time
  // that ends the first buffered range, requiring CanAppendBuffersToEnd to
  // handle this scenario specifically.
  NewCodedFrameGroupAppend("21D10K");
  CheckExpectedRangesByTimestamp("{ [0,31) [100,110) }");

  SeekToTimestampMs(0);
  CheckExpectedBuffers("0D21EK 21D10K");
  CheckNoNextBuffer();
  SeekToTimestampMs(100);
  CheckExpectedBuffers("100D10K");
  CheckNoNextBuffer();
}

}  // namespace media
