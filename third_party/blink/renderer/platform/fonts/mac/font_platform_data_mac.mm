/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.h"

#if BUILDFLAG(IS_MAC)
#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#endif  // BUILDFLAG(IS_MAC)
#import <AvailabilityMacros.h>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkTypes.h"
#import "third_party/skia/include/ports/SkTypeface_mac.h"

using base::apple::ScopedCFTypeRef;

namespace {
#if BUILDFLAG(IS_MAC)
constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
#endif  // BUILDFLAG(IS_MAC)
}

namespace blink {

#if BUILDFLAG(IS_MAC)
bool VariableAxisChangeEffective(SkTypeface* typeface,
                                 SkFourByteTag axis,
                                 float new_value) {
  // First clamp new value to within range of min and max of variable axis.
  int num_axes = typeface->getVariationDesignParameters(nullptr, 0);
  if (num_axes <= 0)
    return false;

  Vector<SkFontParameters::Variation::Axis> axes_parameters(num_axes);
  int returned_axes =
      typeface->getVariationDesignParameters(axes_parameters.data(), num_axes);
  DCHECK_EQ(num_axes, returned_axes);
  DCHECK_GE(num_axes, 0);

  float clamped_new_value = new_value;
  for (auto& axis_parameters : axes_parameters) {
    if (axis_parameters.tag == axis) {
      clamped_new_value = std::min(new_value, axis_parameters.max);
      clamped_new_value = std::max(clamped_new_value, axis_parameters.min);
    }
  }

  int num_coordinates = typeface->getVariationDesignPosition(nullptr, 0);
  if (num_coordinates <= 0)
    return true;  // Font has axes, but no positions, setting one would have an
                  // effect.

  // Then compare if clamped value differs from what is set on the font.
  Vector<SkFontArguments::VariationPosition::Coordinate> coordinates(
      num_coordinates);
  int returned_coordinates =
      typeface->getVariationDesignPosition(coordinates.data(), num_coordinates);

  if (returned_coordinates != num_coordinates)
    return false;  // Something went wrong in retrieving actual axis positions,
                   // font broken?

  for (auto& coordinate : coordinates) {
    if (coordinate.axis == axis)
      return coordinate.value != clamped_new_value;
  }
  return false;
}

static bool CanLoadInProcess(CTFontRef ct_font) {
  ScopedCFTypeRef<CGFontRef> cg_font(
      CTFontCopyGraphicsFont(ct_font, /*attributes=*/nullptr));
  ScopedCFTypeRef<CFStringRef> font_name(
      CGFontCopyPostScriptName(cg_font.get()));
  return CFStringCompare(font_name.get(), CFSTR("LastResort"), 0) !=
         kCFCompareEqualTo;
}

const FontPlatformData* FontPlatformDataFromCTFont(
    CTFontRef ct_font,
    float size,
    float specified_size,
    bool synthetic_bold,
    bool synthetic_italic,
    TextRenderingMode text_rendering,
    ResolvedFontFeatures resolved_font_features,
    FontOrientation orientation,
    OpticalSizing optical_sizing,
    const FontVariationSettings* variation_settings) {
  DCHECK(ct_font);

  // fontd automatically issues a sandbox extension to permit reading
  // activated fonts that would otherwise be restricted by the sandbox.
  DCHECK(CanLoadInProcess(ct_font));

  sk_sp<SkTypeface> typeface = SkMakeTypefaceFromCTFont(ct_font);

  auto make_typeface_fontplatformdata = [&typeface, &size, &synthetic_bold,
                                         &synthetic_italic, &text_rendering,
                                         resolved_font_features,
                                         &orientation]() {
    return MakeGarbageCollected<FontPlatformData>(
        std::move(typeface), std::string(), size, synthetic_bold,
        synthetic_italic, text_rendering, resolved_font_features, orientation);
  };

  wtf_size_t valid_configured_axes =
      variation_settings && variation_settings->size() < UINT16_MAX
          ? variation_settings->size()
          : 0;

  // No variable font requested, return static font.
  if (!valid_configured_axes && optical_sizing == kNoneOpticalSizing)
    return make_typeface_fontplatformdata();

  if (!typeface)
    return nullptr;

  int existing_axes = typeface->getVariationDesignPosition(nullptr, 0);
  // Don't apply variation parameters if the font does not have axes or we
  // fail to retrieve the existing ones.
  if (existing_axes <= 0)
    return make_typeface_fontplatformdata();

  Vector<SkFontArguments::VariationPosition::Coordinate> coordinates_to_set;
  coordinates_to_set.resize(existing_axes);

  if (typeface->getVariationDesignPosition(coordinates_to_set.data(),
                                           existing_axes) != existing_axes) {
    return make_typeface_fontplatformdata();
  }

  // Iterate over the font's axes and find a missing tag from variation
  // settings, special case 'opsz', track the number of axes reconfigured.
  bool axes_reconfigured = false;
  for (auto& coordinate : coordinates_to_set) {
    // Set 'opsz' to specified size but allow having it overridden by
    // font-variation-settings in case it has 'opsz'. Do not use font size here,
    // but specified size in order to account for zoom.
    if (coordinate.axis == kOpszTag && optical_sizing == kAutoOpticalSizing) {
      if (VariableAxisChangeEffective(typeface.get(), coordinate.axis,
                                      specified_size)) {
        coordinate.value = SkFloatToScalar(specified_size);
        axes_reconfigured = true;
      }
    }
    FontVariationAxis found_variation_setting(0, 0);
    if (variation_settings && variation_settings->FindPair(
                                  coordinate.axis, &found_variation_setting)) {
      if (VariableAxisChangeEffective(typeface.get(), coordinate.axis,
                                      found_variation_setting.Value())) {
        coordinate.value = found_variation_setting.Value();
        axes_reconfigured = true;
      }
    }
  }

  if (!axes_reconfigured) {
    // No variable axes touched, return the previous typeface.
    return make_typeface_fontplatformdata();
  }

  SkFontArguments::VariationPosition variation_design_position{
      coordinates_to_set.data(), static_cast<int>(coordinates_to_set.size())};

  sk_sp<SkTypeface> cloned_typeface(typeface->makeClone(
      SkFontArguments().setVariationDesignPosition(variation_design_position)));

  if (!cloned_typeface) {
    // Applying variation parameters failed, return original typeface.
    return make_typeface_fontplatformdata();
  }
  typeface = cloned_typeface;
  return make_typeface_fontplatformdata();
}
#endif  // BUILDFLAG(IS_MAC)

SkFont FontPlatformData::CreateSkFont(
    const FontDescription* font_description) const {
  bool should_smooth_fonts = true;
  bool should_antialias = true;
  bool should_subpixel_position = true;

  if (font_description) {
    switch (font_description->FontSmoothing()) {
      case kAntialiased:
        should_smooth_fonts = false;
        break;
      case kSubpixelAntialiased:
        break;
      case kNoSmoothing:
        should_antialias = false;
        should_smooth_fonts = false;
        break;
      case kAutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default
        // settings.
        break;
    }
  }

  if (WebTestSupport::IsRunningWebTest()) {
    should_smooth_fonts = false;
    should_antialias =
        should_antialias && WebTestSupport::IsFontAntialiasingEnabledForTest();
    should_subpixel_position =
        WebTestSupport::IsTextSubpixelPositioningAllowedForTest();
  }

  if (RuntimeEnabledFeatures::DisableAhemAntialiasEnabled() && IsAhem()) {
    should_antialias = false;
  }

  SkFont skfont(typeface_);
  if (should_antialias && should_smooth_fonts) {
    skfont.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  } else if (should_antialias) {
    skfont.setEdging(SkFont::Edging::kAntiAlias);
  } else {
    skfont.setEdging(SkFont::Edging::kAlias);
  }
  skfont.setEmbeddedBitmaps(false);
  const float ts = text_size_ >= 0 ? text_size_ : 12;
  skfont.setSize(SkFloatToScalar(ts));
  skfont.setEmbolden(synthetic_bold_);
  skfont.setSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);
  skfont.setSubpixel(should_subpixel_position);

  // CoreText always provides linear metrics if it can, so the linear metrics
  // flag setting doesn't affect typefaces backed by CoreText. However, it
  // does affect FreeType backed typefaces, so set the flag for consistency.
  skfont.setLinearMetrics(should_subpixel_position);

  // When rendering using CoreGraphics, disable hinting when
  // webkit-font-smoothing:antialiased or text-rendering:geometricPrecision is
  // used.  See crbug.com/152304
  if (font_description &&
      (font_description->FontSmoothing() == kAntialiased ||
       font_description->TextRendering() == kGeometricPrecision))
    skfont.setHinting(SkFontHinting::kNone);
  return skfont;
}

}  // namespace blink
