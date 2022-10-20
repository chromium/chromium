// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <sstream>

#include "base/ranges/algorithm.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

typedef StreamParser::TrackId TrackId;
typedef StreamParser::BufferQueue BufferQueue;

const int kEnd = -1;
const uint8_t kFakeData[] = {0xFF};
const TrackId kAudioTrackId = 0;
const TrackId kVideoTrackId = 1;
const TrackId kTextTrackIdA = 2;
const TrackId kTextTrackIdB = 3;

static bool IsAudio(scoped_refptr<StreamParserBuffer> buffer) {
  return buffer->type() == DemuxerStream::AUDIO;
}

static bool IsVideo(scoped_refptr<StreamParserBuffer> buffer) {
  return buffer->type() == DemuxerStream::VIDEO;
}

static bool IsText(scoped_refptr<StreamParserBuffer> buffer) {
  return buffer->type() == DemuxerStream::TEXT;
}

// Creates and appends a sequence of StreamParserBuffers to the provided
// |queue|. |decode_timestamps| determines the number of appended buffers and
// their sequence of decode timestamps; a |kEnd| timestamp indicates the
// end of the sequence and no buffer is appended for it. Each new buffer's
// type will be |type| with track ID set to |track_id|.
static void GenerateBuffers(const int* decode_timestamps,
                            StreamParserBuffer::Type type,
                            TrackId track_id,
                            BufferQueue* queue) {
  DCHECK(decode_timestamps);
  DCHECK(queue);
  DCHECK_NE(type, DemuxerStream::UNKNOWN);
  DCHECK_LE(type, DemuxerStream::TYPE_MAX);
  for (int i = 0; decode_timestamps[i] != kEnd; ++i) {
    scoped_refptr<StreamParserBuffer> buffer =
        StreamParserBuffer::CopyFrom(kFakeData, sizeof(kFakeData),
                                     true, type, track_id);
    buffer->SetDecodeTimestamp(
        DecodeTimestamp::FromMicroseconds(decode_timestamps[i]));
    queue->push_back(buffer);
  }
}

class StreamParserTest : public testing::Test {
 public:
  StreamParserTest(const StreamParserTest&) = delete;
  StreamParserTest& operator=(const StreamParserTest&) = delete;

 protected:
  StreamParserTest() = default;

  // Returns the number of buffers in |merged_buffers_| for which |predicate|
  // returns true.
  size_t CountMatchingMergedBuffers(
      bool (*predicate)(scoped_refptr<StreamParserBuffer> buffer)) {
    return static_cast<size_t>(
        base::ranges::count_if(merged_buffers_, predicate));
  }

  // Appends test audio buffers in the sequence described by |decode_timestamps|
  // to |audio_buffers_|. See GenerateBuffers() for |decode_timestamps| format.
  void GenerateAudioBuffers(const int* decode_timestamps) {
    GenerateBuffers(decode_timestamps, DemuxerStream::AUDIO, kAudioTrackId,
                    &buffer_queue_map_[kAudioTrackId]);
  }

  // Appends test video buffers in the sequence described by |decode_timestamps|
  // to |video_buffers_|. See GenerateBuffers() for |decode_timestamps| format.
  void GenerateVideoBuffers(const int* decode_timestamps) {
    GenerateBuffers(decode_timestamps, DemuxerStream::VIDEO, kVideoTrackId,
                    &buffer_queue_map_[kVideoTrackId]);
  }

  // Current tests only need up to two distinct text BufferQueues. This helper
  // conditionally appends buffers to the underlying |buffer_queue_map_| keyed
  // by the respective track ID. If |decode_timestamps_{a,b}|
  // is NULL, then the corresponding BufferQueue is not changed at all.
  // Note that key collision on map insertion does not replace the previous
  // value.
  void GenerateTextBuffers(const int* decode_timestamps_a,
                           const int* decode_timestamps_b) {
    if (decode_timestamps_a) {
      GenerateBuffers(decode_timestamps_a, DemuxerStream::TEXT, kTextTrackIdA,
                      &buffer_queue_map_[kTextTrackIdA]);
    }

    if (decode_timestamps_b) {
      GenerateBuffers(decode_timestamps_b, DemuxerStream::TEXT, kTextTrackIdB,
                      &buffer_queue_map_[kTextTrackIdB]);
    }
  }

