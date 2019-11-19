// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_HELPERS_H_
#define MEDIA_BASE_TEST_HELPERS_H_

#include <stddef.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class RunLoop;
class TimeDelta;
}

namespace media {

class AudioBuffer;
class AudioBus;
class DecoderBuffer;
class MockDemuxerStream;

// Return a callback that expects to be run once.
base::Closure NewExpectedClosure();
base::Callback<void(bool)> NewExpectedBoolCB(bool success);
PipelineStatusCB NewExpectedStatusCB(PipelineStatus status);

// Helper class for running a message loop until a callback has run. Useful for
// testing classes that run on more than a single thread.
//
// Events are intended for single use and cannot be reset.
class WaitableMessageLoopEvent {
 public:
  WaitableMessageLoopEvent();
  explicit WaitableMessageLoopEvent(base::TimeDelta timeout);
  ~WaitableMessageLoopEvent();

  // Returns a thread-safe closure that will signal |this| when executed.
  base::Closure GetClosure();
  PipelineStatusCB GetPipelineStatusCB();

  // Runs the current message loop until |this| has been signaled.
  //
  // Fails the test if the timeout is reached.
  void RunAndWait();

  // Runs the current message loop until |this| has been signaled and asserts
  // that the |expected| status was received.
  //
  // Fails the test if the timeout is reached.
  void RunAndWaitForStatus(PipelineStatus expected);

  bool is_signaled() const { return signaled_; }

 private:
  void OnCallback(PipelineStatus status);
  void OnTimeout();

  bool signaled_;
  PipelineStatus status_;
  std::unique_ptr<base::RunLoop> run_loop_;
  const base::TimeDelta timeout_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WaitableMessageLoopEvent);
};

// Provides pre-canned VideoDecoderConfig. These types are used for tests that
// don't care about detailed parameters of the config.
class TestVideoConfig {
 public:
  // Returns a configuration that is invalid.
  static VideoDecoderConfig Invalid();

  static VideoDecoderConfig Normal(VideoCodec codec = kCodecVP8);
  static VideoDecoderConfig NormalWithColorSpace(
      VideoCodec codec,
      const VideoColorSpace& color_space);
  static VideoDecoderConfig NormalH264(VideoCodecProfile = H264PROFILE_MIN);
  static VideoDecoderConfig NormalCodecProfile(
      VideoCodec codec = kCodecVP8,
      VideoCodecProfile profile = VP8PROFILE_MIN);
  static VideoDecoderConfig NormalEncrypted(VideoCodec codec = kCodecVP8,
                                            VideoCodecProfile = VP8PROFILE_MIN);
  static VideoDecoderConfig NormalRotated(VideoRotation rotation);

  // Returns a configuration that is larger in dimensions than Normal().
  static VideoDecoderConfig Large(VideoCodec codec = kCodecVP8);
  static VideoDecoderConfig LargeEncrypted(VideoCodec codec = kCodecVP8);

  // Returns coded size for Normal and Large config.
  static gfx::Size NormalCodedSize();
  static gfx::Size LargeCodedSize();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVideoConfig);
};

// Provides pre-canned AudioDecoderConfig. These types are used for tests that
// don't care about detailed parameters of the config.
class TestAudioConfig {
 public:
  static AudioDecoderConfig Normal();
  static AudioDecoderConfig NormalEncrypted();
};

// Provides pre-canned AudioParameters objects.
class TestAudioParameters {
 public:
  static AudioParameters Normal();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAudioParameters);
};

// Create an AudioBuffer containing |frames| frames of data, where each sample
// is of type T.  |start| and |increment| are used to specify the values for the
// samples, which are created in channel order.  The value for frame and channel
// is determined by:
//
//   |start| + |channel| * |frames| * |increment| + index * |increment|
//
// E.g., for a stereo buffer the values in channel 0 will be:
//   start
//   start + increment
//   start + 2 * increment, ...
//
// While, values in channel 1 will be:
//   start + frames * increment
//   start + (frames + 1) * increment
//   start + (frames + 2) * increment, ...
//
// |start_time| will be used as the start time for the samples.
template <class T>
scoped_refptr<AudioBuffer> MakeAudioBuffer(SampleFormat format,
                                           ChannelLayout channel_layout,
                                           size_t channel_count,
                                           int sample_rate,
                                           T start,
                                           T increment,
                                           size_t frames,
                                           base::TimeDelta timestamp);

// Create an AudioBuffer containing bitstream data. |start| and |increment| are
// used to specify the values for the data. The value is determined by:
//   start + frames * increment
//   start + (frames + 1) * increment
//   start + (frames + 2) * increment, ...
scoped_refptr<AudioBuffer> MakeBitstreamAudioBuffer(
    SampleFormat format,
    ChannelLayout channel_layout,
    size_t channel_count,
    int sample_rate,
    uint8_t start,
    uint8_t increment,
    size_t frames,
    size_t data_size,
    base::TimeDelta timestamp);

