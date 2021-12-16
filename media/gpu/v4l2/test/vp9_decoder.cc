// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/vp9_decoder.h"

#include <linux/media/vp9-ctrls.h>
#include <sys/ioctl.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/macros.h"

namespace media {

namespace v4l2_test {

#define SET_IF(bit_field, cond, mask) (bit_field) |= ((cond) ? (mask) : 0)

inline void conditionally_set_flag(
    struct v4l2_ctrl_vp9_frame_decode_params& params,
    bool condition,
    enum v4l2_vp9_frame_flags flag) {
  params.flags |= condition ? flag : 0;
}

void FillV4L2VP9QuantizationParams(
    const Vp9QuantizationParams& vp9_quant_params,
    struct v4l2_vp9_quantization* v4l2_quant) {
  v4l2_quant->base_q_idx =
      base::checked_cast<__u8>(vp9_quant_params.base_q_idx);
  v4l2_quant->delta_q_y_dc =
      base::checked_cast<__s8>(vp9_quant_params.delta_q_y_dc);
  v4l2_quant->delta_q_uv_dc =
      base::checked_cast<__s8>(vp9_quant_params.delta_q_uv_dc);
  v4l2_quant->delta_q_uv_ac =
      base::checked_cast<__s8>(vp9_quant_params.delta_q_uv_ac);
}

void FillV4L2VP9MvProbsParams(const Vp9FrameContext& vp9_ctx,
                              struct v4l2_vp9_mv_probabilities* v4l2_mv_probs) {
  SafeArrayMemcpy(v4l2_mv_probs->joint, vp9_ctx.mv_joint_probs);
  SafeArrayMemcpy(v4l2_mv_probs->sign, vp9_ctx.mv_sign_prob);
  SafeArrayMemcpy(v4l2_mv_probs->class_, vp9_ctx.mv_class_probs);
  SafeArrayMemcpy(v4l2_mv_probs->class0_bit, vp9_ctx.mv_class0_bit_prob);
  SafeArrayMemcpy(v4l2_mv_probs->bits, vp9_ctx.mv_bits_prob);
  SafeArrayMemcpy(v4l2_mv_probs->class0_fr, vp9_ctx.mv_class0_fr_probs);
  SafeArrayMemcpy(v4l2_mv_probs->fr, vp9_ctx.mv_fr_probs);
  SafeArrayMemcpy(v4l2_mv_probs->class0_hp, vp9_ctx.mv_class0_hp_prob);
  SafeArrayMemcpy(v4l2_mv_probs->hp, vp9_ctx.mv_hp_prob);
}

void FillV4L2VP9ProbsParams(const Vp9FrameContext& vp9_ctx,
                            struct v4l2_vp9_probabilities* v4l2_probs) {
  SafeArrayMemcpy(v4l2_probs->tx8, vp9_ctx.tx_probs_8x8);
  SafeArrayMemcpy(v4l2_probs->tx16, vp9_ctx.tx_probs_16x16);
  SafeArrayMemcpy(v4l2_probs->tx32, vp9_ctx.tx_probs_32x32);
  SafeArrayMemcpy(v4l2_probs->coef, vp9_ctx.coef_probs);
  SafeArrayMemcpy(v4l2_probs->skip, vp9_ctx.skip_prob);
  SafeArrayMemcpy(v4l2_probs->inter_mode, vp9_ctx.inter_mode_probs);
  SafeArrayMemcpy(v4l2_probs->interp_filter, vp9_ctx.interp_filter_probs);
  SafeArrayMemcpy(v4l2_probs->is_inter, vp9_ctx.is_inter_prob);
  SafeArrayMemcpy(v4l2_probs->comp_mode, vp9_ctx.comp_mode_prob);
  SafeArrayMemcpy(v4l2_probs->single_ref, vp9_ctx.single_ref_prob);
  SafeArrayMemcpy(v4l2_probs->comp_ref, vp9_ctx.comp_ref_prob);
  SafeArrayMemcpy(v4l2_probs->y_mode, vp9_ctx.y_mode_probs);
  SafeArrayMemcpy(v4l2_probs->uv_mode, vp9_ctx.uv_mode_probs);
  SafeArrayMemcpy(v4l2_probs->partition, vp9_ctx.partition_probs);

