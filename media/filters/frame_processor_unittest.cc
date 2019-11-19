// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/frame_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::Values;

namespace {

// Helper to shorten "base::TimeDelta::FromMilliseconds(...)" in these test
// cases for integer milliseconds.
constexpr base::TimeDelta Milliseconds(int64_t milliseconds) {
  return base::TimeDelta::FromMilliseconds(milliseconds);
}

}  // namespace

namespace media {

typedef StreamParser::BufferQueue BufferQueue;
typedef StreamParser::TrackId TrackId;

// Used for setting expectations on callbacks. Using a StrictMock also lets us
// test for missing or extra callbacks.
class FrameProcessorTestCallbackHelper {
 public:
  FrameProcessorTestCallbackHelper() = default;
  virtual ~FrameProcessorTestCallbackHelper() = default;

  MOCK_METHOD1(OnParseWarning, void(const SourceBufferParseWarning));
  MOCK_METHOD1(PossibleDurationIncrease, void(base::TimeDelta new_duration));

  // Helper that calls the mock method as well as does basic sanity checks on
  // |new_duration|.
  void OnPossibleDurationIncrease(base::TimeDelta new_duration) {
    PossibleDurationIncrease(new_duration);
    ASSERT_NE(kNoTimestamp, new_duration);
    ASSERT_NE(kInfiniteDuration, new_duration);
  }

  MOCK_METHOD2(OnAppend,
               void(const DemuxerStream::Type type,
                    const BufferQueue* buffers));
  MOCK_METHOD3(OnGroupStart,
               void(const DemuxerStream::Type type,
                    DecodeTimestamp start_dts,
                    base::TimeDelta start_pts));

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameProcessorTestCallbackHelper);
};

class FrameProcessorTest : public ::testing::TestWithParam<bool> {
 protected:
  FrameProcessorTest()
      : append_window_end_(kInfiniteDuration),
        frame_duration_(Milliseconds(10)),
        audio_id_(1),
        video_id_(2) {
    use_sequence_mode_ = GetParam();
    frame_processor_ = std::make_unique<FrameProcessor>(
        base::Bind(
            &FrameProcessorTestCallbackHelper::OnPossibleDurationIncrease,
            base::Unretained(&callbacks_)),
        &media_log_);
    frame_processor_->SetParseWarningCallback(
        base::Bind(&FrameProcessorTestCallbackHelper::OnParseWarning,
                   base::Unretained(&callbacks_)));
  }

  enum StreamFlags {
    HAS_AUDIO = 1 << 0,
    HAS_VIDEO = 1 << 1,
    OBSERVE_APPENDS_AND_GROUP_STARTS = 1 << 2
  };

  void AddTestTracks(int stream_flags) {
    const bool has_audio = (stream_flags & HAS_AUDIO) != 0;
    const bool has_video = (stream_flags & HAS_VIDEO) != 0;
    ASSERT_TRUE(has_audio || has_video);

    const bool setup_observers =
        (stream_flags & OBSERVE_APPENDS_AND_GROUP_STARTS) != 0;

    if (has_audio) {
      CreateAndConfigureStream(DemuxerStream::AUDIO, setup_observers);
      ASSERT_TRUE(audio_);
      EXPECT_TRUE(frame_processor_->AddTrack(audio_id_, audio_.get()));
      SeekStream(audio_.get(), Milliseconds(0));
    }
    if (has_video) {
      CreateAndConfigureStream(DemuxerStream::VIDEO, setup_observers);
      ASSERT_TRUE(video_);
      EXPECT_TRUE(frame_processor_->AddTrack(video_id_, video_.get()));
      SeekStream(video_.get(), Milliseconds(0));
    }
  }

  void SetTimestampOffset(base::TimeDelta new_offset) {
    timestamp_offset_ = new_offset;
    frame_processor_->SetGroupStartTimestampIfInSequenceMode(timestamp_offset_);
  }

