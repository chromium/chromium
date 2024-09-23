// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <list>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

constexpr gfx::Size kCodedSize(320, 180);
constexpr gfx::Rect kVisibleRect(kCodedSize);
constexpr gfx::Size kNaturalSize = kCodedSize;

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer.get() ? DemuxerStream::kOk : DemuxerStream::kAborted, buffer);
}

MATCHER(ContainsFailedToSendLog, "") {
  return CONTAINS_STRING(arg, "Failed to send");
}

MATCHER(ContainsFailedToDecode, "") {
  return CONTAINS_STRING(arg, "failed to decode");
}

class FFmpegVideoDecoderTest : public testing::Test {
 public:
  FFmpegVideoDecoderTest()
      : decoder_(std::make_unique<FFmpegVideoDecoder>(&media_log_)) {
    // Initialize various test buffers.
    frame_buffer_ = std::make_unique<uint8_t[]>(kCodedSize.GetArea());
    end_of_stream_buffer_ = DecoderBuffer::CreateEOSBuffer();
    i_frame_buffer_ = ReadTestDataFile("h264-320x180-frame-0");
    corrupt_i_frame_buffer_ = ReadTestDataFile("h264-320x180-frame-0");
    memset(corrupt_i_frame_buffer_->writable_data(), 0,
           corrupt_i_frame_buffer_->size() / 2);
  }

  FFmpegVideoDecoderTest(const FFmpegVideoDecoderTest&) = delete;
  FFmpegVideoDecoderTest& operator=(const FFmpegVideoDecoderTest&) = delete;

