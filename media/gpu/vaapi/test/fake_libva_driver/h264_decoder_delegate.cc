// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/fake_libva_driver/h264_decoder_delegate.h"

#include "base/bits.h"
#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_surface.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media::internal {

namespace {

#define DCHECK_FINISHED()                                                      \
  DCHECK_EQ(bits_left_in_reg_, kRegBitSize) << "Pending bits not yet written " \
                                               "to the buffer, call "          \
                                               "FinishNALU() first."

// TODO(b/328430784): Support additional H264 profiles.
enum H264ProfileIDC {
  kProfileIDCBaseline = 66,
  kProfileIDCConstrainedBaseline = kProfileIDCBaseline,
  kProfileIDCMain = 77,
};

enum H264LevelIDC : uint8_t {
  kLevelIDC1p0 = 10,
  kLevelIDC1B = 9,
  kLevelIDC1p1 = 11,
  kLevelIDC1p2 = 12,
  kLevelIDC1p3 = 13,
  kLevelIDC2p0 = 20,
  kLevelIDC2p1 = 21,
  kLevelIDC2p2 = 22,
  kLevelIDC3p0 = 30,
  kLevelIDC3p1 = 31,
  kLevelIDC3p2 = 32,
  kLevelIDC4p0 = 40,
  kLevelIDC4p1 = 41,
  kLevelIDC4p2 = 42,
  kLevelIDC5p0 = 50,
  kLevelIDC5p1 = 51,
  kLevelIDC5p2 = 52,
  kLevelIDC6p0 = 60,
  kLevelIDC6p1 = 61,
  kLevelIDC6p2 = 62,
};

struct H264NALU {
  H264NALU() = default;

  enum Type {
    kUnspecified = 0,
    kNonIDRSlice = 1,
    kSliceDataA = 2,
    kSliceDataB = 3,
    kSliceDataC = 4,
    kIDRSlice = 5,
    kSEIMessage = 6,
    kSPS = 7,
    kPPS = 8,
    kAUD = 9,
    kEOSeq = 10,
    kEOStream = 11,
    kFiller = 12,
    kSPSExt = 13,
    kPrefix = 14,
    kSubsetSPS = 15,
    kDPS = 16,
    kReserved17 = 17,
    kReserved18 = 18,
    kCodedSliceAux = 19,
    kCodedSliceExtension = 20,
  };

  // After (without) start code; we don't own the underlying memory
  // and a shallow copy should be made when copying this struct.
  raw_ptr<const uint8_t, AllowPtrArithmetic | DanglingUntriaged> data = nullptr;
  off_t size = 0;  // From after start code to start code of next NALU (or EOS).

  int nal_ref_idc = 0;
  int nal_unit_type = 0;
};

// H264BitstreamBuilder is mostly a copy&paste from Chromium's
// H26xAnnexBBitstreamBuilder
// (//media/filters/h26x_annex_b_bitstream_builder.h). The reason to not just
// include that file is that the fake libva driver is in the process of being
// moved out of the Chromium tree and into its own project, and we want to
// avoid depending on Chromium's utilities.
class H264BitstreamBuilder {
 public:
  explicit H264BitstreamBuilder(bool insert_emulation_prevention_bytes = false)
      : insert_emulation_prevention_bytes_(insert_emulation_prevention_bytes) {
    Reset();
  }

  template <typename T>
  void AppendBits(size_t num_bits, T val) {
    AppendU64(num_bits, static_cast<uint64_t>(val));
  }

  void AppendBits(size_t num_bits, bool val) {
    DCHECK_EQ(num_bits, 1ul);
    AppendBool(val);
  }

  // Append a one-bit bool/flag value |val| to the stream.
  void AppendBool(bool val) {
    if (bits_left_in_reg_ == 0u) {
      FlushReg();
    }

    reg_ <<= 1;
    reg_ |= (static_cast<uint64_t>(val) & 1u);
    --bits_left_in_reg_;
  }