  // Returns a string that describes the sequence of buffers in
  // |merged_buffers_|. The string is a concatenation of space-delimited buffer
  // descriptors in the same sequence as |merged_buffers_|. Each descriptor is
  // the concatenation of
  // 1) a single character that describes the buffer's type(), e.g. A, V, or T
  //    for audio, video, or text, respectively
  // 2) the buffer's track_id()
  // 3) ":"
  // 4) the buffer's decode timestamp.
  // If |include_type_and_text_track| is false, then items 1, 2, and 3 are
  // not included in descriptors. This is useful when buffers with different
  // media types but the same decode timestamp are expected, and the exact
  // sequence of media types for the tying timestamps is not subject to
  // verification.
  std::string MergedBufferQueueString(bool include_type_and_text_track) {
    std::stringstream results_stream;
    for (BufferQueue::const_iterator itr = merged_buffers_.begin();
         itr != merged_buffers_.end();
         ++itr) {
      if (itr != merged_buffers_.begin())
        results_stream << " ";
      const StreamParserBuffer& buffer = *(itr->get());
      if (include_type_and_text_track) {
        switch (buffer.type()) {
          case DemuxerStream::AUDIO:
            results_stream << "A";
            break;
          case DemuxerStream::VIDEO:
            results_stream << "V";
            break;
          case DemuxerStream::TEXT:
            results_stream << "T";

            break;
          default:
            NOTREACHED();
        }
        results_stream << buffer.track_id() << ":";
      }
      results_stream << buffer.GetDecodeTimestamp().InMicroseconds();
    }

    return results_stream.str();
  }

  // Verifies that MergeBufferQueues() of the current |audio_buffers_|,
  // |video_buffers_|, |text_map_|, and |merged_buffers_| returns true and
  // results in an updated |merged_buffers_| that matches expectation. The
  // expectation, specified in |expected|, is compared to the string resulting
  // from MergedBufferQueueString() (see comments for that method) with
  // |verify_type_and_text_track_sequence| passed. |merged_buffers_| is appended
  // to by the merge, and may be setup by the caller to have some pre-existing
  // buffers; it is both an input and output of this method.
  // Regardless of |verify_type_and_text_track_sequence|, the marginal number
  // of buffers of each type (audio, video, text) resulting from the merge is
  // also verified to match the number of buffers in |audio_buffers_|,
  // |video_buffers_|, and |text_map_|, respectively.
  void VerifyMergeSuccess(const std::string& expected,
                          bool verify_type_and_text_track_sequence) {
    // |merged_buffers| may already have some buffers. Count them by type for
    // later inclusion in verification.
    size_t original_audio_in_merged = CountMatchingMergedBuffers(IsAudio);
    size_t original_video_in_merged = CountMatchingMergedBuffers(IsVideo);
    size_t original_text_in_merged = CountMatchingMergedBuffers(IsText);

    EXPECT_TRUE(MergeBufferQueues(buffer_queue_map_, &merged_buffers_));

    // Verify resulting contents of |merged_buffers| matches |expected|.
    EXPECT_EQ(expected,
              MergedBufferQueueString(verify_type_and_text_track_sequence));

    // Verify that the correct number of each type of buffer is in the merge
    // result.
    size_t audio_in_merged = CountMatchingMergedBuffers(IsAudio);
    size_t video_in_merged = CountMatchingMergedBuffers(IsVideo);
    size_t text_in_merged = CountMatchingMergedBuffers(IsText);

    EXPECT_GE(audio_in_merged, original_audio_in_merged);
    EXPECT_GE(video_in_merged, original_video_in_merged);
    EXPECT_GE(text_in_merged, original_text_in_merged);

    EXPECT_EQ(buffer_queue_map_[kAudioTrackId].size(),
              audio_in_merged - original_audio_in_merged);
    if (buffer_queue_map_[kAudioTrackId].empty())
      buffer_queue_map_.erase(kAudioTrackId);
    EXPECT_EQ(buffer_queue_map_[kVideoTrackId].size(),
              video_in_merged - original_video_in_merged);
    if (buffer_queue_map_[kVideoTrackId].empty())
      buffer_queue_map_.erase(kVideoTrackId);

    size_t expected_text_buffer_count = 0;
    expected_text_buffer_count += buffer_queue_map_[kTextTrackIdA].size();
    if (buffer_queue_map_[kTextTrackIdA].empty())
      buffer_queue_map_.erase(kTextTrackIdA);
    expected_text_buffer_count += buffer_queue_map_[kTextTrackIdB].size();
    if (buffer_queue_map_[kTextTrackIdB].empty())
      buffer_queue_map_.erase(kTextTrackIdB);
    EXPECT_EQ(expected_text_buffer_count,
              text_in_merged - original_text_in_merged);
  }

