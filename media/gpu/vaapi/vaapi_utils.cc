// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_utils.h"

#include <va/va.h>

#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/synchronization/lock.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vp8_picture.h"
#include "media/gpu/vp8_reference_frame_vector.h"

namespace media {

namespace {

template <typename To, typename From>
void CheckedMemcpy(To& to, From& from) {
  static_assert(std::is_array<To>::value, "First parameter must be an array");
  static_assert(std::is_array<From>::value,
                "Second parameter must be an array");
  static_assert(sizeof(to) == sizeof(from), "arrays must be of same size");
  memcpy(&to, &from, sizeof(to));
}

}  // namespace

ScopedVABufferMapping::ScopedVABufferMapping(
    const base::Lock* lock,
    VADisplay va_display,
    VABufferID buffer_id,
    base::OnceCallback<void(VABufferID)> release_callback)
    : lock_(lock), va_display_(va_display), buffer_id_(buffer_id) {
  DCHECK(lock_);
  lock_->AssertAcquired();
  DCHECK_NE(buffer_id, VA_INVALID_ID);

  const VAStatus result =
      vaMapBuffer(va_display_, buffer_id_, &va_buffer_data_);
  const bool success = result == VA_STATUS_SUCCESS;
  LOG_IF(ERROR, !success) << "vaMapBuffer failed: " << vaErrorStr(result);
  DCHECK(success == (va_buffer_data_ != nullptr))
      << "|va_buffer_data| should be null if vaMapBuffer() fails";

  if (!success && release_callback)
    std::move(release_callback).Run(buffer_id_);
}

ScopedVABufferMapping::~ScopedVABufferMapping() {
  if (va_buffer_data_) {
    lock_->AssertAcquired();
    Unmap();
  }
}

VAStatus ScopedVABufferMapping::Unmap() {
  lock_->AssertAcquired();
  const VAStatus result = vaUnmapBuffer(va_display_, buffer_id_);
  if (result == VA_STATUS_SUCCESS)
    va_buffer_data_ = nullptr;
  else
    LOG(ERROR) << "vaUnmapBuffer failed: " << vaErrorStr(result);
  return result;
}

ScopedVAImage::ScopedVAImage(base::Lock* lock,
                             VADisplay va_display,
                             VASurfaceID va_surface_id,
                             VAImageFormat* format,
                             const gfx::Size& size)
    : lock_(lock), va_display_(va_display), image_(new VAImage{}) {
  DCHECK(lock_);
  lock_->AssertAcquired();
  VAStatus result = vaCreateImage(va_display_, format, size.width(),
                                  size.height(), image_.get());
  if (result != VA_STATUS_SUCCESS) {
    DCHECK_EQ(image_->image_id, VA_INVALID_ID);
    LOG(ERROR) << "vaCreateImage failed: " << vaErrorStr(result);
    return;
  }
  DCHECK_NE(image_->image_id, VA_INVALID_ID);

  result = vaGetImage(va_display_, va_surface_id, 0, 0, size.width(),
                      size.height(), image_->image_id);
  if (result != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaGetImage failed: " << vaErrorStr(result);
    return;
  }

  va_buffer_ =
      std::make_unique<ScopedVABufferMapping>(lock_, va_display, image_->buf);
}

ScopedVAImage::~ScopedVAImage() {
  if (image_->image_id != VA_INVALID_ID) {
    base::AutoLock auto_lock(*lock_);

    // |va_buffer_| has to be deleted before vaDestroyImage().
    va_buffer_.reset();
    vaDestroyImage(va_display_, image_->image_id);
  }
}

ScopedVASurface::ScopedVASurface(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                                 VASurfaceID va_surface_id,
                                 const gfx::Size& size,
                                 unsigned int va_rt_format)
    : vaapi_wrapper_(std::move(vaapi_wrapper)),
      va_surface_id_(va_surface_id),
      size_(size),
      va_rt_format_(va_rt_format) {
  DCHECK(vaapi_wrapper_);
}

ScopedVASurface::~ScopedVASurface() {
  if (va_surface_id_ != VA_INVALID_ID)
    vaapi_wrapper_->DestroySurface(va_surface_id_);
}

bool ScopedVASurface::IsValid() const {
  return va_surface_id_ != VA_INVALID_ID && !size_.IsEmpty() &&
         va_rt_format_ != kInvalidVaRtFormat;
}

bool FillVP8DataStructures(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
                           VASurfaceID va_surface_id,
                           const Vp8FrameHeader& frame_header,
                           const Vp8ReferenceFrameVector& reference_frames) {
  DCHECK_NE(va_surface_id, VA_INVALID_SURFACE);
  DCHECK(vaapi_wrapper);

  const Vp8SegmentationHeader& sgmnt_hdr = frame_header.segmentation_hdr;
  const Vp8QuantizationHeader& quant_hdr = frame_header.quantization_hdr;
  VAIQMatrixBufferVP8 iq_matrix_buf{};
  static_assert(base::size(iq_matrix_buf.quantization_index) == kMaxMBSegments,
                "incorrect quantization matrix segment size");
  static_assert(base::size(iq_matrix_buf.quantization_index[0]) == 6,
                "incorrect quantization matrix Q index size");
  for (size_t i = 0; i < kMaxMBSegments; ++i) {
    int q = quant_hdr.y_ac_qi;

    if (sgmnt_hdr.segmentation_enabled) {
      if (sgmnt_hdr.segment_feature_mode ==
          Vp8SegmentationHeader::FEATURE_MODE_ABSOLUTE) {
        q = sgmnt_hdr.quantizer_update_value[i];
      } else {
        q += sgmnt_hdr.quantizer_update_value[i];
      }
    }

#define CLAMP_Q(q) base::ClampToRange(q, 0, 127)
    iq_matrix_buf.quantization_index[i][0] = CLAMP_Q(q);
    iq_matrix_buf.quantization_index[i][1] = CLAMP_Q(q + quant_hdr.y_dc_delta);
    iq_matrix_buf.quantization_index[i][2] = CLAMP_Q(q + quant_hdr.y2_dc_delta);
    iq_matrix_buf.quantization_index[i][3] = CLAMP_Q(q + quant_hdr.y2_ac_delta);
    iq_matrix_buf.quantization_index[i][4] = CLAMP_Q(q + quant_hdr.uv_dc_delta);
    iq_matrix_buf.quantization_index[i][5] = CLAMP_Q(q + quant_hdr.uv_ac_delta);
#undef CLAMP_Q
  }

  if (!vaapi_wrapper->SubmitBuffer(VAIQMatrixBufferType, &iq_matrix_buf))
    return false;

  const Vp8EntropyHeader& entr_hdr = frame_header.entropy_hdr;
  VAProbabilityDataBufferVP8 prob_buf{};
  CheckedMemcpy(prob_buf.dct_coeff_probs, entr_hdr.coeff_probs);

  if (!vaapi_wrapper->SubmitBuffer(VAProbabilityBufferType, &prob_buf))
    return false;

  VAPictureParameterBufferVP8 pic_param{};
  pic_param.frame_width = frame_header.width;
  pic_param.frame_height = frame_header.height;

  const auto last_frame = reference_frames.GetFrame(Vp8RefType::VP8_FRAME_LAST);
  if (last_frame) {
    pic_param.last_ref_frame =
        last_frame->AsVaapiVP8Picture()->GetVASurfaceID();
  } else {
    pic_param.last_ref_frame = VA_INVALID_SURFACE;
  }

  const auto golden_frame =
      reference_frames.GetFrame(Vp8RefType::VP8_FRAME_GOLDEN);
  if (golden_frame) {
    pic_param.golden_ref_frame =
        golden_frame->AsVaapiVP8Picture()->GetVASurfaceID();
  } else {
    pic_param.golden_ref_frame = VA_INVALID_SURFACE;
  }

  const auto alt_frame =
      reference_frames.GetFrame(Vp8RefType::VP8_FRAME_ALTREF);
  if (alt_frame)
    pic_param.alt_ref_frame = alt_frame->AsVaapiVP8Picture()->GetVASurfaceID();
  else
    pic_param.alt_ref_frame = VA_INVALID_SURFACE;

  pic_param.out_of_loop_frame = VA_INVALID_SURFACE;

  const Vp8LoopFilterHeader& lf_hdr = frame_header.loopfilter_hdr;

#define FHDR_TO_PP_PF(a, b) pic_param.pic_fields.bits.a = (b)
  FHDR_TO_PP_PF(key_frame, frame_header.IsKeyframe() ? 0 : 1);
  FHDR_TO_PP_PF(version, frame_header.version);
  FHDR_TO_PP_PF(segmentation_enabled, sgmnt_hdr.segmentation_enabled);
  FHDR_TO_PP_PF(update_mb_segmentation_map,
                sgmnt_hdr.update_mb_segmentation_map);
  FHDR_TO_PP_PF(update_segment_feature_data,
                sgmnt_hdr.update_segment_feature_data);
  FHDR_TO_PP_PF(filter_type, lf_hdr.type);
  FHDR_TO_PP_PF(sharpness_level, lf_hdr.sharpness_level);
  FHDR_TO_PP_PF(loop_filter_adj_enable, lf_hdr.loop_filter_adj_enable);
  FHDR_TO_PP_PF(mode_ref_lf_delta_update, lf_hdr.mode_ref_lf_delta_update);
  FHDR_TO_PP_PF(sign_bias_golden, frame_header.sign_bias_golden);
  FHDR_TO_PP_PF(sign_bias_alternate, frame_header.sign_bias_alternate);
  FHDR_TO_PP_PF(mb_no_coeff_skip, frame_header.mb_no_skip_coeff);
  FHDR_TO_PP_PF(loop_filter_disable, lf_hdr.level == 0);
#undef FHDR_TO_PP_PF

  CheckedMemcpy(pic_param.mb_segment_tree_probs, sgmnt_hdr.segment_prob);

  static_assert(std::extent<decltype(sgmnt_hdr.lf_update_value)>() ==
                    std::extent<decltype(pic_param.loop_filter_level)>(),
                "loop filter level arrays mismatch");
  for (size_t i = 0; i < base::size(sgmnt_hdr.lf_update_value); ++i) {
    int lf_level = lf_hdr.level;
    if (sgmnt_hdr.segmentation_enabled) {
      if (sgmnt_hdr.segment_feature_mode ==
          Vp8SegmentationHeader::FEATURE_MODE_ABSOLUTE) {
        lf_level = sgmnt_hdr.lf_update_value[i];
      } else {
        lf_level += sgmnt_hdr.lf_update_value[i];
      }
    }

    pic_param.loop_filter_level[i] = base::ClampToRange(lf_level, 0, 63);
  }

  static_assert(
      std::extent<decltype(lf_hdr.ref_frame_delta)>() ==
          std::extent<decltype(pic_param.loop_filter_deltas_ref_frame)>(),
      "loop filter deltas arrays size mismatch");
  static_assert(std::extent<decltype(lf_hdr.mb_mode_delta)>() ==
                    std::extent<decltype(pic_param.loop_filter_deltas_mode)>(),
                "loop filter deltas arrays size mismatch");
  static_assert(std::extent<decltype(lf_hdr.ref_frame_delta)>() ==
                    std::extent<decltype(lf_hdr.mb_mode_delta)>(),
                "loop filter deltas arrays size mismatch");
  for (size_t i = 0; i < base::size(lf_hdr.ref_frame_delta); ++i) {
    pic_param.loop_filter_deltas_ref_frame[i] = lf_hdr.ref_frame_delta[i];
    pic_param.loop_filter_deltas_mode[i] = lf_hdr.mb_mode_delta[i];
  }

#define FHDR_TO_PP(a) pic_param.a = frame_header.a
  FHDR_TO_PP(prob_skip_false);
  FHDR_TO_PP(prob_intra);
  FHDR_TO_PP(prob_last);
  FHDR_TO_PP(prob_gf);
#undef FHDR_TO_PP

  CheckedMemcpy(pic_param.y_mode_probs, entr_hdr.y_mode_probs);
  CheckedMemcpy(pic_param.uv_mode_probs, entr_hdr.uv_mode_probs);
  CheckedMemcpy(pic_param.mv_probs, entr_hdr.mv_probs);

  pic_param.bool_coder_ctx.range = frame_header.bool_dec_range;
  pic_param.bool_coder_ctx.value = frame_header.bool_dec_value;
  pic_param.bool_coder_ctx.count = frame_header.bool_dec_count;

  if (!vaapi_wrapper->SubmitBuffer(VAPictureParameterBufferType, &pic_param))
    return false;

  VASliceParameterBufferVP8 slice_param{};
  slice_param.slice_data_size = frame_header.frame_size;
  slice_param.slice_data_offset = frame_header.first_part_offset;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.macroblock_offset = frame_header.macroblock_bit_offset;
  // Number of DCT partitions plus control partition.
  slice_param.num_of_partitions = frame_header.num_of_dct_partitions + 1;

  // Per VAAPI, this size only includes the size of the macroblock data in
  // the first partition (in bytes), so we have to subtract the header size.
  slice_param.partition_size[0] =
      frame_header.first_part_size -
      ((frame_header.macroblock_bit_offset + 7) / 8);

  for (size_t i = 0; i < frame_header.num_of_dct_partitions; ++i)
    slice_param.partition_size[i + 1] = frame_header.dct_partition_sizes[i];

  if (!vaapi_wrapper->SubmitBuffer(VASliceParameterBufferType, &slice_param))
    return false;

  return vaapi_wrapper->SubmitBuffer(
      VASliceDataBufferType, frame_header.frame_size, frame_header.data);
}
}  // namespace media
