// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_vp9_accelerator.h"

#include <type_traits>

#include "base/stl_util.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vp9_picture.h"

namespace media {

VaapiVP9Accelerator::VaapiVP9Accelerator(
    DecodeSurfaceHandler<VASurface>* vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : vaapi_wrapper_(vaapi_wrapper), vaapi_dec_(vaapi_dec) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVP9Accelerator::~VaapiVP9Accelerator() {
  // TODO(mcasas): consider enabling the checker, https://crbug.com/789160
  // CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VP9Picture> VaapiVP9Accelerator::CreateVP9Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto va_surface = vaapi_dec_->CreateSurface();
  if (!va_surface)
    return nullptr;

  return new VaapiVP9Picture(std::move(va_surface));
}

bool VaapiVP9Accelerator::SubmitDecode(
    scoped_refptr<VP9Picture> pic,
    const Vp9SegmentationParams& seg,
    const Vp9LoopFilterParams& lf,
    const Vp9ReferenceFrameVector& ref_frames,
    const base::Closure& done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |done_cb| should be null as we return false from IsFrameContextRequired().
  DCHECK(!done_cb);

  VADecPictureParameterBufferVP9 pic_param;
  memset(&pic_param, 0, sizeof(pic_param));

  const Vp9FrameHeader* frame_hdr = pic->frame_hdr.get();
  DCHECK(frame_hdr);

  pic_param.frame_width = base::checked_cast<uint16_t>(frame_hdr->frame_width);
  pic_param.frame_height =
      base::checked_cast<uint16_t>(frame_hdr->frame_height);
  CHECK_EQ(kVp9NumRefFrames, base::size(pic_param.reference_frames));
  for (size_t i = 0; i < base::size(pic_param.reference_frames); ++i) {
    auto ref_pic = ref_frames.GetFrame(i);
    if (ref_pic) {
      pic_param.reference_frames[i] =
          ref_pic->AsVaapiVP9Picture()->GetVASurfaceID();
    } else {
      pic_param.reference_frames[i] = VA_INVALID_SURFACE;
    }
  }

#define FHDR_TO_PP_PF1(a) pic_param.pic_fields.bits.a = frame_hdr->a
#define FHDR_TO_PP_PF2(a, b) pic_param.pic_fields.bits.a = b
  FHDR_TO_PP_PF2(subsampling_x, frame_hdr->subsampling_x == 1);
  FHDR_TO_PP_PF2(subsampling_y, frame_hdr->subsampling_y == 1);
  FHDR_TO_PP_PF2(frame_type, frame_hdr->IsKeyframe() ? 0 : 1);
  FHDR_TO_PP_PF1(show_frame);
  FHDR_TO_PP_PF1(error_resilient_mode);
  FHDR_TO_PP_PF1(intra_only);
  FHDR_TO_PP_PF1(allow_high_precision_mv);
  FHDR_TO_PP_PF2(mcomp_filter_type, frame_hdr->interpolation_filter);
  FHDR_TO_PP_PF1(frame_parallel_decoding_mode);
  FHDR_TO_PP_PF1(reset_frame_context);
  FHDR_TO_PP_PF1(refresh_frame_context);
  FHDR_TO_PP_PF2(frame_context_idx, frame_hdr->frame_context_idx_to_save_probs);
  FHDR_TO_PP_PF2(segmentation_enabled, seg.enabled);
  FHDR_TO_PP_PF2(segmentation_temporal_update, seg.temporal_update);
  FHDR_TO_PP_PF2(segmentation_update_map, seg.update_map);
  FHDR_TO_PP_PF2(last_ref_frame, frame_hdr->ref_frame_idx[0]);
  FHDR_TO_PP_PF2(last_ref_frame_sign_bias,
                 frame_hdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_LAST]);
  FHDR_TO_PP_PF2(golden_ref_frame, frame_hdr->ref_frame_idx[1]);
  FHDR_TO_PP_PF2(golden_ref_frame_sign_bias,
                 frame_hdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_GOLDEN]);
  FHDR_TO_PP_PF2(alt_ref_frame, frame_hdr->ref_frame_idx[2]);
  FHDR_TO_PP_PF2(alt_ref_frame_sign_bias,
                 frame_hdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_ALTREF]);
  FHDR_TO_PP_PF2(lossless_flag, frame_hdr->quant_params.IsLossless());
