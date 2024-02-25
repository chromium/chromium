// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v4l2_video_decoder_delegate_vp8.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include <algorithm>
#include <type_traits>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/vp8_picture.h"
#include "media/parsers/vp8_parser.h"

namespace media {
namespace {

void FillV4L2SegmentationHeader(const Vp8SegmentationHeader& vp8_sgmnt_hdr,
                                struct v4l2_vp8_segment* v4l2_sgmnt_hdr) {
#define SET_V4L2_SGMNT_HDR_FLAG_IF(cond, flag) \
  v4l2_sgmnt_hdr->flags |= ((vp8_sgmnt_hdr.cond) ? (flag) : 0)
  SET_V4L2_SGMNT_HDR_FLAG_IF(segmentation_enabled,
                             V4L2_VP8_SEGMENT_FLAG_ENABLED);
  SET_V4L2_SGMNT_HDR_FLAG_IF(update_mb_segmentation_map,
                             V4L2_VP8_SEGMENT_FLAG_UPDATE_MAP);
  SET_V4L2_SGMNT_HDR_FLAG_IF(update_segment_feature_data,
                             V4L2_VP8_SEGMENT_FLAG_UPDATE_FEATURE_DATA);
  SET_V4L2_SGMNT_HDR_FLAG_IF(
      segment_feature_mode == Vp8SegmentationHeader::FEATURE_MODE_DELTA,
      V4L2_VP8_SEGMENT_FLAG_DELTA_VALUE_MODE);
#undef SET_V4L2_SGMNT_HDR_FLAG_IF

  SafeArrayMemcpy(v4l2_sgmnt_hdr->quant_update,
                  vp8_sgmnt_hdr.quantizer_update_value);
  SafeArrayMemcpy(v4l2_sgmnt_hdr->lf_update, vp8_sgmnt_hdr.lf_update_value);
  SafeArrayMemcpy(v4l2_sgmnt_hdr->segment_probs, vp8_sgmnt_hdr.segment_prob);
}

void FillV4L2LoopFilterHeader(const Vp8LoopFilterHeader& vp8_loopfilter_hdr,
                              struct v4l2_vp8_loop_filter* v4l2_lf_hdr) {
#define SET_V4L2_LF_HDR_FLAG_IF(cond, flag) \
  v4l2_lf_hdr->flags |= ((vp8_loopfilter_hdr.cond) ? (flag) : 0)
  SET_V4L2_LF_HDR_FLAG_IF(loop_filter_adj_enable, V4L2_VP8_LF_ADJ_ENABLE);
  SET_V4L2_LF_HDR_FLAG_IF(mode_ref_lf_delta_update, V4L2_VP8_LF_DELTA_UPDATE);
  SET_V4L2_LF_HDR_FLAG_IF(type == Vp8LoopFilterHeader::LOOP_FILTER_TYPE_SIMPLE,
                          V4L2_VP8_LF_FILTER_TYPE_SIMPLE);
#undef SET_V4L2_LF_HDR_FLAG_IF

#define LF_HDR_TO_V4L2_LF_HDR(a) v4l2_lf_hdr->a = vp8_loopfilter_hdr.a;
  LF_HDR_TO_V4L2_LF_HDR(level);
  LF_HDR_TO_V4L2_LF_HDR(sharpness_level);
#undef LF_HDR_TO_V4L2_LF_HDR

