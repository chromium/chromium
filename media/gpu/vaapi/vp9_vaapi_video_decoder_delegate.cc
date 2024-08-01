// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vp9_vaapi_video_decoder_delegate.h"

#include <type_traits>

#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

using DecodeStatus = VP9Decoder::VP9Accelerator::Status;

VP9VaapiVideoDecoderDelegate::VP9VaapiVideoDecoderDelegate(
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

VP9VaapiVideoDecoderDelegate::~VP9VaapiVideoDecoderDelegate() {
  DCHECK(!picture_params_);
  DCHECK(!slice_params_);
  DCHECK(!crypto_params_);
  DCHECK(!protected_params_);
}

scoped_refptr<VP9Picture> VP9VaapiVideoDecoderDelegate::CreateVP9Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto va_surface_handle = vaapi_dec_->CreateSurface();
  if (!va_surface_handle) {
    return nullptr;
  }

  return new VaapiVP9Picture(std::move(va_surface_handle));
}

DecodeStatus VP9VaapiVideoDecoderDelegate::SubmitDecode(
    scoped_refptr<VP9Picture> pic,
    const Vp9SegmentationParams& seg,
    const Vp9LoopFilterParams& lf,
    const Vp9ReferenceFrameVector& ref_frames) {
  TRACE_EVENT0("media,gpu", "VP9VaapiVideoDecoderDelegate::SubmitDecode");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Vp9FrameHeader* frame_hdr = pic->frame_hdr.get();
  DCHECK(frame_hdr);

  VADecPictureParameterBufferVP9 pic_param{};
  VASliceParameterBufferVP9 slice_param{};

  if (!picture_params_) {
    picture_params_ = vaapi_wrapper_->CreateVABuffer(
        VAPictureParameterBufferType, sizeof(pic_param));
    if (!picture_params_)
      return DecodeStatus::kFail;
  }
  if (!slice_params_) {
    slice_params_ = vaapi_wrapper_->CreateVABuffer(VASliceParameterBufferType,
                                                   sizeof(slice_param));
    if (!slice_params_)
      return DecodeStatus::kFail;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const DecryptConfig* decrypt_config = pic->decrypt_config();
  if (decrypt_config && !SetDecryptConfig(decrypt_config->Clone()))
    return DecodeStatus::kFail;

  bool uses_crypto = false;
  std::vector<VAEncryptionSegmentInfo> encryption_segment_info;
  VAEncryptionParameters crypto_param{};
  if (IsEncryptedSession()) {
    const ProtectedSessionState state = SetupDecryptDecode(
        /*full_sample=*/false, frame_hdr->frame_size, &crypto_param,
        &encryption_segment_info,
        decrypt_config ? decrypt_config->subsamples()
                       : std::vector<SubsampleEntry>());
    if (state == ProtectedSessionState::kFailed) {
      LOG(ERROR)
          << "SubmitDecode fails because we couldn't setup the protected "
             "session";
      return DecodeStatus::kFail;
    } else if (state != ProtectedSessionState::kCreated) {
      return DecodeStatus::kTryAgain;
    }
    uses_crypto = true;
    if (!crypto_params_) {
      crypto_params_ = vaapi_wrapper_->CreateVABuffer(
          VAEncryptionParameterBufferType, sizeof(crypto_param));
      if (!crypto_params_)
        return DecodeStatus::kFail;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  pic_param.frame_width = base::checked_cast<uint16_t>(frame_hdr->frame_width);
  pic_param.frame_height =
      base::checked_cast<uint16_t>(frame_hdr->frame_height);
  CHECK_EQ(kVp9NumRefFrames, std::size(pic_param.reference_frames));
  for (size_t i = 0; i < std::size(pic_param.reference_frames); ++i) {
    auto ref_pic = ref_frames.GetFrame(i);
    if (ref_pic) {
      pic_param.reference_frames[i] =
          ref_pic->AsVaapiVP9Picture()->va_surface_id();
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

  slice_param.slice_data_size = frame_hdr->frame_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

  static_assert(
      std::extent<decltype(Vp9SegmentationParams::feature_enabled)>() ==
          std::extent<decltype(slice_param.seg_param)>(),
      "seg_param array of incorrect size");
  for (size_t i = 0; i < std::size(slice_param.seg_param); ++i) {
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

  // Create VASliceData buffer |encoded_data| every frame so that decoding can
  // be more asynchronous than reusing the buffer.
  std::unique_ptr<ScopedVABuffer> encoded_data;

  std::vector<std::pair<VABufferID, VaapiWrapper::VABufferDescriptor>> buffers =
      {{picture_params_->id(),
        {picture_params_->type(), picture_params_->size(), &pic_param}},
       {slice_params_->id(),
        {slice_params_->type(), slice_params_->size(), &slice_param}}};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<uint8_t[]> protected_vp9_data;
  std::string amd_decrypt_params;
  if (IsTranscrypted()) {
    CHECK(decrypt_config);
    CHECK_EQ(decrypt_config->subsamples().size(), 1u);
    if (!protected_params_) {
      protected_params_ = vaapi_wrapper_->CreateVABuffer(
          VAProtectedSliceDataBufferType, decrypt_config->key_id().length());
      if (!protected_params_)
        return DecodeStatus::kFail;
    }
    DCHECK_EQ(decrypt_config->key_id().length(), protected_params_->size());
    // For VP9 superframes, the IV may have been incremented, so copy that
    // back into the decryption parameters. The decryption parameters struct has
    // a uint32_t for the first parameter, and the second is the 128-bit IV and
    // then various other fields. Total max structure size is 128 bytes. The
    // structure definition is in ChromeOS internal code so we do not reference
    // it directly here.
    constexpr uint32_t dp_iv_offset = sizeof(uint32_t);
    amd_decrypt_params = decrypt_config->key_id();
    memcpy(&amd_decrypt_params[dp_iv_offset], decrypt_config->iv().data(),
           DecryptConfig::kDecryptionKeySize);
    buffers.push_back({protected_params_->id(),
                       {protected_params_->type(), protected_params_->size(),
                        amd_decrypt_params.data()}});

    // For transcrypted VP9 on AMD we need to send the UCH + cypher_bytes from
    // the buffer as the slice data per AMD's instructions.
    base::CheckedNumeric<size_t> protected_data_size =
        decrypt_config->subsamples()[0].cypher_bytes;
    protected_data_size += frame_hdr->uncompressed_header_size;
    if (!protected_data_size.IsValid()) {
      DVLOG(1) << "Invalid protected_data_size";
      return DecodeStatus::kFail;
    }
    encoded_data = vaapi_wrapper_->CreateVABuffer(
        VASliceDataBufferType, protected_data_size.ValueOrDie());
    if (!encoded_data)
      return DecodeStatus::kFail;
    protected_vp9_data =
        std::make_unique<uint8_t[]>(protected_data_size.ValueOrDie());
    // Copy the UCH.
    memcpy(protected_vp9_data.get(), frame_hdr->data,
           frame_hdr->uncompressed_header_size);
    // Copy the transcrypted data.
    memcpy(protected_vp9_data.get() + frame_hdr->uncompressed_header_size,
           frame_hdr->data + decrypt_config->subsamples()[0].clear_bytes,
           base::strict_cast<size_t>(decrypt_config->subsamples()[0].cypher_bytes));
    buffers.push_back({encoded_data->id(),
                       {encoded_data->type(), encoded_data->size(),
                        protected_vp9_data.get()}});
  } else {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    encoded_data = vaapi_wrapper_->CreateVABuffer(VASliceDataBufferType,
                                                  frame_hdr->frame_size);
    if (!encoded_data)
      return DecodeStatus::kFail;
    buffers.push_back(
        {encoded_data->id(),
         {encoded_data->type(), encoded_data->size(), frame_hdr->data}});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  }
  if (uses_crypto) {
    buffers.push_back(
        {crypto_params_->id(),
         {crypto_params_->type(), crypto_params_->size(), &crypto_param}});
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const VaapiVP9Picture* vaapi_pic = pic->AsVaapiVP9Picture();
  const bool success =
      vaapi_wrapper_->MapAndCopyAndExecute(vaapi_pic->va_surface_id(), buffers);
  if (!success && NeedsProtectedSessionRecovery())
    return DecodeStatus::kTryAgain;

  if (success && IsEncryptedSession())
    ProtectedDecodedSucceeded();

  return success ? DecodeStatus::kOk : DecodeStatus::kFail;
}

bool VP9VaapiVideoDecoderDelegate::OutputPicture(
    scoped_refptr<VP9Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const VaapiVP9Picture* vaapi_pic = pic->AsVaapiVP9Picture();
  vaapi_dec_->SurfaceReady(vaapi_pic->va_surface_id(),
                           vaapi_pic->bitstream_id(), vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

bool VP9VaapiVideoDecoderDelegate::NeedsCompressedHeaderParsed() const {
  return false;
}

void VP9VaapiVideoDecoderDelegate::OnVAContextDestructionSoon() {
  // Destroy the member ScopedVABuffers below since they refer to a VAContextID
  // that will be destroyed soon.
  picture_params_.reset();
  slice_params_.reset();
  crypto_params_.reset();
  protected_params_.reset();
}

}  // namespace media
