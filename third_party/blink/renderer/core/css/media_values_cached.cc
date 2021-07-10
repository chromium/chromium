// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values_cached.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

MediaValuesCached::MediaValuesCachedData::MediaValuesCachedData() = default;

MediaValuesCached::MediaValuesCachedData::MediaValuesCachedData(
    Document& document)
    : MediaValuesCached::MediaValuesCachedData() {
  DCHECK(IsMainThread());
  LocalFrame* frame = document.GetFrame();
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
    color_gamut = MediaValues::CalculateColorGamut(frame);
    preferred_color_scheme = MediaValues::CalculatePreferredColorScheme(frame);
    preferred_contrast = MediaValues::CalculatePreferredContrast(frame);
    prefers_reduced_motion = MediaValues::CalculatePrefersReducedMotion(frame);
    prefers_reduced_data = MediaValues::CalculatePrefersReducedData(frame);
    forced_colors = MediaValues::CalculateForcedColors(frame);
    navigation_controls = MediaValues::CalculateNavigationControls(frame);
    screen_spanning = MediaValues::CalculateScreenSpanning(frame);
    device_posture = MediaValues::CalculateDevicePosture(frame);
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

mojom::blink::PointerType MediaValuesCached::PrimaryPointerType() const {
  return data_.primary_pointer_type;
}

int MediaValuesCached::AvailablePointerTypes() const {
  return data_.available_pointer_types;
}

mojom::blink::HoverType MediaValuesCached::PrimaryHoverType() const {
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

ColorSpaceGamut MediaValuesCached::ColorGamut() const {
  return data_.color_gamut;
}

mojom::blink::PreferredColorScheme MediaValuesCached::GetPreferredColorScheme()
    const {
  return data_.preferred_color_scheme;
}

mojom::blink::PreferredContrast MediaValuesCached::GetPreferredContrast()
    const {
  return data_.preferred_contrast;
}

bool MediaValuesCached::PrefersReducedMotion() const {
  return data_.prefers_reduced_motion;
}

bool MediaValuesCached::PrefersReducedData() const {
  return data_.prefers_reduced_data;
}

ForcedColors MediaValuesCached::GetForcedColors() const {
  return data_.forced_colors;
}

NavigationControls MediaValuesCached::GetNavigationControls() const {
  return data_.navigation_controls;
}

ScreenSpanning MediaValuesCached::GetScreenSpanning() const {
  return data_.screen_spanning;
}

DevicePosture MediaValuesCached::GetDevicePosture() const {
  return data_.device_posture;
}

}  // namespace blink
