// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/h264_vaapi_video_decoder_delegate.h"

#include <va/va.h>

#include "base/memory/aligned_memory.h"
#include "base/trace_event/trace_event.h"
#include "media/base/cdm_context.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

using DecodeStatus = H264Decoder::H264Accelerator::Status;

namespace {

// from ITU-T REC H.264 spec
// section 8.5.6
// "Inverse scanning process for 4x4 transform coefficients and scaling lists"
static constexpr int kZigzagScan4x4[16] = {0, 1,  4,  8,  5, 2,  3,  6,
                                           9, 12, 13, 10, 7, 11, 14, 15};

// section 8.5.7
// "Inverse scanning process for 8x8 transform coefficients and scaling lists"
static constexpr uint8_t kZigzagScan8x8[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

#if BUILDFLAG(IS_CHROMEOS_ASH)
int GetSliceHeaderCounter() {
  // Needs to be static in case there are multiple active at once, in which case
  // they all need unique values.
  static base::AtomicSequenceNumber parsed_slice_hdr_counter;
  return parsed_slice_hdr_counter.GetNext();
}
#endif

}  // namespace

// This is the size of the data block which the AMD_SLICE_PARAMS is stored in.
constexpr size_t kAmdEncryptedSliceHeaderSize = 1024;
#if BUILDFLAG(IS_CHROMEOS_ASH)
// These structures match what AMD uses to pass back the extra slice header
// parameters we need for CENCv1. This is stored in the first 1KB of the
// encrypted subsample returned by the cdm-oemcrypto daemon on ChromeOS.
typedef struct AMD_EXTRA_SLICE_PARAMS {
  uint8_t bottom_field_flag;
  uint8_t num_ref_idx_l0_active_minus1;
  uint8_t num_ref_idx_l1_active_minus1;
} AMD_EXTRA_SLICE_PARAMS;

typedef struct AMD_SLICE_PARAMS {
  AMD_EXTRA_SLICE_PARAMS va_param;
  uint8_t reserved[64 - sizeof(AMD_EXTRA_SLICE_PARAMS)];
  VACencSliceParameterBufferH264 cenc_param;
} AMD_SLICE_PARAMS;

static_assert(sizeof(AMD_SLICE_PARAMS) <= kAmdEncryptedSliceHeaderSize,
              "Invalid size for AMD_SLICE_PARAMS");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

H264VaapiVideoDecoderDelegate::H264VaapiVideoDecoderDelegate(
    VaapiDecodeSurfaceHandler* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    ProtectedSessionUpdateCB on_protected_session_update_cb,
    CdmContext* cdm_context,
    EncryptionScheme encryption_scheme)
    : VaapiVideoDecoderDelegate(vaapi_dec,
                                std::move(vaapi_wrapper),
                                std::move(on_protected_session_update_cb),
                                cdm_context,
                                encryption_scheme) {}

H264VaapiVideoDecoderDelegate::~H264VaapiVideoDecoderDelegate() = default;

scoped_refptr<H264Picture> H264VaapiVideoDecoderDelegate::CreateH264Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto va_surface_handle = vaapi_dec_->CreateSurface();
  if (!va_surface_handle) {
    return nullptr;
  }

  return new VaapiH264Picture(std::move(va_surface_handle));
}

// Fill |va_pic| with default/neutral values.
static void InitVAPicture(VAPictureH264* va_pic) {
  memset(va_pic, 0, sizeof(*va_pic));
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

void H264VaapiVideoDecoderDelegate::ProcessSPS(
    const H264SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_sps_nalu_data_.assign(sps_nalu_data.begin(), sps_nalu_data.end());
}

void H264VaapiVideoDecoderDelegate::ProcessPPS(
    const H264PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_pps_nalu_data_.assign(pps_nalu_data.begin(), pps_nalu_data.end());
}

DecodeStatus H264VaapiVideoDecoderDelegate::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu",
               "H264VaapiVideoDecoderDelegate::SubmitFrameMetadata");
  VAPictureParameterBufferH264 pic_param;
  memset(&pic_param, 0, sizeof(pic_param));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  memset(&crypto_params_, 0, sizeof(crypto_params_));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  full_sample_ = false;