#undef FHDR_TO_PP_PF2
#undef FHDR_TO_PP_PF1

  pic_param.filter_level = lf.level;
  pic_param.sharpness_level = lf.sharpness;
  pic_param.log2_tile_rows = frame_hdr->tile_rows_log2;
  pic_param.log2_tile_columns = frame_hdr->tile_cols_log2;
  pic_param.frame_header_length_in_bytes = frame_hdr->uncompressed_header_size;
  pic_param.first_partition_size = frame_hdr->header_size_in_bytes;

  SafeArrayMemcpy(pic_param.mb_segment_tree_probs, seg.tree_probs);
  SafeArrayMemcpy(pic_param.segment_pred_probs, seg.pred_probs);

  pic_param.profile = frame_hdr->profile;
  pic_param.bit_depth = frame_hdr->bit_depth;
  DCHECK((pic_param.profile == 0 && pic_param.bit_depth == 8) ||
         (pic_param.profile == 2 && pic_param.bit_depth == 10));

  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType, &pic_param))
    return false;

  VASliceParameterBufferVP9 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));
  slice_param.slice_data_size = frame_hdr->frame_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

  static_assert(
      std::extent<decltype(Vp9SegmentationParams::feature_enabled)>() ==
          std::extent<decltype(slice_param.seg_param)>(),
      "seg_param array of incorrect size");
  for (size_t i = 0; i < base::size(slice_param.seg_param); ++i) {
    VASegmentParameterVP9& seg_param = slice_param.seg_param[i];
#define SEG_TO_SP_SF(a, b) seg_param.segment_flags.fields.a = b
    SEG_TO_SP_SF(
        segment_reference_enabled,
        seg.FeatureEnabled(i, Vp9SegmentationParams::SEG_LVL_REF_FRAME));
    SEG_TO_SP_SF(segment_reference,
                 seg.FeatureData(i, Vp9SegmentationParams::SEG_LVL_REF_FRAME));
    SEG_TO_SP_SF(segment_reference_skipped,
                 seg.FeatureEnabled(i, Vp9SegmentationParams::SEG_LVL_SKIP));
#undef SEG_TO_SP_SF

    SafeArrayMemcpy(seg_param.filter_level, lf.lvl[i]);

    seg_param.luma_dc_quant_scale = seg.y_dequant[i][0];
    seg_param.luma_ac_quant_scale = seg.y_dequant[i][1];
    seg_param.chroma_dc_quant_scale = seg.uv_dequant[i][0];
    seg_param.chroma_ac_quant_scale = seg.uv_dequant[i][1];
  }

  if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType, &slice_param))
    return false;

  if (!vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType,
                                    frame_hdr->frame_size, frame_hdr->data))
    return false;

  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
      pic->AsVaapiVP9Picture()->va_surface()->id());
}

bool VaapiVP9Accelerator::OutputPicture(scoped_refptr<VP9Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const VaapiVP9Picture* vaapi_pic = pic->AsVaapiVP9Picture();
  vaapi_dec_->SurfaceReady(vaapi_pic->va_surface(), vaapi_pic->bitstream_id(),
                           vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

bool VaapiVP9Accelerator::IsFrameContextRequired() const {
  return false;
}

bool VaapiVP9Accelerator::GetFrameContext(scoped_refptr<VP9Picture> pic,
                                          Vp9FrameContext* frame_ctx) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED() << "Frame context update not supported";
  return false;
}

}  // namespace media