  SafeArrayMemcpy(v4l2_lf_hdr->ref_frm_delta,
                  vp8_loopfilter_hdr.ref_frame_delta);
  SafeArrayMemcpy(v4l2_lf_hdr->mb_mode_delta, vp8_loopfilter_hdr.mb_mode_delta);
}

void FillV4L2QuantizationHeader(const Vp8QuantizationHeader& vp8_quant_hdr,
                                struct v4l2_vp8_quantization* v4l2_quant_hdr) {
  v4l2_quant_hdr->y_ac_qi = vp8_quant_hdr.y_ac_qi;
  v4l2_quant_hdr->y_dc_delta = vp8_quant_hdr.y_dc_delta;
  v4l2_quant_hdr->y2_dc_delta = vp8_quant_hdr.y2_dc_delta;
  v4l2_quant_hdr->y2_ac_delta = vp8_quant_hdr.y2_ac_delta;
  v4l2_quant_hdr->uv_dc_delta = vp8_quant_hdr.uv_dc_delta;
  v4l2_quant_hdr->uv_ac_delta = vp8_quant_hdr.uv_ac_delta;
}

void FillV4L2Vp8EntropyHeader(const Vp8EntropyHeader& vp8_entropy_hdr,
                              struct v4l2_vp8_entropy* v4l2_entropy_hdr) {
  SafeArrayMemcpy(v4l2_entropy_hdr->coeff_probs, vp8_entropy_hdr.coeff_probs);
  SafeArrayMemcpy(v4l2_entropy_hdr->y_mode_probs, vp8_entropy_hdr.y_mode_probs);
  SafeArrayMemcpy(v4l2_entropy_hdr->uv_mode_probs,
                  vp8_entropy_hdr.uv_mode_probs);
  SafeArrayMemcpy(v4l2_entropy_hdr->mv_probs, vp8_entropy_hdr.mv_probs);
}

}  // namespace

class V4L2VP8Picture : public VP8Picture {
 public:
  explicit V4L2VP8Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2VP8Picture(const V4L2VP8Picture&) = delete;
  V4L2VP8Picture& operator=(const V4L2VP8Picture&) = delete;

  V4L2VP8Picture* AsV4L2VP8Picture() override { return this; }
  scoped_refptr<V4L2DecodeSurface> dec_surface() { return dec_surface_; }

 private:
  ~V4L2VP8Picture() override {}

  scoped_refptr<V4L2DecodeSurface> dec_surface_;
};

V4L2VideoDecoderDelegateVP8::V4L2VideoDecoderDelegateVP8(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler), device_(device) {
  DCHECK(surface_handler_);
}

V4L2VideoDecoderDelegateVP8::~V4L2VideoDecoderDelegateVP8() {}

scoped_refptr<VP8Picture> V4L2VideoDecoderDelegateVP8::CreateVP8Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface)
    return nullptr;

  return new V4L2VP8Picture(dec_surface);
}