#define FROM_SPS_TO_PP(a) pic_param.a = sps->a
#define FROM_SPS_TO_PP2(a, b) pic_param.b = sps->a
  FROM_SPS_TO_PP2(pic_width_in_mbs_minus1, picture_width_in_mbs_minus1);
  // This assumes non-interlaced video
  FROM_SPS_TO_PP2(pic_height_in_map_units_minus1, picture_height_in_mbs_minus1);
  FROM_SPS_TO_PP(bit_depth_luma_minus8);
  FROM_SPS_TO_PP(bit_depth_chroma_minus8);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2

#define FROM_SPS_TO_PP_SF(a) pic_param.seq_fields.bits.a = sps->a
#define FROM_SPS_TO_PP_SF2(a, b) pic_param.seq_fields.bits.b = sps->a
  FROM_SPS_TO_PP_SF(chroma_format_idc);
  FROM_SPS_TO_PP_SF2(separate_colour_plane_flag,
                     residual_colour_transform_flag);
  FROM_SPS_TO_PP_SF(gaps_in_frame_num_value_allowed_flag);
  FROM_SPS_TO_PP_SF(frame_mbs_only_flag);
  FROM_SPS_TO_PP_SF(mb_adaptive_frame_field_flag);
  FROM_SPS_TO_PP_SF(direct_8x8_inference_flag);
  pic_param.seq_fields.bits.MinLumaBiPredSize8x8 = (sps->level_idc >= 31);
  FROM_SPS_TO_PP_SF(log2_max_frame_num_minus4);
  FROM_SPS_TO_PP_SF(pic_order_cnt_type);
  FROM_SPS_TO_PP_SF(log2_max_pic_order_cnt_lsb_minus4);
  FROM_SPS_TO_PP_SF(delta_pic_order_always_zero_flag);
#undef FROM_SPS_TO_PP_SF
#undef FROM_SPS_TO_PP_SF2

#define FROM_PPS_TO_PP(a) pic_param.a = pps->a
  FROM_PPS_TO_PP(pic_init_qp_minus26);
  FROM_PPS_TO_PP(pic_init_qs_minus26);
  FROM_PPS_TO_PP(chroma_qp_index_offset);
  FROM_PPS_TO_PP(second_chroma_qp_index_offset);
#undef FROM_PPS_TO_PP

#define FROM_PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps->a
#define FROM_PPS_TO_PP_PF2(a, b) pic_param.pic_fields.bits.b = pps->a
  FROM_PPS_TO_PP_PF(entropy_coding_mode_flag);
  FROM_PPS_TO_PP_PF(weighted_pred_flag);
  FROM_PPS_TO_PP_PF(weighted_bipred_idc);
  FROM_PPS_TO_PP_PF(transform_8x8_mode_flag);

  pic_param.pic_fields.bits.field_pic_flag = 0;
  FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
  FROM_PPS_TO_PP_PF2(bottom_field_pic_order_in_frame_present_flag,
                     pic_order_present_flag);
  FROM_PPS_TO_PP_PF(deblocking_filter_control_present_flag);
  FROM_PPS_TO_PP_PF(redundant_pic_cnt_present_flag);
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#undef FROM_PPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF2

  pic_param.frame_num = pic->frame_num;

  InitVAPicture(&pic_param.CurrPic);
  FillVAPicture(&pic_param.CurrPic, std::move(pic));

  // Init reference pictures' array.
  for (int i = 0; i < 16; ++i)
    InitVAPicture(&pic_param.ReferenceFrames[i]);

  // And fill it with our reference frames.
  for (size_t i = 0; i < ref_pic_listp0.size(); i++) {
    FillVAPicture(pic_param.ReferenceFrames + i, ref_pic_listp0[i]);
  }

  pic_param.num_ref_frames = sps->max_num_ref_frames;

  VAIQMatrixBufferH264 iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(iq_matrix_buf));

  if (pps->pic_scaling_matrix_present_flag) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][kZigzagScan4x4[j]] =
            pps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][kZigzagScan8x8[j]] =
            pps->scaling_list8x8[i][j];
    }
  } else {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][kZigzagScan4x4[j]] =
            sps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][kZigzagScan8x8[j]] =
            sps->scaling_list8x8[i][j];
    }
  }

  const bool success = vaapi_wrapper_->SubmitBuffers(
      {{VAPictureParameterBufferType, sizeof(pic_param), &pic_param},
       {VAIQMatrixBufferType, sizeof(iq_matrix_buf), &iq_matrix_buf}});
  return success ? DecodeStatus::kOk : DecodeStatus::kFail;
}

