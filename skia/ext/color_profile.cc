// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/color_profile.h"

#include "base/check.h"
#include "base/notreached.h"
#include "skia/ext/cicp.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/private/chromium/SkCodecsICCProfileChromium.h"

namespace skia {

namespace {

skcms_PixelFormat SkColorTypeToSkcmsPixelFormat(SkColorType color_type) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
      return skcms_PixelFormat_RGBA_8888;
    case kBGRA_8888_SkColorType:
      return skcms_PixelFormat_BGRA_8888;
    case kRGBA_F16_SkColorType:
      return skcms_PixelFormat_RGBA_hhhh;
    default:
      NOTREACHED();
  }
}

skcms_AlphaFormat SkAlphaTypeToSkcmsAlphaFormat(SkAlphaType alpha_type) {
  switch (alpha_type) {
    case kOpaque_SkAlphaType:
      return skcms_AlphaFormat_Opaque;
    case kPremul_SkAlphaType:
      return skcms_AlphaFormat_PremulAsEncoded;
    case kUnpremul_SkAlphaType:
      return skcms_AlphaFormat_Unpremul;
    default:
      NOTREACHED();
  }
}

}  // namespace

void ColorProfile::ComputeSkColorSpace() {
  // If the ICC profile has CICP data, prefer to use that.
  if (profile_.has_CICP) {
    sk_color_space_ = skia::CICPGetSkColorSpace(
        profile_.CICP.color_primaries, profile_.CICP.transfer_characteristics,
        profile_.CICP.matrix_coefficients, profile_.CICP.video_full_range_flag,
        /*prefer_srgb_trfn=*/true);
    if (sk_color_space_) {
      is_sk_color_space_exact_ = true;
      return;
    }
  }

  // If there was no CICP data, then use the ICC profile.
  sk_color_space_ = SkColorSpace::Make(profile_);
  if (sk_color_space_) {
    is_sk_color_space_exact_ = true;
    return;
  }

  // If the embedded color space isn't supported by Skia, transform
  // to a supported color space. Preserve the gamut, but convert to a
  // standard transfer function.
  if (profile_.has_toXYZD50) {
    skcms_ICCProfile with_srgb = profile_;
    skcms_SetTransferFunction(&with_srgb, skcms_sRGB_TransferFunction());
    sk_color_space_ = SkColorSpace::Make(with_srgb);
    if (sk_color_space_) {
      is_sk_color_space_exact_ = false;
      return;
    }
  }

  // For color spaces without an identifiable gamut, just default to sRGB.
  sk_color_space_ = SkColorSpace::MakeSRGB();
  is_sk_color_space_exact_ = false;
}

ColorProfile::ColorProfile(const skcms_ICCProfile& profile)
    : profile_(profile) {
  ComputeSkColorSpace();
}

ColorProfile::ColorProfile(
    std::unique_ptr<SkCodecs::ICCProfileChromium> skia_profile)
    : profile_(skia_profile->GetProfile()),
      skia_profile_(std::move(skia_profile)) {
  ComputeSkColorSpace();
}

ColorProfile::~ColorProfile() = default;

sk_sp<ColorProfile> ColorProfile::Make(const skcms_ICCProfile& profile) {
  return sk_sp<ColorProfile>(new ColorProfile(profile));
}

sk_sp<ColorProfile> ColorProfile::Make(
    std::unique_ptr<SkCodecs::ICCProfileChromium> skia_profile) {
  if (!skia_profile) {
    return nullptr;
  }
  return sk_sp<ColorProfile>(new ColorProfile(std::move(skia_profile)));
}

sk_sp<ColorProfile> ColorProfile::Make(base::span<const uint8_t> buffer) {
  auto owned_data = SkData::MakeWithCopy(buffer.data(), buffer.size());
  auto skia_profile = SkCodecs::ICCProfileChromium::Make(std::move(owned_data));
  if (!skia_profile) {
    return nullptr;
  }
  return Make(std::move(skia_profile));
}

void ColorProfile::TransformInPlace(
    const SkPixmap& pixmap,
    const SkIRect& rect,
    std::optional<SkColorType> override_src_color_type,
    std::optional<SkAlphaType> override_src_alpha_type) const {
  const skcms_ICCProfile* src_profile = &profile_;
  skcms_ICCProfile dst_profile;
  if (pixmap.colorSpace()) {
    pixmap.colorSpace()->toProfile(&dst_profile);
  } else {
    SkColorSpace::MakeSRGB()->toProfile(&dst_profile);
  }

  const skcms_AlphaFormat dst_alpha_format =
      SkAlphaTypeToSkcmsAlphaFormat(pixmap.alphaType());
  const skcms_AlphaFormat src_alpha_format =
      override_src_alpha_type.has_value()
          ? SkAlphaTypeToSkcmsAlphaFormat(*override_src_alpha_type)
          : dst_alpha_format;

  const skcms_PixelFormat dst_pixel_format =
      SkColorTypeToSkcmsPixelFormat(pixmap.colorType());
  const skcms_PixelFormat src_pixel_format =
      override_src_color_type.has_value()
          ? SkColorTypeToSkcmsPixelFormat(*override_src_color_type)
          : dst_pixel_format;

  if (pixmap.colorType() == kRGBA_F16_SkColorType) {
    CHECK(!override_src_color_type.has_value());
  }

  for (int y = rect.top(); y < rect.bottom(); ++y) {
    void* const row = pixmap.writable_addr(rect.left(), y);
    const bool success = skcms_Transform(
        row, src_pixel_format, src_alpha_format, src_profile, row,
        dst_pixel_format, dst_alpha_format, &dst_profile, rect.width());
    DCHECK(success);
  }
}

}  // namespace skia