  FillV4L2VP9MvProbsParams(vp9_ctx, &v4l2_probs->mv);
}

void FillV4L2VP9LoopFilterParams(const Vp9LoopFilterParams& vp9_lf_params,
                                 struct v4l2_vp9_loop_filter* v4l2_lf) {
  SET_IF(v4l2_lf->flags, vp9_lf_params.delta_enabled,
         V4L2_VP9_LOOP_FILTER_FLAG_DELTA_ENABLED);

  SET_IF(v4l2_lf->flags, vp9_lf_params.delta_update,
         V4L2_VP9_LOOP_FILTER_FLAG_DELTA_UPDATE);

  v4l2_lf->level = vp9_lf_params.level;
  v4l2_lf->sharpness = vp9_lf_params.sharpness;
  SafeArrayMemcpy(v4l2_lf->ref_deltas, vp9_lf_params.ref_deltas);
  SafeArrayMemcpy(v4l2_lf->mode_deltas, vp9_lf_params.mode_deltas);
  SafeArrayMemcpy(v4l2_lf->level_lookup, vp9_lf_params.lvl);
}

void FillV4L2VP9SegmentationParams(const Vp9SegmentationParams& vp9_seg_params,
                                   struct v4l2_vp9_segmentation* v4l2_seg) {
  SET_IF(v4l2_seg->flags, vp9_seg_params.enabled,
         V4L2_VP9_SEGMENTATION_FLAG_ENABLED);
  SET_IF(v4l2_seg->flags, vp9_seg_params.update_map,
         V4L2_VP9_SEGMENTATION_FLAG_UPDATE_MAP);
  SET_IF(v4l2_seg->flags, vp9_seg_params.temporal_update,
         V4L2_VP9_SEGMENTATION_FLAG_TEMPORAL_UPDATE);
  SET_IF(v4l2_seg->flags, vp9_seg_params.update_data,
         V4L2_VP9_SEGMENTATION_FLAG_UPDATE_DATA);
  SET_IF(v4l2_seg->flags, vp9_seg_params.abs_or_delta_update,
         V4L2_VP9_SEGMENTATION_FLAG_ABS_OR_DELTA_UPDATE);

  SafeArrayMemcpy(v4l2_seg->tree_probs, vp9_seg_params.tree_probs);
  SafeArrayMemcpy(v4l2_seg->pred_probs, vp9_seg_params.pred_probs);

  static_assert(static_cast<size_t>(Vp9SegmentationParams::SEG_LVL_MAX) ==
                    static_cast<size_t>(V4L2_VP9_SEGMENT_FEATURE_CNT),
                "mismatch in number of segmentation features");

  for (size_t j = 0;
       j < std::extent<decltype(vp9_seg_params.feature_enabled), 0>::value;
       j++) {
    for (size_t i = 0;
         i < std::extent<decltype(vp9_seg_params.feature_enabled), 1>::value;
         i++) {
      if (vp9_seg_params.feature_enabled[j][i])
        v4l2_seg->feature_enabled[j] |= V4L2_VP9_SEGMENT_FEATURE_ENABLED(i);
    }
  }

  SafeArrayMemcpy(v4l2_seg->feature_data, vp9_seg_params.feature_data);
}

Vp9Decoder::Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       std::unique_ptr<V4L2Queue> OUTPUT_queue,
                       std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : ivf_parser_(std::move(ivf_parser)),
      vp9_parser_(
          std::make_unique<Vp9Parser>(/*parsing_compressed_header=*/false)),
      v4l2_ioctl_(std::move(v4l2_ioctl)),
      OUTPUT_queue_(std::move(OUTPUT_queue)),
      CAPTURE_queue_(std::move(CAPTURE_queue)) {}

Vp9Decoder::~Vp9Decoder() = default;

// static
std::unique_ptr<Vp9Decoder> Vp9Decoder::Create(
    std::unique_ptr<IvfParser> ivf_parser,
    const media::IvfFileHeader& file_header) {
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_VP9_FRAME;

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  const uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  LOG(INFO) << "Ivf file header: " << file_header.width << " x "
            << file_header.height;

  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/1,
      V4L2_MEMORY_MMAP, /*num_buffers=*/1);

  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kUncompressedFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/2,
      V4L2_MEMORY_MMAP, /*num_buffers=*/8);

