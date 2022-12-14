// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h264_decoder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/h264-ctrls-upstream.h>
#endif

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"

namespace media {

namespace v4l2_test {

namespace {

constexpr uint8_t zigzag_4x4[] = {0, 1,  4,  8,  5, 2,  3,  6,
                                  9, 12, 13, 10, 7, 11, 14, 15};

constexpr uint8_t zigzag_8x8[] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

// Translates SPS into h264 sps ctrl structure.
v4l2_ctrl_h264_sps SetupSPSCtrl(const H264SPS* sps) {
  struct v4l2_ctrl_h264_sps v4l2_sps = {};

  v4l2_sps.profile_idc = sps->profile_idc;
  v4l2_sps.constraint_set_flags =
      (sps->constraint_set0_flag ? V4L2_H264_SPS_CONSTRAINT_SET0_FLAG : 0) |
      (sps->constraint_set1_flag ? V4L2_H264_SPS_CONSTRAINT_SET1_FLAG : 0) |
      (sps->constraint_set2_flag ? V4L2_H264_SPS_CONSTRAINT_SET2_FLAG : 0) |
      (sps->constraint_set3_flag ? V4L2_H264_SPS_CONSTRAINT_SET3_FLAG : 0) |
      (sps->constraint_set4_flag ? V4L2_H264_SPS_CONSTRAINT_SET4_FLAG : 0) |
      (sps->constraint_set5_flag ? V4L2_H264_SPS_CONSTRAINT_SET5_FLAG : 0);

  v4l2_sps.level_idc = sps->level_idc;
  v4l2_sps.seq_parameter_set_id = sps->seq_parameter_set_id;
  v4l2_sps.chroma_format_idc = sps->chroma_format_idc;
  v4l2_sps.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
  v4l2_sps.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
  v4l2_sps.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
  v4l2_sps.pic_order_cnt_type = sps->pic_order_cnt_type;
  v4l2_sps.log2_max_pic_order_cnt_lsb_minus4 =
      sps->log2_max_pic_order_cnt_lsb_minus4;
  v4l2_sps.max_num_ref_frames = sps->max_num_ref_frames;
  v4l2_sps.num_ref_frames_in_pic_order_cnt_cycle =
      sps->num_ref_frames_in_pic_order_cnt_cycle;

  // Check that SPS offsets for ref frames size matches v4l2 sps.
  static_assert(std::extent<decltype(v4l2_sps.offset_for_ref_frame)>() ==
                    std::extent<decltype(sps->offset_for_ref_frame)>(),
                "SPS Offsets for ref frames size must match");
  for (size_t i = 0; i < std::size(v4l2_sps.offset_for_ref_frame); i++)
    v4l2_sps.offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];

  v4l2_sps.offset_for_non_ref_pic = sps->offset_for_non_ref_pic;
  v4l2_sps.offset_for_top_to_bottom_field = sps->offset_for_top_to_bottom_field;
  v4l2_sps.pic_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1;
  v4l2_sps.pic_height_in_map_units_minus1 = sps->pic_height_in_map_units_minus1;

