// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/gav1_video_decoder.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace media {

namespace {

MATCHER(ContainsDecoderErrorLog, "") {
  return CONTAINS_STRING(arg, "libgav1::Decoder::DequeueFrame failed");
}

// Similar to VideoFrame::HashFrameForTesting(), but uses visible_data() and
// visible_rect() instead of data() and coded_size() to determine the region to
// hash.
//
// The VideoFrame objects created by Gav1VideoDecoder have extended pixels
// outside the visible_rect(). Those extended pixels are for libgav1 internal
// use and are not part of the actual video frames. Unlike
// VideoFrame::HashFrameForTesting(), this function excludes the extended pixels
// and hashes only the actual video frames.
void HashFrameVisibleRectForTesting(base::MD5Context* context,
                                    const VideoFrame& frame) {
  DCHECK(context);
  for (size_t plane = 0; plane < VideoFrame::NumPlanes(frame.format());
       ++plane) {
    int rows = frame.Rows(plane, frame.format(), frame.visible_rect().height());
    for (int row = 0; row < rows; ++row) {
      int row_bytes =
          frame.RowBytes(plane, frame.format(), frame.visible_rect().width());
      base::MD5Update(context, base::StringPiece(reinterpret_cast<const char*>(
                                                     frame.visible_data(plane) +
                                                     frame.stride(plane) * row),
                                                 row_bytes));
    }
  }
}

}  // namespace

class Gav1VideoDecoderTest : public testing::Test {
 public:
  Gav1VideoDecoderTest()
      : decoder_(new Gav1VideoDecoder(&media_log_)),
        i_frame_buffer_(ReadTestDataFile("av1-I-frame-320x240")) {}