  return base::WrapUnique(
      new Vp9Decoder(std::move(ivf_parser), std::move(v4l2_ioctl),
                     std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

bool Vp9Decoder::Initialize() {
  // TODO(stevecho): remove VIDIOC_ENUM_FRAMESIZES ioctl call
  //   after b/193237015 is resolved.
  if (!v4l2_ioctl_->EnumFrameSizes(OUTPUT_queue_->fourcc()))
    LOG(ERROR) << "EnumFrameSizes for OUTPUT queue failed.";

  if (!v4l2_ioctl_->SetFmt(OUTPUT_queue_))
    LOG(ERROR) << "SetFmt for OUTPUT queue failed.";

  gfx::Size coded_size;
  uint32_t num_planes;
  if (!v4l2_ioctl_->GetFmt(CAPTURE_queue_->type(), &coded_size, &num_planes))
    LOG(ERROR) << "GetFmt for CAPTURE queue failed.";

  CAPTURE_queue_->set_coded_size(coded_size);
  CAPTURE_queue_->set_num_planes(num_planes);

  // VIDIOC_TRY_FMT() ioctl is equivalent to VIDIOC_S_FMT
  // with one exception that it does not change driver state.
  // VIDIOC_TRY_FMT may or may not be needed; it's used by the stateful
  // Chromium V4L2VideoDecoder backend, see b/190733055#comment78.
  // TODO(b/190733055): try and remove it after landing all the code.
  if (!v4l2_ioctl_->TryFmt(CAPTURE_queue_))
    LOG(ERROR) << "TryFmt for CAPTURE queue failed.";

  if (!v4l2_ioctl_->SetFmt(CAPTURE_queue_))
    LOG(ERROR) << "SetFmt for CAPTURE queue failed.";

  if (!v4l2_ioctl_->ReqBufs(OUTPUT_queue_))
    LOG(ERROR) << "ReqBufs for OUTPUT queue failed.";

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(OUTPUT_queue_))
    LOG(ERROR) << "QueryAndMmapQueueBuffers for OUTPUT queue failed";

  if (!v4l2_ioctl_->ReqBufs(CAPTURE_queue_))
    LOG(ERROR) << "ReqBufs for CAPTURE queue failed.";

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(CAPTURE_queue_))
    LOG(ERROR) << "QueryAndMmapQueueBuffers for CAPTURE queue failed.";

  for (uint32_t i = 0; i < CAPTURE_queue_->num_buffers(); ++i) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, i))
      LOG(ERROR) << "VIDIOC_QBUF failed for CAPTURE queue.";
  }

  int media_request_fd;
  if (!v4l2_ioctl_->MediaIocRequestAlloc(&media_request_fd))
    LOG(ERROR) << "MEDIA_IOC_REQUEST_ALLOC failed";

  OUTPUT_queue_->set_media_request_fd(media_request_fd);

  if (!v4l2_ioctl_->StreamOn(OUTPUT_queue_->type()))
    LOG(ERROR) << "StreamOn for OUTPUT queue failed.";

  if (!v4l2_ioctl_->StreamOn(CAPTURE_queue_->type()))
    LOG(ERROR) << "StreamOn for CAPTURE queue failed.";

  return true;
}

void Vp9Decoder::RefreshReferenceSlots(const uint8_t refresh_frame_flags,
                                       scoped_refptr<MmapedBuffer> buffer) {
  const std::bitset<kVp9NumRefFrames> slots(refresh_frame_flags);

  static_assert(kVp9NumRefFrames == sizeof(refresh_frame_flags) * CHAR_BIT,
                "|refresh_frame_flags| size should not be larger than "
                "|kVp9NumRefFrames|");

  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    if (slots[i])
      ref_frames_[i] = buffer;
  }
}

