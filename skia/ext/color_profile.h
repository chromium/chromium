// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_COLOR_PROFILE_H_
#define SKIA_EXT_COLOR_PROFILE_H_

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace SkCodecs {
class ICCProfileChromium;
}

namespace skia {

class SK_API ColorProfile final : public SkRefCnt {
 public:
  // TODO(https://crbug.com/540759552): Remove this interface.
  static sk_sp<ColorProfile> Make(const skcms_ICCProfile& profile);

  // TODO(https://crbug.com/540759552): Remove this interface.
  static sk_sp<ColorProfile> Make(
      std::unique_ptr<SkCodecs::ICCProfileChromium> skia_profile);

  // Make a ColorProfile from ICC profile data.
  // TODO(https://crbug.com/540759552): Make this take an SkData.
  static sk_sp<ColorProfile> Make(base::span<const uint8_t> buffer);

  ColorProfile(const ColorProfile&) = delete;
  ColorProfile& operator=(const ColorProfile&) = delete;
  ~ColorProfile() override;

  // Query the type of color profile (1-channel greyscale, 3-channel RGB, or
  // 4-channel CMYK). This is used to, e.g, ignore 1-channel color profiles when
  // attached to a 3-channel image.
  bool IsGray() const {
    return profile_.data_color_space == skcms_Signature_Gray;
  }
  bool IsRGB() const {
    return profile_.data_color_space == skcms_Signature_RGB;
  }
  bool IsCMYK() const {
    return profile_.data_color_space == skcms_Signature_CMYK;
  }

  // Return the SkColorSpace that best approximates the specified profile.
  // This will always return a valid SkColorSpace (falling back to sRGB when
  // the profile is unusable).
  sk_sp<SkColorSpace> GetSkColorSpace() const { return sk_color_space_; }

  // Return if GetSkColorSpace returns an exact representation of the
  // provided ICC profile (and false if the ICC profile cannot be represented
  // as an SkColorSpace, e.g, because it is LUT-based).
  bool IsSkColorSpaceExact() const { return is_sk_color_space_exact_; }

  // Transform `rect` in `pixmap`. The source color space is the color space of
  // `this`, and the destination color space is the color space of `pixmap`. If
  // specified, `override_src_color_type` and `override_src_alpha_type` are the
  // properties of the source for the transformation. The transformation is
  // performed in-place in `pixmap`'s data, so `override_src_color_type` must
  // be the same bytes per pixel as `pixmap`.
  void TransformInPlace(
      const SkPixmap& pixmap,
      const SkIRect& rect,
      std::optional<SkColorType> override_src_color_type = std::nullopt,
      std::optional<SkAlphaType> override_src_alpha_type = std::nullopt) const;

 private:
  explicit ColorProfile(const skcms_ICCProfile& profile);
  explicit ColorProfile(
      std::unique_ptr<SkCodecs::ICCProfileChromium> skia_profile);

  void ComputeSkColorSpace();

  skcms_ICCProfile profile_;
  // Retains the parsed profile data so that pointers in profile_ remain valid.
  std::unique_ptr<SkCodecs::ICCProfileChromium> skia_profile_;
  sk_sp<SkColorSpace> sk_color_space_;
  bool is_sk_color_space_exact_ = false;
};

}  // namespace skia

#endif  // SKIA_EXT_COLOR_PROFILE_H_