  // Append a signed value in |val| in Exp-Golomb code.
  void AppendSE(int val) {
    if (val > 0) {
      AppendUE(val * 2 - 1);
    } else {
      AppendUE(-val * 2);
    }
  }

  // Append an unsigned value in |val| in Exp-Golomb code.
  void AppendUE(unsigned int val) {
    size_t num_zeros = 0u;
    unsigned int v = val + 1u;

    while (v > 1) {
      v >>= 1;
      ++num_zeros;
    }

    AppendBits(num_zeros, 0);
    AppendBits(num_zeros + 1, val + 1u);
  }

  void BeginNALU(H264NALU::Type nalu_type, int nal_ref_idc) {
    DCHECK(!in_nalu_);
    DCHECK_FINISHED();

    DCHECK_LE(nalu_type, H264NALU::kEOStream);
    DCHECK_GE(nal_ref_idc, 0);
    DCHECK_LE(nal_ref_idc, 3);

    AppendBits(32, 0x00000001);
    Flush();
    in_nalu_ = true;
    AppendBits(1, 0);  // forbidden_zero_bit.
    AppendBits(2, nal_ref_idc);
    CHECK_NE(nalu_type, 0);
    AppendBits(5, nalu_type);
  }

  void FinishNALU() {
    // RBSP stop one bit.
    AppendBits(1, 1);

    // Byte-alignment zero bits.
    AppendBits(bits_left_in_reg_ % 8, 0);

    Flush();
    in_nalu_ = false;
  }

  void Flush() {
    if (bits_left_in_reg_ != kRegBitSize) {
      FlushReg();
    }
  }

  size_t BytesInBuffer() const {
    DCHECK_FINISHED();
    return pos_;
  }

  const uint8_t* data() const {
    DCHECK(!data_.empty());
    DCHECK_FINISHED();

    return data_.data();
  }

 private:
  typedef uint64_t RegType;
  enum {
    // Sizes of reg_.
    kRegByteSize = sizeof(RegType),
    kRegBitSize = kRegByteSize * 8,
    // Amount of bytes to grow the buffer by when we run out of
    // previously-allocated memory for it.
    kGrowBytes = 4096,
  };

  void Grow() {
    static_assert(kGrowBytes >= kRegByteSize,
                  "kGrowBytes must be larger than kRegByteSize");
    auto grown = base::HeapArray<uint8_t>::Uninit(data_.size() + kGrowBytes);
    // The first `pos_` bytes in `data_` are initialized. Copy them but don't
    // read from the uninitialized stuff after it.
    grown.copy_prefix_from(data_.first(pos_));
    data_ = std::move(grown);
  }

  void Reset() {
    data_ = base::HeapArray<uint8_t>();
    pos_ = 0;
    bits_in_buffer_ = 0;
    reg_ = 0;

    Grow();

    bits_left_in_reg_ = kRegBitSize;

    in_nalu_ = false;
  }

  void AppendU64(size_t num_bits, uint64_t val) {
    CHECK_LE(num_bits, kRegBitSize);
    while (num_bits > 0u) {
      if (bits_left_in_reg_ == 0u) {
        FlushReg();
      }

      uint64_t bits_to_write =
          num_bits > bits_left_in_reg_ ? bits_left_in_reg_ : num_bits;
      uint64_t val_to_write = (val >> (num_bits - bits_to_write));
      if (bits_to_write < 64u) {
        val_to_write &= ((1ull << bits_to_write) - 1);
        reg_ <<= bits_to_write;
        reg_ |= val_to_write;
      } else {
        reg_ = val_to_write;
      }
      num_bits -= bits_to_write;
      bits_left_in_reg_ -= bits_to_write;
    }
  }

