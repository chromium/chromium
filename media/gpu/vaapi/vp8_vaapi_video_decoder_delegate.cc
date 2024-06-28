// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp8_vaapi_video_decoder_delegate.h"

#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

VP8VaapiVideoDecoderDelegate::VP8VaapiVideoDecoderDelegate(
    VaapiDecodeSurfaceHandler* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : VaapiVideoDecoderDelegate(vaapi_dec,
                                std::move(vaapi_wrapper),
                                base::DoNothing(),
                                nullptr) {}

VP8VaapiVideoDecoderDelegate::~VP8VaapiVideoDecoderDelegate() {
  DCHECK(!iq_matrix_);
  DCHECK(!prob_buffer_);
  DCHECK(!picture_params_);
  DCHECK(!slice_params_);
}

scoped_refptr<VP8Picture> VP8VaapiVideoDecoderDelegate::CreateVP8Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto va_surface_handle = vaapi_dec_->CreateSurface();
  if (!va_surface_handle) {
    return nullptr;
  }

  return new VaapiVP8Picture(std::move(va_surface_handle));
}

bool VP8VaapiVideoDecoderDelegate::SubmitDecode(
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& reference_frames) {
  TRACE_EVENT0("media,gpu", "VP8VaapiVideoDecoderDelegate::SubmitDecode");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto va_surface_id = pic->AsVaapiVP8Picture()->va_surface_id();
  DCHECK_NE(va_surface_id, VA_INVALID_SURFACE);

  VAIQMatrixBufferVP8 iq_matrix_buf{};
  VAProbabilityDataBufferVP8 prob_buf{};
  VAPictureParameterBufferVP8 pic_param{};
  VASliceParameterBufferVP8 slice_param{};

  const Vp8FrameHeader* const header = pic->frame_hdr.get();
  DCHECK(header);

  FillVP8DataStructures(*header, reference_frames, &iq_matrix_buf, &prob_buf,
                        &pic_param, &slice_param);

  if (!iq_matrix_) {
    iq_matrix_ = vaapi_wrapper_->CreateVABuffer(VAIQMatrixBufferType,
                                                sizeof(iq_matrix_buf));
    if (!iq_matrix_)
      return false;
  }
  if (!prob_buffer_) {
    prob_buffer_ = vaapi_wrapper_->CreateVABuffer(VAProbabilityBufferType,
                                                  sizeof(prob_buf));
    if (!prob_buffer_)
      return false;
  }
  if (!picture_params_) {
    picture_params_ = vaapi_wrapper_->CreateVABuffer(
        VAPictureParameterBufferType, sizeof(pic_param));
    if (!picture_params_)
      return false;
  }
  if (!slice_params_) {
    slice_params_ = vaapi_wrapper_->CreateVABuffer(VASliceParameterBufferType,
                                                   sizeof(slice_param));
    if (!slice_params_)
      return false;
  }

  // Create VASliceData buffer |encoded_data| every frame so that decoding can
  // be more asynchronous than reusing the buffer.
  std::unique_ptr<ScopedVABuffer> encoded_data =
      vaapi_wrapper_->CreateVABuffer(VASliceDataBufferType, header->frame_size);
  if (!encoded_data)
    return false;

  return vaapi_wrapper_->MapAndCopyAndExecute(
      va_surface_id,
      {{iq_matrix_->id(),
        {iq_matrix_->type(), iq_matrix_->size(), &iq_matrix_buf}},
       {prob_buffer_->id(),
        {prob_buffer_->type(), prob_buffer_->size(), &prob_buf}},
       {picture_params_->id(),
        {picture_params_->type(), picture_params_->size(), &pic_param}},
       {slice_params_->id(),
        {slice_params_->type(), slice_params_->size(), &slice_param}},
       {encoded_data->id(),
        {encoded_data->type(), header->frame_size, header->data.get()}}});
}

bool VP8VaapiVideoDecoderDelegate::OutputPicture(
    scoped_refptr<VP8Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const VaapiVP8Picture* vaapi_pic = pic->AsVaapiVP8Picture();
  vaapi_dec_->SurfaceReady(vaapi_pic->va_surface_id(),
                           vaapi_pic->bitstream_id(), vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

void VP8VaapiVideoDecoderDelegate::OnVAContextDestructionSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Destroy the member ScopedVABuffers below since they refer to a VAContextID
  // that will be destroyed soon.
  iq_matrix_.reset();
  prob_buffer_.reset();
  picture_params_.reset();
  slice_params_.reset();
}

}  // namespace media
