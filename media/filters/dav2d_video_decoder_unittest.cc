// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dav2d_video_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

namespace {

// Minimal MSB-first bit writer for constructing compact AV2 OBU test payloads.
class BitWriter {
 public:
  void WriteBit(uint32_t bit) {
    current_byte_ = static_cast<uint8_t>((current_byte_ << 1) | (bit & 1u));
    if (++bits_in_byte_ == 8) {
      bytes_.push_back(current_byte_);
      current_byte_ = 0;
      bits_in_byte_ = 0;
    }
  }

  void WriteBits(uint32_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
      WriteBit((value >> i) & 1u);
    }
  }

  void ByteAlignWithZeroes() {
    while (bits_in_byte_ != 0) {
      WriteBit(0);
    }
  }

  const std::vector<uint8_t>& bytes() const { return bytes_; }

 private:
  std::vector<uint8_t> bytes_;
  uint8_t current_byte_ = 0;
  int bits_in_byte_ = 0;
};

// Wraps an OBU payload with a leb128 length and 1-byte AV2 OBU header.
std::vector<uint8_t> WrapObu(uint8_t obu_type,
                             const std::vector<uint8_t>& payload) {
  size_t obu_size = 1 + payload.size();
  std::vector<uint8_t> out;
  do {
    uint8_t byte = static_cast<uint8_t>(obu_size & 0x7f);
    obu_size >>= 7;
    if (obu_size != 0) {
      byte |= 0x80;
    }
    out.push_back(byte);
  } while (obu_size != 0);
  // has_extension (1 bit = 0) | type (5 bits) | tlayer_id (2 bits = 0).
  out.push_back(static_cast<uint8_t>((obu_type & 0x1f) << 2));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

// Builds a reduced-still-picture AV2 Sequence Header OBU (DAV2D_OBU_SEQ_HDR =
// 1).
std::vector<uint8_t> BuildAv2SequenceHeaderObu(int width,
                                               int height,
                                               int bit_depth,
                                               bool monochrome) {
  BitWriter bw;
  bw.WriteBit(1);      // id = vlc(0)
  bw.WriteBits(0, 5);  // profile = 0
  bw.WriteBit(1);      // reduced_still_picture_header = 1
  bw.WriteBits(0, 5);  // level = 0

  // layout: vlc(0) ("1") -> I420, vlc(1) ("010") -> I400.
  if (monochrome) {
    bw.WriteBits(0b010, 3);
  } else {
    bw.WriteBit(1);
  }

  // hbd: 8-bit -> vlc(1) ("010"), 10-bit -> vlc(0) ("1"),
  //      12-bit -> vlc(2) ("011").
  if (bit_depth == 8) {
    bw.WriteBits(0b010, 3);
  } else if (bit_depth == 10) {
    bw.WriteBit(1);
  } else {
    CHECK_EQ(bit_depth, 12);
    bw.WriteBits(0b011, 3);
  }

  bw.WriteBits(15, 4);           // width_n_bits = 16
  bw.WriteBits(15, 4);           // height_n_bits = 16
  bw.WriteBits(width - 1, 16);   // max_width
  bw.WriteBits(height - 1, 16);  // max_height
  bw.WriteBit(0);                // crop.enabled = 0
  bw.WriteBits(0b00, 2);         // sbsz = DAV2D_SB_SIZE_64
  if (!monochrome) {
    bw.WriteBit(0);  // sdp = 0
  }
  bw.WriteBit(0);  // ext_partitions = 0
  bw.WriteBit(0);  // max_pb_aspect_ratio_log2 = 3

  // Configure non-adaptive segmentation with segment 0 marked as skip so that
  // lossless intra blocks require no coefficient or segment-ID symbols.
  bw.WriteBit(0);          // segmentation.ext = 0
  bw.WriteBit(1);          // segmentation.info_present = 1
  bw.WriteBit(0);          // segmentation.adaptive = 0
  bw.WriteBits(0b010, 3);  // seg 0: delta_q=0, skip=1, globalmv=0
  bw.WriteBits(0, 21);     // segs 1..7: all disabled

  bw.WriteBit(0);  // intra_dip = 0
  bw.WriteBit(0);  // intra_edge_filter = 0
  bw.WriteBit(0);  // mrls = 0
  bw.WriteBit(0);  // cfl = 0
  if (!monochrome) {
    bw.WriteBits(0, 2);  // cfl_ds_filter_index = 0
  }
  bw.WriteBit(0);  // mhccp = 0
  bw.WriteBit(0);  // ibp = 0
  bw.WriteBit(0);  // refmv_bank = 0
  bw.WriteBit(1);  // drl_reorder = 0
  bw.WriteBit(0);  // def_max_bvp_drl_bits = 1
  bw.WriteBit(0);  // allow_max_bvp_drl_bits = 0
  bw.WriteBit(0);  // bawp = 0
  bw.WriteBit(0);  // fsc = 0
  bw.WriteBit(0);  // idtx_intra = 0
  bw.WriteBit(0);  // ist[0] = 0
  bw.WriteBit(0);  // ist[1] = 0
  if (!monochrome) {
    bw.WriteBit(0);  // chroma_dctonly = 0
  }
  bw.WriteBit(0);  // reduced_tx_part_set = 0
  if (!monochrome) {
    bw.WriteBit(0);  // cctx = 0
  }
  bw.WriteBit(0);  // tcq = 0
  bw.WriteBit(0);  // parity_hiding = 0
  if (!monochrome) {
    bw.WriteBit(0);  // separate_uv_delta_q = 0
  }
  bw.WriteBit(1);  // equal_ac_dc_q = 1
  if (!monochrome) {
    bw.WriteBits(23, 5);  // base_uvac_dq = 0 (23 - 23)
    bw.WriteBit(0);       // uvac_dq_enabled = 0
  }
  bw.WriteBit(0);      // disable_loopfilters_across_tiles = 0
  bw.WriteBit(0);      // cdef = 0
  bw.WriteBit(0);      // gdf = 0
  bw.WriteBit(0);      // restoration = 0
  bw.WriteBit(0);      // ccso = 0
  bw.WriteBits(0, 2);  // df_par_bits = 2

  // Non-adaptive 1x1 uniform tile layout.
  bw.WriteBit(1);  // tiling.present = 1
  bw.WriteBit(0);  // non-adaptive tiling
  bw.WriteBit(1);  // uniform = 1
  bw.WriteBit(0);  // log2_cols = 0 (1 tile column)
  bw.WriteBit(0);  // log2_rows = 0 (1 tile row)

  bw.WriteBit(0);  // film_grain_present = 0
  bw.WriteBit(0);  // has_extension = 0
  bw.WriteBit(1);  // trailing_one_bit = 1
  bw.ByteAlignWithZeroes();

  return WrapObu(/*DAV2D_OBU_SEQ_HDR=*/1, bw.bytes());
}

// Builds a Closed-Loop Keyframe OBU (DAV2D_OBU_CLOSED_LOOP_KF = 4) matching
// the sequence header created by BuildAv2SequenceHeaderObu().
std::vector<uint8_t> BuildAv2KeyframeObu(int bit_depth) {
  BitWriter bw;
  bw.WriteBit(1);                          // first_tile = 1
  bw.WriteBit(1);                          // multi_frame_hdr_id = vlc(0)
  bw.WriteBit(1);                          // seqhdr_idx = vlc(0)
  bw.WriteBit(0);                          // allow_screen_content_tools = 0
  bw.WriteBit(0);                          // allow_intrabc = 0
  bw.WriteBit(1);                          // disable_cdf_update = 1
  bw.WriteBits(0, bit_depth > 8 ? 9 : 8);  // quant.yac = 0 (all_lossless = 1)
  bw.WriteBit(1);                          // segmentation.enabled = 1
  bw.WriteBit(0);                          // quant.qm.enabled = 0
  bw.WriteBits(0, 2);                      // reduced_txtp_set = 0
  bw.ByteAlignWithZeroes();

  std::vector<uint8_t> payload = bw.bytes();
  // Append zeroed MSAC tile bytes so the arithmetic decoder decodes flat
  // DC_PRED skip superblocks without overreading.
  payload.resize(payload.size() + 256, 0);
  return WrapObu(/*DAV2D_OBU_CLOSED_LOOP_KF=*/4, payload);
}

scoped_refptr<DecoderBuffer> BuildAv2IFrameBuffer(int width,
                                                  int height,
                                                  int bit_depth = 8,
                                                  bool monochrome = false) {
  std::vector<uint8_t> bytes =
      BuildAv2SequenceHeaderObu(width, height, bit_depth, monochrome);
  std::vector<uint8_t> frame = BuildAv2KeyframeObu(bit_depth);
  bytes.insert(bytes.end(), frame.begin(), frame.end());
  return DecoderBuffer::CopyFrom(bytes);
}

}  // namespace