  void FlushReg() {
    // Flush all bytes that have at least one bit cached, but not more
    // (on Flush(), reg_ may not be full).
    size_t bits_in_reg = kRegBitSize - bits_left_in_reg_;
    if (bits_in_reg == 0u) {
      return;
    }

    size_t bytes_in_reg = base::bits::AlignUp(bits_in_reg, size_t{8}) / 8u;
    reg_ <<= (kRegBitSize - bits_in_reg);

    // Convert to MSB and append as such to the stream.
    std::array<uint8_t, 8> reg_be = base::U64ToBigEndian(reg_);

    if (insert_emulation_prevention_bytes_ && in_nalu_) {
      // The EPB only works on complete bytes being flushed.
      CHECK_EQ(bits_in_reg % 8u, 0u);
      // Insert emulation prevention bytes (spec 7.3.1).
      constexpr uint8_t kEmulationByte = 0x03u;

      for (size_t i = 0; i < bytes_in_reg; ++i) {
        // This will possibly check the NALU header byte. However the
        // CHECK_NE(nalu_type, 0) makes sure that it is not 0.
        if (pos_ >= 2u && data_[pos_ - 2u] == 0 && data_[pos_ - 1u] == 0u &&
            reg_be[i] <= kEmulationByte) {
          if (pos_ + 1u > data_.size()) {
            Grow();
          }
          data_[pos_++] = kEmulationByte;
          bits_in_buffer_ += 8u;
        }
        if (pos_ + 1u > data_.size()) {
          Grow();
        }
        data_[pos_++] = reg_be[i];
        bits_in_buffer_ += 8u;
      }
    } else {
      // Make sure we have enough space.
      if (pos_ + bytes_in_reg > data_.size()) {
        Grow();
      }

      data_.subspan(pos_).copy_prefix_from(
          base::span(reg_be).first(bytes_in_reg));
      bits_in_buffer_ = pos_ * 8u + bits_in_reg;
      pos_ += bytes_in_reg;
    }

    reg_ = 0u;
    bits_left_in_reg_ = kRegBitSize;
  }

  // Whether to insert emulation prevention bytes in RBSP.
  bool insert_emulation_prevention_bytes_;

  // Whether BeginNALU() has been called but not FinishNALU().
  bool in_nalu_;

  // Unused bits left in reg_.
  size_t bits_left_in_reg_;

  // Cache for appended bits. Bits are flushed to data_ with kRegByteSize
  // granularity, i.e. when reg_ becomes full, or when an explicit FlushReg()
  // is called.
  RegType reg_;

  // Current byte offset in data_ (points to the start of unwritten bits).
  size_t pos_;
  // Current last bit in data_ (points to the start of unwritten bit).
  size_t bits_in_buffer_;

