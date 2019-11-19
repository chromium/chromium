// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values_cached.h"

#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/common/css/preferred_color_scheme.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"

namespace blink {

MediaValuesCached::MediaValuesCachedData::MediaValuesCachedData()
    : viewport_width(0),
      viewport_height(0),
      device_width(0),
      device_height(0),
      device_pixel_ratio(1.0),
      color_bits_per_component(24),
      monochrome_bits_per_component(0),
      primary_pointer_type(kPointerTypeNone),
      available_pointer_types(kPointerTypeNone),
      primary_hover_type(kHoverTypeNone),
      available_hover_types(kHoverTypeNone),
      default_font_size(16),
      three_d_enabled(false),
      immersive_mode(false),
      strict_mode(true),
      display_mode(blink::mojom::DisplayMode::kBrowser),
      display_shape(kDisplayShapeRect),
      color_gamut(ColorSpaceGamut::kUnknown),
      preferred_color_scheme(PreferredColorScheme::kNoPreference),
      prefers_reduced_motion(false),
      forced_colors(ForcedColors::kNone),
      navigation_controls(NavigationControls::kNone) {}

MediaValuesCached::MediaValuesCachedData::MediaValuesCachedData(
    Document& document)
    : MediaValuesCached::MediaValuesCachedData() {
  DCHECK(IsMainThread());
  LocalFrame* frame = document.GetFrameOfMasterDocument();
  // TODO(hiroshige): Clean up |frame->view()| conditions.
  DCHECK(!frame || frame->View());
  if (frame && frame->View()) {
    DCHECK(frame->GetDocument());
    DCHECK(frame->GetDocument()->GetLayoutView());

    // In case that frame is missing (e.g. for images that their document does
    // not have a frame)
    // We simply leave the MediaValues object with the default
    // MediaValuesCachedData values.
    viewport_width = MediaValues::CalculateViewportWidth(frame);
    viewport_height = MediaValues::CalculateViewportHeight(frame);
    device_width = MediaValues::CalculateDeviceWidth(frame);
    device_height = MediaValues::CalculateDeviceHeight(frame);
    device_pixel_ratio = MediaValues::CalculateDevicePixelRatio(frame);
    color_bits_per_component =
        MediaValues::CalculateColorBitsPerComponent(frame);
    monochrome_bits_per_component =
        MediaValues::CalculateMonochromeBitsPerComponent(frame);
    primary_pointer_type = MediaValues::CalculatePrimaryPointerType(frame);
    available_pointer_types =
        MediaValues::CalculateAvailablePointerTypes(frame);
    primary_hover_type = MediaValues::CalculatePrimaryHoverType(frame);
    available_hover_types = MediaValues::CalculateAvailableHoverTypes(frame);
    default_font_size = MediaValues::CalculateDefaultFontSize(frame);
    three_d_enabled = MediaValues::CalculateThreeDEnabled(frame);
    immersive_mode = MediaValues::CalculateInImmersiveMode(frame);
    strict_mode = MediaValues::CalculateStrictMode(frame);
    display_mode = MediaValues::CalculateDisplayMode(frame);
    media_type = MediaValues::CalculateMediaType(frame);
    display_shape = MediaValues::CalculateDisplayShape(frame);
    color_gamut = MediaValues::CalculateColorGamut(frame);
    preferred_color_scheme = MediaValues::CalculatePreferredColorScheme(frame);
    prefers_reduced_motion = MediaValues::CalculatePrefersReducedMotion(frame);
    forced_colors = MediaValues::CalculateForcedColors();
    navigation_controls = MediaValues::CalculateNavigationControls(frame);
  }
}

MediaValuesCached::MediaValuesCached() = default;

MediaValuesCached::MediaValuesCached(const MediaValuesCachedData& data)
    : data_(data) {}

MediaValues* MediaValuesCached::Copy() const {
  return MakeGarbageCollected<MediaValuesCached>(data_);
}

bool MediaValuesCached::ComputeLength(double value,
                                      CSSPrimitiveValue::UnitType type,
                                      int& result) const {
  return MediaValues::ComputeLength(value, type, data_.default_font_size,
                                    data_.viewport_width, data_.viewport_height,
                                    result);
}

bool MediaValuesCached::ComputeLength(double value,
                                      CSSPrimitiveValue::UnitType type,
                                      double& result) const {
  return MediaValues::ComputeLength(value, type, data_.default_font_size,
                                    data_.viewport_width, data_.viewport_height,
                                    result);
}

double MediaValuesCached::ViewportWidth() const {
  return data_.viewport_width;
}

double MediaValuesCached::ViewportHeight() const {
  return data_.viewport_height;
}

int MediaValuesCached::DeviceWidth() const {
  return data_.device_width;
}

int MediaValuesCached::DeviceHeight() const {
  return data_.device_height;
}

float MediaValuesCached::DevicePixelRatio() const {
  return data_.device_pixel_ratio;
}

int MediaValuesCached::ColorBitsPerComponent() const {
  return data_.color_bits_per_component;
}

int MediaValuesCached::MonochromeBitsPerComponent() const {
  return data_.monochrome_bits_per_component;
}

PointerType MediaValuesCached::PrimaryPointerType() const {
  return data_.primary_pointer_type;
}

int MediaValuesCached::AvailablePointerTypes() const {
  return data_.available_pointer_types;
}

HoverType MediaValuesCached::PrimaryHoverType() const {
  return data_.primary_hover_type;
}

int MediaValuesCached::AvailableHoverTypes() const {
  return data_.available_hover_types;
}

bool MediaValuesCached::ThreeDEnabled() const {
  return data_.three_d_enabled;
}

bool MediaValuesCached::InImmersiveMode() const {
  return data_.immersive_mode;
}

bool MediaValuesCached::StrictMode() const {
  return data_.strict_mode;
}

const String MediaValuesCached::MediaType() const {
  return data_.media_type;
}

blink::mojom::DisplayMode MediaValuesCached::DisplayMode() const {
  return data_.display_mode;
}

Document* MediaValuesCached::GetDocument() const {
  return nullptr;
}

bool MediaValuesCached::HasValues() const {
  return true;
}

void MediaValuesCached::OverrideViewportDimensions(double width,
                                                   double height) {
  data_.viewport_width = width;
  data_.viewport_height = height;
}

DisplayShape MediaValuesCached::GetDisplayShape() const {
  return data_.display_shape;
}

ColorSpaceGamut MediaValuesCached::ColorGamut() const {
  return data_.color_gamut;
}

PreferredColorScheme MediaValuesCached::GetPreferredColorScheme() const {
  return data_.preferred_color_scheme;
}

bool MediaValuesCached::PrefersReducedMotion() const {
  return data_.prefers_reduced_motion;
}

ForcedColors MediaValuesCached::GetForcedColors() const {
  return data_.forced_colors;
}

NavigationControls MediaValuesCached::GetNavigationControls() const {
  return data_.navigation_controls;
}

}  // namespace blink