  BufferQueue StringToBufferQueue(const std::string& buffers_to_append,
                                  const TrackId track_id,
                                  const DemuxerStream::Type type) {
    std::vector<std::string> timestamps = base::SplitString(
        buffers_to_append, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    BufferQueue buffers;
    for (size_t i = 0; i < timestamps.size(); i++) {
      bool is_keyframe = false;
      if (base::EndsWith(timestamps[i], "K", base::CompareCase::SENSITIVE)) {
        is_keyframe = true;
        // Remove the "K" off of the token.
        timestamps[i] = timestamps[i].substr(0, timestamps[i].length() - 1);
      }

      // Use custom decode timestamp if included.
      std::vector<std::string> buffer_timestamps = base::SplitString(
          timestamps[i], "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (buffer_timestamps.size() == 1)
        buffer_timestamps.push_back(buffer_timestamps[0]);
      CHECK_EQ(2u, buffer_timestamps.size());

      double time_in_ms, decode_time_in_ms;
      CHECK(base::StringToDouble(buffer_timestamps[0], &time_in_ms));
      CHECK(base::StringToDouble(buffer_timestamps[1], &decode_time_in_ms));

      // Create buffer. Encode the original time_in_ms as the buffer's data to
      // enable later verification of possible buffer relocation in presentation
      // timeline due to coded frame processing.
      const uint8_t* timestamp_as_data =
          reinterpret_cast<uint8_t*>(&time_in_ms);
      scoped_refptr<StreamParserBuffer> buffer =
          StreamParserBuffer::CopyFrom(timestamp_as_data, sizeof(time_in_ms),
                                       is_keyframe, type, track_id);
      buffer->set_timestamp(base::TimeDelta::FromMillisecondsD(time_in_ms));
      if (time_in_ms != decode_time_in_ms) {
        buffer->SetDecodeTimestamp(DecodeTimestamp::FromPresentationTime(
            base::TimeDelta::FromMillisecondsD(decode_time_in_ms)));
      }

      buffer->set_duration(frame_duration_);
      buffers.push_back(buffer);
    }
    return buffers;
  }

  bool ProcessFrames(const std::string& audio_timestamps,
                     const std::string& video_timestamps) {
    StreamParser::BufferQueueMap buffer_queue_map;
    const auto& audio_buffers =
        StringToBufferQueue(audio_timestamps, audio_id_, DemuxerStream::AUDIO);
    if (!audio_buffers.empty())
      buffer_queue_map.insert(std::make_pair(audio_id_, audio_buffers));
    const auto& video_buffers =
        StringToBufferQueue(video_timestamps, video_id_, DemuxerStream::VIDEO);
    if (!video_buffers.empty())
      buffer_queue_map.insert(std::make_pair(video_id_, video_buffers));
    return frame_processor_->ProcessFrames(
        buffer_queue_map, append_window_start_, append_window_end_,
        &timestamp_offset_);
  }

  // Compares |expected| to the buffered ranges of |stream| formatted into a
  // string as follows:
  //
  // If no ranges: "{ }"
  // If one range: "{ [start1,end1) }"
  // If multiple ranges, they are added space-delimited in sequence, like:
  // "{ [start1,end1) [start2,end2) }"
  //
  // startN and endN are the respective buffered start and end times of the
  // range in integer milliseconds.
  void CheckExpectedRangesByTimestamp(ChunkDemuxerStream* stream,
                                      const std::string& expected) {
    // Note, DemuxerStream::TEXT streams return [0,duration (==infinity here))
    Ranges<base::TimeDelta> r = stream->GetBufferedRanges(kInfiniteDuration);

    std::stringstream ss;
    ss << "{ ";
    for (size_t i = 0; i < r.size(); ++i) {
      int64_t start = r.start(i).InMilliseconds();
      int64_t end = r.end(i).InMilliseconds();
      ss << "[" << start << "," << end << ") ";
    }
    ss << "}";
    EXPECT_EQ(expected, ss.str());
  }

  void CheckReadStalls(ChunkDemuxerStream* stream) {
    int loop_count = 0;

    do {
      read_callback_called_ = false;
      stream->Read(base::BindOnce(&FrameProcessorTest::StoreStatusAndBuffer,
                                  base::Unretained(this)));
      base::RunLoop().RunUntilIdle();
    } while (++loop_count < 2 && read_callback_called_ &&
             last_read_status_ == DemuxerStream::kAborted);

    ASSERT_FALSE(read_callback_called_ &&
                 last_read_status_ == DemuxerStream::kAborted)
        << "2 kAborted reads in a row. Giving up.";
    EXPECT_FALSE(read_callback_called_);
  }

  // Format of |expected| is a space-delimited sequence of
  // timestamp_in_ms:original_timestamp_in_ms
  // original_timestamp_in_ms (and the colon) must be omitted if it is the same
  // as timestamp_in_ms.
  void CheckReadsThenReadStalls(ChunkDemuxerStream* stream,
                                const std::string& expected) {
    std::vector<std::string> timestamps = base::SplitString(
        expected, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::stringstream ss;
    for (size_t i = 0; i < timestamps.size(); ++i) {
      int loop_count = 0;

      do {
        read_callback_called_ = false;
        stream->Read(base::BindOnce(&FrameProcessorTest::StoreStatusAndBuffer,
                                    base::Unretained(this)));
        base::RunLoop().RunUntilIdle();
        EXPECT_TRUE(read_callback_called_);
      } while (++loop_count < 2 &&
               last_read_status_ == DemuxerStream::kAborted);

      ASSERT_FALSE(last_read_status_ == DemuxerStream::kAborted)
          << "2 kAborted reads in a row. Giving up.";
      EXPECT_EQ(DemuxerStream::kOk, last_read_status_);
      EXPECT_FALSE(last_read_buffer_->end_of_stream());

      if (i > 0)
        ss << " ";

      int time_in_ms = last_read_buffer_->timestamp().InMilliseconds();
      ss << time_in_ms;

      // Decode the original_time_in_ms from the buffer's data.
      double original_time_in_ms;
      ASSERT_EQ(sizeof(original_time_in_ms), last_read_buffer_->data_size());
      original_time_in_ms = *(reinterpret_cast<const double*>(
          last_read_buffer_->data()));
      if (original_time_in_ms != time_in_ms)
        ss << ":" << original_time_in_ms;

      // Detect full-discard preroll buffer.
      if (last_read_buffer_->discard_padding().first == kInfiniteDuration &&
          last_read_buffer_->discard_padding().second.is_zero()) {
        ss << "P";
      }
    }

    EXPECT_EQ(expected, ss.str());
    CheckReadStalls(stream);
  }

  // TODO(wolenetz): Refactor to instead verify the expected signalling or lack
  // thereof of new coded frame group by the FrameProcessor. See
  // https://crbug.com/580613.
  bool in_coded_frame_group() {
    return !frame_processor_->pending_notify_all_group_start_;
  }

  void SeekStream(ChunkDemuxerStream* stream, base::TimeDelta seek_time) {
    stream->AbortReads();
    stream->Seek(seek_time);
    stream->StartReturningData();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockMediaLog> media_log_;
  StrictMock<FrameProcessorTestCallbackHelper> callbacks_;

  bool use_sequence_mode_;

  std::unique_ptr<FrameProcessor> frame_processor_;
  base::TimeDelta append_window_start_;
  base::TimeDelta append_window_end_;
  base::TimeDelta timestamp_offset_;
  base::TimeDelta frame_duration_;
  std::unique_ptr<ChunkDemuxerStream> audio_;
  std::unique_ptr<ChunkDemuxerStream> video_;
  const TrackId audio_id_;
  const TrackId video_id_;
  const BufferQueue empty_queue_;

  // StoreStatusAndBuffer's most recent result.
  DemuxerStream::Status last_read_status_;
  scoped_refptr<DecoderBuffer> last_read_buffer_;
  bool read_callback_called_;

 private:
  void StoreStatusAndBuffer(DemuxerStream::Status status,
                            scoped_refptr<DecoderBuffer> buffer) {
    if (status == DemuxerStream::kOk && buffer.get()) {
      DVLOG(3) << __func__ << "status: " << status
               << " ts: " << buffer->timestamp().InSecondsF();
    } else {
      DVLOG(3) << __func__ << "status: " << status << " ts: n/a";
    }

    read_callback_called_ = true;
    last_read_status_ = status;
    last_read_buffer_ = buffer;
  }

  void CreateAndConfigureStream(DemuxerStream::Type type,
                                bool setup_observers) {
    // TODO(wolenetz/dalecurtis): Also test with splicing disabled?

    ChunkDemuxerStream* stream;
    switch (type) {
      case DemuxerStream::AUDIO: {
        ASSERT_FALSE(audio_);
        audio_.reset(
            new ChunkDemuxerStream(DemuxerStream::AUDIO, MediaTrack::Id("1")));
        AudioDecoderConfig decoder_config(
            kCodecVorbis, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 1000,
            EmptyExtraData(), EncryptionScheme::kUnencrypted);
        frame_processor_->OnPossibleAudioConfigUpdate(decoder_config);
        ASSERT_TRUE(
            audio_->UpdateAudioConfig(decoder_config, false, &media_log_));

        stream = audio_.get();
        break;
      }
      case DemuxerStream::VIDEO: {
        ASSERT_FALSE(video_);
        video_.reset(
            new ChunkDemuxerStream(DemuxerStream::VIDEO, MediaTrack::Id("2")));
        ASSERT_TRUE(video_->UpdateVideoConfig(TestVideoConfig::Normal(), false,
                                              &media_log_));
        stream = video_.get();
        break;
      }
      // TODO(wolenetz): Test text coded frame processing.
      case DemuxerStream::TEXT:
      case DemuxerStream::UNKNOWN: {
        ASSERT_FALSE(true);
      }
    }

    if (setup_observers) {
      stream->set_append_observer_for_testing(
          base::BindRepeating(&FrameProcessorTestCallbackHelper::OnAppend,
                              base::Unretained(&callbacks_), type));
      stream->set_group_start_observer_for_testing(
          base::BindRepeating(&FrameProcessorTestCallbackHelper::OnGroupStart,
                              base::Unretained(&callbacks_), type));
    }
  }

  DISALLOW_COPY_AND_ASSIGN(FrameProcessorTest);
};

TEST_P(FrameProcessorTest, WrongTypeInAppendedBuffer) {
  AddTestTracks(HAS_AUDIO);
  EXPECT_FALSE(in_coded_frame_group());

  StreamParser::BufferQueueMap buffer_queue_map;
  const auto& audio_buffers =
      StringToBufferQueue("0K", audio_id_, DemuxerStream::VIDEO);
  buffer_queue_map.insert(std::make_pair(audio_id_, audio_buffers));
  EXPECT_MEDIA_LOG(FrameTypeMismatchesTrackType("video", "1"));
  ASSERT_FALSE(
      frame_processor_->ProcessFrames(buffer_queue_map, append_window_start_,
                                      append_window_end_, &timestamp_offset_));
  EXPECT_FALSE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ }");
  CheckReadStalls(audio_.get());
}

TEST_P(FrameProcessorTest, NonMonotonicallyIncreasingTimestampInOneCall) {
  AddTestTracks(HAS_AUDIO);

  EXPECT_MEDIA_LOG(ParsedBuffersNotInDTSSequence());
  EXPECT_FALSE(ProcessFrames("10K 0K", ""));
  EXPECT_FALSE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ }");
  CheckReadStalls(audio_.get());
}

TEST_P(FrameProcessorTest, AudioOnly_SingleFrame) {
  // Tests A: P(A) -> (a)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("0K", ""));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,10) }");
  CheckReadsThenReadStalls(audio_.get(), "0");
}

TEST_P(FrameProcessorTest, VideoOnly_SingleFrame) {
  // Tests V: P(V) -> (v)
  InSequence s;
  AddTestTracks(HAS_VIDEO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("", "0K"));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,10) }");
  CheckReadsThenReadStalls(video_.get(), "0");
}

TEST_P(FrameProcessorTest, AudioOnly_TwoFrames) {
  // Tests A: P(A0, A10) -> (a0, a10)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  EXPECT_TRUE(ProcessFrames("0K 10K", ""));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0 10");
}

