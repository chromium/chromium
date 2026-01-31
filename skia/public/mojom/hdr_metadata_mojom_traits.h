// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "skia/public/mojom/hdr_metadata.mojom-shared.h"
#include "skia/public/mojom/skcolorspace_primaries_mojom_traits.h"
#include "third_party/skia/include/private/SkHdrMetadata.h"

namespace mojo {

template <>
struct StructTraits<skia::mojom::SkHdrContentLightLevelInformationDataView,
                    skhdr::ContentLightLevelInformation> {
  static float max_cll(const skhdr::ContentLightLevelInformation& info) {
    return info.fMaxCLL;
  }
  static float max_fall(const skhdr::ContentLightLevelInformation& info) {
    return info.fMaxFALL;
  }

  static bool Read(skia::mojom::SkHdrContentLightLevelInformationDataView data,
                   skhdr::ContentLightLevelInformation* out) {
    out->fMaxCLL = data.max_cll();
    out->fMaxFALL = data.max_fall();
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrMasteringDisplayColorVolumeDataView,
                    skhdr::MasteringDisplayColorVolume> {
  static const SkColorSpacePrimaries& primaries(
      const skhdr::MasteringDisplayColorVolume& volume) {
    return volume.fDisplayPrimaries;
  }
  static float max_luminance(const skhdr::MasteringDisplayColorVolume& volume) {
    return volume.fMaximumDisplayMasteringLuminance;
  }
  static float min_luminance(const skhdr::MasteringDisplayColorVolume& volume) {
    return volume.fMinimumDisplayMasteringLuminance;
  }

  static bool Read(skia::mojom::SkHdrMasteringDisplayColorVolumeDataView data,
                   skhdr::MasteringDisplayColorVolume* out) {
    if (!data.ReadPrimaries(&out->fDisplayPrimaries)) {
      return false;
    }
    out->fMaximumDisplayMasteringLuminance = data.max_luminance();
    out->fMinimumDisplayMasteringLuminance = data.min_luminance();
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAgtmGainCurveControlPointDataView,
                    skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint> {
  static float x(
      const skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint& point) {
    return point.fX;
  }
  static float y(
      const skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint& point) {
    return point.fY;
  }
  static float m(
      const skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint& point) {
    return point.fM;
  }

  static bool Read(skia::mojom::SkHdrAgtmGainCurveControlPointDataView data,
                   skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint* out) {
    out->fX = data.x();
    out->fY = data.y();
    out->fM = data.m();
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAgtmGainCurveDataView,
                    skhdr::AdaptiveGlobalToneMap::GainCurve> {
  static const std::vector<
      skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint>&
  control_points(const skhdr::AdaptiveGlobalToneMap::GainCurve& gain_curve) {
    return gain_curve.fControlPoints;
  }

  static bool Read(skia::mojom::SkHdrAgtmGainCurveDataView data,
                   skhdr::AdaptiveGlobalToneMap::GainCurve* out) {
    if (!data.ReadControlPoints(&out->fControlPoints)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAgtmComponentMixingFunctionDataView,
                    skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction> {
  static float red(
      const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction& function) {
    return function.fRed;
  }
  static float green(
      const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction& function) {
    return function.fGreen;
  }
  static float blue(
      const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction& function) {
    return function.fBlue;
  }
  static float max(
      const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction& function) {
    return function.fMax;
  }
  static float min(
      const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction& function) {
    return function.fMin;
  }
  static float component(
      const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction& function) {
    return function.fComponent;
  }

  static bool Read(skia::mojom::SkHdrAgtmComponentMixingFunctionDataView data,
                   skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction* out) {
    out->fRed = data.red();
    out->fGreen = data.green();
    out->fBlue = data.blue();
    out->fMax = data.max();
    out->fMin = data.min();
    out->fComponent = data.component();
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAgtmColorGainFunctionDataView,
                    skhdr::AdaptiveGlobalToneMap::ColorGainFunction> {
  static const skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction&
  component_mixing(const skhdr::AdaptiveGlobalToneMap::ColorGainFunction&
                       color_gain_function) {
    return color_gain_function.fComponentMixing;
  }
  static const skhdr::AdaptiveGlobalToneMap::GainCurve& gain_curve(
      const skhdr::AdaptiveGlobalToneMap::ColorGainFunction&
          color_gain_function) {
    return color_gain_function.fGainCurve;
  }

  static bool Read(skia::mojom::SkHdrAgtmColorGainFunctionDataView data,
                   skhdr::AdaptiveGlobalToneMap::ColorGainFunction* out) {
    if (!data.ReadComponentMixing(&out->fComponentMixing)) {
      return false;
    }
    if (!data.ReadGainCurve(&out->fGainCurve)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAgtmAlternateImageDataView,
                    skhdr::AdaptiveGlobalToneMap::AlternateImage> {
  static float hdr_headroom(
      const skhdr::AdaptiveGlobalToneMap::AlternateImage& alternate_image) {
    return alternate_image.fHdrHeadroom;
  }
  static const skhdr::AdaptiveGlobalToneMap::ColorGainFunction&
  color_gain_function(
      const skhdr::AdaptiveGlobalToneMap::AlternateImage& alternate_image) {
    return alternate_image.fColorGainFunction;
  }

  static bool Read(skia::mojom::SkHdrAgtmAlternateImageDataView data,
                   skhdr::AdaptiveGlobalToneMap::AlternateImage* out) {
    out->fHdrHeadroom = data.hdr_headroom();
    if (!data.ReadColorGainFunction(&out->fColorGainFunction)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAgtmHeadroomAdaptiveToneMapDataView,
                    skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap> {
  static float baseline_hdr_headroom(
      const skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap& tone_map) {
    return tone_map.fBaselineHdrHeadroom;
  }
  static const SkColorSpacePrimaries& gain_application_space_primaries(
      const skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap& tone_map) {
    return tone_map.fGainApplicationSpacePrimaries;
  }
  static const std::vector<skhdr::AdaptiveGlobalToneMap::AlternateImage>&
  alternate_images(
      const skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap& tone_map) {
    return tone_map.fAlternateImages;
  }

  static bool Read(skia::mojom::SkHdrAgtmHeadroomAdaptiveToneMapDataView data,
                   skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap* out) {
    out->fBaselineHdrHeadroom = data.baseline_hdr_headroom();
    if (!data.ReadGainApplicationSpacePrimaries(
            &out->fGainApplicationSpacePrimaries)) {
      return false;
    }
    if (!data.ReadAlternateImages(&out->fAlternateImages)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<skia::mojom::SkHdrAdaptiveGlobalToneMapDataView,
                    skhdr::AdaptiveGlobalToneMap> {
  static float hdr_reference_white(
      const skhdr::AdaptiveGlobalToneMap& tone_map) {
    return tone_map.fHdrReferenceWhite;
  }
  static const std::optional<
      skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap>&
  headroom_adaptive_tone_map(const skhdr::AdaptiveGlobalToneMap& tone_map) {
    return tone_map.fHeadroomAdaptiveToneMap;
  }

  static bool Read(skia::mojom::SkHdrAdaptiveGlobalToneMapDataView data,
                   skhdr::AdaptiveGlobalToneMap* out) {
    out->fHdrReferenceWhite = data.hdr_reference_white();
    if (!data.ReadHeadroomAdaptiveToneMap(&out->fHeadroomAdaptiveToneMap)) {
      return false;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