  // Buffer for stream data. Only the bytes before `pos_` have been initialized.
  base::HeapArray<uint8_t> data_;
};

void BuildPackedH264SPS(const VAPictureParameterBufferH264* pic_param_buffer,
                        const VAProfile profile,
                        H264BitstreamBuilder& bitstream_builder) {
  // Build NAL header following spec section 7.3.1.
  bitstream_builder.BeginNALU(H264NALU::kSPS, 3);

  // Build SPS following spec section 7.3.2.1.
  switch (profile) {
    case VAProfileH264Baseline:
    case VAProfileH264ConstrainedBaseline:
      bitstream_builder.AppendBits(8,
                                   kProfileIDCBaseline);  // profile_idc u(8).
      bitstream_builder.AppendBool(0);     // Constraint Set0 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set1 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set2 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set3 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set4 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set5 Flag u(1).
      bitstream_builder.AppendBits(2, 0);  // Reserved zero 2bits u(2).
      bitstream_builder.AppendBits(8, kLevelIDC1p0);  // level_idc u(8).
      break;
    case VAProfileH264Main:
      bitstream_builder.AppendBits(8, kProfileIDCMain);
      bitstream_builder.AppendBool(0);     // Constraint Set0 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set1 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set2 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set3 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set4 Flag u(1).
      bitstream_builder.AppendBool(0);     // Constraint Set5 Flag u(1).
      bitstream_builder.AppendBits(2, 0);  // Reserved zero 5bits u(2).
      bitstream_builder.AppendBits(8, kLevelIDC1p3);  // level_idc u(8).
      break;
    // TODO(b/328430784): Support additional H264 profiles.
    default:
      NOTREACHED();
  }

  // TODO(b/328430784): find a way to get the seq_parameter_set_id.
  bitstream_builder.AppendUE(0);  // seq_parameter_set_id ue(v).

  bitstream_builder.AppendUE(
      pic_param_buffer->seq_fields.bits
          .log2_max_frame_num_minus4);  // log2_max_frame_num_minus4 ue(v).
  bitstream_builder.AppendUE(
      pic_param_buffer->seq_fields.bits
          .pic_order_cnt_type);  // pic_order_cnt_type ue(v).

  if (pic_param_buffer->seq_fields.bits.pic_order_cnt_type == 0) {
    // log2_max_pic_order_cnt_lsb_minus4 ue(v).
    bitstream_builder.AppendUE(
        pic_param_buffer->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4);
  } else if (pic_param_buffer->seq_fields.bits.pic_order_cnt_type == 1) {
    // Ignoring the content of this branch as we don't produce
    // pic_order_cnt_type == 1.
    NOTREACHED();
  }

  bitstream_builder.AppendUE(
      pic_param_buffer->num_ref_frames);  // num_ref_frames ue(v).
  bitstream_builder.AppendBool(
      // gaps_in_frame_num_value_allowed_flag u(1).
      pic_param_buffer->seq_fields.bits.gaps_in_frame_num_value_allowed_flag);
  bitstream_builder.AppendUE(
      pic_param_buffer
          ->picture_width_in_mbs_minus1);  // pic_width_in_mbs_minus1 ue(v).
  bitstream_builder.AppendUE(
      pic_param_buffer
          ->picture_height_in_mbs_minus1);  // pic_height_in_map_units_minus1
                                            // ue(v).
  bitstream_builder.AppendBool(
      pic_param_buffer->seq_fields.bits
          .frame_mbs_only_flag);  // frame_mbs_only_flag u(1).
  if (!pic_param_buffer->seq_fields.bits.frame_mbs_only_flag) {
    bitstream_builder.AppendBool(
        pic_param_buffer->seq_fields.bits
            .mb_adaptive_frame_field_flag);  // mb_adaptive_frame_field_flag
                                             // u(1).
  }

  bitstream_builder.AppendBool(
      pic_param_buffer->seq_fields.bits
          .direct_8x8_inference_flag);  // direct_8x8_inference_flag u(1).

  // TODO(b/328430784): find a way to get these values.
  bitstream_builder.AppendBool(0);  // frame_cropping_flag u(1).
  bitstream_builder.AppendBool(0);  // vui_parameters_present_flag u(1).

  bitstream_builder.FinishNALU();
}

void BuildPackedH264PPS(
    const VAPictureParameterBufferH264* pic_param_buffer,
    std::vector<raw_ptr<const FakeBuffer>> slice_param_buffers,
    const VAProfile profile,
    H264BitstreamBuilder& bitstream_builder) {
  // Build NAL header following spec section 7.3.1.
  bitstream_builder.BeginNALU(H264NALU::kPPS, 3);

  // Build PPS following spec section 7.3.2.2.

  // TODO(b/328430784): find a way to get these values.
  bitstream_builder.AppendUE(0);  // pic_parameter_set_id ue(v).
  bitstream_builder.AppendUE(0);  // seq_parameter_set_id ue(v).

  bitstream_builder.AppendBool(
      pic_param_buffer->pic_fields.bits
          .entropy_coding_mode_flag);  // entropy_coding_mode_flag u(1).
  bitstream_builder.AppendBool(
      pic_param_buffer->pic_fields.bits
          .pic_order_present_flag);  // pic_order_present_flag u(1).

  // TODO(b/328430784): find a way to get this value.
  bitstream_builder.AppendUE(0);  // num_slice_groups_minus1 ue(v).

  LOG_ASSERT(!slice_param_buffers.empty());
  const VASliceParameterBufferH264* first_sp =
      reinterpret_cast<VASliceParameterBufferH264*>(
          slice_param_buffers[0]->GetData());

  // TODO(b/328430784): we don't have access to the
  // num_ref_idx_l0_default_active_minus1 and
  // num_ref_idx_l1_default_active_minus1 syntax elements here. Instead, we use
  // the num_ref_idx_l0_active_minus1 and num_ref_idx_l1_active_minus1 from the
  // first slice. This may be good enough for now but will probably not work in
  // general. Figure out what to do.
  bitstream_builder.AppendUE(first_sp->num_ref_idx_l0_active_minus1);
  bitstream_builder.AppendUE(first_sp->num_ref_idx_l1_active_minus1);

  bitstream_builder.AppendBool(
      pic_param_buffer->pic_fields.bits
          .weighted_pred_flag);  // weighted_pred_flag u(1).
  bitstream_builder.AppendBits(
      2, pic_param_buffer->pic_fields.bits
             .weighted_bipred_idc);  // weighted_bipred_idc u(2).
  bitstream_builder.AppendSE(
      pic_param_buffer->pic_init_qp_minus26);  // pic_init_qp_minus26 se(v).
  bitstream_builder.AppendSE(
      pic_param_buffer->pic_init_qs_minus26);  // pic_init_qs_minus26 se(v).
  bitstream_builder.AppendSE(
      pic_param_buffer
          ->chroma_qp_index_offset);  // chroma_qp_index_offset se(v).

  // deblocking_filter_control_present_flag u(1).
  bitstream_builder.AppendBool(
      pic_param_buffer->pic_fields.bits.deblocking_filter_control_present_flag);
  bitstream_builder.AppendBool(
      pic_param_buffer->pic_fields.bits
          .constrained_intra_pred_flag);  // constrained_intra_pred_flag u(1).
  bitstream_builder.AppendBool(
      pic_param_buffer->pic_fields.bits
          .redundant_pic_cnt_present_flag);  // redundant_pic_cnt_present_flag
                                             // u(1).

  bitstream_builder.FinishNALU();
}

}  // namespace

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;