TEST_P(FrameProcessorTest, AudioOnly_SetOffsetThenSingleFrame) {
  // Tests A: STSO(50)+P(A0) -> TSO==50,(a0@50)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  SetTimestampOffset(Milliseconds(50));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(60)));
  EXPECT_TRUE(ProcessFrames("0K", ""));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(50), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [50,60) }");

  // We do not stall on reading without seeking to 50ms due to
  // SourceBufferStream::kSeekToStartFudgeRoom().
  CheckReadsThenReadStalls(audio_.get(), "50:0");
}

TEST_P(FrameProcessorTest, AudioOnly_SetOffsetThenFrameTimestampBelowOffset) {
  // Tests A: STSO(50)+P(A20) ->
  //   if sequence mode: TSO==30,(a20@50)
  //   if segments mode: TSO==50,(a20@70)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  SetTimestampOffset(Milliseconds(50));

  if (use_sequence_mode_) {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(60)));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(80)));
  }

  EXPECT_TRUE(ProcessFrames("20K", ""));
  EXPECT_TRUE(in_coded_frame_group());

  // We do not stall on reading without seeking to 50ms / 70ms due to
  // SourceBufferStream::kSeekToStartFudgeRoom().
  if (use_sequence_mode_) {
    EXPECT_EQ(Milliseconds(30), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [50,60) }");
    CheckReadsThenReadStalls(audio_.get(), "50:20");
  } else {
    EXPECT_EQ(Milliseconds(50), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [70,80) }");
    CheckReadsThenReadStalls(audio_.get(), "70:20");
  }
}

TEST_P(FrameProcessorTest, AudioOnly_SequentialProcessFrames) {
  // Tests A: P(A0,A10)+P(A20,A30) -> (a0,a10,a20,a30)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  EXPECT_TRUE(ProcessFrames("0K 10K", ""));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(40)));
  EXPECT_TRUE(ProcessFrames("20K 30K", ""));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");

  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest, AudioOnly_NonSequentialProcessFrames) {
  // Tests A: P(A20,A30)+P(A0,A10) ->
  //   if sequence mode: TSO==-20 after first P(), 20 after second P(), and
  //                     a(20@0,a30@10,a0@20,a10@30)
  //   if segments mode: TSO==0,(a0,a10,a20,a30)
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(40)));
  }

  EXPECT_TRUE(ProcessFrames("20K 30K", ""));
  EXPECT_TRUE(in_coded_frame_group());

  if (use_sequence_mode_) {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
    EXPECT_EQ(Milliseconds(-20), timestamp_offset_);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(40)));
  } else {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [20,40) }");
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  }

  EXPECT_TRUE(ProcessFrames("0K 10K", ""));
  EXPECT_TRUE(in_coded_frame_group());

  if (use_sequence_mode_) {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");
    EXPECT_EQ(Milliseconds(20), timestamp_offset_);
    CheckReadsThenReadStalls(audio_.get(), "0:20 10:30 20:0 30:10");
  } else {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    // Re-seek to 0ms now that we've appended data earlier than what has already
    // satisfied our initial seek to start, above.
    SeekStream(audio_.get(), Milliseconds(0));
    CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
  }
}

TEST_P(FrameProcessorTest, AudioVideo_SequentialProcessFrames) {
  // Tests AV: P(A0,A10;V0k,V10,V20)+P(A20,A30,A40,V30) ->
  //   (a0,a10,a20,a30,a40);(v0,v10,v20,v30)
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  if (use_sequence_mode_) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_,
                OnParseWarning(SourceBufferParseWarning::kMuxedSequenceMode));
    EXPECT_MEDIA_LOG(MuxedSequenceModeWarning());
  }

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(30)));
  EXPECT_TRUE(ProcessFrames("0K 10K", "0K 10 20"));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,30) }");

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(50)));
  EXPECT_TRUE(ProcessFrames("20K 30K 40K", "30"));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,50) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,40) }");

  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30 40");
  CheckReadsThenReadStalls(video_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest, AudioVideo_Discontinuity) {
  // Tests AV: P(A0,A10,A30,A40,A50;V0key,V10,V40,V50key) ->
  //   if sequence mode: TSO==10,(a0,a10,a30,a40,a50@60);(v0,v10,v50@60)
  //   if segments mode: TSO==0,(a0,a10,a30,a40,a50);(v0,v10,v50)
  // This assumes A40K is processed before V40, which depends currently on
  // MergeBufferQueues() behavior.
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  if (use_sequence_mode_) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_,
                OnParseWarning(SourceBufferParseWarning::kMuxedSequenceMode));
    EXPECT_MEDIA_LOG(MuxedSequenceModeWarning());
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(70)));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(60)));
  }

  EXPECT_TRUE(ProcessFrames("0K 10K 30K 40K 50K", "0K 10 40 50K"));
  EXPECT_TRUE(in_coded_frame_group());

  if (use_sequence_mode_) {
    EXPECT_EQ(Milliseconds(10), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,70) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [0,20) [60,70) }");
    CheckReadsThenReadStalls(audio_.get(), "0 10 30 40 60:50");
    CheckReadsThenReadStalls(video_.get(), "0 10");
    SeekStream(video_.get(), Milliseconds(60));
    CheckReadsThenReadStalls(video_.get(), "60:50");
  } else {
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,60) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [0,20) [50,60) }");
    CheckReadsThenReadStalls(audio_.get(), "0 10 30 40 50");
    CheckReadsThenReadStalls(video_.get(), "0 10");
    SeekStream(video_.get(), Milliseconds(50));
    CheckReadsThenReadStalls(video_.get(), "50");
  }
}

TEST_P(FrameProcessorTest, AudioVideo_Discontinuity_TimestampOffset) {
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);
  if (use_sequence_mode_) {
    EXPECT_CALL(callbacks_,
                OnParseWarning(SourceBufferParseWarning::kMuxedSequenceMode));
    EXPECT_MEDIA_LOG(MuxedSequenceModeWarning());
  }

  // Start a coded frame group at time 100ms. Note the jagged start still uses
  // the coded frame group's start time as the range start for both streams.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(140)));
  SetTimestampOffset(Milliseconds(100));
  EXPECT_TRUE(ProcessFrames("0K 10K 20K", "10K 20K 30K"));
  EXPECT_EQ(Milliseconds(100), timestamp_offset_);
  EXPECT_TRUE(in_coded_frame_group());
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [100,130) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) }");

  // Test the behavior of both 'sequence' and 'segments' mode if the coded frame
  // sequence jumps forward beyond the normal discontinuity threshold.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(240)));
  SetTimestampOffset(Milliseconds(200));
  EXPECT_TRUE(ProcessFrames("0K 10K 20K", "10K 20K 30K"));
  EXPECT_EQ(Milliseconds(200), timestamp_offset_);
  EXPECT_TRUE(in_coded_frame_group());
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [100,130) [200,230) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) [200,240) }");

  // Test the behavior when timestampOffset adjustment causes next frames to be
  // in the past relative to the previously processed frame and triggers a new
  // coded frame group.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(95)));
  SetTimestampOffset(Milliseconds(55));
  EXPECT_TRUE(ProcessFrames("0K 10K 20K", "10K 20K 30K"));
  EXPECT_EQ(Milliseconds(55), timestamp_offset_);
  EXPECT_TRUE(in_coded_frame_group());
  // The new audio range is not within SourceBufferStream's coalescing threshold
  // relative to the next range, but the new video range is within the
  // threshold.
  CheckExpectedRangesByTimestamp(audio_.get(),
                                 "{ [55,85) [100,130) [200,230) }");
  // Note that the range adjacency logic used in this case considers
  // DTS 85 to be close enough to [100,140), even though the first DTS in video
  // range [100,140) is actually 110. The muxed data started a coded frame
  // group at time 100, informing the adjacency logic.
  CheckExpectedRangesByTimestamp(video_.get(), "{ [55,140) [200,240) }");

  // Verify the buffers.
  // Re-seek now that we've appended data earlier than what already satisfied
  // our initial seek to start.
  SeekStream(audio_.get(), Milliseconds(55));
  CheckReadsThenReadStalls(audio_.get(), "55:0 65:10 75:20");
  SeekStream(audio_.get(), Milliseconds(100));
  CheckReadsThenReadStalls(audio_.get(), "100:0 110:10 120:20");
  SeekStream(audio_.get(), Milliseconds(200));
  CheckReadsThenReadStalls(audio_.get(), "200:0 210:10 220:20");

  SeekStream(video_.get(), Milliseconds(55));
  CheckReadsThenReadStalls(video_.get(),
                           "65:10 75:20 85:30 110:10 120:20 130:30");
  SeekStream(video_.get(), Milliseconds(200));
  CheckReadsThenReadStalls(video_.get(), "210:10 220:20 230:30");
}

