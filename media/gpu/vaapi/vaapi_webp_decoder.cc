// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_webp_decoder.h"

#include <va/va.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vp8_reference_frame_vector.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/webp_parser.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

constexpr VAProfile kWebPVAProfile = VAProfileVP8Version0_3;
constexpr unsigned int kWebPVARtFormat = VA_RT_FORMAT_YUV420;

static bool IsVaapiSupportedWebP(const Vp8FrameHeader& webp_header) {
  if (!VaapiWrapper::IsDecodingSupportedForInternalFormat(kWebPVAProfile,
                                                          kWebPVARtFormat)) {
    DLOG(ERROR) << "The WebP's subsampling format is unsupported";
    return false;
  }

  // Validate the size.
  // TODO(crbug.com/984971): Make sure visible size and coded size are treated
  // similarly here: we don't currently know if we really have to provide the
  // coded size to the VAAPI. So far, it seems to work by just passing the
  // visible size, but we have to learn more, probably by looking into the
  // driver. If we need to pass the coded size, then when checking against the
  // min/max resolutions, we should use the coded size and not the visible size.
  const gfx::Size webp_size(base::strict_cast<int>(webp_header.width),
                            base::strict_cast<int>(webp_header.height));
  if (webp_size.IsEmpty()) {
    DLOG(ERROR) << "Width or height cannot be zero: " << webp_size.ToString();
    return false;
  }

  gfx::Size min_webp_resolution;
  if (!VaapiWrapper::GetDecodeMinResolution(kWebPVAProfile,
                                            &min_webp_resolution)) {
    DLOG(ERROR) << "Could not get the minimum resolution";
    return false;
  }
  if (webp_size.width() < min_webp_resolution.width() ||
      webp_size.height() < min_webp_resolution.height()) {
    DLOG(ERROR) << "VAAPI doesn't support size " << webp_size.ToString()
                << ": under minimum resolution "
                << min_webp_resolution.ToString();
    return false;
  }

  gfx::Size max_webp_resolution;
  if (!VaapiWrapper::GetDecodeMaxResolution(kWebPVAProfile,
                                            &max_webp_resolution)) {
    DLOG(ERROR) << "Could not get the maximum resolution";
    return false;
  }
  if (webp_size.width() > max_webp_resolution.width() ||
      webp_size.height() > max_webp_resolution.height()) {
    DLOG(ERROR) << "VAAPI doesn't support size " << webp_size.ToString()
                << ": above maximum resolution "
                << max_webp_resolution.ToString();
    return false;
  }
  return true;
}

}  // namespace

VaapiWebPDecoder::VaapiWebPDecoder() : VaapiImageDecoder(kWebPVAProfile) {}

VaapiWebPDecoder::~VaapiWebPDecoder() = default;

gpu::ImageDecodeAcceleratorType VaapiWebPDecoder::GetType() const {
  return gpu::ImageDecodeAcceleratorType::kWebP;
}

SkYUVColorSpace VaapiWebPDecoder::GetYUVColorSpace() const {
  return SkYUVColorSpace::kRec601_SkYUVColorSpace;
}

VaapiImageDecodeStatus VaapiWebPDecoder::AllocateVASurfaceAndSubmitVABuffers(
    base::span<const uint8_t> encoded_image) {
  DCHECK(vaapi_wrapper_);
  std::unique_ptr<Vp8FrameHeader> parse_result = ParseWebPImage(encoded_image);
  if (!parse_result)
    return VaapiImageDecodeStatus::kParseFailed;

  // Make sure this WebP can be decoded.
  if (!IsVaapiSupportedWebP(*parse_result))
    return VaapiImageDecodeStatus::kUnsupportedImage;

  // Prepare the VaSurface for decoding.
  const gfx::Size new_visible_size(
      base::strict_cast<int>(parse_result->width),
      base::strict_cast<int>(parse_result->height));
  DCHECK(!scoped_va_context_and_surface_ ||
         scoped_va_context_and_surface_->IsValid());
  DCHECK(!scoped_va_context_and_surface_ ||
         (scoped_va_context_and_surface_->format() == kWebPVARtFormat));
  if (!scoped_va_context_and_surface_ ||
      new_visible_size != scoped_va_context_and_surface_->size()) {
    scoped_va_context_and_surface_.reset();
    scoped_va_context_and_surface_ = ScopedVAContextAndSurface(
        vaapi_wrapper_
            ->CreateContextAndScopedVASurface(kWebPVARtFormat, new_visible_size)
            .release());
    if (!scoped_va_context_and_surface_) {
      VLOGF(1) << "CreateContextAndScopedVASurface() failed";
      return VaapiImageDecodeStatus::kSurfaceCreationFailed;
    }
    DCHECK(scoped_va_context_and_surface_->IsValid());
  }

  if (!FillVP8DataStructures(vaapi_wrapper_,
                             scoped_va_context_and_surface_->id(),
                             *parse_result, Vp8ReferenceFrameVector())) {
    return VaapiImageDecodeStatus::kSubmitVABuffersFailed;
  }

  return VaapiImageDecodeStatus::kSuccess;
}

}  // namespace media
