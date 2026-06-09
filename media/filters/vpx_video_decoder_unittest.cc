// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "media/filters/vpx_video_decoder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/byte_conversions.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkData.h"

using ::testing::_;

namespace media {

class VpxVideoDecoderTest : public testing::Test {
 public:
  VpxVideoDecoderTest()
      : decoder_(std::make_unique<VpxVideoDecoder>()),
        i_frame_buffer_(ReadTestDataFile("vp9-I-frame-320x240")) {}

  VpxVideoDecoderTest(const VpxVideoDecoderTest&) = delete;
  VpxVideoDecoderTest& operator=(const VpxVideoDecoderTest&) = delete;

  ~VpxVideoDecoderTest() override { Destroy(); }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal(VideoCodec::kVP9));
  }

  void InitializeWithConfigWithResult(const VideoDecoderConfig& config,
                                      bool success) {
    decoder_->Initialize(config, false, nullptr,
                         base::BindOnce(
                             [](bool success, DecoderStatus status) {
                               EXPECT_EQ(status.is_ok(), success);
                             },
                             success),
                         base::BindRepeating(&VpxVideoDecoderTest::FrameReady,
                                             base::Unretained(this)),
                         base::NullCallback());
    base::RunLoop().RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigWithResult(config, true);
  }

  void Reinitialize() {
    InitializeWithConfig(TestVideoConfig::Large(VideoCodec::kVP9));
  }

  void Reset() {
    decoder_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  void Destroy() {
    decoder_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Sets up expectations and actions to put VpxVideoDecoder in an active
  // decoding state.
  void ExpectDecodingState() {
    EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put VpxVideoDecoder in an end
  // of stream state.
  void ExpectEndOfStreamState() {
    EXPECT_TRUE(DecodeSingleFrame(DecoderBuffer::CreateEOSBuffer()).is_ok());
    ASSERT_FALSE(output_frames_.empty());
  }

  using InputBuffers = std::vector<scoped_refptr<DecoderBuffer>>;
  using OutputFrames = std::vector<scoped_refptr<VideoFrame>>;

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
          NOTREACHED();
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
    input_buffers.push_back(std::move(buffer));
    input_buffers.push_back(DecoderBuffer::CreateEOSBuffer());

    return DecodeMultipleFrames(input_buffers);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
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
                     base::BindOnce(&VpxVideoDecoderTest::DecodeDone,
                                    base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    return status;
  }

  void FrameReady(scoped_refptr<VideoFrame> frame) {
    DCHECK(!frame->metadata().end_of_stream);
    output_frames_.push_back(std::move(frame));
  }

  // Extracts the compressed video data from the AVPacket and also checks for
  // side data containing an alpha channel. If found, it copies the alpha data
  // into the DecoderBuffer's side data. This is necessary because FFmpeg
  // demuxes alpha channel data as side data associated with the video packet.
  static scoped_refptr<DecoderBuffer> CreateBufferWithAlphaFromPacket(
      const AVPacket* packet) {
    auto buffer = DecoderBuffer::CopyFrom(AVPacketData(*packet));
    size_t side_data_size = 0;
    uint8_t* side_data_ptr = av_packet_get_side_data(
        packet, AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL, &side_data_size);
    if (side_data_size > 8) {
      // SAFETY: The best we can do here is trust the size reported by ffmpeg.
      auto side_data =
          UNSAFE_BUFFERS(base::span(side_data_ptr, side_data_size));
      if (base::U64FromBigEndian(side_data.first<8u>()) == 1) {
        buffer->WritableSideData().alpha_data =
            base::HeapArray<uint8_t>::CopiedFrom(side_data.subspan(8u));
      }
    }
    return buffer;
  }

  MOCK_METHOD1(DecodeDone, void(DecoderStatus));

  base::test::TaskEnvironment task_env_;
  std::unique_ptr<VideoDecoder> decoder_;

  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  OutputFrames output_frames_;
};

TEST_F(VpxVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(VpxVideoDecoderTest, Reinitialize_AfterReset) {
  Initialize();
  ExpectDecodingState();
  Reset();
  Reinitialize();
}

TEST_F(VpxVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  ASSERT_EQ(1U, output_frames_.size());
}

TEST_F(VpxVideoDecoderTest, DecodeFrame_OOM) {
  Initialize();
  static_cast<VpxVideoDecoder*>(decoder_.get())
      ->force_allocation_error_for_testing();
  EXPECT_FALSE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  EXPECT_TRUE(output_frames_.empty());
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(VpxVideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("vp9-I-frame-1280x720", gfx::Size(1280, 720));
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(VpxVideoDecoderTest, Offloaded_DecodeFrame_LargerWidth) {
  decoder_ = std::make_unique<OffloadingVpxVideoDecoder>();
  DecodeIFrameThenTestFile("vp9-I-frame-1280x720", gfx::Size(1280, 720));
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(VpxVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(VpxVideoDecoderTest, Reset_Decoding) {
  Initialize();
  ExpectDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(VpxVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Reset();
}

// Test destruction when decoder has initialized but not decoded.
TEST_F(VpxVideoDecoderTest, Destroy_Initialized) {
  Initialize();
  Destroy();
}

// Test destruction when decoder has decoded single frame.
TEST_F(VpxVideoDecoderTest, Destroy_Decoding) {
  Initialize();
  ExpectDecodingState();
  Destroy();
}

// Test destruction when decoder has hit end of stream.
TEST_F(VpxVideoDecoderTest, Destroy_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Destroy();
}

TEST_F(VpxVideoDecoderTest, SimpleFrameReuse) {
  Initialize();
  Decode(i_frame_buffer_);

  ASSERT_EQ(1u, output_frames_.size());
  scoped_refptr<VideoFrame> frame = std::move(output_frames_.front());
  const uint8_t* old_y_data = frame->data(VideoFrame::Plane::kY);
  output_frames_.pop_back();

  // Clear frame reference to return the frame to the pool.
  frame = nullptr;

  // Since we're decoding I-frames which are marked as having dependent frames,
  // libvpx will still have a ref on the previous buffer. So verify we see an
  // increase to two frames.
  Decode(i_frame_buffer_);
  EXPECT_NE(old_y_data, output_frames_.front()->data(VideoFrame::Plane::kY));

  // Issuing another decode should reuse the first buffer now that the refs have
  // been dropped by the previous decode.
  Decode(i_frame_buffer_);

  ASSERT_EQ(2u, output_frames_.size());
  EXPECT_EQ(old_y_data, output_frames_.back()->data(VideoFrame::Plane::kY));
}

TEST_F(VpxVideoDecoderTest, SimpleAlphaFrameReuse) {
  VideoDecoderConfig config = TestVideoConfig::Normal(VideoCodec::kVP9);
  config.Initialize(
      config.codec(), config.profile(),
      VideoDecoderConfig::AlphaMode::kHasAlpha, config.color_space_info(),
      config.video_transformation(), config.coded_size(), config.visible_rect(),
      config.natural_size(), config.extra_data(), config.encryption_scheme());
  InitializeWithConfig(config);
  scoped_refptr<DecoderBuffer> alpha_frame = ReadTestDataFile("bear-vp9a.webm");

  // Read frames from the webm file.
  InMemoryUrlProtocol protocol(*alpha_frame, false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  auto packet = ScopedAVPacket::Allocate();

  // Decode first frame
  ASSERT_GE(av_read_frame(glue.format_context(), packet.get()), 0);
  auto buffer = CreateBufferWithAlphaFromPacket(packet.get());
  Decode(buffer);
  av_packet_unref(packet.get());

  ASSERT_EQ(1u, output_frames_.size());
  scoped_refptr<VideoFrame> frame = std::move(output_frames_.front());
  EXPECT_EQ(PIXEL_FORMAT_I420A, frame->format());
  const uint8_t* old_y_data = frame->data(VideoFrame::Plane::kY);
  const uint8_t* old_a_data = frame->data(VideoFrame::Plane::kA);
  output_frames_.pop_back();

  // Clear frame reference to return the frame to the pool.
  frame = nullptr;

  // Decode second frame.
  Decode(buffer);
  const uint8_t* mid_y_data =
      output_frames_.front()->data(VideoFrame::Plane::kY);
  const uint8_t* mid_a_data =
      output_frames_.front()->data(VideoFrame::Plane::kA);
  output_frames_.clear();

  // Issuing another decode should reuse buffers from the pool.
  Decode(buffer);

  ASSERT_EQ(1u, output_frames_.size());
  const uint8_t* new_y_data =
      output_frames_.back()->data(VideoFrame::Plane::kY);
  const uint8_t* new_a_data =
      output_frames_.back()->data(VideoFrame::Plane::kA);

  // The pool is shared, so buffers might be reused in a different order (e.g. Y
  // might get the buffer previously used for A). Because libvpx allocates the
  // new frame before releasing the old reference frame, we need to check across
  // all previously allocated buffers.
  bool reused_y = new_y_data == old_y_data || new_y_data == old_a_data ||
                  new_y_data == mid_y_data || new_y_data == mid_a_data;
  bool reused_a = new_a_data == old_y_data || new_a_data == old_a_data ||
                  new_a_data == mid_y_data || new_a_data == mid_a_data;
  EXPECT_TRUE(reused_y);
  EXPECT_TRUE(reused_a);
}

TEST_F(VpxVideoDecoderTest, SimpleFormatChange) {
  scoped_refptr<DecoderBuffer> large_frame =
      ReadTestDataFile("vp9-I-frame-1280x720");

  Initialize();
  Decode(i_frame_buffer_);
  Decode(i_frame_buffer_);
  output_frames_.clear();
  base::RunLoop().RunUntilIdle();
  Decode(large_frame);
}

TEST_F(VpxVideoDecoderTest, FrameValidAfterPoolDestruction) {
  Initialize();
  Decode(i_frame_buffer_);
  Destroy();

  // Write to the Y plane. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  std::ranges::fill(
      output_frames_.front()->writable_span(VideoFrame::Plane::kY), 0xff);
}

TEST_F(VpxVideoDecoderTest, AlphaFrameValidAfterPoolDestruction) {
  VideoDecoderConfig config = TestVideoConfig::Normal(VideoCodec::kVP9);
  config.Initialize(
      config.codec(), config.profile(),
      VideoDecoderConfig::AlphaMode::kHasAlpha, config.color_space_info(),
      config.video_transformation(), config.coded_size(), config.visible_rect(),
      config.natural_size(), config.extra_data(), config.encryption_scheme());
  InitializeWithConfig(config);
  scoped_refptr<DecoderBuffer> alpha_frame = ReadTestDataFile("bear-vp9a.webm");

  InMemoryUrlProtocol protocol(*alpha_frame, false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  auto packet = ScopedAVPacket::Allocate();
  ASSERT_GE(av_read_frame(glue.format_context(), packet.get()), 0);
  auto buffer = CreateBufferWithAlphaFromPacket(packet.get());
  Decode(std::move(buffer));
  av_packet_unref(packet.get());

  ASSERT_EQ(1u, output_frames_.size());
  EXPECT_EQ(PIXEL_FORMAT_I420A, output_frames_.front()->format());

  Destroy();

  // Write to the Y and A planes. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  std::ranges::fill(
      output_frames_.front()->writable_span(VideoFrame::Plane::kY), 0xff);
  std::ranges::fill(
      output_frames_.front()->writable_span(VideoFrame::Plane::kA), 0xff);
}

// The test stream uses profile 2, which needs high bit depth support in libvpx.
// On ARM we fail to decode the final, duplicate frame, so there is no point in
// running this test (https://crbug.com/864458).
#if !defined(LIBVPX_NO_HIGH_BIT_DEPTH) && !defined(ARCH_CPU_ARM_FAMILY)
TEST_F(VpxVideoDecoderTest, MemoryPoolAllowsMultipleDisplay) {
  // Initialize with dummy data, we could read it from the test clip, but it's
  // not necessary for this test.
  Initialize();

  scoped_refptr<DecoderBuffer> data =
      ReadTestDataFile("vp9-duplicate-frame.webm");
  InMemoryUrlProtocol protocol(*data, false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  auto packet = ScopedAVPacket::Allocate();
  while (av_read_frame(glue.format_context(), packet.get()) >= 0) {
    DecoderStatus decode_status =
        Decode(DecoderBuffer::CopyFrom(AVPacketData(*packet)));
    av_packet_unref(packet.get());
    if (!decode_status.is_ok())
      break;
  }

  ASSERT_EQ(output_frames_.size(), 26u);

  // The final frame is a duplicate of the third-from-final one.
  scoped_refptr<VideoFrame> last_frame = output_frames_[25];
  scoped_refptr<VideoFrame> dupe_frame = output_frames_[23];

  EXPECT_EQ(last_frame->data(VideoFrame::Plane::kY),
            dupe_frame->data(VideoFrame::Plane::kY));
  EXPECT_EQ(last_frame->data(VideoFrame::Plane::kU),
            dupe_frame->data(VideoFrame::Plane::kU));
  EXPECT_EQ(last_frame->data(VideoFrame::Plane::kV),
            dupe_frame->data(VideoFrame::Plane::kV));

  // This will release all frames held by the memory pool, but should not
  // release |last_frame| since we still have a ref despite sharing the same
  // memory as |dupe_frame|.
  output_frames_.clear();
  dupe_frame = nullptr;
  Destroy();

  // ASAN will be very unhappy with this line if the above is incorrect.
  std::ranges::fill(last_frame->writable_span(VideoFrame::Plane::kY), 0);
}
#endif  // !defined(LIBVPX_NO_HIGH_BIT_DEPTH) && !defined(ARCH_CPU_ARM_FAMILY)

TEST_F(VpxVideoDecoderTest, AgtmMetadata) {
  Initialize();

  scoped_refptr<DecoderBuffer> data = ReadTestDataFile("vp9-agtm.webm");
  InMemoryUrlProtocol protocol(*data, false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  auto packet = ScopedAVPacket::Allocate();
  ASSERT_GE(av_read_frame(glue.format_context(), packet.get()), 0);
  ASSERT_EQ(packet->side_data_elems, 1);
  auto buffer = DecoderBuffer::CopyFrom(AVPacketData(*packet));
  // SAFETY: The best we can do here is trust the size reported by ffmpeg.
  auto side_data = UNSAFE_BUFFERS(
      base::span(packet->side_data[0].data, packet->side_data[0].size));
  ASSERT_EQ(base::U64FromBigEndian(side_data.first<8u>()), 4u);
  buffer->WritableSideData().itu_t35_data =
      base::HeapArray<uint8_t>::CopiedFrom(side_data.subspan(8u));
  DecoderStatus decode_status = Decode(buffer);
  av_packet_unref(packet.get());
  ASSERT_TRUE(decode_status.is_ok());

  const auto& frame = output_frames_.front();
  ASSERT_TRUE(frame->hdr_metadata().has_value());
  ASSERT_TRUE(frame->hdr_metadata()->agtm.has_value());
  EXPECT_EQ(frame->hdr_metadata()->agtm->payload->size(), 533u);

  Destroy();
}

TEST_F(VpxVideoDecoderTest, AgtmMetadataWithItut35CountryCodeExtension) {
  Initialize();

  scoped_refptr<DecoderBuffer> data =
      ReadTestDataFile("vp9-agtm-country-code-extension.webm");
  InMemoryUrlProtocol protocol(*data, false);
  FFmpegGlue glue(&protocol);
  ASSERT_TRUE(glue.OpenContext());

  auto packet = ScopedAVPacket::Allocate();
  ASSERT_GE(av_read_frame(glue.format_context(), packet.get()), 0);
  ASSERT_EQ(packet->side_data_elems, 1);
  auto buffer = DecoderBuffer::CopyFrom(AVPacketData(*packet));
  // SAFETY: The best we can do here is trust the size reported by ffmpeg.
  auto side_data = UNSAFE_BUFFERS(
      base::span(packet->side_data[0].data, packet->side_data[0].size));
  ASSERT_EQ(base::U64FromBigEndian(side_data.first<8u>()), 4u);
  buffer->WritableSideData().itu_t35_data =
      base::HeapArray<uint8_t>::CopiedFrom(side_data.subspan(8u));
  DecoderStatus decode_status = Decode(buffer);
  av_packet_unref(packet.get());
  ASSERT_TRUE(decode_status.is_ok());

  const auto& frame = output_frames_.front();
  ASSERT_TRUE(frame->hdr_metadata().has_value());
  ASSERT_TRUE(frame->hdr_metadata()->agtm.has_value());
  EXPECT_EQ(frame->hdr_metadata()->agtm->payload->size(), 533u);

  Destroy();
}

}  // namespace media