TEST_P(FrameProcessorTest, AudioVideo_OutOfSequence_After_Discontinuity) {
  // Once a discontinuity is detected (and all tracks drop everything until the
  // next keyframe per each track), we should gracefully handle the case where
  // some tracks' first keyframe after the discontinuity are appended after, but
  // end up earlier in timeline than some other track(s). In particular, we
  // shouldn't notify all tracks that a new coded frame group is starting and
  // begin dropping leading non-keyframes from all tracks.  Rather, we should
  // notify just the track encountering this new type of discontinuity.  Since
  // MSE doesn't require all media segments to contain media from every track,
  // these append sequences can occur.
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Begin with a simple set of appends for all tracks.
  if (use_sequence_mode_) {
    // Allow room in the timeline for the last audio append (50K, below) in this
    // test to remain within default append window [0, +Infinity]. Moving the
    // sequence mode appends to begin at time 100ms, the same time as the first
    // append, below, results in a -20ms offset (instead of a -120ms offset)
    // applied to frames beginning at the first frame after the discontinuity
    // caused by the video append at 160K, below.
    SetTimestampOffset(Milliseconds(100));
    EXPECT_CALL(callbacks_,
                OnParseWarning(SourceBufferParseWarning::kMuxedSequenceMode));
    EXPECT_MEDIA_LOG(MuxedSequenceModeWarning());
  }
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(140)));
  EXPECT_TRUE(ProcessFrames("100K 110K 120K", "110K 120K 130K"));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [100,130) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) }");

  // Trigger (normal) discontinuity with one track (video).
  if (use_sequence_mode_)
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(150)));
  else
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(170)));

  EXPECT_TRUE(ProcessFrames("", "160K"));
  EXPECT_TRUE(in_coded_frame_group());

  if (use_sequence_mode_) {
    // The new video buffer is relocated into [140,150).
    EXPECT_EQ(Milliseconds(-20), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [100,130) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [100,150) }");
  } else {
    // The new video buffer is at [160,170).
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [100,130) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) [160,170) }");
  }

  // Append to the other track (audio) with lower time than the video frame we
  // just appended. Append with a timestamp such that segments mode demonstrates
  // we don't retroactively extend the new video buffer appended above's range
  // start back to this audio start time.
  if (use_sequence_mode_)
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(150)));
  else
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(170)));

  EXPECT_TRUE(ProcessFrames("50K", ""));
  EXPECT_TRUE(in_coded_frame_group());

  // Because this is the first audio buffer appended following the discontinuity
  // detected while appending the video frame, above, a new coded frame group
  // for video is not triggered.
  if (use_sequence_mode_) {
    // The new audio buffer is relocated into [30,40). Note the muxed 'sequence'
    // mode append mode results in a buffered range gap in this case.
    EXPECT_EQ(Milliseconds(-20), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [30,40) [100,130) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [100,150) }");
  } else {
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [50,60) [100,130) }");
    CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) [160,170) }");
  }

  // Finally, append a non-keyframe to the first track (video), to continue the
  // GOP that started the normal discontinuity on the previous video append.
  if (use_sequence_mode_)
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(160)));
  else
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(180)));

  EXPECT_TRUE(ProcessFrames("", "170"));
  EXPECT_TRUE(in_coded_frame_group());

  // Verify the final buffers. First, re-seek audio since we appended data
  // earlier than what already satisfied our initial seek to start. We satisfy
  // the seek with the first buffer in [0,1000).
  SeekStream(audio_.get(), Milliseconds(0));
  if (use_sequence_mode_) {
    // The new video buffer is relocated into [150,160).
    EXPECT_EQ(Milliseconds(-20), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [30,40) [100,130) }");
    CheckReadsThenReadStalls(audio_.get(), "30:50");
    SeekStream(audio_.get(), Milliseconds(100));
    CheckReadsThenReadStalls(audio_.get(), "100 110 120");

    CheckExpectedRangesByTimestamp(video_.get(), "{ [100,160) }");
    CheckReadsThenReadStalls(video_.get(), "110 120 130 140:160 150:170");
  } else {
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [50,60) [100,130) }");
    CheckReadsThenReadStalls(audio_.get(), "50");
    SeekStream(audio_.get(), Milliseconds(100));
    CheckReadsThenReadStalls(audio_.get(), "100 110 120");

    CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) [160,180) }");
    CheckReadsThenReadStalls(video_.get(), "110 120 130");
    SeekStream(video_.get(), Milliseconds(160));
    CheckReadsThenReadStalls(video_.get(), "160 170");
  }
}

TEST_P(FrameProcessorTest,
       AppendWindowFilterOfNegativeBufferTimestampsWithPrerollDiscard) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  SetTimestampOffset(Milliseconds(-20));
  EXPECT_MEDIA_LOG(DroppedFrame("audio", -20000));
  EXPECT_MEDIA_LOG(DroppedFrame("audio", -10000));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("0K 10K 20K", ""));
  EXPECT_TRUE(in_coded_frame_group());
  EXPECT_EQ(Milliseconds(-20), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,10) }");
  CheckReadsThenReadStalls(audio_.get(), "0:10P 0:20");
}

TEST_P(FrameProcessorTest, AppendWindowFilterWithInexactPreroll) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);
  SetTimestampOffset(Milliseconds(-10));
  EXPECT_MEDIA_LOG(DroppedFrame("audio", -10000));
  EXPECT_MEDIA_LOG(TruncatedFrame(-250, 9750, "start", 0));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  EXPECT_TRUE(ProcessFrames("0K 9.75K 20K", ""));
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0P 0:9.75 10:20");
}

TEST_P(FrameProcessorTest, AppendWindowFilterWithInexactPreroll_2) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);
  SetTimestampOffset(Milliseconds(-10));

  EXPECT_MEDIA_LOG(DroppedFrame("audio", -10000));
  // Splice trimming checks are done on every audio frame following either a
  // discontinuity or the beginning of ProcessFrames(), and are also done on
  // audio frames with PTS not directly continuous with the highest frame end
  // PTS already processed.
  if (use_sequence_mode_)
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(-10)));
  else
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(0)));
  EXPECT_TRUE(ProcessFrames("0K", ""));

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(
                              base::TimeDelta::FromMicroseconds(10250)));
  EXPECT_TRUE(ProcessFrames("10.25K", ""));

  EXPECT_MEDIA_LOG(SkippingSpliceTooLittleOverlap(10000, 250));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  EXPECT_TRUE(ProcessFrames("20K", ""));

  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0P 0:10.25 10:20");
}

TEST_P(FrameProcessorTest, AllowNegativeFramePTSAndDTSBeforeOffsetAdjustment) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(30)));
  } else {
    EXPECT_MEDIA_LOG(TruncatedFrame(-5000, 5000, "start", 0));
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(25)));
  }

  EXPECT_TRUE(ProcessFrames("-5K 5K 15K", ""));

  if (use_sequence_mode_) {
    EXPECT_EQ(Milliseconds(5), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,30) }");
    CheckReadsThenReadStalls(audio_.get(), "0:-5 10:5 20:15");
  } else {
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,25) }");
    CheckReadsThenReadStalls(audio_.get(), "0:-5 5 15");
  }
}