// Verify the bitstream data in an AudioBus. |start| and |increment| are
// used to specify the values for the data. The value is determined by:
//   start + frames * increment
//   start + (frames + 1) * increment
//   start + (frames + 2) * increment, ...
void VerifyBitstreamAudioBus(AudioBus* bus,
                             size_t data_size,
                             uint8_t start,
                             uint8_t increment);

// Create a fake video DecoderBuffer for testing purpose. The buffer contains
// part of video decoder config info embedded so that the testing code can do
// some sanity check.
scoped_refptr<DecoderBuffer> CreateFakeVideoBufferForTest(
    const VideoDecoderConfig& config,
    base::TimeDelta timestamp,
    base::TimeDelta duration);

// Verify if a fake video DecoderBuffer is valid.
bool VerifyFakeVideoBufferForTest(const DecoderBuffer& buffer,
                                  const VideoDecoderConfig& config);

// Create a MockDemuxerStream for testing purposes.
std::unique_ptr<::testing::StrictMock<MockDemuxerStream>>
CreateMockDemuxerStream(DemuxerStream::Type type, bool encrypted);

// Compares two {Audio|Video}DecoderConfigs
MATCHER_P(DecoderConfigEq, config, "") {
  return arg.Matches(config);
}

MATCHER_P(HasTimestamp, timestamp_in_ms, "") {
  return arg.get() && !arg->end_of_stream() &&
         arg->timestamp().InMilliseconds() == timestamp_in_ms;
}

MATCHER(IsEndOfStream, "") {
  return arg.get() && arg->end_of_stream();
}

MATCHER(EosBeforeHaveMetadata, "") {
  return CONTAINS_STRING(
      arg,
      "MediaSource endOfStream before demuxer initialization completes (before "
      "HAVE_METADATA) is treated as an error. This may also occur as "
      "consequence of other MediaSource errors before HAVE_METADATA.");
}

MATCHER_P(SegmentMissingFrames, track_id, "") {
  return CONTAINS_STRING(
      arg, "Media segment did not contain any coded frames for track " +
               std::string(track_id));
}

MATCHER(MuxedSequenceModeWarning, "") {
  return CONTAINS_STRING(arg,
                         "Warning: using MSE 'sequence' AppendMode for a "
                         "SourceBuffer with multiple tracks");
}

MATCHER_P2(KeyframeTimeGreaterThanDependant,
           keyframe_time_string,
           nonkeyframe_time_string,
           "") {
  return CONTAINS_STRING(
      arg,
      "Warning: presentation time of most recently processed random access "
      "point (" +
          std::string(keyframe_time_string) +
          " s) is later than the presentation time of a non-keyframe (" +
          nonkeyframe_time_string +
          " s) that depends on it. This type of random access point is not "
          "well supported by MSE; buffered range reporting may be less "
          "precise.");
}

MATCHER(StreamParsingFailed, "") {
  return CONTAINS_STRING(arg, "Append: stream parsing failed.");
}

MATCHER(ParsedBuffersNotInDTSSequence, "") {
  return CONTAINS_STRING(arg, "Parsed buffers not in DTS sequence");
}

MATCHER_P2(CodecUnsupportedInContainer, codec, container, "") {
  return CONTAINS_STRING(arg, std::string(codec) + "' is not supported for '" +
                                  std::string(container));
}

MATCHER_P(FoundStream, stream_type_string, "") {
  return CONTAINS_STRING(
      arg, "found_" + std::string(stream_type_string) + "_stream\":true");
}

MATCHER_P2(CodecName, stream_type_string, codec_string, "") {
  return CONTAINS_STRING(arg,
                         std::string(stream_type_string) + "_codec_name") &&
         CONTAINS_STRING(arg, std::string(codec_string));
}

MATCHER_P2(FlacAudioSampleRateOverriddenByStreaminfo,
           original_rate_string,
           streaminfo_rate_string,
           "") {
  return CONTAINS_STRING(
      arg, "FLAC AudioSampleEntry sample rate " +
               std::string(original_rate_string) + " overridden by rate " +
               std::string(streaminfo_rate_string) +
               " from FLACSpecificBox's STREAMINFO metadata");
}

MATCHER_P2(InitSegmentMismatchesMimeType, stream_type, codec_name, "") {
  return CONTAINS_STRING(arg, std::string(stream_type) + " stream codec " +
                                  std::string(codec_name) +
                                  " doesn't match SourceBuffer codecs.");
}

MATCHER_P(InitSegmentMissesExpectedTrack, missing_codec, "") {
  return CONTAINS_STRING(arg, "Initialization segment misses expected " +
                                  std::string(missing_codec) + " track.");
}

MATCHER_P2(UnexpectedTrack, track_type, id, "") {
  return CONTAINS_STRING(arg, std::string("Got unexpected ") + track_type +
                                  " track track_id=" + id);
}

MATCHER_P2(FrameTypeMismatchesTrackType, frame_type, track_type, "") {
  return CONTAINS_STRING(arg, std::string("Frame type ") + frame_type +
                                  " doesn't match track buffer type " +
                                  track_type);
}