H264DecoderDelegate::H264DecoderDelegate(int picture_width_hint,
                                         int picture_height_hint,
                                         VAProfile profile)
    : profile_(profile), ts_to_render_target_(kTimestampCacheSize) {
  CHECK_EQ(WelsCreateDecoder(&(svc_decoder_.AsEphemeralRawAddr())), 0);

  int32_t num_of_threads = 1;
  svc_decoder_->SetOption(DECODER_OPTION_NUM_OF_THREADS, &num_of_threads);

  SDecodingParam sDecParam = {0};
  sDecParam.sVideoProperty.size = sizeof(sDecParam.sVideoProperty);

  CHECK_EQ(svc_decoder_->Initialize(&sDecParam), 0);
}

H264DecoderDelegate::~H264DecoderDelegate() {
  svc_decoder_->Uninitialize();
  WelsDestroyDecoder(svc_decoder_);
}

void H264DecoderDelegate::SetRenderTarget(const FakeSurface& surface) {
  render_target_ = &surface;
  ts_to_render_target_.Put(current_ts_, &surface);
}

void H264DecoderDelegate::EnqueueWork(
    const std::vector<raw_ptr<const FakeBuffer>>& buffers) {
  CHECK(render_target_);
  LOG_ASSERT(slice_data_buffers_.empty());
  for (auto buffer : buffers) {
    switch (buffer->GetType()) {
      case VASliceDataBufferType:
        slice_data_buffers_.push_back(buffer);
        break;
      case VAPictureParameterBufferType:
        pic_param_buffer_ = buffer;
        break;
      case VAIQMatrixBufferType:
        matrix_buffer_ = buffer;
        break;
      case VASliceParameterBufferType:
        slice_param_buffers_.push_back(buffer);
        break;
      default:
        break;
    };
  }
}