TEST_P(FrameProcessorTest, PartialAppendWindowFilterNoDiscontinuity) {
  // Tests that spurious discontinuity is not introduced by a partially
  // trimmed frame.
  append_window_start_ = Milliseconds(7);

  InSequence s;
  AddTestTracks(HAS_AUDIO);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);
  EXPECT_MEDIA_LOG(TruncatedFrame(0, 10000, "start", 7000));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(29)));

  EXPECT_TRUE(ProcessFrames("0K 19K", ""));

  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [7,29) }");
  CheckReadsThenReadStalls(audio_.get(), "7:0 19");
}

TEST_P(FrameProcessorTest,
       PartialAppendWindowFilterNoDiscontinuity_DtsAfterPts) {
  // Tests that spurious discontinuity is not introduced by a partially trimmed
  // frame that originally had DTS > PTS.
  InSequence s;
  AddTestTracks(HAS_AUDIO);

  if (use_sequence_mode_) {
    frame_processor_->SetSequenceMode(true);
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  } else {
    EXPECT_MEDIA_LOG(TruncatedFrame(-7000, 3000, "start", 0));
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(13)));
  }

  // Process a sequence of two audio frames:
  // A: PTS -7ms, DTS 10ms, duration 10ms, keyframe
  // B: PTS  3ms, DTS 20ms, duration 10ms, keyframe
  EXPECT_TRUE(ProcessFrames("-7|10K 3|20K", ""));

  if (use_sequence_mode_) {
    // Sequence mode detected that frame A needs to be relocated 7ms into the
    // future to begin the sequence at time 0. There is no append window
    // filtering because the PTS result of the relocation is within the append
    // window of [0,+Infinity).
    // Frame A is relocated by 7 to PTS 0, DTS 17, duration 10.
    // Frame B is relocated by 7 to PTS 10, DTS 27, duration 10.
    EXPECT_EQ(Milliseconds(7), timestamp_offset_);

    // Start of frame A (0) through end of frame B (10+10).
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");

    // Frame A is now at PTS 0 (originally at PTS -7)
    // Frame B is now at PTS 10 (originally at PTS 3)
    CheckReadsThenReadStalls(audio_.get(), "0:-7 10:3");
  } else {
    // Segments mode does not update timestampOffset automatically, so it
    // remained 0 and neither frame was relocated by timestampOffset.
    // Frame A's start *was* relocated by append window partial audio cropping:
    // Append window filtering (done by PTS, regardless of range buffering API)
    // did a partial crop of the first 7ms of frame A which was before
    // the default append window start time 0, and moved both the PTS and DTS of
    // frame A forward by 7 and reduced its duration by 7. Frame B was fully
    // inside the append window and remained uncropped and unrelocated.
    // Frame A is buffered at PTS -7+7=0, DTS 10+7=17, duration 10-7=3.
    // Frame B is buffered at PTS 3, DTS 20, duration 10.
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);

    // Start of frame A (0) through end of frame B (3+10).
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,13) }");

    // Frame A is now at PTS 0 (originally at PTS -7)
    // Frame B is now at PTS 3 (same as it was originally)
    CheckReadsThenReadStalls(audio_.get(), "0:-7 3");
  }
}

TEST_P(FrameProcessorTest, PartialAppendWindowFilterNoNewMediaSegment) {
  // Tests that a new media segment is not forcibly signalled for audio frame
  // partial front trim, to prevent incorrect introduction of a discontinuity
  // and potentially a non-keyframe video frame to be processed next after the
  // discontinuity.
  InSequence s;
  AddTestTracks(HAS_AUDIO | HAS_VIDEO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);
  if (use_sequence_mode_) {
    EXPECT_CALL(callbacks_,
                OnParseWarning(SourceBufferParseWarning::kMuxedSequenceMode));
    EXPECT_MEDIA_LOG(MuxedSequenceModeWarning());
  }
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("", "0K"));
  EXPECT_MEDIA_LOG(TruncatedFrame(-5000, 5000, "start", 0));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("-5K", ""));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  EXPECT_TRUE(ProcessFrames("", "10"));

  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  EXPECT_TRUE(in_coded_frame_group());
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,5) }");
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0:-5");
  CheckReadsThenReadStalls(video_.get(), "0 10");
}

TEST_P(FrameProcessorTest, AudioOnly_SequenceModeContinuityAcrossReset) {
  if (!use_sequence_mode_) {
    DVLOG(1) << "Skipping segments mode variant; inapplicable to this case.";
    return;
  }

  InSequence s;
  AddTestTracks(HAS_AUDIO);
  frame_processor_->SetSequenceMode(true);
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("0K", ""));
  frame_processor_->Reset();
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(20)));
  EXPECT_TRUE(ProcessFrames("100K", ""));

  EXPECT_EQ(Milliseconds(-90), timestamp_offset_);
  EXPECT_TRUE(in_coded_frame_group());
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,20) }");
  CheckReadsThenReadStalls(audio_.get(), "0 10:100");
}

TEST_P(FrameProcessorTest, PartialAppendWindowZeroDurationPreroll) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  append_window_start_ = Milliseconds(5);

  EXPECT_MEDIA_LOG(DroppedFrame("audio", use_sequence_mode_ ? 0 : 4000));
  // Append a 0 duration frame that falls just before the append window.
  frame_duration_ = Milliseconds(0);
  EXPECT_FALSE(in_coded_frame_group());
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(0)));
  EXPECT_TRUE(ProcessFrames("4K", ""));
  // Verify buffer is not part of ranges. It should be silently saved for
  // preroll for future append.
  CheckExpectedRangesByTimestamp(audio_.get(), "{ }");
  CheckReadsThenReadStalls(audio_.get(), "");
  EXPECT_FALSE(in_coded_frame_group());

  // Abort the reads from last stall. We don't want those reads to "complete"
  // when we append below. We will initiate new reads to confirm the buffer
  // looks as we expect.
  SeekStream(audio_.get(), Milliseconds(0));

  if (use_sequence_mode_) {
    EXPECT_MEDIA_LOG(TruncatedFrame(0, 10000, "start", 5000));
  } else {
    EXPECT_MEDIA_LOG(TruncatedFrame(4000, 14000, "start", 5000));
  }
  // Append a frame with 10ms duration, with 9ms falling after the window start.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(
                              Milliseconds(use_sequence_mode_ ? 10 : 14)));
  frame_duration_ = Milliseconds(10);
  EXPECT_TRUE(ProcessFrames("4K", ""));
  EXPECT_TRUE(in_coded_frame_group());

  // Verify range updated to reflect last append was processed and trimmed, and
  // also that zero duration buffer was saved and attached as preroll.
  if (use_sequence_mode_) {
    // For sequence mode, append window trimming is applied after the append
    // is adjusted for timestampOffset. Basically, everything gets rebased to 0
    // and trimming then removes 5 seconds from the front.
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [5,10) }");
    CheckReadsThenReadStalls(audio_.get(), "5:4P 5:4");
  } else {  // segments mode
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [5,14) }");
    CheckReadsThenReadStalls(audio_.get(), "5:4P 5:4");
  }

  // Verify the preroll buffer still has zero duration.
  StreamParserBuffer* last_read_parser_buffer =
      static_cast<StreamParserBuffer*>(last_read_buffer_.get());
  ASSERT_EQ(Milliseconds(0),
            last_read_parser_buffer->preroll_buffer()->duration());
}

TEST_P(FrameProcessorTest,
       OOOKeyframePrecededByDependantNonKeyframeShouldWarn) {
  InSequence s;
  AddTestTracks(HAS_VIDEO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  if (use_sequence_mode_) {
    // Allow room in the timeline for the last video append (40|70, below) in
    // this test to remain within default append window [0, +Infinity]. Moving
    // the sequence mode appends to begin at time 50ms, the same time as the
    // first append, below, also results in identical expectation checks for
    // buffered ranges and buffer reads for both segments and sequence modes.
    SetTimestampOffset(Milliseconds(50));
  }

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(70)));
  EXPECT_TRUE(ProcessFrames("", "50K 60"));

  CheckExpectedRangesByTimestamp(video_.get(), "{ [50,70) }");

  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.05", "0.04"));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(70)));
  EXPECT_TRUE(ProcessFrames("", "40|70"));  // PTS=40, DTS=70

  // This reflects the expectation that PTS start is not "pulled backward" for
  // the new frame at PTS=40 because current spec doesn't support SAP Type 2; it
  // has no steps in the coded frame processing algorithm that would do that
  // "pulling backward". See https://github.com/w3c/media-source/issues/187.
  CheckExpectedRangesByTimestamp(video_.get(), "{ [50,70) }");

  SeekStream(video_.get(), Milliseconds(0));
  CheckReadsThenReadStalls(video_.get(), "50 60 40");
}

