// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_webp_decoder.h"

#include <va/va.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/config/gpu_finch_features.h"
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
  // TODO(crbug.com/41471307): Make sure visible size and coded size are treated
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
  gfx::Size max_webp_resolution;
  if (!VaapiWrapper::GetSupportedResolutions(
          kWebPVAProfile, VaapiWrapper::CodecMode::kDecode, min_webp_resolution,
          max_webp_resolution)) {
    DLOG(ERROR) << "Could not get the minimum and maximum resolutions";
    return false;
  }
  if (webp_size.width() < min_webp_resolution.width() ||
      webp_size.height() < min_webp_resolution.height()) {
    DLOG(ERROR) << "VAAPI doesn't support size " << webp_size.ToString()
                << ": under minimum resolution "
                << min_webp_resolution.ToString();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  return SkYUVColorSpace::kRec601_SkYUVColorSpace;
}

// static
std::optional<gpu::ImageDecodeAcceleratorSupportedProfile>
VaapiWebPDecoder::GetSupportedProfile() {
  if (!base::FeatureList::IsEnabled(
          features::kVaapiWebPImageDecodeAcceleration)) {
    return std::nullopt;
  }
  gpu::ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = gpu::ImageDecodeAcceleratorType::kWebP;

  const bool got_supported_resolutions = VaapiWrapper::GetSupportedResolutions(
      kWebPVAProfile, VaapiWrapper::CodecMode::kDecode,
      profile.min_encoded_dimensions, profile.max_encoded_dimensions);
  if (!got_supported_resolutions) {
    return std::nullopt;
  }

  // TODO(andrescj): Ideally, we would advertise support for all the formats
  // supported by the driver. However, for now, we will only support exposing
  // YUV 4:2:0 surfaces as DmaBufs.
  CHECK(VaapiWrapper::GetDecodeSupportedInternalFormats(kWebPVAProfile).yuv420);
  profile.subsamplings.push_back(gpu::ImageDecodeAcceleratorSubsampling::k420);
  return profile;
}

VaapiImageDecodeStatus VaapiWebPDecoder::AllocateVASurfaceAndSubmitVABuffers(
    base::span<const uint8_t> encoded_image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
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
    auto scoped_va_surfaces = vaapi_wrapper_->CreateContextAndScopedVASurfaces(
        kWebPVARtFormat, new_visible_size,
        {VaapiWrapper::SurfaceUsageHint::kGeneric}, 1u,
        /*visible_size=*/std::nullopt);
    if (scoped_va_surfaces.empty()) {
      VLOGF(1) << "CreateContextAndScopedVASurface() failed";
      return VaapiImageDecodeStatus::kSurfaceCreationFailed;
    }

    scoped_va_context_and_surface_ =
        ScopedVAContextAndSurface(scoped_va_surfaces[0].release());
    DCHECK(scoped_va_context_and_surface_->IsValid());
  }
  DCHECK_NE(scoped_va_context_and_surface_->id(), VA_INVALID_SURFACE);

  VAIQMatrixBufferVP8 iq_matrix_buf{};
  VAProbabilityDataBufferVP8 prob_buf{};
  VAPictureParameterBufferVP8 pic_param{};
  VASliceParameterBufferVP8 slice_param{};
  FillVP8DataStructures(*parse_result, Vp8ReferenceFrameVector(),
                        &iq_matrix_buf, &prob_buf, &pic_param, &slice_param);

  const bool success = vaapi_wrapper_->SubmitBuffers(
      {{VAIQMatrixBufferType, sizeof(iq_matrix_buf), &iq_matrix_buf},
       {VAProbabilityBufferType, sizeof(prob_buf), &prob_buf},
       {VAPictureParameterBufferType, sizeof(pic_param), &pic_param},
       {VASliceParameterBufferType, sizeof(slice_param), &slice_param},
       {VASliceDataBufferType, parse_result->frame_size,
        parse_result->data.get()}});

  return success ? VaapiImageDecodeStatus::kSuccess
                 : VaapiImageDecodeStatus::kSubmitVABuffersFailed;
}

}  // namespace media