DecodeStatus H264VaapiVideoDecoderDelegate::ParseEncryptedSliceHeader(
    const std::vector<base::span<const uint8_t>>& data,
    const std::vector<SubsampleEntry>& subsamples,
    uint64_t /*secure_handle*/,
    H264SliceHeader* slice_header_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(slice_header_out);
  DCHECK(!subsamples.empty());
  DCHECK(!data.empty());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto slice_param_buf = std::make_unique<VACencSliceParameterBufferH264>();
  // For AMD, we get the slice parameters as structures in the last encrypted
  // range.
  if (IsTranscrypted()) {
    if (subsamples.back().cypher_bytes < kAmdEncryptedSliceHeaderSize) {
      DLOG(ERROR) << "AMD CENCv1 data is wrong size: "
                  << subsamples.back().cypher_bytes;
      return DecodeStatus::kFail;
    }
    const AMD_SLICE_PARAMS* amd_slice_params =
        reinterpret_cast<const AMD_SLICE_PARAMS*>(
            data.back().data() + subsamples.back().clear_bytes);
    // Fill in the AMD specific params.
    slice_header_out->bottom_field_flag =
        amd_slice_params->va_param.bottom_field_flag;
    slice_header_out->num_ref_idx_l0_active_minus1 =
        amd_slice_params->va_param.num_ref_idx_l0_active_minus1;
    slice_header_out->num_ref_idx_l1_active_minus1 =
        amd_slice_params->va_param.num_ref_idx_l1_active_minus1;
    // Copy the common parameters that we will fill in below.
    memcpy(slice_param_buf.get(), &amd_slice_params->cenc_param,
           sizeof(VACencSliceParameterBufferH264));
  } else {
    // For Intel, this is done by sending in the encryption parameters and the
    // encrypted slice header. Then the vaEndPicture call is blocking while it
    // decrypts and parses the header parameters. We use VACencStatusBuf which
    // allows us to extract the slice header parameters of interest and return
    // them to the caller.

    VAEncryptionParameters crypto_params = {};
    // Don't use the VAEncryptionSegmentInfo vector in the class since we do not
    // need to hold this data across calls.
    std::vector<VAEncryptionSegmentInfo> segment_info;
    ProtectedSessionState state =
        SetupDecryptDecode(true /* full sample */, data[0].size(),
                           &crypto_params, &segment_info, subsamples);
    if (state == ProtectedSessionState::kFailed) {
      LOG(ERROR) << "ParseEncryptedSliceHeader fails because we couldn't setup "
                    "the protected session";
      return DecodeStatus::kFail;
    } else if (state != ProtectedSessionState::kCreated) {
      return DecodeStatus::kTryAgain;
    }

    // For encrypted header parsing, we need to also send the SPS and PPS. Both
    // of those and the slice NALU need to be prefixed with the 0x000001 start
    // code.
    constexpr size_t kStartCodeSize = 3;
    constexpr size_t kExtraDataBytes = 3 * kStartCodeSize;

    // Adjust the first segment length and init length to compensate for
    // inserting the SPS, PPS and 3 start codes.
    size_t size_adjustment = last_sps_nalu_data_.size() +
                             last_pps_nalu_data_.size() + kExtraDataBytes;
    size_t total_size = 0;
    size_t offset_adjustment = 0;
    for (auto& segment : segment_info) {
      segment.segment_length += size_adjustment;
      segment.init_byte_length += size_adjustment;
      segment.segment_start_offset += offset_adjustment;
      offset_adjustment += size_adjustment;
      // Any additional segments are only adjusted by the start code size;
      size_adjustment = kStartCodeSize;
      total_size += segment.segment_length;
    }

    crypto_params.status_report_index = GetSliceHeaderCounter();

    // This is based on a sample from Intel for how to use this API.
    constexpr size_t kDecryptQuerySizeAndAlignment = 4096;
    std::unique_ptr<void, base::AlignedFreeDeleter> surface_memory(
        base::AlignedAlloc(kDecryptQuerySizeAndAlignment,
                           kDecryptQuerySizeAndAlignment));
    constexpr size_t kVaQueryCencBufferSize = 2048;
    auto back_buffer_mem = std::make_unique<uint8_t[]>(kVaQueryCencBufferSize);
    VACencStatusBuf* status_buf =
        reinterpret_cast<VACencStatusBuf*>(surface_memory.get());
    status_buf->status = VA_ENCRYPTION_STATUS_INCOMPLETE;
    status_buf->buf = back_buffer_mem.get();
    status_buf->buf_size = kVaQueryCencBufferSize;

    status_buf->slice_buf_type = VaCencSliceBufParamter;
    status_buf->slice_buf_size = sizeof(VACencSliceParameterBufferH264);
    status_buf->slice_buf = slice_param_buf.get();

    constexpr int kCencStatusSurfaceDimension = 64;
    auto buffer_ptr_alloc = std::make_unique<uintptr_t>();
    uintptr_t* buffer_ptr = buffer_ptr_alloc.get();
    buffer_ptr[0] = reinterpret_cast<uintptr_t>(surface_memory.get());

    auto surface = vaapi_wrapper_->CreateVASurfaceForUserPtr(
        gfx::Size(kCencStatusSurfaceDimension, kCencStatusSurfaceDimension),
        buffer_ptr,
        3 * kCencStatusSurfaceDimension * kCencStatusSurfaceDimension);
    if (!surface) {
      DVLOG(1) << "Failed allocating surface for decrypt status";
      return DecodeStatus::kFail;
    }

    // Assembles the 'slice data' which is the SPS, PPS, encrypted SEIS and
    // encrypted slice data, each of which is also prefixed by the 0x000001
    // start code.
    std::vector<uint8_t> full_data;
    const std::vector<uint8_t> start_code = {0u, 0u, 1u};
    full_data.reserve(total_size);
    full_data.insert(full_data.end(), start_code.begin(), start_code.end());
    full_data.insert(full_data.end(), last_sps_nalu_data_.begin(),
                     last_sps_nalu_data_.end());
    full_data.insert(full_data.end(), start_code.begin(), start_code.end());
    full_data.insert(full_data.end(), last_pps_nalu_data_.begin(),
                     last_pps_nalu_data_.end());
    for (auto& nalu : data) {
      full_data.insert(full_data.end(), start_code.begin(), start_code.end());
      full_data.insert(full_data.end(), nalu.begin(), nalu.end());
    }
    if (!vaapi_wrapper_->SubmitBuffers(
            {{VAEncryptionParameterBufferType, sizeof(crypto_params),
              &crypto_params},
             {VAProtectedSliceDataBufferType, full_data.size(),
              full_data.data()}})) {
      DVLOG(1) << "Failure submitting encrypted slice header buffers";
      return DecodeStatus::kFail;
    }
    if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(surface->id())) {
      LOG(ERROR) << "Failed executing for slice header decrypt";
      return DecodeStatus::kFail;
    }
    if (status_buf->status != VA_ENCRYPTION_STATUS_SUCCESSFUL) {
      LOG(ERROR) << "Failure status in encrypted header parsing: "
                 << static_cast<int>(status_buf->status);
      return DecodeStatus::kFail;
    }
    slice_header_out->full_sample_index =
        status_buf->status_report_index_feedback;
  }

  // Read the parsed slice header data back and populate the structure with it.
  slice_header_out->idr_pic_flag = !!slice_param_buf->idr_pic_flag;
  slice_header_out->nal_ref_idc = slice_param_buf->nal_ref_idc;
  // The last span in |data| will be the slice header NALU.
  slice_header_out->nalu_data = data.back().data();
  slice_header_out->nalu_size = data.back().size();
  slice_header_out->slice_type = slice_param_buf->slice_type;
  slice_header_out->frame_num = slice_param_buf->frame_number;
  slice_header_out->idr_pic_id = slice_param_buf->idr_pic_id;
  slice_header_out->pic_order_cnt_lsb = slice_param_buf->pic_order_cnt_lsb;
  slice_header_out->delta_pic_order_cnt_bottom =
      slice_param_buf->delta_pic_order_cnt_bottom;
  slice_header_out->delta_pic_order_cnt0 =
      slice_param_buf->delta_pic_order_cnt[0];
  slice_header_out->delta_pic_order_cnt1 =
      slice_param_buf->delta_pic_order_cnt[1];
  slice_header_out->no_output_of_prior_pics_flag =
      slice_param_buf->ref_pic_fields.bits.no_output_of_prior_pics_flag;
  slice_header_out->long_term_reference_flag =
      slice_param_buf->ref_pic_fields.bits.long_term_reference_flag;
  slice_header_out->adaptive_ref_pic_marking_mode_flag =
      slice_param_buf->ref_pic_fields.bits.adaptive_ref_pic_marking_mode_flag;
  const size_t num_dec_ref_pics =
      slice_param_buf->ref_pic_fields.bits.dec_ref_pic_marking_count;
  if (num_dec_ref_pics > H264SliceHeader::kRefListSize) {
    DVLOG(1) << "Invalid number of dec_ref_pics: " << num_dec_ref_pics;
    return DecodeStatus::kFail;
  }
  for (size_t i = 0; i < num_dec_ref_pics; ++i) {
    slice_header_out->ref_pic_marking[i].memory_mgmnt_control_operation =
        slice_param_buf->memory_management_control_operation[i];
    slice_header_out->ref_pic_marking[i].difference_of_pic_nums_minus1 =
        slice_param_buf->difference_of_pic_nums_minus1[i];
    slice_header_out->ref_pic_marking[i].long_term_pic_num =
        slice_param_buf->long_term_pic_num[i];
    slice_header_out->ref_pic_marking[i].long_term_frame_idx =
        slice_param_buf->long_term_frame_idx[i];
    slice_header_out->ref_pic_marking[i].max_long_term_frame_idx_plus1 =
        slice_param_buf->max_long_term_frame_idx_plus1[i];
  }
  slice_header_out->full_sample_encryption = true;
  return DecodeStatus::kOk;
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
  return DecodeStatus::kFail;
