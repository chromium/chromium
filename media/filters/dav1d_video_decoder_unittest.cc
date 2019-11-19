// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/dav1d_video_decoder.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace media {

// TODO(dalecurtis): There is some weird collision going on with this matcher
// and the one defined in the AomVideoDecoder unittest. Somehow only one ends
// up in the final binary so one test or the other will fail. To workaround this
// give it a unique name for now...
MATCHER(ContainsDav1dDecoderErrorLog, "") {
  return CONTAINS_STRING(arg, "dav1d_get_picture() failed");
}

class Dav1dVideoDecoderTest : public testing::Test {
 public:
  Dav1dVideoDecoderTest()
      : decoder_(new Dav1dVideoDecoder(&media_log_)),
        i_frame_buffer_(ReadTestDataFile("av1-I-frame-320x240")) {}

  ~Dav1dVideoDecoderTest() override { Destroy(); }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal(kCodecAV1));
  }

  void InitializeWithConfigWithResult(const VideoDecoderConfig& config,
                                      bool success) {
    decoder_->Initialize(
        config, true,  // Use low delay so we get 1 frame out for each frame in.
        nullptr, NewExpectedBoolCB(success),
        base::BindRepeating(&Dav1dVideoDecoderTest::FrameReady,
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

  // Sets up expectations and actions to put Dav1dVideoDecoder in an active
  // decoding state.
  void ExpectDecodingState() {
    EXPECT_EQ(DecodeStatus::OK, DecodeSingleFrame(i_frame_buffer_));
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put Dav1dVideoDecoder in an end
  // of stream state.
  void ExpectEndOfStreamState() {
    EXPECT_EQ(DecodeStatus::OK,
              DecodeSingleFrame(DecoderBuffer::CreateEOSBuffer()));
    ASSERT_FALSE(output_frames_.empty());
  }

  using InputBuffers = std::vector<scoped_refptr<DecoderBuffer>>;
  using OutputFrames = std::vector<scoped_refptr<VideoFrame>>;

  // Decodes all buffers in |input_buffers| and push all successfully decoded
  // output frames into |output_frames|. Returns the last decode status returned
  // by the decoder.
  DecodeStatus DecodeMultipleFrames(const InputBuffers& input_buffers) {
    for (auto iter = input_buffers.begin(); iter != input_buffers.end();
         ++iter) {
      DecodeStatus status = Decode(*iter);
      switch (status) {
        case DecodeStatus::OK:
          break;
        case DecodeStatus::ABORTED:
          NOTREACHED();
          FALLTHROUGH;
        case DecodeStatus::DECODE_ERROR:
          DCHECK(output_frames_.empty());
          return status;
      }
    }
    return DecodeStatus::OK;
  }

  // Decodes the single compressed frame in |buffer|.
  DecodeStatus DecodeSingleFrame(scoped_refptr<DecoderBuffer> buffer) {
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

    DecodeStatus status = DecodeMultipleFrames(input_buffers);

    EXPECT_EQ(DecodeStatus::OK, status);
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

  DecodeStatus Decode(scoped_refptr<DecoderBuffer> buffer) {
    DecodeStatus status;
    EXPECT_CALL(*this, DecodeDone(_)).WillOnce(testing::SaveArg<0>(&status));

    decoder_->Decode(std::move(buffer),
                     base::BindRepeating(&Dav1dVideoDecoderTest::DecodeDone,
                                         base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    return status;
  }

  void FrameReady(scoped_refptr<VideoFrame> frame) {
    DCHECK(!frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
    output_frames_.push_back(std::move(frame));
  }

  MOCK_METHOD1(DecodeDone, void(DecodeStatus));

  testing::StrictMock<MockMediaLog> media_log_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<Dav1dVideoDecoder> decoder_;

  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  OutputFrames output_frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Dav1dVideoDecoderTest);
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
  EXPECT_EQ(DecodeStatus::OK, DecodeSingleFrame(i_frame_buffer_));
  ASSERT_EQ(1U, output_frames_.size());
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
// TODO(dalecurtis): Get an I-frame from a larger video.
TEST_F(Dav1dVideoDecoderTest, DISABLED_DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("av1-I-frame-320x240", gfx::Size(1280, 720));
}

// Decode a VP9 frame which should trigger a decoder error.
TEST_F(Dav1dVideoDecoderTest, DecodeFrame_Error) {
  Initialize();
  EXPECT_MEDIA_LOG(ContainsDav1dDecoderErrorLog());
  DecodeSingleFrame(ReadTestDataFile("vp9-I-frame-320x240"));
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
  memset(output_frames_.front()->data(VideoFrame::kYPlane), 0xff,
         output_frames_.front()->rows(VideoFrame::kYPlane) *
             output_frames_.front()->stride(VideoFrame::kYPlane));
}

}  // namespace media