Vp9Parser::Result Vp9Decoder::ReadNextFrame(Vp9FrameHeader& vp9_frame_header,
                                            gfx::Size& size) {
  // TODO(jchinlee): reexamine this loop for cleanup.
  while (true) {
    std::unique_ptr<DecryptConfig> null_config;
    Vp9Parser::Result res =
        vp9_parser_->ParseNextFrame(&vp9_frame_header, &size, &null_config);
    if (res == Vp9Parser::kEOStream) {
      IvfFrameHeader ivf_frame_header{};
      const uint8_t* ivf_frame_data;

      if (!ivf_parser_->ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
        return Vp9Parser::kEOStream;

      vp9_parser_->SetStream(ivf_frame_data, ivf_frame_header.frame_size,
                             /*stream_config=*/nullptr);
      continue;
    }

    return res;
  }
}

void Vp9Decoder::SetupFrameParams(
    const Vp9FrameHeader& frame_hdr,
    struct v4l2_ctrl_vp9_frame_decode_params* v4l2_frame_params) {
  conditionally_set_flag(*v4l2_frame_params,
                         frame_hdr.frame_type == Vp9FrameHeader::KEYFRAME,
                         V4L2_VP9_FRAME_FLAG_KEY_FRAME);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.show_frame,
                         V4L2_VP9_FRAME_FLAG_SHOW_FRAME);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.error_resilient_mode,
                         V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.intra_only,
                         V4L2_VP9_FRAME_FLAG_INTRA_ONLY);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.allow_high_precision_mv,
                         V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.refresh_frame_context,
                         V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX);
  conditionally_set_flag(*v4l2_frame_params,
                         frame_hdr.frame_parallel_decoding_mode,
                         V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.subsampling_x,
                         V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.subsampling_y,
                         V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING);
  conditionally_set_flag(*v4l2_frame_params, frame_hdr.color_range,
                         V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING);

  v4l2_frame_params->compressed_header_size = frame_hdr.header_size_in_bytes;
  v4l2_frame_params->uncompressed_header_size =
      frame_hdr.uncompressed_header_size;
  v4l2_frame_params->profile = frame_hdr.profile;
  // As per the VP9 specification:
  switch (frame_hdr.reset_frame_context) {
    // "0 or 1 implies don’t reset."
    case 0:
    case 1:
      v4l2_frame_params->reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
    // "2 resets just the context specified in the frame header."
    case 2:
      v4l2_frame_params->reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_SPEC;
      break;
    // "3 reset all contexts."
    case 3:
      v4l2_frame_params->reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_ALL;
      break;
    default:
      LOG(FATAL) << "Invalid reset frame context value!";
      v4l2_frame_params->reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
  }
  v4l2_frame_params->frame_context_idx =
      frame_hdr.frame_context_idx_to_save_probs;
  v4l2_frame_params->bit_depth = frame_hdr.bit_depth;
  v4l2_frame_params->interpolation_filter = frame_hdr.interpolation_filter;
  v4l2_frame_params->tile_cols_log2 = frame_hdr.tile_cols_log2;
  v4l2_frame_params->tile_rows_log2 = frame_hdr.tile_rows_log2;
  v4l2_frame_params->tx_mode = frame_hdr.compressed_header.tx_mode;
  v4l2_frame_params->reference_mode =
      frame_hdr.compressed_header.reference_mode;
  static_assert(VP9_FRAME_LAST + (V4L2_REF_ID_CNT - 1) <
                    std::extent<decltype(frame_hdr.ref_frame_sign_bias)>::value,
                "array sizes are incompatible");
  for (size_t i = 0; i < V4L2_REF_ID_CNT; i++) {
    v4l2_frame_params->ref_frame_sign_biases |=
        (frame_hdr.ref_frame_sign_bias[i + VP9_FRAME_LAST] ? (1 << i) : 0);
  }
  v4l2_frame_params->frame_width_minus_1 = frame_hdr.frame_width - 1;
  v4l2_frame_params->frame_height_minus_1 = frame_hdr.frame_height - 1;
  v4l2_frame_params->render_width_minus_1 = frame_hdr.render_width - 1;
  v4l2_frame_params->render_height_minus_1 = frame_hdr.render_height - 1;

  constexpr uint64_t kInvalidSurface = std::numeric_limits<uint32_t>::max();

  for (size_t i = 0; i < base::size(frame_hdr.ref_frame_idx); ++i) {
    const auto idx = frame_hdr.ref_frame_idx[i];

    LOG_ASSERT(idx < kVp9NumRefFrames) << "Invalid reference frame index.\n";

    static_assert(
        std::extent<decltype(frame_hdr.ref_frame_idx)>::value ==
            std::extent<decltype(v4l2_frame_params->refs)>::value,
        "The number of reference frames in |Vp9FrameHeader| does not match "
        "|v4l2_ctrl_vp9_frame_decode_params|. Fix |Vp9FrameHeader|.");

    v4l2_frame_params->refs[i] =
        ref_frames_[idx] ? ref_frames_[idx]->reference_id() : kInvalidSurface;
  }
  // TODO(stevecho): fill in the rest of |v4l2_frame_params| fields.
  FillV4L2VP9QuantizationParams(frame_hdr.quant_params,
                                &v4l2_frame_params->quant);
  FillV4L2VP9ProbsParams(frame_hdr.frame_context, &v4l2_frame_params->probs);

  const Vp9Parser::Context& context = vp9_parser_->context();
  const Vp9LoopFilterParams& lf_params = context.loop_filter();
  const Vp9SegmentationParams& segm_params = context.segmentation();

  FillV4L2VP9LoopFilterParams(lf_params, &v4l2_frame_params->lf);
  FillV4L2VP9SegmentationParams(segm_params, &v4l2_frame_params->seg);
}