#endif
}

DecodeStatus H264VaapiVideoDecoderDelegate::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "H264VaapiVideoDecoderDelegate::SubmitSlice");
  if (slice_hdr->full_sample_encryption && !IsTranscrypted()) {
    // We do not need to submit all the slice data, instead we just submit the
    // index for what was already sent for parsing. The HW decoder already has
    // the full slice data from when we decrypted the header on Intel.
    full_sample_ = true;
    VACencStatusParameters cenc_status = {};
    cenc_status.status_report_index_feedback = slice_hdr->full_sample_index;
    return vaapi_wrapper_->SubmitBuffer(VACencStatusParameterBufferType,
                                        sizeof(VACencStatusParameters),
                                        &cenc_status)
               ? DecodeStatus::kOk
               : DecodeStatus::kFail;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsEncryptedSession()) {
    const ProtectedSessionState state = SetupDecryptDecode(
        /*full_sample=*/false, size, &crypto_params_, &encryption_segment_info_,
        subsamples);
    if (state == ProtectedSessionState::kFailed) {
      LOG(ERROR) << "SubmitSlice fails because we couldn't setup the protected "
                    "session";
      return DecodeStatus::kFail;
    } else if (state != ProtectedSessionState::kCreated) {
      return DecodeStatus::kTryAgain;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  VASliceParameterBufferH264 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));

  slice_param.slice_data_size = slice_hdr->nalu_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.slice_data_bit_offset = slice_hdr->header_bit_size;