class Dav2dVideoDecoderTest : public testing::Test {
 public:
  Dav2dVideoDecoderTest()
      : decoder_(std::make_unique<Dav2dVideoDecoder>(
            std::make_unique<NullMediaLog>())),
        i_frame_buffer_(BuildAv2IFrameBuffer(320, 240)) {}

  Dav2dVideoDecoderTest(const Dav2dVideoDecoderTest&) = delete;
  Dav2dVideoDecoderTest& operator=(const Dav2dVideoDecoderTest&) = delete;

  ~Dav2dVideoDecoderTest() override { Destroy(); }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal(VideoCodec::kAV2));
  }

  void InitializeWithConfigWithResult(const VideoDecoderConfig& config,
                                      bool success) {
    base::RunLoop run_loop;
    decoder_->Initialize(
        config, true,  // Use low delay so we get 1 frame out for each frame in.
        nullptr,
        base::BindOnce(
            [](bool success, base::OnceClosure quit_closure,
               DecoderStatus status) {
              EXPECT_EQ(status.is_ok(), success);
              std::move(quit_closure).Run();
            },
            success, run_loop.QuitClosure()),
        base::BindRepeating(&Dav2dVideoDecoderTest::FrameReady,
                            base::Unretained(this)),
        base::NullCallback());
    run_loop.Run();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigWithResult(config, true);
  }

  void Reinitialize() {
    InitializeWithConfig(TestVideoConfig::Large(VideoCodec::kAV2));
  }

  void Reset() {
    base::RunLoop run_loop;
    decoder_->Reset(run_loop.QuitClosure());
    run_loop.Run();
  }

  void Destroy() { decoder_.reset(); }

  // Sets up expectations and actions to put Dav2dVideoDecoder in an active
  // decoding state.
  void ExpectDecodingState() {
    EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put Dav2dVideoDecoder in an end
  // of stream state.
  void ExpectEndOfStreamState() {
    EXPECT_TRUE(DecodeSingleFrame(DecoderBuffer::CreateEOSBuffer()).is_ok());
    ASSERT_FALSE(output_frames_.empty());
  }

  using InputBuffers = std::vector<scoped_refptr<DecoderBuffer>>;
  using OutputFrames = std::vector<scoped_refptr<VideoFrame>>;

  // Decodes all buffers in |input_buffers| and pushes all successfully decoded
  // output frames into |output_frames_|. Returns the last decode status
  // returned by the decoder.
  DecoderStatus DecodeMultipleFrames(const InputBuffers& input_buffers) {
    for (const auto& buffer : input_buffers) {
      DecoderStatus status = Decode(buffer);
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

  // Decodes |i_frame_buffer_| and then decodes |second_buffer|.
  void DecodeIFrameThenBuffer(scoped_refptr<DecoderBuffer> second_buffer,
                              const gfx::Size& expected_size) {
    Initialize();

    InputBuffers input_buffers;
    input_buffers.push_back(i_frame_buffer_);
    input_buffers.push_back(std::move(second_buffer));
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
    base::RunLoop run_loop;
    DecoderStatus status;
    decoder_->Decode(std::move(buffer), base::BindOnce(
                                            [](DecoderStatus* out_status,
                                               base::OnceClosure quit_closure,
                                               DecoderStatus status) {
                                              *out_status = std::move(status);
                                              std::move(quit_closure).Run();
                                            },
                                            &status, run_loop.QuitClosure()));
    run_loop.Run();

    return status;
  }

  void FrameReady(scoped_refptr<VideoFrame> frame) {
    DCHECK(!frame->metadata().end_of_stream);
    output_frames_.push_back(std::move(frame));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<Dav2dVideoDecoder> decoder_;

  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  OutputFrames output_frames_;
};

TEST_F(Dav2dVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(Dav2dVideoDecoderTest, Initialize_UnsupportedCodec) {
  InitializeWithConfigWithResult(TestVideoConfig::Normal(VideoCodec::kVP9),
                                 false);
}

TEST_F(Dav2dVideoDecoderTest, Initialize_EncryptedConfigFails) {
  InitializeWithConfigWithResult(
      TestVideoConfig::NormalEncrypted(VideoCodec::kAV2), false);
}

TEST_F(Dav2dVideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  Reinitialize();
}

TEST_F(Dav2dVideoDecoderTest, Reinitialize_AfterDecodeFrame) {
  Initialize();
  ExpectDecodingState();
  Reinitialize();
}

TEST_F(Dav2dVideoDecoderTest, Reinitialize_AfterReset) {
  Initialize();
  ExpectDecodingState();
  Reset();
  Reinitialize();
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_TRUE(DecodeSingleFrame(i_frame_buffer_).is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(TestVideoConfig::NormalCodedSize(), frame->visible_rect().size());
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_OutOfBandExtraData) {
  std::vector<uint8_t> seq_hdr =
      BuildAv2SequenceHeaderObu(320, 240, /*bit_depth=*/8,
                                /*monochrome=*/false);
  VideoDecoderConfig config = TestVideoConfig::Normal(VideoCodec::kAV2);
  config.SetExtraData(seq_hdr);
  InitializeWithConfig(config);

  // Decode a frame buffer that contains only a keyframe OBU without an in-band
  // sequence header OBU.
  scoped_refptr<DecoderBuffer> frame_only_buffer =
      DecoderBuffer::CopyFrom(BuildAv2KeyframeObu(/*bit_depth=*/8));
  EXPECT_TRUE(DecodeSingleFrame(frame_only_buffer).is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(TestVideoConfig::NormalCodedSize(), frame->visible_rect().size());
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_8bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(BuildAv2IFrameBuffer(320, 240, /*bit_depth=*/8,
                                                     /*monochrome=*/true))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU),
            frame->data(VideoFrame::Plane::kV));
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_10bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(BuildAv2IFrameBuffer(320, 240, /*bit_depth=*/10,
                                                     /*monochrome=*/true))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_YUV420P10, frame->format());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU),
            frame->data(VideoFrame::Plane::kV));
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_12bitMono) {
  Initialize();
  EXPECT_TRUE(DecodeSingleFrame(BuildAv2IFrameBuffer(320, 240, /*bit_depth=*/12,
                                                     /*monochrome=*/true))
                  .is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  EXPECT_EQ(PIXEL_FORMAT_YUV420P12, frame->format());
  EXPECT_EQ(frame->data(VideoFrame::Plane::kU),
            frame->data(VideoFrame::Plane::kV));
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_SideDataHdrMetadata) {
  Initialize();

  scoped_refptr<DecoderBuffer> buffer = BuildAv2IFrameBuffer(320, 240);
  skhdr::MasteringDisplayColorVolume mdcv;
  mdcv.fDisplayPrimaries.fRX = 0.1f;
  mdcv.fMaximumDisplayMasteringLuminance = 1000.0f;
  buffer->WritableSideData().hdr_metadata.SetMDCV(mdcv);

  // Simulate decoding a single frame with side data.
  EXPECT_TRUE(DecodeSingleFrame(buffer).is_ok());
  ASSERT_EQ(1U, output_frames_.size());

  const auto& frame = output_frames_.front();
  ASSERT_TRUE(frame->hdr_metadata().HasMDCV());
  EXPECT_EQ(frame->hdr_metadata().GetMDCV().fDisplayPrimaries.fRX, 0.1f);
  EXPECT_EQ(frame->hdr_metadata().GetMDCV().fMaximumDisplayMasteringLuminance,
            1000.0f);

  // Now decode an EOS buffer. This should not crash and should correctly drain
  // any remaining frames.
  output_frames_.clear();
  EXPECT_TRUE(DecodeSingleFrame(DecoderBuffer::CreateEOSBuffer()).is_ok());
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(Dav2dVideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenBuffer(BuildAv2IFrameBuffer(1280, 720), gfx::Size(1280, 720));
}

// Decode invalid OBU data which should trigger a decoder error.
TEST_F(Dav2dVideoDecoderTest, DecodeFrame_Error) {
  Initialize();
  static constexpr uint8_t kInvalidObuData[] = {0xff, 0xff, 0xff, 0xff};
  EXPECT_FALSE(
      DecodeSingleFrame(DecoderBuffer::CopyFrom(kInvalidObuData)).is_ok());
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_Timestamps) {
  Initialize();

  scoped_refptr<DecoderBuffer> first = BuildAv2IFrameBuffer(320, 240);
  first->set_timestamp(base::Milliseconds(10));
  scoped_refptr<DecoderBuffer> second = BuildAv2IFrameBuffer(320, 240);
  second->set_timestamp(base::Milliseconds(40));

  EXPECT_TRUE(DecodeMultipleFrames({std::move(first), std::move(second),
                                    DecoderBuffer::CreateEOSBuffer()})
                  .is_ok());
  ASSERT_EQ(2U, output_frames_.size());
  EXPECT_EQ(base::Milliseconds(10), output_frames_[0]->timestamp());
  EXPECT_EQ(base::Milliseconds(40), output_frames_[1]->timestamp());
}

TEST_F(Dav2dVideoDecoderTest, DecodeFrame_MissingTimestampFails) {
  Initialize();
  scoped_refptr<DecoderBuffer> buffer = BuildAv2IFrameBuffer(320, 240);
  buffer->set_timestamp(kNoTimestamp);
  EXPECT_FALSE(DecodeSingleFrame(std::move(buffer)).is_ok());
  EXPECT_TRUE(output_frames_.empty());
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(Dav2dVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(Dav2dVideoDecoderTest, Reset_Decoding) {
  Initialize();
  ExpectDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(Dav2dVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Reset();
}

// Test destruction when decoder has initialized but not decoded.
TEST_F(Dav2dVideoDecoderTest, Destroy_Initialized) {
  Initialize();
  Destroy();
}

// Test destruction when decoder has decoded single frame.
TEST_F(Dav2dVideoDecoderTest, Destroy_Decoding) {
  Initialize();
  ExpectDecodingState();
  Destroy();
}

// Test destruction when decoder has hit end of stream.
TEST_F(Dav2dVideoDecoderTest, Destroy_EndOfStream) {
  Initialize();
  ExpectDecodingState();
  ExpectEndOfStreamState();
  Destroy();
}

}  // namespace media