TEST_P(FrameProcessorTest, OOOKeyframePts_1) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(1010)));
  // Note that the following does not contain a DTS continuity, but *does*
  // contain a PTS discontinuity (keyframe at 0.1s after keyframe at 1s).
  EXPECT_TRUE(ProcessFrames("0K 1000|10K 100|20K", ""));

  // Force sequence mode to place the next frames where segments mode would put
  // them, to simplify this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(500));

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(510)));
  EXPECT_TRUE(ProcessFrames("500|100K", ""));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  // Note that the PTS discontinuity (100ms) in the first ProcessFrames() call,
  // above, overlaps the previously buffered range [0,1010), so the frame at
  // 100ms is processed with an adjusted coded frame group start to be 0.001ms,
  // which is just after the highest timestamp before it in the overlapped
  // range. This enables it to be continuous with the frame before it. The
  // remainder of the overlapped range (the buffer at [1000,1010)) is adjusted
  // to have a range start time at the split point (110), and is within fudge
  // room and merged into [0,110). The same happens with the buffer appended
  // [500,510).
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,1010) }");
  CheckReadsThenReadStalls(audio_.get(), "0 100 500 1000");
}

TEST_P(FrameProcessorTest, OOOKeyframePts_2) {
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(1010)));
  EXPECT_TRUE(ProcessFrames("0K 1000|10K", ""));

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(1010)));
  EXPECT_TRUE(ProcessFrames("100|20K", ""));

  // Note that the PTS discontinuity (100ms) in the first ProcessFrames() call,
  // above, overlaps the previously buffered range [0,1010), so the frame at
  // 100ms is processed with an adjusted coded frame group start to be 0.001ms,
  // which is just after the highest timestamp before it in the overlapped
  // range. This enables it to be continuous with the frame before it. The
  // remainder of the overlapped range (the buffer at [1000,1010)) is adjusted
  // to have a range start time at the split point (110), and is within fudge
  // room and merged into [0,110).
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,1010) }");
  CheckReadsThenReadStalls(audio_.get(), "0 100 1000");
}

TEST_P(FrameProcessorTest, AudioNonKeyframeChangedToKeyframe) {
  // Verifies that an audio non-keyframe is changed to a keyframe with a media
  // log warning. An exact overlap append of the preceding keyframe is also done
  // to ensure that the (original non-keyframe) survives (because it was changed
  // to a keyframe, so no longer depends on the original preceding keyframe).
  // The sequence mode test version uses SetTimestampOffset to make it behave
  // like segments mode to simplify the tests.
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  EXPECT_MEDIA_LOG(AudioNonKeyframe(10000, 10000));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(30)));
  EXPECT_TRUE(ProcessFrames("0K 10 20K", ""));

  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(0));

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("0K", ""));

  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,30) }");
  SeekStream(audio_.get(), Milliseconds(0));
  CheckReadsThenReadStalls(audio_.get(), "0 10 20");
}

TEST_P(FrameProcessorTest, TimestampOffsetNegativeDts) {
  // Shift a GOP earlier using timestampOffset such that the GOP
  // starts with negative DTS, but PTS 0.
  InSequence s;
  AddTestTracks(HAS_VIDEO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  if (!use_sequence_mode_) {
    // Simulate the offset that sequence mode would apply, to make the results
    // the same regardless of sequence vs segments mode.
    SetTimestampOffset(Milliseconds(-100));
  }

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(40)));
  EXPECT_TRUE(ProcessFrames("", "100|70K 130|80"));
  EXPECT_EQ(Milliseconds(-100), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,40) }");
  SeekStream(video_.get(), Milliseconds(0));
  CheckReadsThenReadStalls(video_.get(), "0:100 30:130");
}

TEST_P(FrameProcessorTest, LargeTimestampOffsetJumpForward) {
  // Verifies that jumps forward in buffers emitted from the coded frame
  // processing algorithm can create discontinuous buffered ranges if those
  // jumps are large enough, in both kinds of AppendMode.
  InSequence s;
  AddTestTracks(HAS_AUDIO);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10)));
  EXPECT_TRUE(ProcessFrames("0K", ""));

  SetTimestampOffset(Milliseconds(5000));

  // Along with the new timestampOffset set above, this should cause a large
  // jump forward in both PTS and DTS for both sequence and segments append
  // modes.
  if (use_sequence_mode_) {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(5010)));
  } else {
    EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(10010)));
  }
  EXPECT_TRUE(ProcessFrames("5000|100K", ""));
  if (use_sequence_mode_) {
    EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  } else {
    EXPECT_EQ(Milliseconds(5000), timestamp_offset_);
  }

  if (use_sequence_mode_) {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,10) [5000,5010) }");
    CheckReadsThenReadStalls(audio_.get(), "0");
    SeekStream(audio_.get(), Milliseconds(5000));
    CheckReadsThenReadStalls(audio_.get(), "5000");
  } else {
    CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,10) [10000,10010) }");
    CheckReadsThenReadStalls(audio_.get(), "0");
    SeekStream(audio_.get(), Milliseconds(10000));
    CheckReadsThenReadStalls(audio_.get(), "10000:5000");
  }
}

TEST_P(FrameProcessorTest, ContinuousDts_SapType2_and_PtsJumpForward) {
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(1060));

  // Note that the PTS of GOP non-keyframes earlier than the keyframe doesn't
  // modify the GOP start of the buffered range here. This may change if we
  // decide to improve spec for SAP Type 2 GOPs that begin a coded frame group.
  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(1060)));
  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("1.06", "1"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(1070)));
  EXPECT_TRUE(ProcessFrames(
      "", "1060|0K 1000|10 1050|20 1010|30 1040|40 1020|50 1030|60"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [1060,1070) }");

  // Process just the keyframe of the next SAP Type 2 GOP in decode continuity
  // with the previous one.
  // Note that this second GOP is buffered continuous with the first because
  // there is no decode discontinuity detected. This results in inclusion of
  // the significant PTS jump forward in the same continuous range.
  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(60)),
                   Milliseconds(1070)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(1140)));
  EXPECT_TRUE(ProcessFrames("", "1130|70K"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [1060,1140) }");

  // Process the remainder of the second GOP.
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(1140)));
  EXPECT_TRUE(
      ProcessFrames("", "1070|80 1120|90 1080|100 1110|110 1090|120 1100|130"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [1060,1140) }");

  // [1060,1140) should demux continuously without read stall in the middle.
  SeekStream(video_.get(), Milliseconds(1060));
  CheckReadsThenReadStalls(
      video_.get(),
      "1060 1000 1050 1010 1040 1020 1030 1130 1070 1120 1080 1110 1090 1100");
  // Verify that seek and read of the second GOP is correct.
  SeekStream(video_.get(), Milliseconds(1130));
  CheckReadsThenReadStalls(video_.get(), "1130 1070 1120 1080 1110 1090 1100");
}

TEST_P(FrameProcessorTest, ContinuousDts_NewGopEndOverlapsLastGop_1) {
  // API user might craft a continuous-in-DTS-with-previous-append GOP that has
  // PTS interval overlapping the previous append.
  // Tests SAP-Type-1 GOPs, where newly appended GOP overlaps a nonkeyframe of
  // the last GOP appended.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(100));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(100)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(140)));
  EXPECT_TRUE(ProcessFrames("", "100|0K 110|10 120|20 130|30"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(30)),
                   Milliseconds(125)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(165)));
  EXPECT_TRUE(ProcessFrames("", "125|40K 135|50 145|60 155|70"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,165) }");
  CheckReadsThenReadStalls(video_.get(), "100 110 120 125 135 145 155");
}

