// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_vp8_accelerator.h"

#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

VaapiVP8Accelerator::VaapiVP8Accelerator(
    DecodeSurfaceHandler<VASurface>* vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : vaapi_wrapper_(vaapi_wrapper), vaapi_dec_(vaapi_dec) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVP8Accelerator::~VaapiVP8Accelerator() {
  // TODO(mcasas): consider enabling the checker, https://crbug.com/789160
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VP8Picture> VaapiVP8Accelerator::CreateVP8Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto va_surface = vaapi_dec_->CreateSurface();
  if (!va_surface)
    return nullptr;

  return new VaapiVP8Picture(std::move(va_surface));
}

bool VaapiVP8Accelerator::SubmitDecode(
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& reference_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto va_surface_id = pic->AsVaapiVP8Picture()->va_surface()->id();

  if (!FillVP8DataStructures(vaapi_wrapper_, va_surface_id, *pic->frame_hdr,
                             reference_frames)) {
    return false;
  }

  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface_id);
}

bool VaapiVP8Accelerator::OutputPicture(const scoped_refptr<VP8Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const VaapiVP8Picture* vaapi_pic = pic->AsVaapiVP8Picture();
  vaapi_dec_->SurfaceReady(vaapi_pic->va_surface(), vaapi_pic->bitstream_id(),
                           vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

}  // namespace media