Vp9Decoder::Result Vp9Decoder::DecodeNextFrame() {
  gfx::Size size;
  Vp9FrameHeader frame_hdr{};

  Vp9Parser::Result parser_res = ReadNextFrame(frame_hdr, size);
  switch (parser_res) {
    case Vp9Parser::kInvalidStream:
      LOG_ASSERT(false) << "Failed to parse frame.";
      return Vp9Decoder::kError;
    case Vp9Parser::kAwaitingRefresh:
      LOG_ASSERT(false) << "Unsupported parser return value.";
      return Vp9Decoder::kError;
    case Vp9Parser::kEOStream:
      return Vp9Decoder::kEOStream;
    case Vp9Parser::kOk:
      break;
  }

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0))
    LOG(ERROR) << "VIDIOC_QBUF failed for OUTPUT queue.";

  struct v4l2_ctrl_vp9_frame_decode_params v4l2_frame_params;
  memset(&v4l2_frame_params, 0, sizeof(v4l2_frame_params));

  SetupFrameParams(frame_hdr, &v4l2_frame_params);

  if (!v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, v4l2_frame_params))
    LOG(ERROR) << "VIDIOC_S_EXT_CTRLS failed.";

  if (!v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_))
    LOG(ERROR) << "MEDIA_REQUEST_IOC_QUEUE failed.";

  uint32_t index;

  if (!v4l2_ioctl_->DQBuf(CAPTURE_queue_, &index))
    LOG(ERROR) << "VIDIOC_DQBUF failed for CAPTURE queue.";

  if (!v4l2_ioctl_->DQBuf(OUTPUT_queue_, &index))
    LOG(ERROR) << "VIDIOC_DQBUF failed for OUTPUT queue.";

  // TODO(stevecho): With current VP9 API, VIDIOC_G_EXT_CTRLS ioctl call is
  // needed when forward probabilities update is used. With new VP9 API landing
  // in kernel 5.17, VIDIOC_G_EXT_CTRLS ioctl call is no longer needed, see:
  // https://lwn.net/Articles/855419/

  // TODO(stevecho): call RefreshReferenceSlots() once decoded buffer is ready.

  if (!v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_))
    LOG(ERROR) << "MEDIA_REQUEST_IOC_REINIT failed.";

  return Vp9Decoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