TEST_P(FrameProcessorTest, ContinuousDts_NewGopEndOverlapsLastGop_2) {
  // API user might craft a continuous-in-DTS-with-previous-append GOP that has
  // PTS interval overlapping the previous append.
  // Tests SAP-Type 1 GOPs, where newly appended GOP overlaps the keyframe of
  // the last GOP appended.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(100));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(100)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(140)));
  EXPECT_TRUE(ProcessFrames("", "100|0K 110|10 120|20K 130|30"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(30)),
                   Milliseconds(115)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  // TODO(wolenetz): Duration shouldn't be allowed to possibly increase to 140ms
  // here. See https://crbug.com/763620.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(140)));
  EXPECT_TRUE(ProcessFrames("", "115|40K 125|50"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,135) }");
  CheckReadsThenReadStalls(video_.get(), "100 110 115 125");
}

TEST_P(FrameProcessorTest, ContinuousDts_NewSap2GopEndOverlapsLastGop_1) {
  // API user might craft a continuous-in-DTS-with-previous-append GOP that has
  // PTS interval overlapping the previous append, using SAP Type 2 GOPs.
  // Tests SAP-Type 2 GOPs, where newly appended GOP overlaps nonkeyframes of
  // the last GOP appended.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(120));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(120)));
  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.12", "0.1"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(140)));
  EXPECT_TRUE(ProcessFrames("", "120|0K 100|10 130|20 110|30"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  // Note, we *don't* expect another OnGroupStart during the next ProcessFrames,
  // since the next GOP's keyframe PTS is after the first GOP and close enough
  // to be assured adjacent.
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(165)));
  EXPECT_TRUE(ProcessFrames("", "145|40K 125|50 155|60 135|70"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(video_.get(), "{ [120,165) }");
  // [120,165) should demux continuously without read stall in the middle.
  CheckReadsThenReadStalls(video_.get(), "120 100 130 110 145 125 155 135");
  // Verify that seek and read of the second GOP is correct.
  SeekStream(video_.get(), Milliseconds(145));
  CheckReadsThenReadStalls(video_.get(), "145 125 155 135");
}

TEST_P(FrameProcessorTest, ContinuousDts_NewSap2GopEndOverlapsLastGop_2) {
  // API user might craft a continuous-in-DTS-with-previous-append GOP that has
  // PTS interval overlapping the previous append, using SAP Type 2 GOPs.
  // Tests SAP-Type 2 GOPs, where newly appended GOP overlaps the keyframe of
  // last GOP appended.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(120));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(120)));
  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.12", "0.1"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  // There is a second GOP that is SAP-Type-2 within this first ProcessFrames,
  // with PTS jumping forward far enough to trigger group start signalling and a
  // flush.
  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(30)),
                   Milliseconds(140)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(180)));
  EXPECT_TRUE(ProcessFrames(
      "", "120|0K 100|10 130|20 110|30 160|40K 140|50 170|60 150|70"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(70)),
                   Milliseconds(155)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  // TODO(wolenetz): Duration shouldn't be allowed to possibly increase to 180ms
  // here. See https://crbug.com/763620.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(180)));
  EXPECT_TRUE(ProcessFrames("", "155|80K 145|90"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(video_.get(), "{ [120,165) }");
  // [120,165) should demux continuously without read stall in the middle.
  CheckReadsThenReadStalls(video_.get(), "120 100 130 110 155 145");
  // Verify seek and read of the second GOP is correct.
  SeekStream(video_.get(), Milliseconds(155));
  CheckReadsThenReadStalls(video_.get(), "155 145");
}

TEST_P(FrameProcessorTest,
       ContinuousDts_NewSap2GopEndOverlapsLastGop_3_GopByGop) {
  // API user might craft a continuous-in-DTS-with-previous-append GOP that has
  // PTS interval overlapping the previous append, using SAP Type 2 GOPs.  Tests
  // SAP-Type 2 GOPs, where newly appended GOP overlaps enough nonkeyframes of
  // the previous GOP such that dropped decode dependencies might cause problems
  // if the first nonkeyframe with PTS prior to the GOP's keyframe PTS is
  // flushed at the same time as its keyframe, but the second GOP's keyframe PTS
  // is close enough to the end of the first GOP's presentation interval to not
  // signal a new coded frame group start.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(500));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(500)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(530)));
  EXPECT_TRUE(ProcessFrames("", "500|0K 520|10 510|20"));
  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,530) }");

  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.54", "0.52"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(550)));
  EXPECT_TRUE(ProcessFrames("", "540|30K 520|40 530|50"));

  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,550) }");
  SeekStream(video_.get(), Milliseconds(500));
  CheckReadsThenReadStalls(video_.get(), "500 520 510 540 520 530");
}

TEST_P(FrameProcessorTest,
       ContinuousDts_NewSap2GopEndOverlapsLastGop_3_FrameByFrame) {
  // Tests that the buffered range results match the previous GopByGop test if
  // each frame of the second GOP is explicitly appended by the app
  // one-at-a-time.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(500));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(500)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(530)));
  EXPECT_TRUE(ProcessFrames("", "500|0K 520|10 510|20"));
  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,530) }");

  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(550)));
  EXPECT_TRUE(ProcessFrames("", "540|30K"));

  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.54", "0.52"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(550)));
  EXPECT_TRUE(ProcessFrames("", "520|40"));

  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(550)));
  EXPECT_TRUE(ProcessFrames("", "530|50"));

  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,550) }");
  SeekStream(video_.get(), Milliseconds(500));
  CheckReadsThenReadStalls(video_.get(), "500 520 510 540 520 530");
}

TEST_P(FrameProcessorTest,
       ContinuousDts_NewSap2GopEndOverlapsLastGop_4_GopByGop) {
  // API user might craft a continuous-in-DTS-with-previous-append GOP that has
  // PTS interval overlapping the previous append, using SAP Type 2 GOPs.  Tests
  // SAP-Type 2 GOPs, where newly appended GOP overlaps enough nonkeyframes of
  // the previous GOP such that dropped decode dependencies might cause problems
  // if the first nonkeyframe with PTS prior to the GOP's keyframe PTS is
  // flushed at the same time as its keyframe.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(500));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(500)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(530)));
  EXPECT_TRUE(ProcessFrames("", "500|0K 520|10 510|20"));
  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,530) }");

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(20)),
                   Milliseconds(530)));
  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.55", "0.52"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(560)));
  EXPECT_TRUE(ProcessFrames("", "550|30K 520|40 530|50 540|60"));

  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,560) }");
  SeekStream(video_.get(), Milliseconds(500));
  CheckReadsThenReadStalls(video_.get(), "500 520 510 550 520 530 540");
}

TEST_P(FrameProcessorTest,
       ContinuousDts_NewSap2GopEndOverlapsLastGop_4_FrameByFrame) {
  // Tests that the buffered range results match the previous GopByGop test if
  // each frame of the second GOP is explicitly appended by the app
  // one-at-a-time.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(500));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(500)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(530)));
  EXPECT_TRUE(ProcessFrames("", "500|0K 520|10 510|20"));
  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,530) }");

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(20)),
                   Milliseconds(530)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(560)));
  EXPECT_TRUE(ProcessFrames("", "550|30K"));

  EXPECT_CALL(callbacks_,
              OnParseWarning(
                  SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant));
  EXPECT_MEDIA_LOG(KeyframeTimeGreaterThanDependant("0.55", "0.52"));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(560)));
  EXPECT_TRUE(ProcessFrames("", "520|40"));

  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(560)));
  EXPECT_TRUE(ProcessFrames("", "530|50"));

  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(560)));
  EXPECT_TRUE(ProcessFrames("", "540|60"));

  CheckExpectedRangesByTimestamp(video_.get(), "{ [500,560) }");
  SeekStream(video_.get(), Milliseconds(500));
  CheckReadsThenReadStalls(video_.get(), "500 520 510 550 520 530 540");
}