#define SHDRToSP(a) slice_param.a = slice_hdr->a
  SHDRToSP(first_mb_in_slice);
  slice_param.slice_type = slice_hdr->slice_type % 5;
  SHDRToSP(direct_spatial_mv_pred_flag);

  // TODO posciak: make sure parser sets those even when override flags
  // in slice header is off.
  SHDRToSP(num_ref_idx_l0_active_minus1);
  SHDRToSP(num_ref_idx_l1_active_minus1);
  SHDRToSP(cabac_init_idc);
  SHDRToSP(slice_qp_delta);
  SHDRToSP(disable_deblocking_filter_idc);
  SHDRToSP(slice_alpha_c0_offset_div2);
  SHDRToSP(slice_beta_offset_div2);

  if (((slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) &&
       pps->weighted_pred_flag) ||
      (slice_hdr->IsBSlice() && pps->weighted_bipred_idc == 1)) {
    SHDRToSP(luma_log2_weight_denom);
    SHDRToSP(chroma_log2_weight_denom);

    SHDRToSP(luma_weight_l0_flag);
    SHDRToSP(luma_weight_l1_flag);

    SHDRToSP(chroma_weight_l0_flag);
    SHDRToSP(chroma_weight_l1_flag);

    for (int i = 0; i <= slice_param.num_ref_idx_l0_active_minus1; ++i) {
      slice_param.luma_weight_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_weight[i];
      slice_param.luma_offset_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_offset[i];

      for (int j = 0; j < 2; ++j) {
        slice_param.chroma_weight_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_weight[i][j];
        slice_param.chroma_offset_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_offset[i][j];
      }
    }

    if (slice_hdr->IsBSlice()) {
      for (int i = 0; i <= slice_param.num_ref_idx_l1_active_minus1; ++i) {
        slice_param.luma_weight_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_weight[i];
        slice_param.luma_offset_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_offset[i];

        for (int j = 0; j < 2; ++j) {
          slice_param.chroma_weight_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_weight[i][j];
          slice_param.chroma_offset_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_offset[i][j];
        }
      }
    }
  }

  static_assert(
      std::size(slice_param.RefPicList0) == std::size(slice_param.RefPicList1),
      "Invalid RefPicList sizes");

  for (size_t i = 0; i < std::size(slice_param.RefPicList0); ++i) {
    InitVAPicture(&slice_param.RefPicList0[i]);
    InitVAPicture(&slice_param.RefPicList1[i]);
  }

  for (size_t i = 0;
       i < ref_pic_list0.size() && i < std::size(slice_param.RefPicList0);
       ++i) {
    if (ref_pic_list0[i])
      FillVAPicture(&slice_param.RefPicList0[i], ref_pic_list0[i]);
  }
  for (size_t i = 0;
       i < ref_pic_list1.size() && i < std::size(slice_param.RefPicList1);
       ++i) {
    if (ref_pic_list1[i])
      FillVAPicture(&slice_param.RefPicList1[i], ref_pic_list1[i]);
  }
  if (IsTranscrypted()) {
    CHECK_EQ(subsamples.size(), 1u);
    uint32_t cypher_skip =
        slice_hdr->full_sample_encryption ? kAmdEncryptedSliceHeaderSize : 0;
    return vaapi_wrapper_->SubmitBuffers(
               {{VAProtectedSliceDataBufferType, GetDecryptKeyId().length(),
                 GetDecryptKeyId().data()},
                {VASliceParameterBufferType, sizeof(slice_param), &slice_param},
                {VASliceDataBufferType,
                 subsamples[0].cypher_bytes - cypher_skip,
                 data + subsamples[0].clear_bytes + cypher_skip}})
               ? DecodeStatus::kOk
               : DecodeStatus::kFail;
  }

  return vaapi_wrapper_->SubmitBuffers(
             {{VASliceParameterBufferType, sizeof(slice_param), &slice_param},
              {VASliceDataBufferType, size, data}})
             ? DecodeStatus::kOk
             : DecodeStatus::kFail;
}