  // Verifies that MergeBufferQueues() of the current |buffer_queue_map_| and
  // |merged_buffers_| returns false.
  void VerifyMergeFailure() {
    EXPECT_FALSE(MergeBufferQueues(buffer_queue_map_, &merged_buffers_));
  }

  // Helper to allow tests to clear all the input BufferQueues (except
  // |merged_buffers_|) and the BufferQueueMap that are used in
  // VerifyMerge{Success/Failure}().
  void ClearBufferQueuesButKeepAnyMergedBuffers() { buffer_queue_map_.clear(); }

 private:
  StreamParser::BufferQueueMap buffer_queue_map_;
  BufferQueue merged_buffers_;
};

TEST_F(StreamParserTest, MergeBufferQueues_AllEmpty) {
  std::string expected = "";
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_SingleAudioBuffer) {
  std::string expected = "A0:100";
  int audio_timestamps[] = { 100, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_SingleVideoBuffer) {
  std::string expected = "V1:100";
  int video_timestamps[] = { 100, kEnd };
  GenerateVideoBuffers(video_timestamps);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_SingleTextBuffer) {
  std::string expected = "T2:100";
  int text_timestamps[] = { 100, kEnd };
  GenerateTextBuffers(text_timestamps, NULL);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_OverlappingAudioVideo) {
  std::string expected = "A0:100 V1:101 V1:102 A0:103 A0:104 V1:105";
  int audio_timestamps[] = { 100, 103, 104, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 101, 102, 105, kEnd };
  GenerateVideoBuffers(video_timestamps);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_OverlappingMultipleText) {
  std::string expected = "T2:100 T2:101 T3:103 T2:104 T3:105 T3:106";
  int text_timestamps_a[] = { 100, 101, 104, kEnd };
  int text_timestamps_b[] = { 103, 105, 106, kEnd };
  GenerateTextBuffers(text_timestamps_a, text_timestamps_b);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_OverlappingAudioVideoText) {
  std::string expected = "A0:100 V1:101 T2:102 V1:103 T3:104 A0:105 V1:106 "
                         "T2:107";
  int audio_timestamps[] = { 100, 105, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 101, 103, 106, kEnd };
  GenerateVideoBuffers(video_timestamps);
  int text_timestamps_a[] = { 102, 107, kEnd };
  int text_timestamps_b[] = { 104, kEnd };
  GenerateTextBuffers(text_timestamps_a, text_timestamps_b);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_NonDecreasingNoCrossMediaDuplicate) {
  std::string expected = "A0:100 A0:100 A0:100 V1:101 V1:101 V1:101 A0:102 "
                         "V1:103 V1:103";
  int audio_timestamps[] = { 100, 100, 100, 102, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 101, 101, 101, 103, 103, kEnd };
  GenerateVideoBuffers(video_timestamps);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_CrossStreamDuplicates) {
  // Interface keeps the choice undefined of which stream's buffer wins the
  // selection when timestamps are tied. Verify at least the right number of
  // each kind of buffer results, and that buffers are in nondecreasing order.
  std::string expected = "100 100 100 100 100 100 102 102 102 102 102 102 102";
  int audio_timestamps[] = { 100, 100, 100, 102, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 100, 100, 102, 102, 102, kEnd };
  GenerateVideoBuffers(video_timestamps);
  int text_timestamps[] = { 100, 102, 102, 102, kEnd };
  GenerateTextBuffers(text_timestamps, NULL);
  VerifyMergeSuccess(expected, false);
}

TEST_F(StreamParserTest, MergeBufferQueues_InvalidDecreasingSingleStream) {
  int audio_timestamps[] = { 101, 102, 100, 103, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  VerifyMergeFailure();
}

TEST_F(StreamParserTest, MergeBufferQueues_InvalidDecreasingMultipleStreams) {
  int audio_timestamps[] = { 101, 102, 100, 103, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 104, 100, kEnd };
  GenerateVideoBuffers(video_timestamps);
  VerifyMergeFailure();
}

TEST_F(StreamParserTest, MergeBufferQueues_ValidAppendToExistingMerge) {
  std::string expected = "A0:100 V1:101 T2:102 V1:103 T3:104 A0:105 V1:106 "
                         "T2:107";
  int audio_timestamps[] = { 100, 105, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 101, 103, 106, kEnd };
  GenerateVideoBuffers(video_timestamps);
  int text_timestamps_a[] = { 102, 107, kEnd };
  int text_timestamps_b[] = { 104, kEnd };
  GenerateTextBuffers(text_timestamps_a, text_timestamps_b);
  VerifyMergeSuccess(expected, true);

  ClearBufferQueuesButKeepAnyMergedBuffers();

  expected = "A0:100 V1:101 T2:102 V1:103 T3:104 A0:105 V1:106 T2:107 "
             "A0:107 V1:111 T2:112 V1:113 T3:114 A0:115 V1:116 T2:117";
  int more_audio_timestamps[] = { 107, 115, kEnd };
  GenerateAudioBuffers(more_audio_timestamps);
  int more_video_timestamps[] = { 111, 113, 116, kEnd };
  GenerateVideoBuffers(more_video_timestamps);
  int more_text_timestamps_a[] = { 112, 117, kEnd };
  int more_text_timestamps_b[] = { 114, kEnd };
  GenerateTextBuffers(more_text_timestamps_a, more_text_timestamps_b);
  VerifyMergeSuccess(expected, true);
}

TEST_F(StreamParserTest, MergeBufferQueues_InvalidAppendToExistingMerge) {
  std::string expected = "A0:100 V1:101 T2:102 V1:103 T3:104 A0:105 V1:106 "
                         "T2:107";
  int audio_timestamps[] = { 100, 105, kEnd };
  GenerateAudioBuffers(audio_timestamps);
  int video_timestamps[] = { 101, 103, 106, kEnd };
  GenerateVideoBuffers(video_timestamps);
  int text_timestamps_a[] = { 102, 107, kEnd };
  int text_timestamps_b[] = { 104, kEnd };
  GenerateTextBuffers(text_timestamps_a, text_timestamps_b);
  VerifyMergeSuccess(expected, true);

  // Appending empty buffers to pre-existing merge result should succeed and not
  // change the existing result.
  ClearBufferQueuesButKeepAnyMergedBuffers();
  VerifyMergeSuccess(expected, true);

  // But appending something with a lower timestamp than the last timestamp
  // in the pre-existing merge result should fail.
  int more_audio_timestamps[] = { 106, kEnd };
  GenerateAudioBuffers(more_audio_timestamps);
  VerifyMergeFailure();
}

}  // namespace media