MATCHER_P2(AudioNonKeyframe, pts_microseconds, dts_microseconds, "") {
  return CONTAINS_STRING(
      arg, std::string("Bytestream with audio frame PTS ") +
               base::NumberToString(pts_microseconds) + "us and DTS " +
               base::NumberToString(dts_microseconds) +
               "us indicated the frame is not a random access point (key "
               "frame). All audio frames are expected to be key frames.");
}

MATCHER_P2(SkippingSpliceAtOrBefore,
           new_microseconds,
           existing_microseconds,
           "") {
  return CONTAINS_STRING(
      arg, "Skipping splice frame generation: first new buffer at " +
               base::NumberToString(new_microseconds) +
               "us begins at or before existing buffer at " +
               base::NumberToString(existing_microseconds) + "us.");
}

MATCHER_P(SkippingSpliceAlreadySpliced, time_microseconds, "") {
  return CONTAINS_STRING(
      arg, "Skipping splice frame generation: overlapped buffers at " +
               base::NumberToString(time_microseconds) +
               "us are in a previously buffered splice.");
}

MATCHER_P2(SkippingSpliceTooLittleOverlap,
           pts_microseconds,
           overlap_microseconds,
           "") {
  return CONTAINS_STRING(
      arg, "Skipping audio splice trimming at PTS=" +
               base::NumberToString(pts_microseconds) + "us. Found only " +
               base::NumberToString(overlap_microseconds) +
               "us of overlap, need at least 1000us. Multiple occurrences may "
               "result in loss of A/V sync.");
}

// Prefer WebMSimpleBlockDurationEstimated over this matcher, unless the actual
// estimated duration value is unimportant to the test.
MATCHER(WebMSimpleBlockDurationEstimatedAny, "") {
  return CONTAINS_STRING(arg, "Estimating WebM block duration=");
}

MATCHER_P(WebMSimpleBlockDurationEstimated, estimated_duration_ms, "") {
  return CONTAINS_STRING(arg, "Estimating WebM block duration=" +
                                  base::NumberToString(estimated_duration_ms));
}

MATCHER_P(WebMNegativeTimecodeOffset, timecode_string, "") {
  return CONTAINS_STRING(arg, "Got a block with negative timecode offset " +
                                  std::string(timecode_string));
}

MATCHER(WebMOutOfOrderTimecode, "") {
  return CONTAINS_STRING(
      arg, "Got a block with a timecode before the previous block.");
}

MATCHER(WebMClusterBeforeFirstInfo, "") {
  return CONTAINS_STRING(arg, "Found Cluster element before Info.");
}

MATCHER_P3(TrimmedSpliceOverlap,
           splice_time_us,
           overlapped_start_us,
           trim_duration_us,
           "") {
  return CONTAINS_STRING(
      arg,
      "Audio buffer splice at PTS=" + base::NumberToString(splice_time_us) +
          "us. Trimmed tail of overlapped buffer (PTS=" +
          base::NumberToString(overlapped_start_us) + "us) by " +
          base::NumberToString(trim_duration_us));
}

MATCHER_P2(NoSpliceForBadMux, overlapped_buffer_count, splice_time_us, "") {
  return CONTAINS_STRING(arg,
                         "Media is badly muxed. Detected " +
                             base::NumberToString(overlapped_buffer_count) +
                             " overlapping audio buffers at time " +
                             base::NumberToString(splice_time_us));
}

MATCHER(ChunkDemuxerCtor, "") {
  return CONTAINS_STRING(arg, "ChunkDemuxer");
}

MATCHER_P2(DiscardingEmptyFrame, pts_us, dts_us, "") {
  return CONTAINS_STRING(arg,
                         "Discarding empty audio or video coded frame, PTS=" +
                             base::NumberToString(pts_us) +
                             "us, DTS=" + base::NumberToString(dts_us) + "us");
}

MATCHER_P4(TruncatedFrame,
           pts_us,
           pts_end_us,
           start_or_end,
           append_window_us,
           "") {
  const std::string expected = base::StringPrintf(
      "Truncating audio buffer which overlaps append window %s. PTS %dus "
      "frame_end_timestamp %dus append_window_%s %dus",
      start_or_end, pts_us, pts_end_us, start_or_end, append_window_us);
  *result_listener << "Expected TruncatedFrame contains '" << expected << "'";
  return CONTAINS_STRING(arg, expected);
}

MATCHER_P2(DroppedFrame, frame_type, pts_us, "") {
  return CONTAINS_STRING(arg,
                         "Dropping " + std::string(frame_type) + " frame") &&
         CONTAINS_STRING(arg, "PTS " + base::NumberToString(pts_us));
}

MATCHER_P3(DroppedFrameCheckAppendWindow,
           frame_type,
           append_window_start_us,
           append_window_end_us,
           "") {
  return CONTAINS_STRING(arg,
                         "Dropping " + std::string(frame_type) + " frame") &&
         CONTAINS_STRING(
             arg, "outside append window [" +
                      base::NumberToString(append_window_start_us) + "us," +
                      base::NumberToString(append_window_end_us) + "us");
}

}  // namespace media

#endif  // MEDIA_BASE_TEST_HELPERS_H_
