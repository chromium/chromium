// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dav1d_video_decoder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkData.h"

using ::testing::_;

namespace media {

namespace {

MATCHER(ContainsDecoderErrorLog, "") {
  return CONTAINS_STRING(arg, "dav1d_send_data() failed");
}

}  // namespace

class Dav1dVideoDecoderTest : public testing::Test {
 public:
  Dav1dVideoDecoderTest()
      : decoder_(std::make_unique<Dav1dVideoDecoder>(
            std::make_unique<NullMediaLog>())),
        i_frame_buffer_(ReadTestDataFile("av1-I-frame-320x240")) {}

  Dav1dVideoDecoderTest(const Dav1dVideoDecoderTest&) = delete;
  Dav1dVideoDecoderTest& operator=(const Dav1dVideoDecoderTest&) = delete;

  ~Dav1dVideoDecoderTest() override { Destroy(); }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal(VideoCodec::kAV1));
  }

  void InitializeWithConfigWithResult(const VideoDecoderConfig& config,
                                      bool success) {
    decoder_->Initialize(
        config, true,  // Use low delay so we get 1 frame out for each frame in.
        nullptr,
        base::BindOnce(
            [](bool success, DecoderStatus status) {
              EXPECT_EQ(status.is_ok(), success);
            },
            success),
        base::BindRepeating(&Dav1dVideoDecoderTest::FrameReady,
                            base::Unretained(this)),
        base::NullCallback());
    base::RunLoop().RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigWithResult(config, true);
  }

  void Reinitialize() {
    InitializeWithConfig(TestVideoConfig::Large(VideoCodec::kAV1));
  }

  void Reset() {
    decoder_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  void Destroy() {
    decoder_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Sets up expectations and actions to put Dav1dVideoDecoder in an active
  // decoding state.
  void ExpectDecodingState() {
    EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put Dav1dVideoDecoder in an end
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
  DecoderStatus DecodeMultipleFrames(const InputBuffers& input_buffers) {
    for (auto iter = input_buffers.begin(); iter != input_buffers.end();
         ++iter) {
      DecoderStatus status = Decode(*iter);
      switch (status.code()) {
        case DecoderStatus::Codes::kOk:
          break;
        case DecoderStatus::Codes::kAborted:
          NOTREACHED();
        default:
          DCHECK(output_frames_.empty());
          return status;
      }
    }
    return DecoderStatus::Codes::kOk;
  }

  // Decodes the single compressed frame in |buffer|.
  DecoderStatus DecodeSingleFrame(scoped_refptr<DecoderBuffer> buffer) {
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

    DecoderStatus status = DecodeMultipleFrames(input_buffers);

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

  DecoderStatus Decode(scoped_refptr<DecoderBuffer> buffer) {
    DecoderStatus status;
    EXPECT_CALL(*this, DecodeDone(_)).WillOnce(testing::SaveArg<0>(&status));

    decoder_->Decode(std::move(buffer),
                     base::BindOnce(&Dav1dVideoDecoderTest::DecodeDone,
                                    base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    return status;
  }

  void FrameReady(scoped_refptr<VideoFrame> frame) {
    DCHECK(!frame->metadata().end_of_stream);
    output_frames_.push_back(std::move(frame));
  }

  MOCK_METHOD1(DecodeDone, void(DecoderStatus));

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<Dav1dVideoDecoder> decoder_;

  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  OutputFrames output_frames_;
};

TEST_F(Dav1dVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(Dav1dVideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  Reinitialize();
}

TEST_F(Dav1dVideoDecoderTest, Reinitialize_AfterDecodeFrame) {
  Initialize();
  ExpectDecodingState();
  Reinitialize();
}

TEST_F(Dav1dVideoDecoderTest, Reinitialize_AfterReset) {
  Initialize();
  ExpectDecodingState();
  Reset();
  Reinitialize();
}

TEST_F(Dav1dVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ("52b7d8e65b031f09c0db38d1f36113a332bd7bfcafde95ee794112261535e223",
            VideoFrame::HexHashOfFrameForTesting(*frame));
}

TEST_F(Dav1dVideoDecoderTest, DecodeFrame_8bitMono) {
  Initialize();
  EXPECT_TRUE(
      DecodeSingleFrame(ReadTestDataFile("av1-monochrome-I-frame-320x240-8bpp"))
          .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU),
            frame->data(VideoFrame::Plane::kV));
  EXPECT_EQ("3d85366c6607ea2f829bd7058a3f77f23ecd10327612bc62171dbff08421e3ad",
            VideoFrame::HexHashOfFrameForTesting(*frame));
}

TEST_F(Dav1dVideoDecoderTest, DecodeFrame_10bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(
                  ReadTestDataFile("av1-monochrome-I-frame-320x240-10bpp"))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_YUV420P10, frame->format());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU),
            frame->data(VideoFrame::Plane::kV));
  EXPECT_EQ("0a659dd4f04ecee14ca1881435ad8d18ce862ef519aaa990191cc8fa0ba66eb2",
            VideoFrame::HexHashOfFrameForTesting(*frame));
}

TEST_F(Dav1dVideoDecoderTest, DecodeFrame_12bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(
                  ReadTestDataFile("av1-monochrome-I-frame-320x240-12bpp"))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_YUV420P12, frame->format());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU),
            frame->data(VideoFrame::Plane::kV));
  EXPECT_EQ("f1acdafc4a9fa0840d7d938a9dea41ac55f612cecce2d6b89095c44fc7f29c46",
            VideoFrame::HexHashOfFrameForTesting(*frame));
}

TEST_F(Dav1dVideoDecoderTest, DecodeFrame_AgtmMetadata) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_TRUE(
      DecodeSingleFrame(ReadTestDataFile("av1-I-frame-320x240-agtm")).is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  ASSERT_TRUE(frame->hdr_metadata().has_value());
  ASSERT_TRUE(frame->hdr_metadata()->agtm.has_value());
  EXPECT_EQ(frame->hdr_metadata()->agtm->payload->size(), 99u);
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(Dav1dVideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("av1-I-frame-1280x720", gfx::Size(1280, 720));
}

// Decode a VP9 frame which should trigger a decoder error.
TEST_F(Dav1dVideoDecoderTest, DecodeFrame_Error) {
  Initialize();
  EXPECT_FALSE(
      DecodeSingleFrame(ReadTestDataFile("vp9-I-frame-320x240")).is_ok());
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(Dav1dVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(Dav1dVideoDecoderTest, Reset_Decoding) {
  Initialize();
  ExpectDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(Dav1dVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Reset();
}

// Test destruction when decoder has initialized but not decoded.
TEST_F(Dav1dVideoDecoderTest, Destroy_Initialized) {
  Initialize();
  Destroy();
}

// Test destruction when decoder has decoded single frame.
TEST_F(Dav1dVideoDecoderTest, Destroy_Decoding) {
  Initialize();
  ExpectDecodingState();
  Destroy();
}

// Test destruction when decoder has hit end of stream.
TEST_F(Dav1dVideoDecoderTest, Destroy_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Destroy();
}

TEST_F(Dav1dVideoDecoderTest, FrameValidAfterPoolDestruction) {
  Initialize();
  Decode(i_frame_buffer_);
  Destroy();

  ASSERT_FALSE(output_frames_.empty());

  // Write to the Y plane. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  UNSAFE_TODO(
      memset(output_frames_.front()->writable_data(VideoFrame::Plane::kY), 0xff,
             output_frames_.front()->rows(VideoFrame::Plane::kY) *
                 output_frames_.front()->stride(VideoFrame::Plane::kY)));
}

}  // namespace media
