// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/vp8_decoder.h"

#include <va/va.h>

#include <algorithm>
#include <memory>

#include "media/gpu/vaapi/test/macros.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp8_parser.h"

namespace media {
namespace vaapi_test {

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

Vp8Decoder::Vp8Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       const VaapiDevice& va_device,
                       SharedVASurface::FetchPolicy fetch_policy)
    : VideoDecoder(va_device, fetch_policy),
      va_config_(
          std::make_unique<ScopedVAConfig>(*va_device_,
                                           VAProfile::VAProfileVP8Version0_3,
                                           VA_RT_FORMAT_YUV420)),
      vp8_parser_(std::make_unique<Vp8Parser>()),
      ref_frames_(kNumVp8ReferenceBuffers),
      ivf_parser_(std::move(ivf_parser)) {
  std::fill(ref_frames_.begin(), ref_frames_.end(), nullptr);
}

Vp8Decoder::~Vp8Decoder() {
  // We destroy the VA handles explicitly to ensure the correct order.
  // The configuration must be destroyed after the context so that the
  // configuration reference remains valid in the context, and surfaces can only
  // be destroyed after the context as per
  // https://github.com/intel/libva/blob/8c6126e67c446f4c7808cb51b609077e4b9bd8fe/va/va.h#L1549
  va_context_.reset();
  va_config_.reset();

  last_decoded_surface_.reset();
  ref_frames_.clear();
}

Vp8Decoder::ParseResult Vp8Decoder::ReadNextFrame(
    Vp8FrameHeader& vp8_frame_header) {
  IvfFrameHeader ivf_frame_header{};
  const uint8_t* ivf_frame_data;
  if (!ivf_parser_->ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
    return kEOStream;

  const bool result = vp8_parser_->ParseFrame(
      ivf_frame_data, ivf_frame_header.frame_size, &vp8_frame_header);
  return result ? kOk : kError;
}

// The implementation of method is mostly lifted from vaapi_utils.h
// FillVP8DataStructures
// (https://source.chromium.org/chromium/chromium/src/+/main:media/gpu/vaapi/vaapi_utils.cc;l=195;drc=9d70e034c6a4c2b1ed56c94aace3f3c8d2b1f771).
void Vp8Decoder::FillVp8DataStructures(const Vp8FrameHeader& frame_hdr,
                                       VAIQMatrixBufferVP8& iq_matrix_buf,
                                       VAProbabilityDataBufferVP8& prob_buf,
                                       VAPictureParameterBufferVP8& pic_param,
                                       VASliceParameterBufferVP8& slice_param) {
  const Vp8SegmentationHeader& sgmnt_hdr = frame_hdr.segmentation_hdr;
  const Vp8QuantizationHeader& quant_hdr = frame_hdr.quantization_hdr;
  static_assert(
      std::size(decltype(iq_matrix_buf.quantization_index){}) == kMaxMBSegments,
      "incorrect quantization matrix segment size");
  static_assert(std::size(decltype(iq_matrix_buf.quantization_index){}[0]) == 6,
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

#define CLAMP_Q(q) std::clamp(q, 0, 127)
    iq_matrix_buf.quantization_index[i][0] = CLAMP_Q(q);
    iq_matrix_buf.quantization_index[i][1] = CLAMP_Q(q + quant_hdr.y_dc_delta);
    iq_matrix_buf.quantization_index[i][2] = CLAMP_Q(q + quant_hdr.y2_dc_delta);
    iq_matrix_buf.quantization_index[i][3] = CLAMP_Q(q + quant_hdr.y2_ac_delta);
    iq_matrix_buf.quantization_index[i][4] = CLAMP_Q(q + quant_hdr.uv_dc_delta);
    iq_matrix_buf.quantization_index[i][5] = CLAMP_Q(q + quant_hdr.uv_ac_delta);
#undef CLAMP_Q
  }

  const Vp8EntropyHeader& entr_hdr = frame_hdr.entropy_hdr;
  CheckedMemcpy(prob_buf.dct_coeff_probs, entr_hdr.coeff_probs);

  pic_param.frame_width = frame_hdr.width;
  pic_param.frame_height = frame_hdr.height;
  pic_param.last_ref_frame = ref_frames_[VP8_FRAME_LAST]
                                 ? ref_frames_[VP8_FRAME_LAST]->id()
                                 : VA_INVALID_SURFACE;
  pic_param.golden_ref_frame = ref_frames_[VP8_FRAME_GOLDEN]
                                   ? ref_frames_[VP8_FRAME_GOLDEN]->id()
                                   : VA_INVALID_SURFACE;
  pic_param.alt_ref_frame = ref_frames_[VP8_FRAME_ALTREF]
                                ? ref_frames_[VP8_FRAME_ALTREF]->id()
                                : VA_INVALID_SURFACE;
  const Vp8LoopFilterHeader& lf_hdr = frame_hdr.loopfilter_hdr;

#define FHDR_TO_PP_PF(a, b) pic_param.pic_fields.bits.a = (b)
  FHDR_TO_PP_PF(key_frame, frame_hdr.IsKeyframe() ? 0 : 1);
  FHDR_TO_PP_PF(version, frame_hdr.version);
  FHDR_TO_PP_PF(segmentation_enabled, sgmnt_hdr.segmentation_enabled);
  FHDR_TO_PP_PF(update_mb_segmentation_map,
                sgmnt_hdr.update_mb_segmentation_map);
  FHDR_TO_PP_PF(update_segment_feature_data,
                sgmnt_hdr.update_segment_feature_data);
  FHDR_TO_PP_PF(filter_type, lf_hdr.type);
  FHDR_TO_PP_PF(sharpness_level, lf_hdr.sharpness_level);
  FHDR_TO_PP_PF(loop_filter_adj_enable, lf_hdr.loop_filter_adj_enable);
  FHDR_TO_PP_PF(mode_ref_lf_delta_update, lf_hdr.mode_ref_lf_delta_update);
  FHDR_TO_PP_PF(sign_bias_golden, frame_hdr.sign_bias_golden);
  FHDR_TO_PP_PF(sign_bias_alternate, frame_hdr.sign_bias_alternate);
  FHDR_TO_PP_PF(mb_no_coeff_skip, frame_hdr.mb_no_skip_coeff);
  FHDR_TO_PP_PF(loop_filter_disable, lf_hdr.level == 0);
#undef FHDR_TO_PP_PF

  CheckedMemcpy(pic_param.mb_segment_tree_probs, sgmnt_hdr.segment_prob);

  static_assert(std::extent<decltype(sgmnt_hdr.lf_update_value)>() ==
                    std::extent<decltype(pic_param.loop_filter_level)>(),
                "loop filter level arrays mismatch");
  for (size_t i = 0; i < std::size(sgmnt_hdr.lf_update_value); ++i) {
    int lf_level = lf_hdr.level;
    if (sgmnt_hdr.segmentation_enabled) {
      if (sgmnt_hdr.segment_feature_mode ==
          Vp8SegmentationHeader::FEATURE_MODE_ABSOLUTE) {
        lf_level = sgmnt_hdr.lf_update_value[i];
      } else {
        lf_level += sgmnt_hdr.lf_update_value[i];
      }
    }

    pic_param.loop_filter_level[i] = std::clamp(lf_level, 0, 63);
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
  for (size_t i = 0; i < std::size(lf_hdr.ref_frame_delta); ++i) {
    pic_param.loop_filter_deltas_ref_frame[i] = lf_hdr.ref_frame_delta[i];
    pic_param.loop_filter_deltas_mode[i] = lf_hdr.mb_mode_delta[i];
  }

#define FHDR_TO_PP(a) pic_param.a = frame_hdr.a
  FHDR_TO_PP(prob_skip_false);
  FHDR_TO_PP(prob_intra);
  FHDR_TO_PP(prob_last);
  FHDR_TO_PP(prob_gf);
#undef FHDR_TO_PP

  CheckedMemcpy(pic_param.y_mode_probs, entr_hdr.y_mode_probs);
  CheckedMemcpy(pic_param.uv_mode_probs, entr_hdr.uv_mode_probs);
  CheckedMemcpy(pic_param.mv_probs, entr_hdr.mv_probs);

  pic_param.bool_coder_ctx.range = frame_hdr.bool_dec_range;
  pic_param.bool_coder_ctx.value = frame_hdr.bool_dec_value;
  pic_param.bool_coder_ctx.count = frame_hdr.bool_dec_count;

  slice_param.slice_data_size = frame_hdr.frame_size;
  slice_param.slice_data_offset = frame_hdr.first_part_offset;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.macroblock_offset = frame_hdr.macroblock_bit_offset;
  // Number of DCT partitions plus control partition.
  slice_param.num_of_partitions = frame_hdr.num_of_dct_partitions + 1;

  // Per VAAPI, this size only includes the size of the macroblock data in
  // the first partition (in bytes), so we have to subtract the header size.
  slice_param.partition_size[0] =
      frame_hdr.first_part_size - ((frame_hdr.macroblock_bit_offset + 7) / 8);

  for (size_t i = 0; i < frame_hdr.num_of_dct_partitions; ++i)
    slice_param.partition_size[i + 1] = frame_hdr.dct_partition_sizes[i];
}

// Based on update_reference_frames() in libvpx: vp8/encoder/onyx_if.c
void Vp8Decoder::RefreshReferenceSlots(Vp8FrameHeader& frame_hdr,
                                       scoped_refptr<SharedVASurface> surface) {
  if (frame_hdr.IsKeyframe()) {
    ref_frames_[VP8_FRAME_LAST] = surface;
    ref_frames_[VP8_FRAME_GOLDEN] = surface;
    ref_frames_[VP8_FRAME_ALTREF] = surface;
    return;
  }

  if (frame_hdr.refresh_alternate_frame) {
    ref_frames_[VP8_FRAME_ALTREF] = surface;
  } else {
    switch (frame_hdr.copy_buffer_to_alternate) {
      case Vp8FrameHeader::COPY_LAST_TO_ALT:
        DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_LAST]);
        ref_frames_[VP8_FRAME_ALTREF] = ref_frames_[VP8_FRAME_LAST];
        break;
      case Vp8FrameHeader::COPY_GOLDEN_TO_ALT:
        DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_GOLDEN]);
        ref_frames_[VP8_FRAME_ALTREF] = ref_frames_[VP8_FRAME_GOLDEN];
        break;
      case Vp8FrameHeader::NO_ALT_REFRESH:
        DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_ALTREF]);
        break;
    }
  }

  if (frame_hdr.refresh_golden_frame) {
    ref_frames_[VP8_FRAME_GOLDEN] = surface;
  } else {
    switch (frame_hdr.copy_buffer_to_golden) {
      case Vp8FrameHeader::COPY_LAST_TO_GOLDEN:
        DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_LAST]);
        ref_frames_[VP8_FRAME_GOLDEN] = ref_frames_[VP8_FRAME_LAST];
        break;
      case Vp8FrameHeader::COPY_ALT_TO_GOLDEN:
        DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_ALTREF]);
        ref_frames_[VP8_FRAME_GOLDEN] = ref_frames_[VP8_FRAME_ALTREF];
        break;
      case Vp8FrameHeader::NO_GOLDEN_REFRESH:
        DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_GOLDEN]);
        break;
    }
  }

  if (frame_hdr.refresh_last)
    ref_frames_[VP8_FRAME_LAST] = surface;
  else
    DCHECK(ref_frames_[Vp8RefType::VP8_FRAME_LAST]);
}