bool V4L2VideoDecoderDelegateVP8::SubmitDecode(
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& reference_frames) {
  struct v4l2_ctrl_vp8_frame v4l2_frame_hdr;
  memset(&v4l2_frame_hdr, 0, sizeof(v4l2_frame_hdr));

  const auto& frame_hdr = pic->frame_hdr;
#define FHDR_TO_V4L2_FHDR(a) v4l2_frame_hdr.a = frame_hdr->a
  FHDR_TO_V4L2_FHDR(version);
  FHDR_TO_V4L2_FHDR(width);
  FHDR_TO_V4L2_FHDR(horizontal_scale);
  FHDR_TO_V4L2_FHDR(height);
  FHDR_TO_V4L2_FHDR(vertical_scale);
  FHDR_TO_V4L2_FHDR(prob_skip_false);
  FHDR_TO_V4L2_FHDR(prob_intra);
  FHDR_TO_V4L2_FHDR(prob_last);
  FHDR_TO_V4L2_FHDR(prob_gf);
#undef FHDR_TO_V4L2_FHDR
  v4l2_frame_hdr.coder_state.range = frame_hdr->bool_dec_range;
  v4l2_frame_hdr.coder_state.value = frame_hdr->bool_dec_value;
  v4l2_frame_hdr.coder_state.bit_count = frame_hdr->bool_dec_count;

#define SET_V4L2_FRM_HDR_FLAG_IF(cond, flag) \
  v4l2_frame_hdr.flags |= ((frame_hdr->cond) ? (flag) : 0)
  SET_V4L2_FRM_HDR_FLAG_IF(frame_type == Vp8FrameHeader::KEYFRAME,
                           V4L2_VP8_FRAME_FLAG_KEY_FRAME);
  SET_V4L2_FRM_HDR_FLAG_IF(sign_bias_golden,
                           V4L2_VP8_FRAME_FLAG_SIGN_BIAS_GOLDEN);
  SET_V4L2_FRM_HDR_FLAG_IF(sign_bias_alternate,
                           V4L2_VP8_FRAME_FLAG_SIGN_BIAS_ALT);
  SET_V4L2_FRM_HDR_FLAG_IF(is_experimental, V4L2_VP8_FRAME_FLAG_EXPERIMENTAL);
  SET_V4L2_FRM_HDR_FLAG_IF(show_frame, V4L2_VP8_FRAME_FLAG_SHOW_FRAME);
  SET_V4L2_FRM_HDR_FLAG_IF(mb_no_skip_coeff,
                           V4L2_VP8_FRAME_FLAG_MB_NO_SKIP_COEFF);
#undef SET_V4L2_FRM_HDR_FLAG_IF

  FillV4L2SegmentationHeader(frame_hdr->segmentation_hdr,
                             &v4l2_frame_hdr.segment);

  FillV4L2LoopFilterHeader(frame_hdr->loopfilter_hdr, &v4l2_frame_hdr.lf);

  FillV4L2QuantizationHeader(frame_hdr->quantization_hdr,
                             &v4l2_frame_hdr.quant);

  FillV4L2Vp8EntropyHeader(frame_hdr->entropy_hdr, &v4l2_frame_hdr.entropy);

  v4l2_frame_hdr.first_part_size =
      base::checked_cast<__u32>(frame_hdr->first_part_size);
  v4l2_frame_hdr.first_part_header_bits =
      base::checked_cast<__u32>(frame_hdr->macroblock_bit_offset);
  v4l2_frame_hdr.num_dct_parts = frame_hdr->num_of_dct_partitions;

  static_assert(std::extent<decltype(v4l2_frame_hdr.dct_part_sizes)>() ==
                    std::extent<decltype(frame_hdr->dct_partition_sizes)>(),
                "DCT partition size arrays must have equal number of elements");
  for (size_t i = 0; i < frame_hdr->num_of_dct_partitions &&
                     i < std::size(v4l2_frame_hdr.dct_part_sizes);
       ++i)
    v4l2_frame_hdr.dct_part_sizes[i] = frame_hdr->dct_partition_sizes[i];

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      VP8PictureToV4L2DecodeSurface(pic.get());
  std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces;

  const auto last_frame = reference_frames.GetFrame(Vp8RefType::VP8_FRAME_LAST);
  if (last_frame) {
    scoped_refptr<V4L2DecodeSurface> last_frame_surface =
        VP8PictureToV4L2DecodeSurface(last_frame.get());
    v4l2_frame_hdr.last_frame_ts = last_frame_surface->GetReferenceID();
    ref_surfaces.push_back(last_frame_surface);
  }

  const auto golden_frame =
      reference_frames.GetFrame(Vp8RefType::VP8_FRAME_GOLDEN);
  if (golden_frame) {
    scoped_refptr<V4L2DecodeSurface> golden_frame_surface =
        VP8PictureToV4L2DecodeSurface(golden_frame.get());
    v4l2_frame_hdr.golden_frame_ts = golden_frame_surface->GetReferenceID();
    ref_surfaces.push_back(golden_frame_surface);
  }

  const auto alt_frame =
      reference_frames.GetFrame(Vp8RefType::VP8_FRAME_ALTREF);
  if (alt_frame) {
    scoped_refptr<V4L2DecodeSurface> alt_frame_surface =
        VP8PictureToV4L2DecodeSurface(alt_frame.get());
    v4l2_frame_hdr.alt_frame_ts = alt_frame_surface->GetReferenceID();
    ref_surfaces.push_back(alt_frame_surface);
  }

  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_VP8_FRAME;
  ctrl.size = sizeof(v4l2_frame_hdr);
  ctrl.ptr = &v4l2_frame_hdr;

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = 1;
  ext_ctrls.controls = &ctrl;
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return false;
  }

  dec_surface->SetReferenceSurfaces(ref_surfaces);

  if (!surface_handler_->SubmitSlice(dec_surface.get(), frame_hdr->data,
                                     frame_hdr->frame_size))
    return false;

  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);
  return true;
}

bool V4L2VideoDecoderDelegateVP8::OutputPicture(scoped_refptr<VP8Picture> pic) {
  surface_handler_->SurfaceReady(VP8PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

scoped_refptr<V4L2DecodeSurface>
V4L2VideoDecoderDelegateVP8::VP8PictureToV4L2DecodeSurface(VP8Picture* pic) {
  V4L2VP8Picture* v4l2_pic = pic->AsV4L2VP8Picture();
  CHECK(v4l2_pic);
  return v4l2_pic->dec_surface();
}

}  // namespace media