void H264DecoderDelegate::Run() {
  H264BitstreamBuilder bitstream_builder;

  CHECK(pic_param_buffer_);
  const VAPictureParameterBufferH264* pic_param_buffer =
      reinterpret_cast<VAPictureParameterBufferH264*>(
          pic_param_buffer_->GetData());

  BuildPackedH264SPS(pic_param_buffer, profile_, bitstream_builder);
  BuildPackedH264PPS(pic_param_buffer, slice_param_buffers_, profile_,
                     bitstream_builder);

  for (const auto& slice_data_buffer : slice_data_buffers_) {
    // Add the H264 start code for each slice.
    bitstream_builder.AppendBits(32, 0x00000001);
    const base::span<const uint8_t> data(
        reinterpret_cast<uint8_t*>(slice_data_buffer->GetData()),
        slice_data_buffer->GetDataSize());
    for (size_t i = 0; i < slice_data_buffer->GetDataSize(); i++) {
      bitstream_builder.AppendBits<uint8_t>(8, data[i]);
    }
  }

  bitstream_builder.Flush();

  unsigned char* pData[3];
  SBufferInfo sDstBufInfo;
  memset(&sDstBufInfo, 0, sizeof(SBufferInfo));
  sDstBufInfo.uiInBsTimeStamp = current_ts_++;
  CHECK_EQ(svc_decoder_->DecodeFrameNoDelay(bitstream_builder.data(),
                                            bitstream_builder.BytesInBuffer(),
                                            pData, &sDstBufInfo),
           0);

  if (sDstBufInfo.iBufferStatus == 1) {
    OnFrameReady(pData, &sDstBufInfo);
  }

  int32_t num_of_frames_in_buffer = 0;
  svc_decoder_->GetOption(DECODER_OPTION_NUM_OF_FRAMES_REMAINING_IN_BUFFER,
                          &num_of_frames_in_buffer);
  for (int32_t i = 0; i < num_of_frames_in_buffer; i++) {
    memset(&sDstBufInfo, 0, sizeof(SBufferInfo));
    svc_decoder_->FlushFrame(pData, &sDstBufInfo);
    OnFrameReady(pData, &sDstBufInfo);
  }

  slice_data_buffers_.clear();
  slice_param_buffers_.clear();
}

void H264DecoderDelegate::OnFrameReady(unsigned char* pData[3],
                                       SBufferInfo* pDstInfo) {
  const uint32_t ts = pDstInfo->uiOutYuvTimeStamp;
  auto render_target_it = ts_to_render_target_.Peek(ts);
  LOG_ASSERT(render_target_it != ts_to_render_target_.end());
  raw_ptr<const FakeSurface> render_target = render_target_it->second;
  LOG_ASSERT(render_target);

  const ScopedBOMapping& bo_mapping = render_target->GetMappedBO();
  CHECK(bo_mapping.IsValid());
  const ScopedBOMapping::ScopedAccess mapped_bo = bo_mapping.BeginAccess();

  const int convert_result = libyuv::I420ToNV12(
      /*src_y=*/static_cast<uint8_t*>(pData[0]),
      /*src_stride_y=*/
      base::checked_cast<int>(pDstInfo->UsrData.sSystemBuffer.iStride[0]),
      /*src_u=*/static_cast<uint8_t*>(pData[1]),
      /*src_stride_u=*/
      base::checked_cast<int>(pDstInfo->UsrData.sSystemBuffer.iStride[1]),
      /*src_v=*/static_cast<uint8_t*>(pData[2]),
      /*src_stride_v=*/
      base::checked_cast<int>(pDstInfo->UsrData.sSystemBuffer.iStride[1]),
      /*dst_y=*/mapped_bo.GetData(0),
      /*dst_stride_y=*/base::checked_cast<int>(mapped_bo.GetStride(0)),
      /*dst_uv=*/mapped_bo.GetData(1),
      /*dst_stride_uv=*/base::checked_cast<int>(mapped_bo.GetStride(1)),
      /*width=*/pDstInfo->UsrData.sSystemBuffer.iWidth,
      /*height=*/pDstInfo->UsrData.sSystemBuffer.iHeight);
  CHECK_EQ(convert_result, 0);
}

}  // namespace media::internal