VideoDecoder::Result Vp8Decoder::DecodeNextFrame() {
  // Parse next frame from stream.
  Vp8FrameHeader frame_hdr{};
  const ParseResult parser_res = ReadNextFrame(frame_hdr);
  if (parser_res == kEOStream)
    return VideoDecoder::kEOStream;
  LOG_ASSERT(parser_res == kOk) << "Failed to parse next frame.";

  if (frame_hdr.IsKeyframe()) {
    const gfx::Size new_size(frame_hdr.width, frame_hdr.height);
    LOG_ASSERT(!new_size.IsEmpty()) << "New key frame size is empty.";

    if (!va_context_ || new_size != va_context_->size()) {
      va_context_ =
          std::make_unique<ScopedVAContext>(*va_device_, *va_config_, new_size);
    }
  } else {
    frame_hdr.height = va_context_->size().height();
    frame_hdr.width = va_context_->size().width();
  }
  LOG_ASSERT(va_context_ != nullptr)
      << "VA Context not set. First frame was not a key frame.";

  VLOG_IF(2, !frame_hdr.show_frame) << "not displaying frame";
  last_decoded_frame_visible_ = frame_hdr.show_frame;

  // Create surfaces for decode.
  VASurfaceAttrib attribute{};
  attribute.type = VASurfaceAttribUsageHint;
  attribute.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribute.value.type = VAGenericValueTypeInteger;
  attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
  scoped_refptr<SharedVASurface> surface = SharedVASurface::Create(
      *va_device_, va_config_->va_rt_format(), va_context_->size(), attribute);

  // Create the VP8 data structures.
  VAIQMatrixBufferVP8 iq_matrix_buf{};
  VAProbabilityDataBufferVP8 prob_buf{};
  VAPictureParameterBufferVP8 pic_param{};
  VASliceParameterBufferVP8 slice_param{};
  FillVp8DataStructures(frame_hdr, iq_matrix_buf, prob_buf, pic_param,
                        slice_param);

  // Populate the VA API buffers.
  std::vector<VABufferID> buffers;

  VABufferID iq_matrix_id;
  VAStatus res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                                VAIQMatrixBufferType, sizeof(iq_matrix_buf), 1u,
                                nullptr, &iq_matrix_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  void* iq_matrix_data;
  res = vaMapBuffer(va_device_->display(), iq_matrix_id, &iq_matrix_data);
  VA_LOG_ASSERT(res, "vaMapBuffer");
  memcpy(iq_matrix_data, &iq_matrix_buf, sizeof(iq_matrix_buf));
  buffers.push_back(iq_matrix_id);

  VABufferID prob_buffer_id;
  res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                       VAProbabilityBufferType, sizeof(prob_buf), 1u, nullptr,
                       &prob_buffer_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  void* prob_buffer_data;
  res = vaMapBuffer(va_device_->display(), prob_buffer_id, &prob_buffer_data);
  VA_LOG_ASSERT(res, "vaMapBuffer");
  memcpy(prob_buffer_data, &prob_buf, sizeof(prob_buf));
  buffers.push_back(prob_buffer_id);

  VABufferID picture_params_id;
  res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                       VAPictureParameterBufferType, sizeof(pic_param), 1u,
                       nullptr, &picture_params_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  void* picture_params_data;
  res = vaMapBuffer(va_device_->display(), picture_params_id,
                    &picture_params_data);
  VA_LOG_ASSERT(res, "vaMapBuffer");
  memcpy(picture_params_data, &pic_param, sizeof(pic_param));
  buffers.push_back(picture_params_id);

  VABufferID slice_params_id;
  res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                       VASliceParameterBufferType, sizeof(slice_param), 1u,
                       nullptr, &slice_params_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  void* slice_params_data;
  res = vaMapBuffer(va_device_->display(), slice_params_id, &slice_params_data);
  VA_LOG_ASSERT(res, "vaMapBuffer");
  memcpy(slice_params_data, &slice_param, sizeof(pic_param));
  buffers.push_back(slice_params_id);

  VABufferID encoded_data_id;
  res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                       VASliceDataBufferType, frame_hdr.frame_size, 1u, nullptr,
                       &encoded_data_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  void* encoded_data;
  res = vaMapBuffer(va_device_->display(), encoded_data_id, &encoded_data);
  VA_LOG_ASSERT(res, "vaMapBuffer");
  memcpy(encoded_data, frame_hdr.data, frame_hdr.frame_size);
  buffers.push_back(encoded_data_id);

  // Time to render!
  res = vaBeginPicture(va_device_->display(), va_context_->id(), surface->id());
  VA_LOG_ASSERT(res, "vaBeginPicture");
  res =
      vaRenderPicture(va_device_->display(), va_context_->id(), buffers.data(),
                      base::checked_cast<int>(buffers.size()));
  VA_LOG_ASSERT(res, "vaRenderPicture");
  res = vaEndPicture(va_device_->display(), va_context_->id());
  VA_LOG_ASSERT(res, "vaEndPicture");

  RefreshReferenceSlots(frame_hdr, surface);
  last_decoded_surface_ = surface;

  for (const auto buffer_id : buffers) {
    res = vaUnmapBuffer(va_device_->display(), buffer_id);
    VA_LOG_ASSERT(res, "vaUnmapBuffer");

    res = vaDestroyBuffer(va_device_->display(), buffer_id);
    VA_LOG_ASSERT(res, "vaDestroyBuffer");
  }

  return VideoDecoder::kOk;
}

}  // namespace vaapi_test
}  // namespace media