  ~Gav1VideoDecoderTest() override { Destroy(); }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal(kCodecAV1));
  }

  void InitializeWithConfigWithResult(const VideoDecoderConfig& config,
                                      bool success) {
    decoder_->Initialize(config, false, nullptr,
                         base::BindOnce(
                             [](bool success, Status status) {
                               EXPECT_EQ(status.is_ok(), success);
                             },
                             success),
                         base::BindRepeating(&Gav1VideoDecoderTest::FrameReady,
                                             base::Unretained(this)),
                         base::NullCallback());
    base::RunLoop().RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigWithResult(config, true);
  }

  void Reinitialize() {
    InitializeWithConfig(TestVideoConfig::Large(kCodecAV1));
  }

  void Reset() {
    decoder_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  void Destroy() {
    decoder_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Sets up expectations and actions to put Gav1VideoDecoder in an active
  // decoding state.
  void ExpectDecodingState() {
    EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put Gav1VideoDecoder in an end
  // of stream state.
  void ExpectEndOfStreamState() {
    EXPECT_TRUE(DecodeSingleFrame(DecoderBuffer::CreateEOSBuffer()).is_ok());
    ASSERT_FALSE(output_frames_.empty());
  }

  using InputBuffers = std::vector<scoped_refptr<DecoderBuffer>>;
  using OutputFrames = std::vector<scoped_refptr<VideoFrame>>;

  // Decodes all buffers in |input_buffers| and push all successfully decoded
  // output frames into |output_frames|. Returns the last decode status returned
  // by the decoder.
  Status DecodeMultipleFrames(const InputBuffers& input_buffers) {
    for (auto iter = input_buffers.begin(); iter != input_buffers.end();
         ++iter) {
      Status status = Decode(*iter);
      switch (status.code()) {
        case StatusCode::kOk:
          break;
        case StatusCode::kAborted:
          NOTREACHED();
          FALLTHROUGH;
        default:
          DCHECK(output_frames_.empty());
          return status;
      }
    }
    return StatusCode::kOk;
  }

  // Decodes the single compressed frame in |buffer|.
  Status DecodeSingleFrame(scoped_refptr<DecoderBuffer> buffer) {
    InputBuffers input_buffers;
    input_buffers.push_back(std::move(buffer));
    return DecodeMultipleFrames(input_buffers);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in the file
  // named |test_file_name|. This function expects both buffers to decode to
  // frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name,
                                const gfx::Size& expected_size) {
    Initialize();
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(test_file_name);

    InputBuffers input_buffers;
    input_buffers.push_back(i_frame_buffer_);
    input_buffers.push_back(buffer);
    input_buffers.push_back(DecoderBuffer::CreateEOSBuffer());

    Status status = DecodeMultipleFrames(input_buffers);

    EXPECT_TRUE(status.is_ok());
    ASSERT_EQ(2U, output_frames_.size());

    gfx::Size original_size = TestVideoConfig::NormalCodedSize();
    EXPECT_EQ(original_size.width(),
              output_frames_[0]->visible_rect().size().width());
    EXPECT_EQ(original_size.height(),
              output_frames_[0]->visible_rect().size().height());
    EXPECT_EQ(expected_size.width(),
              output_frames_[1]->visible_rect().size().width());
    EXPECT_EQ(expected_size.height(),
              output_frames_[1]->visible_rect().size().height());
  }

  Status Decode(scoped_refptr<DecoderBuffer> buffer) {
    Status status;
    EXPECT_CALL(*this, DecodeDone(_)).WillOnce(testing::SaveArg<0>(&status));

    decoder_->Decode(std::move(buffer),
                     base::BindOnce(&Gav1VideoDecoderTest::DecodeDone,
                                    base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    return status;
  }

  void FrameReady(scoped_refptr<VideoFrame> frame) {
    DCHECK(!frame->metadata().end_of_stream);
    output_frames_.push_back(std::move(frame));
  }

  std::string GetVideoFrameHash(const VideoFrame& frame) {
    base::MD5Context md5_context;
    base::MD5Init(&md5_context);
    HashFrameVisibleRectForTesting(&md5_context, frame);
    base::MD5Digest digest;
    base::MD5Final(&digest, &md5_context);
    return base::MD5DigestToBase16(digest);
  }

  MOCK_METHOD1(DecodeDone, void(Status));

  testing::StrictMock<MockMediaLog> media_log_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<Gav1VideoDecoder> decoder_;

  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  OutputFrames output_frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Gav1VideoDecoderTest);
};

TEST_F(Gav1VideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(Gav1VideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  Reinitialize();
}

TEST_F(Gav1VideoDecoderTest, Reinitialize_AfterDecodeFrame) {
  Initialize();
  ExpectDecodingState();
  Reinitialize();
}

TEST_F(Gav1VideoDecoderTest, Reinitialize_AfterReset) {
  Initialize();
  ExpectDecodingState();
  Reset();
  Reinitialize();
}

TEST_F(Gav1VideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ("589dc641b7742ffe7a2b0d4c16aa3e86", GetVideoFrameHash(*frame));
}

TEST_F(Gav1VideoDecoderTest, DecodeFrame_8bitMono) {
  Initialize();
  EXPECT_TRUE(
      DecodeSingleFrame(ReadTestDataFile("av1-monochrome-I-frame-320x240-8bpp"))
          .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ("eeba03dcc9c22c4632bf74b481db36b2", GetVideoFrameHash(*frame));
}

TEST_F(Gav1VideoDecoderTest, DecodeFrame_10bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(
                  ReadTestDataFile("av1-monochrome-I-frame-320x240-10bpp"))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_YUV420P10, frame->format());
  EXPECT_EQ("026c1fed9e161f09d816ac7278458a80", GetVideoFrameHash(*frame));
}

// libgav1 does not support bit depth 12.
TEST_F(Gav1VideoDecoderTest, DISABLED_DecodeFrame_12bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(
                  ReadTestDataFile("av1-monochrome-I-frame-320x240-12bpp"))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_YUV420P12, frame->format());
  EXPECT_EQ("32115092dc00fbe86823b0b714a0f63e", GetVideoFrameHash(*frame));
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(Gav1VideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("av1-I-frame-1280x720", gfx::Size(1280, 720));
}

// Decode a VP9 frame which should trigger a decoder error.
TEST_F(Gav1VideoDecoderTest, DecodeFrame_Error) {
  Initialize();
  EXPECT_MEDIA_LOG(ContainsDecoderErrorLog());
  DecodeSingleFrame(ReadTestDataFile("vp9-I-frame-320x240"));
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(Gav1VideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(Gav1VideoDecoderTest, Reset_Decoding) {
  Initialize();
  ExpectDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(Gav1VideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Reset();
}

// Test destruction when decoder has initialized but not decoded.
TEST_F(Gav1VideoDecoderTest, Destroy_Initialized) {
  Initialize();
  Destroy();
}

// Test destruction when decoder has decoded single frame.
TEST_F(Gav1VideoDecoderTest, Destroy_Decoding) {
  Initialize();
  ExpectDecodingState();
  Destroy();
}

// Test destruction when decoder has hit end of stream.
TEST_F(Gav1VideoDecoderTest, Destroy_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Destroy();
}

TEST_F(Gav1VideoDecoderTest, FrameValidAfterPoolDestruction) {
  Initialize();
  Decode(i_frame_buffer_);
  Destroy();

  ASSERT_FALSE(output_frames_.empty());

  // Write to the Y plane. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  memset(output_frames_.front()->data(VideoFrame::kYPlane), 0xff,
         output_frames_.front()->rows(VideoFrame::kYPlane) *
             output_frames_.front()->stride(VideoFrame::kYPlane));
}

}  // namespace media