  ~FFmpegVideoDecoderTest() override { Destroy(); }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal(VideoCodec::kH264));
  }

  void InitializeWithConfigWithResult(const VideoDecoderConfig& config,
                                      bool success) {
    decoder_->Initialize(
        config, false, nullptr,
        base::BindOnce(
            [](bool success, DecoderStatus status) {
              EXPECT_EQ(status.is_ok(), success);
            },
            success),
        base::BindRepeating(&FFmpegVideoDecoderTest::FrameReady,
                            base::Unretained(this)),
        base::NullCallback());
    base::RunLoop().RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigWithResult(config, true);
  }

  void Reinitialize() {
    InitializeWithConfig(TestVideoConfig::Large(VideoCodec::kH264));
  }

  void Reset() {
    decoder_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  void Destroy() {
    decoder_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an active
  // decoding state.
  void EnterDecodingState() {
    EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an end
  // of stream state.
  void EnterEndOfStreamState() {
    EXPECT_TRUE(DecodeSingleFrame(end_of_stream_buffer_).is_ok());
    ASSERT_FALSE(output_frames_.empty());
  }

  typedef std::vector<scoped_refptr<DecoderBuffer> > InputBuffers;
  typedef std::vector<scoped_refptr<VideoFrame> > OutputFrames;

  // Decodes all buffers in |input_buffers| and push all successfully decoded
  // output frames into |output_frames|.
  // Returns the last decode status returned by the decoder.
  DecoderStatus DecodeMultipleFrames(const InputBuffers& input_buffers) {
    for (auto iter = input_buffers.begin(); iter != input_buffers.end();
         ++iter) {
      DecoderStatus status = Decode(*iter);
      switch (status.code()) {
        case DecoderStatus::Codes::kOk:
          break;
        case DecoderStatus::Codes::kAborted:
          NOTREACHED_IN_MIGRATION();
          [[fallthrough]];
        default:
          DCHECK(output_frames_.empty());
          return status;
      }
    }
    return DecoderStatus::Codes::kOk;
  }

  // Decodes the single compressed frame in |buffer| and writes the
  // uncompressed output to |video_frame|. This method works with single
  // and multithreaded decoders. End of stream buffers are used to trigger
  // the frame to be returned in the multithreaded decoder case.
  DecoderStatus DecodeSingleFrame(scoped_refptr<DecoderBuffer> buffer) {
    InputBuffers input_buffers;
    input_buffers.push_back(buffer);
    input_buffers.push_back(end_of_stream_buffer_);

    return DecodeMultipleFrames(input_buffers);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name,
                                int expected_width,
                                int expected_height,
                                size_t expected_frames = 2u) {
    Initialize();
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(test_file_name);

    InputBuffers input_buffers;
    input_buffers.push_back(i_frame_buffer_);
    input_buffers.push_back(buffer);
    input_buffers.push_back(end_of_stream_buffer_);

    DecoderStatus status = DecodeMultipleFrames(input_buffers);

    EXPECT_TRUE(status.is_ok());
    ASSERT_EQ(expected_frames, output_frames_.size());

    gfx::Size original_size = kVisibleRect.size();
    EXPECT_EQ(original_size.width(),
              output_frames_[0]->visible_rect().size().width());
    EXPECT_EQ(original_size.height(),
              output_frames_[0]->visible_rect().size().height());
    EXPECT_EQ(expected_width,
              output_frames_[1]->visible_rect().size().width());
    EXPECT_EQ(expected_height,
              output_frames_[1]->visible_rect().size().height());
  }

  DecoderStatus Decode(scoped_refptr<DecoderBuffer> buffer) {
    DecoderStatus status;
    EXPECT_CALL(*this, DecodeDone(_)).WillOnce(SaveArg<0>(&status));

    decoder_->Decode(buffer, base::BindOnce(&FFmpegVideoDecoderTest::DecodeDone,
                                            base::Unretained(this)));

    base::RunLoop().RunUntilIdle();

    return status;
  }

  void FrameReady(scoped_refptr<VideoFrame> frame) {
    DCHECK(!frame->metadata().end_of_stream);
    output_frames_.push_back(std::move(frame));
  }

  MOCK_METHOD1(DecodeDone, void(DecoderStatus));

  StrictMock<MockMediaLog> media_log_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{kBuiltInH264Decoder};
  std::unique_ptr<FFmpegVideoDecoder> decoder_;

  // Various buffers for testing.
  std::unique_ptr<uint8_t[]> frame_buffer_;
  scoped_refptr<DecoderBuffer> end_of_stream_buffer_;
  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  scoped_refptr<DecoderBuffer> corrupt_i_frame_buffer_;

  OutputFrames output_frames_;
};

TEST_F(FFmpegVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open2() fails.
  VideoDecoderConfig config(VideoCodec::kTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace(), kNoTransformation, kCodedSize,
                            kVisibleRect, kNaturalSize, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
  InitializeWithConfigWithResult(config, false);
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  Reinitialize();
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_AfterDecodeFrame) {
  Initialize();
  EnterDecodingState();
  Reinitialize();
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_AfterReset) {
  Initialize();
  EnterDecodingState();
  Reset();
  Reinitialize();
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  ASSERT_EQ(1U, output_frames_.size());
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_OOM) {
  Initialize();
  decoder_->force_allocation_error_for_testing();
  EXPECT_MEDIA_LOG(_);
  EXPECT_FALSE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  EXPECT_TRUE(output_frames_.empty());
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeError) {
  Initialize();

  EXPECT_MEDIA_LOG(ContainsFailedToSendLog());

  // The error is only raised on the second decode attempt, so we expect at
  // least one successful decode but we don't expect valid frame to be decoded.
  // During the second decode attempt an error is raised.
  EXPECT_TRUE(Decode(corrupt_i_frame_buffer_).is_ok());
  EXPECT_TRUE(output_frames_.empty());
  EXPECT_THAT(Decode(i_frame_buffer_), IsDecodeErrorStatus());
  EXPECT_TRUE(output_frames_.empty());

  // After a decode error occurred, all following decodes will return
  // DecoderStatus::Codes::kFailed.
  EXPECT_THAT(Decode(i_frame_buffer_), IsDecodeErrorStatus());
  EXPECT_TRUE(output_frames_.empty());
}

// A corrupt frame followed by an EOS buffer should raise a decode error.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeErrorAtEndOfStream) {
  Initialize();

  EXPECT_MEDIA_LOG(ContainsFailedToDecode());

  EXPECT_THAT(DecodeSingleFrame(corrupt_i_frame_buffer_),
              IsDecodeErrorStatus());
}

// Decode |i_frame_buffer_| and then a smaller frame and verify the output size
// was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Smaller) {
  DecodeIFrameThenTestFile("red-green.h264", 80, 128, /*expected_frames=*/4);
}

// Decode |i_frame_buffer_| and then a larger frame and verify the output size
// was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Larger) {
  DecodeIFrameThenTestFile("bear-320x192-baseline-frame-0.h264", 320, 192);
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Reset_Decoding) {
  Initialize();
  EnterDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test destruction when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Destroy_Initialized) {
  Initialize();
  Destroy();
}

// Test destruction when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Destroy_Decoding) {
  Initialize();
  EnterDecodingState();
  Destroy();
}

// Test destruction when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Destroy_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Destroy();
}

}  // namespace media