DecodeStatus H264VaapiVideoDecoderDelegate::SubmitDecode(
    scoped_refptr<H264Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "H264VaapiVideoDecoderDelegate::SubmitDecode");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsEncryptedSession() && !full_sample_ &&
      !vaapi_wrapper_->SubmitBuffer(VAEncryptionParameterBufferType,
                                    sizeof(crypto_params_), &crypto_params_)) {
    return DecodeStatus::kFail;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  const VaapiH264Picture* vaapi_pic = pic->AsVaapiH264Picture();
  const bool success = vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
      vaapi_pic->va_surface_id());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  encryption_segment_info_.clear();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (!success && NeedsProtectedSessionRecovery())
    return DecodeStatus::kTryAgain;

  if (success && IsEncryptedSession())
    ProtectedDecodedSucceeded();
  return success ? DecodeStatus::kOk : DecodeStatus::kFail;
}

bool H264VaapiVideoDecoderDelegate::OutputPicture(
    scoped_refptr<H264Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const VaapiH264Picture* vaapi_pic = pic->AsVaapiH264Picture();
  vaapi_dec_->SurfaceReady(vaapi_pic->va_surface_id(),
                           vaapi_pic->bitstream_id(), vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

void H264VaapiVideoDecoderDelegate::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  encryption_segment_info_.clear();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  vaapi_wrapper_->DestroyPendingBuffers();
}

DecodeStatus H264VaapiVideoDecoderDelegate::SetStream(
    base::span<const uint8_t> /*stream*/,
    const DecryptConfig* decrypt_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!decrypt_config)
    return Status::kOk;
  return SetDecryptConfig(decrypt_config->Clone()) ? Status::kOk
                                                   : Status::kFail;
}

bool H264VaapiVideoDecoderDelegate::RequiresRefLists() {
  return true;
}

void H264VaapiVideoDecoderDelegate::FillVAPicture(
    VAPictureH264* va_pic,
    scoped_refptr<H264Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VASurfaceID va_surface_id = VA_INVALID_SURFACE;

  if (!pic->nonexisting)
    va_surface_id = pic->AsVaapiH264Picture()->va_surface_id();

  va_pic->picture_id = va_surface_id;
  va_pic->frame_idx = pic->frame_num;
  va_pic->flags = 0;

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      break;
    case H264Picture::FIELD_TOP:
      va_pic->flags |= VA_PICTURE_H264_TOP_FIELD;
      break;
    case H264Picture::FIELD_BOTTOM:
      va_pic->flags |= VA_PICTURE_H264_BOTTOM_FIELD;
      break;
  }

  if (pic->ref) {
    va_pic->flags |= pic->long_term ? VA_PICTURE_H264_LONG_TERM_REFERENCE
                                    : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  }

  va_pic->TopFieldOrderCnt = pic->top_field_order_cnt;
  va_pic->BottomFieldOrderCnt = pic->bottom_field_order_cnt;
}

}  // namespace media
