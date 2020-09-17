// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"

#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

VaapiVideoDecoderDelegate::VaapiVideoDecoderDelegate(
    DecodeSurfaceHandler<VASurface>* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : vaapi_dec_(vaapi_dec), vaapi_wrapper_(std::move(vaapi_wrapper)) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVideoDecoderDelegate::~VaapiVideoDecoderDelegate() {
  // TODO(mcasas): consider enabling the checker, https://crbug.com/789160
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VaapiVideoDecoderDelegate::set_vaapi_wrapper(
    scoped_refptr<VaapiWrapper> vaapi_wrapper) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  vaapi_wrapper_ = std::move(vaapi_wrapper);
}

void VaapiVideoDecoderDelegate::OnVAContextDestructionSoon() {}

}  // namespace media