TEST_P(FrameProcessorTest, ContinuousDts_GopKeyframePtsOrder_2_1_3) {
  // White-box test, demonstrating expected behavior for a specially crafted
  // sequence that "should" be unusual, but gracefully handled:
  // SAP-Type 1 GOPs for simplicity of test. First appended GOP is highest in
  // timeline. Second appended GOP is earliest in timeline. Third appended GOP
  // is continuous in time with highest end time of first appended GOP. The
  // result should be a single continuous range containing just the second and
  // third appended GOPs (since the first-appended GOP was overlap-removed from
  // the timeline due to being in the gap between the second and third appended
  // GOPs). Note that MseTrackBuffer::ResetHighestPresentationTimestamp() done
  // at the beginning of the second appended GOP is the key to gracefully
  // handling the third appended GOP.
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(200));

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::VIDEO, DecodeTimestamp(),
                                       Milliseconds(200)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(240)));
  EXPECT_TRUE(ProcessFrames("", "200|0K 210|10 220|20 230|30"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [200,240) }");

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(30)),
                   Milliseconds(100)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  // TODO(wolenetz): Duration shouldn't be allowed to possibly increase to 240ms
  // here. See https://crbug.com/763620.
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(240)));
  EXPECT_TRUE(ProcessFrames("", "100|40K 110|50 120|60 130|70"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,140) [200,240) }");

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(70)),
                   Milliseconds(140)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(260)));
  EXPECT_TRUE(ProcessFrames("", "240|80K 250|90"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [100,260) }");

  SeekStream(video_.get(), Milliseconds(100));
  CheckReadsThenReadStalls(video_.get(), "100 110 120 130 240 250");
}

TEST_P(FrameProcessorTest, ContinuousPts_DiscontinuousDts_AcrossGops) {
  // GOPs which overlap in DTS, but are continuous in PTS should be buffered
  // correctly. In particular, monotonic increase of DTS in continuous-in-PTS
  // append sequences is not required across GOPs (just within GOPs).
  InSequence s;
  AddTestTracks(HAS_VIDEO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  frame_processor_->SetSequenceMode(use_sequence_mode_);

  // Make the sequence mode buffering appear just like segments mode to simplify
  // this test case.
  if (use_sequence_mode_)
    SetTimestampOffset(Milliseconds(200));

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(200)),
                   Milliseconds(200)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(240)));
  EXPECT_TRUE(ProcessFrames("", "200K 210 220 230"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  CheckExpectedRangesByTimestamp(video_.get(), "{ [200,240) }");

  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::VIDEO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(225)),
                   Milliseconds(240)));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::VIDEO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(280)));
  // Append a second GOP whose first DTS is below the last DTS of the first GOP,
  // but whose PTS interval is continuous with the end of the first GOP.
  EXPECT_TRUE(ProcessFrames("", "240|225K 250|235 260|245 270|255"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);
  SeekStream(video_.get(), Milliseconds(200));

  CheckExpectedRangesByTimestamp(video_.get(), "{ [200,280) }");
  CheckReadsThenReadStalls(video_.get(), "200 210 220 230 240 250 260 270");
}

TEST_P(FrameProcessorTest, OnlyKeyframes_ContinuousDts_ContinousPts_1) {
  // Verifies that precisely one group start and one stream append occurs for a
  // single continuous set of frames.
  InSequence s;
  AddTestTracks(HAS_AUDIO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  // Default test frame duration is 10 milliseconds.

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::AUDIO, DecodeTimestamp(),
                                       base::TimeDelta()));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::AUDIO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(40)));
  EXPECT_TRUE(ProcessFrames("0K 10K 20K 30K", ""));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,40) }");
  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest, OnlyKeyframes_ContinuousDts_ContinuousPts_2) {
  // Verifies that precisely one group start and one stream append occurs while
  // processing a single continuous set of frames that uses fudge room to just
  // barely remain adjacent.
  InSequence s;
  AddTestTracks(HAS_AUDIO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  frame_duration_ = Milliseconds(5);

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::AUDIO, DecodeTimestamp(),
                                       base::TimeDelta()));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::AUDIO, _));
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(35)));
  EXPECT_TRUE(ProcessFrames("0K 10K 20K 30K", ""));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,35) }");
  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest,
       OnlyKeyframes_ContinuousDts_DiscontinuousPtsJustBeyondFudgeRoom) {
  // Verifies that multiple group starts and distinct appends occur
  // when processing a single DTS-continuous set of frames with PTS deltas that
  // just barely exceed the adjacency assumption in FrameProcessor.
  InSequence s;
  AddTestTracks(HAS_AUDIO | OBSERVE_APPENDS_AND_GROUP_STARTS);
  if (use_sequence_mode_)
    frame_processor_->SetSequenceMode(true);

  frame_duration_ = base::TimeDelta::FromMicroseconds(4999);

  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::AUDIO, DecodeTimestamp(),
                                       base::TimeDelta()));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::AUDIO, _));
  // Frame "10|5K" following "0K" triggers start of new group and eventual
  // append.
  EXPECT_CALL(callbacks_, OnGroupStart(DemuxerStream::AUDIO, DecodeTimestamp(),
                                       frame_duration_));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::AUDIO, _));

  // Frame "20|10K" following "10|5K" triggers start of new group and eventual
  // append.
  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::AUDIO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(5)),
                   Milliseconds(10) + frame_duration_));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::AUDIO, _));

  // Frame "30|15K" following "20|10K" triggers start of new group and
  // eventual append.
  EXPECT_CALL(
      callbacks_,
      OnGroupStart(DemuxerStream::AUDIO,
                   DecodeTimestamp::FromPresentationTime(Milliseconds(10)),
                   Milliseconds(20) + frame_duration_));
  EXPECT_CALL(callbacks_, OnAppend(DemuxerStream::AUDIO, _));

  EXPECT_CALL(callbacks_, PossibleDurationIncrease(
                              base::TimeDelta::FromMicroseconds(34999)));
  EXPECT_TRUE(ProcessFrames("0K 10|5K 20|10K 30|15K", ""));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  // Note that the result is still buffered continuous since DTS was continuous
  // and PTS was monotonically increasing (such that each group start was
  // signalled by FrameProcessor to be continuous with the end of the previous
  // group, if any.)
  CheckExpectedRangesByTimestamp(audio_.get(), "{ [0,34) }");
  CheckReadsThenReadStalls(audio_.get(), "0 10 20 30");
}

TEST_P(FrameProcessorTest,
       GroupEndTimestampDecreaseWithinMediaSegmentShouldWarn) {
  // This parse warning requires:
  // 1) a decode time discontinuity within the set of frames being processed,
  // 2) the highest frame end time of any frame successfully processed
  //    before that discontinuity is higher than the highest frame end time of
  //    all frames processed after that discontinuity.
  // TODO(wolenetz): Adjust this case once direction on spec is informed by
  // data. See https://crbug.com/920853 and
  // https://github.com/w3c/media-source/issues/203.
  if (use_sequence_mode_) {
    // Sequence mode modifies the presentation timestamps following a decode
    // discontinuity such that this scenario should not repro with that mode.
    DVLOG(1) << "Skipping segments mode variant; inapplicable to this case.";
    return;
  }

  InSequence s;
  AddTestTracks(HAS_VIDEO);

  EXPECT_CALL(callbacks_,
              OnParseWarning(SourceBufferParseWarning::
                                 kGroupEndTimestampDecreaseWithinMediaSegment));

  frame_duration_ = Milliseconds(10);
  EXPECT_CALL(callbacks_, PossibleDurationIncrease(Milliseconds(15)));
  EXPECT_TRUE(ProcessFrames("", "0K 10K 5|40K"));
  EXPECT_EQ(Milliseconds(0), timestamp_offset_);

  CheckExpectedRangesByTimestamp(video_.get(), "{ [0,15) }");
  CheckReadsThenReadStalls(video_.get(), "0 5");
}

INSTANTIATE_TEST_SUITE_P(SequenceMode, FrameProcessorTest, Values(true));
INSTANTIATE_TEST_SUITE_P(SegmentsMode, FrameProcessorTest, Values(false));

}  // namespace media