  v4l2_sps.flags = 0;
  if (sps->separate_colour_plane_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_SEPARATE_COLOUR_PLANE;
  if (sps->qpprime_y_zero_transform_bypass_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_QPPRIME_Y_ZERO_TRANSFORM_BYPASS;
  if (sps->delta_pic_order_always_zero_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO;
  if (sps->gaps_in_frame_num_value_allowed_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_GAPS_IN_FRAME_NUM_VALUE_ALLOWED;
  if (sps->frame_mbs_only_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY;
  if (sps->mb_adaptive_frame_field_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD;
  if (sps->direct_8x8_inference_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE;

  return v4l2_sps;
}

// Translates PPS into h264 pps ctrl structure.
v4l2_ctrl_h264_pps SetupPPSCtrl(const H264PPS* pps) {
  struct v4l2_ctrl_h264_pps v4l2_pps = {};
  v4l2_pps.pic_parameter_set_id = pps->pic_parameter_set_id;
  v4l2_pps.seq_parameter_set_id = pps->seq_parameter_set_id;
  v4l2_pps.num_slice_groups_minus1 = pps->num_slice_groups_minus1;
  v4l2_pps.num_ref_idx_l0_default_active_minus1 =
      pps->num_ref_idx_l0_default_active_minus1;
  v4l2_pps.num_ref_idx_l1_default_active_minus1 =
      pps->num_ref_idx_l1_default_active_minus1;
  v4l2_pps.weighted_bipred_idc = pps->weighted_bipred_idc;
  v4l2_pps.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
  v4l2_pps.pic_init_qs_minus26 = pps->pic_init_qs_minus26;
  v4l2_pps.chroma_qp_index_offset = pps->chroma_qp_index_offset;
  v4l2_pps.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;

  v4l2_pps.flags = 0;
  if (pps->entropy_coding_mode_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE;
  if (pps->bottom_field_pic_order_in_frame_present_flag)
    v4l2_pps.flags |=
        V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT;
  if (pps->weighted_pred_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_WEIGHTED_PRED;
  if (pps->deblocking_filter_control_present_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT;
  if (pps->constrained_intra_pred_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED;
  if (pps->redundant_pic_cnt_present_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT;
  if (pps->transform_8x8_mode_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE;
  if (pps->pic_scaling_matrix_present_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT;

  return v4l2_pps;
}

// Sets up the h264 scaling matrix ctrl and checks against sps
// and pps scaling matrix sizes.
v4l2_ctrl_h264_scaling_matrix SetupScalingMatrix(const H264SPS* sps,
                                                 const H264PPS* pps) {
  struct v4l2_ctrl_h264_scaling_matrix matrix = {};

  // Makes sure that the size of the matrix scaling lists correspond
  // to the PPS scaling matrix sizes.
  static_assert(std::extent<decltype(matrix.scaling_list_4x4)>() <=
                        std::extent<decltype(pps->scaling_list4x4)>() &&
                    std::extent<decltype(matrix.scaling_list_4x4[0])>() <=
                        std::extent<decltype(pps->scaling_list4x4[0])>() &&
                    std::extent<decltype(matrix.scaling_list_8x8)>() <=
                        std::extent<decltype(pps->scaling_list8x8)>() &&
                    std::extent<decltype(matrix.scaling_list_8x8[0])>() <=
                        std::extent<decltype(pps->scaling_list8x8[0])>(),
                "PPS scaling_lists must be of correct size");

  // Makes sure that the size of the matrix scaling lists correspond
  // to the SPS scaling matrix sizes.
  static_assert(std::extent<decltype(matrix.scaling_list_4x4)>() <=
                        std::extent<decltype(sps->scaling_list4x4)>() &&
                    std::extent<decltype(matrix.scaling_list_4x4[0])>() <=
                        std::extent<decltype(sps->scaling_list4x4[0])>() &&
                    std::extent<decltype(matrix.scaling_list_8x8)>() <=
                        std::extent<decltype(sps->scaling_list8x8)>() &&
                    std::extent<decltype(matrix.scaling_list_8x8[0])>() <=
                        std::extent<decltype(sps->scaling_list8x8[0])>(),
                "SPS scaling_lists must be of correct size");

  const auto* scaling_list4x4 = &sps->scaling_list4x4[0];
  const auto* scaling_list8x8 = &sps->scaling_list8x8[0];
  if (pps->pic_scaling_matrix_present_flag) {
    scaling_list4x4 = &pps->scaling_list4x4[0];
    scaling_list8x8 = &pps->scaling_list8x8[0];
  }

  static_assert(std::extent<decltype(matrix.scaling_list_4x4), 1>() ==
                std::extent<decltype(zigzag_4x4)>());
  for (size_t i = 0; i < std::size(matrix.scaling_list_4x4); ++i) {
    for (size_t j = 0; j < std::size(matrix.scaling_list_4x4[i]); ++j) {
      matrix.scaling_list_4x4[i][zigzag_4x4[j]] = scaling_list4x4[i][j];
    }
  }

  static_assert(std::extent<decltype(matrix.scaling_list_8x8), 1>() ==
                std::extent<decltype(zigzag_8x8)>());
  for (size_t i = 0; i < std::size(matrix.scaling_list_8x8); ++i) {
    for (size_t j = 0; j < std::size(matrix.scaling_list_8x8[i]); ++j) {
      matrix.scaling_list_8x8[i][zigzag_8x8[j]] = scaling_list8x8[i][j];
    }
  }

  return matrix;
}

// Sets up h264 decode parameters ctrl from data in the H264SliceHeader.
v4l2_ctrl_h264_decode_params SetupDecodeParams(const H264SliceHeader slice) {
  struct v4l2_ctrl_h264_decode_params v4l2_decode_param = {};
  v4l2_decode_param.nal_ref_idc = slice.nal_ref_idc;
  v4l2_decode_param.frame_num = slice.frame_num;
  v4l2_decode_param.idr_pic_id = slice.idr_pic_id;
  v4l2_decode_param.pic_order_cnt_lsb = slice.pic_order_cnt_lsb;
  v4l2_decode_param.delta_pic_order_cnt_bottom =
      slice.delta_pic_order_cnt_bottom;
  v4l2_decode_param.delta_pic_order_cnt0 = slice.delta_pic_order_cnt0;
  v4l2_decode_param.delta_pic_order_cnt1 = slice.delta_pic_order_cnt1;
  v4l2_decode_param.dec_ref_pic_marking_bit_size =
      slice.dec_ref_pic_marking_bit_size;
  v4l2_decode_param.pic_order_cnt_bit_size = slice.pic_order_cnt_bit_size;

  v4l2_decode_param.flags = 0;
  if (slice.idr_pic_flag)
    v4l2_decode_param.flags |= V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC;

  v4l2_decode_param.top_field_order_cnt = 0;
  v4l2_decode_param.bottom_field_order_cnt = 0;

  return v4l2_decode_param;
}

// Determines whether the current slice is part of the same
// frame as the previous slice.
// From h264 specification 7.4.1.2.4
bool IsNewFrame(H264SliceHeader* prev_slice,
                H264SliceHeader* curr_slice,
                const H264SPS* sps) {
  bool nalu_size_error = prev_slice->nalu_size < 1;

  bool slice_changed =
      curr_slice->frame_num != prev_slice->frame_num ||
      curr_slice->pic_parameter_set_id != prev_slice->pic_parameter_set_id ||
      curr_slice->nal_ref_idc != prev_slice->nal_ref_idc ||
      curr_slice->idr_pic_flag != prev_slice->idr_pic_flag ||
      curr_slice->idr_pic_id != prev_slice->idr_pic_id;

  bool slice_pic_order_changed = false;

  if (sps->pic_order_cnt_type == 0) {
    slice_pic_order_changed =
        curr_slice->pic_order_cnt_lsb != prev_slice->pic_order_cnt_lsb ||
        curr_slice->delta_pic_order_cnt_bottom !=
            prev_slice->delta_pic_order_cnt_bottom;

  } else if (sps->pic_order_cnt_type == 1) {
    slice_pic_order_changed =
        curr_slice->delta_pic_order_cnt0 != prev_slice->delta_pic_order_cnt0 ||
        curr_slice->delta_pic_order_cnt1 != prev_slice->delta_pic_order_cnt1;
  }

  return (nalu_size_error || slice_changed || slice_pic_order_changed);
}

}  // namespace

VideoDecoder::Result H264Decoder::StartNewFrame(const int sps_id,
                                                const int pps_id) {
  const H264SPS* sps = parser_->GetSPS(sps_id);
  const H264PPS* pps = parser_->GetPPS(pps_id);

  struct v4l2_ctrl_h264_sps v4l2_sps = SetupSPSCtrl(sps);
  struct v4l2_ctrl_h264_pps v4l2_pps = SetupPPSCtrl(pps);
  struct v4l2_ctrl_h264_scaling_matrix v4l2_matrix =
      SetupScalingMatrix(sps, pps);

  struct v4l2_ext_control ctrls[] = {
      {.id = V4L2_CID_STATELESS_H264_SPS,
       .size = sizeof(v4l2_sps),
       .ptr = &v4l2_sps},
      {.id = V4L2_CID_STATELESS_H264_PPS,
       .size = sizeof(v4l2_pps),
       .ptr = &v4l2_pps},
      {.id = V4L2_CID_STATELESS_H264_SCALING_MATRIX,
       .size = sizeof(v4l2_matrix),
       .ptr = &v4l2_matrix}};
  struct v4l2_ext_controls ext_ctrls = {
      .count = (sizeof(ctrls) / sizeof(ctrls[0])), .controls = ctrls};

  if (!v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls)) {
    VLOG(4) << "VIDIOC_S_EXT_CTRLS failed.";
    return VideoDecoder::kError;
  }

  return VideoDecoder::kOk;
}

// Processes NALU's until reaching the end of the current frame.  To
// know the end of the current frame it may be necessary to start parsing
// the next frame.  If this occurs the NALU that was parsed needs to be
// held over until the next frame.  This is done in |pending_nalu_|
// Not every frame has a SPS/PPS associated with it.  The SPS/PPS must
// occur on an IDR frame.  Store the last seen slice header in
// |pending_slice_header_| so it will be available for the next frame.
H264Parser::Result H264Decoder::ProcessNextFrame(
    std::unique_ptr<H264SliceHeader>* resulting_slice_header) {
  bool reached_end_of_frame = false;
  std::unique_ptr<H264SliceHeader> curr_slice_header =
      std::move(pending_slice_header_);
  std::unique_ptr<H264NALU> nalu = std::move(pending_nalu_);
  while (!reached_end_of_frame) {
    if (!nalu) {
      nalu = std::make_unique<H264NALU>();
      if (parser_->AdvanceToNextNALU(nalu.get()) == H264Parser::kEOStream)
        break;
    }

    switch (nalu->nal_unit_type) {
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice: {
        if (!curr_slice_header) {
          curr_slice_header = std::make_unique<H264SliceHeader>();
          if (parser_->ParseSliceHeader(*nalu, curr_slice_header.get()) !=
              H264Parser::kOk)
            return H264Parser::kInvalidStream;
        }

        const int pps_id = curr_slice_header->pic_parameter_set_id;
        const int sps_id =
            parser_->GetPPS(curr_slice_header->pic_parameter_set_id)
                ->seq_parameter_set_id;

        if (!pending_slice_header_) {
          if (StartNewFrame(sps_id, pps_id) != VideoDecoder::kOk)
            return H264Parser::kInvalidStream;

          pending_slice_header_ = std::move(curr_slice_header);
          break;
        }

        if (IsNewFrame(pending_slice_header_.get(), curr_slice_header.get(),
                       parser_->GetSPS(sps_id))) {
          // The parser has read into the next frame.  This is the only
          // way that the end of the current frame is indicated.  The
          // parser can not be rewound, so the decoder needs to execute
          // the end of this frame and save the next frames nalu data.
          reached_end_of_frame = true;
          *resulting_slice_header = std::move(pending_slice_header_);
          pending_slice_header_ = std::move(curr_slice_header);
          pending_nalu_ = std::move(nalu);

          // |pending_slice_header_| needs to be set after
          // |resulting_slice_header| which can't be done at the end of the
          // function, so return here.
          return H264Parser::kOk;
        }
        // TODO(bchoobineh): Add additional logic for when
        // there are multiple slices per frame.
        break;
      }
      case H264NALU::kSPS: {
        int sps_id;
        if (parser_->ParseSPS(&sps_id) != H264Parser::kOk)
          return H264Parser::kInvalidStream;

        if (pending_slice_header_)
          reached_end_of_frame = true;
        break;
      }
      case H264NALU::kPPS: {
        int pps_id;
        if (parser_->ParsePPS(&pps_id) != H264Parser::kOk)
          return H264Parser::kInvalidStream;

        if (pending_slice_header_)
          reached_end_of_frame = true;
        break;
      }
      default: {
        reached_end_of_frame = true;
        break;
      }
    }

    nalu = nullptr;
  }

  *resulting_slice_header = std::move(pending_slice_header_);
  return H264Parser::kOk;
}

VideoDecoder::Result H264Decoder::SubmitSlice(const H264SliceHeader curr_slice,
                                              const int frame_num) {
  struct v4l2_ctrl_h264_decode_params v4l2_decode_param =
      SetupDecodeParams(curr_slice);

  struct v4l2_ext_control ctrls[] = {
      {.id = V4L2_CID_STATELESS_H264_DECODE_PARAMS,
       .size = sizeof(v4l2_decode_param),
       .ptr = &v4l2_decode_param},
      {.id = V4L2_CID_STATELESS_H264_DECODE_MODE,
       .value = V4L2_STATELESS_H264_DECODE_MODE_FRAME_BASED}};
  struct v4l2_ext_controls ext_ctrls = {
      .count = (sizeof(ctrls) / sizeof(ctrls[0])), .controls = ctrls};

  if (!v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls)) {
    VLOG(4) << "VIDIOC_S_EXT_CTRLS failed.";
    return VideoDecoder::kError;
  }

  std::vector<uint8_t> data_copy(
      sizeof(V4L2_STATELESS_H264_START_CODE_ANNEX_B) - 1);
  data_copy[2] = V4L2_STATELESS_H264_START_CODE_ANNEX_B;
  data_copy.insert(data_copy.end(), curr_slice.nalu_data,
                   curr_slice.nalu_data + curr_slice.nalu_size);

  scoped_refptr<MmapedBuffer> OUTPUT_buffer = OUTPUT_queue_->GetBuffer(0);
  OUTPUT_buffer->mmaped_planes()[0].CopyIn(&data_copy[0], data_copy.size());
  OUTPUT_buffer->set_frame_number(frame_num);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0)) {
    VLOG(4) << "VIDIOC_QBUF failed for OUTPUT queue.";
    return VideoDecoder::kError;
  }

  if (!v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_)) {
    VLOG(4) << "MEDIA_REQUEST_IOC_QUEUE failed.";
    return VideoDecoder::kError;
  }

  return VideoDecoder::kOk;
}

// static
std::unique_ptr<H264Decoder> H264Decoder::Create(
    const base::MemoryMappedFile& stream) {
  auto parser = std::make_unique<H264Parser>();
  parser->SetStream(stream.data(), stream.length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an h.264 bistreams starts with an SPS.
  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser->AdvanceToNextNALU(&nalu);
    if (res != H264Parser::kOk) {
      LOG(ERROR) << "Unable to find SPS in stream";
      return nullptr;
    }

    if (nalu.nal_unit_type == H264NALU::kSPS)
      break;
  }

  int id;
  H264Parser::Result res = parser->ParseSPS(&id);
  CHECK(res == H264Parser::kOk);

  const H264SPS* sps = parser->GetSPS(id);
  CHECK(sps);

  absl::optional<gfx::Size> coded_size = sps->GetCodedSize();
  CHECK(coded_size);
  LOG(INFO) << "h.264 coded size : " << coded_size->ToString();

  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_H264_SLICE;

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>(kDriverCodecFourcc);
  uint32_t uncompressed_fourcc = V4L2_PIX_FMT_NV12;
  int num_planes = 1;

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      uncompressed_fourcc)) {
    // Fall back to MM21 for MediaTek platforms
    uncompressed_fourcc = v4l2_fourcc('M', 'M', '2', '1');
    num_planes = 2;

    if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                        uncompressed_fourcc)) {
      LOG(ERROR) << "Device doesn't support the provided FourCCs.";
      return nullptr;
    }
  }

  // TODO(stevecho): might need to consider using more than 1 file descriptor
  // (fd) & buffer with the output queue for 4K60 requirement.
  // https://buganizer.corp.google.com/issues/202214561#comment31
  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc, coded_size.value(),
      /*num_planes=*/1, V4L2_MEMORY_MMAP, /*num_buffers=*/1);

  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, uncompressed_fourcc,
      coded_size.value(),
      /*num_planes=*/num_planes, V4L2_MEMORY_MMAP,
      /*num_buffers=*/kNumberOfBuffersInCaptureQueue);

  return base::WrapUnique(
      new H264Decoder(std::move(parser), std::move(v4l2_ioctl),
                      std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

H264Decoder::H264Decoder(std::unique_ptr<H264Parser> parser,
                         std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         std::unique_ptr<V4L2Queue> OUTPUT_queue,
                         std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)),
      parser_(std::move(parser)) {}

H264Decoder::~H264Decoder() = default;

VideoDecoder::Result H264Decoder::DecodeNextFrame(std::vector<char>& y_plane,
                                                  std::vector<char>& u_plane,
                                                  std::vector<char>& v_plane,
                                                  gfx::Size& size,
                                                  const int frame_number) {
  std::unique_ptr<H264SliceHeader> resulting_slice_header;
  if (ProcessNextFrame(&resulting_slice_header) != H264Parser::kOk) {
    VLOG(4) << "Frame Processing Failed";
    return VideoDecoder::kError;
  }

  if (!resulting_slice_header)
    return VideoDecoder::kEOStream;

  if (SubmitSlice(*resulting_slice_header, frame_number) != VideoDecoder::kOk) {
    VLOG(4) << "Slice Submission Failed";
    return VideoDecoder::kError;
  }

  uint32_t CAPTURE_index;
  if (!v4l2_ioctl_->DQBuf(CAPTURE_queue_, &CAPTURE_index)) {
    VLOG(4) << "VIDIOC_DQBUF failed for CAPTURE queue";
    return VideoDecoder::kError;
  }
  CHECK_LT(CAPTURE_index, kNumberOfBuffersInCaptureQueue)
      << "Capture Queue Index greater than number of buffers";

  scoped_refptr<MmapedBuffer> buffer = CAPTURE_queue_->GetBuffer(CAPTURE_index);
  size = CAPTURE_queue_->display_size();
  if (CAPTURE_queue_->fourcc() == V4L2_PIX_FMT_NV12) {
    CHECK_EQ(buffer->mmaped_planes().size(), 1u)
        << "NV12 should have exactly 1 plane but CAPTURE queue does not.";

    ConvertNV12ToYUV(y_plane, u_plane, v_plane, size,
                     static_cast<char*>(buffer->mmaped_planes()[0].start_addr),
                     CAPTURE_queue_->coded_size());
  } else if (CAPTURE_queue_->fourcc() == v4l2_fourcc('M', 'M', '2', '1')) {
    CHECK_EQ(buffer->mmaped_planes().size(), 2u)
        << "MM21 should have exactly 2 planes but CAPTURE queue does not.";

    ConvertMM21ToYUV(y_plane, u_plane, v_plane, size,
                     static_cast<char*>(buffer->mmaped_planes()[0].start_addr),
                     static_cast<char*>(buffer->mmaped_planes()[1].start_addr),
                     CAPTURE_queue_->coded_size());
  } else {
    LOG(FATAL) << "Unsupported CAPTURE queue format";
  }

  if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, CAPTURE_index)) {
    VLOG(4) << "VIDIOC_QBUF failed for CAPTURE queue.";
    return VideoDecoder::kError;
  }

  uint32_t OUTPUT_index;
  if (!v4l2_ioctl_->DQBuf(OUTPUT_queue_, &OUTPUT_index)) {
    VLOG(4) << "VIDIOC_DQBUF failed for OUTPUT queue.";
    return VideoDecoder::kError;
  }
  CHECK_EQ(OUTPUT_index, uint32_t(0)) << "OUTPUT Queue Index not zero";

  if (!v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_)) {
    VLOG(4) << "MEDIA_REQUEST_IOC_REINIT failed.";
    return VideoDecoder::kError;
  }

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
