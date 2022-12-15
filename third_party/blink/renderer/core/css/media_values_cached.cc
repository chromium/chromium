// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values_cached.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

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
    small_viewport_width = MediaValues::CalculateSmallViewportWidth(frame);
    small_viewport_height = MediaValues::CalculateSmallViewportHeight(frame);
    large_viewport_width = MediaValues::CalculateLargeViewportWidth(frame);
    large_viewport_height = MediaValues::CalculateLargeViewportHeight(frame);
    dynamic_viewport_width = MediaValues::CalculateDynamicViewportWidth(frame);
    dynamic_viewport_height =
        MediaValues::CalculateDynamicViewportHeight(frame);
    device_width = MediaValues::CalculateDeviceWidth(frame);
    device_height = MediaValues::CalculateDeviceHeight(frame);
    device_pixel_ratio = MediaValues::CalculateDevicePixelRatio(frame);
    device_supports_hdr = MediaValues::CalculateDeviceSupportsHDR(frame);
    color_bits_per_component =
        MediaValues::CalculateColorBitsPerComponent(frame);
    monochrome_bits_per_component =
        MediaValues::CalculateMonochromeBitsPerComponent(frame);
    primary_pointer_type = MediaValues::CalculatePrimaryPointerType(frame);
    available_pointer_types =
        MediaValues::CalculateAvailablePointerTypes(frame);
    primary_hover_type = MediaValues::CalculatePrimaryHoverType(frame);
    available_hover_types = MediaValues::CalculateAvailableHoverTypes(frame);
    em_size = MediaValues::CalculateEmSize(frame);
    // Use 0.5em as the fallback for ex, ch, ic, and lh units. CalculateEx()
    // etc would trigger unconditional font metrics retrieval for
    // MediaValuesCached regardless of whether they are being used in a media
    // query.
    //
    // If this is changed, beware that tests like this may start failing because
    // font loading may be triggered before the call to
    // testRunner.setTextSubpixelPositioning(true):
    //
    // virtual/text-antialias/sub-pixel/text-scaling-pixel.html
    ex_size = em_size / 2.0;
    ch_size = em_size / 2.0;
    ic_size = em_size;
    line_height = em_size;
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
    horizontal_viewport_segments =
        MediaValues::CalculateHorizontalViewportSegments(frame);
    vertical_viewport_segments =
        MediaValues::CalculateVerticalViewportSegments(frame);
    device_posture = MediaValues::CalculateDevicePosture(frame);
  }
}

MediaValuesCached::MediaValuesCached() = default;

MediaValuesCached::MediaValuesCached(const MediaValuesCachedData& data)
    : data_(data) {}

MediaValuesCached::MediaValuesCached(Document& document) : data_(document) {}

MediaValues* MediaValuesCached::Copy() const {
  return MakeGarbageCollected<MediaValuesCached>(data_);
}

float MediaValuesCached::EmFontSize(float zoom) const {
  DCHECK_EQ(1.0f, zoom);
  return data_.em_size;
}

float MediaValuesCached::RemFontSize(float zoom) const {
  DCHECK_EQ(1.0f, zoom);
  // For media queries rem and em units are both based on the initial font.
  return data_.em_size;
}

float MediaValuesCached::ExFontSize(float zoom) const {
  DCHECK_EQ(1.0f, zoom);
  return data_.ex_size;
}

float MediaValuesCached::ChFontSize(float zoom) const {
  DCHECK_EQ(1.0f, zoom);
  return data_.ch_size;
}

float MediaValuesCached::IcFontSize(float zoom) const {
  DCHECK_EQ(1.0f, zoom);
  return data_.ic_size;
}

float MediaValuesCached::LineHeight(float zoom) const {
  DCHECK_EQ(1.0f, zoom);
  return data_.line_height;
}

double MediaValuesCached::ViewportWidth() const {
  return data_.viewport_width;
}

double MediaValuesCached::ViewportHeight() const {
  return data_.viewport_height;
}

double MediaValuesCached::SmallViewportWidth() const {
  return data_.small_viewport_width;
}

double MediaValuesCached::SmallViewportHeight() const {
  return data_.small_viewport_height;
}

double MediaValuesCached::LargeViewportWidth() const {
  return data_.large_viewport_width;
}

double MediaValuesCached::LargeViewportHeight() const {
  return data_.large_viewport_height;
}

double MediaValuesCached::DynamicViewportWidth() const {
  return data_.dynamic_viewport_width;
}

double MediaValuesCached::DynamicViewportHeight() const {
  return data_.dynamic_viewport_height;
}

double MediaValuesCached::ContainerWidth() const {
  return SmallViewportWidth();
}

double MediaValuesCached::ContainerHeight() const {
  return SmallViewportHeight();
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

bool MediaValuesCached::DeviceSupportsHDR() const {
  return data_.device_supports_hdr;
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

int MediaValuesCached::GetHorizontalViewportSegments() const {
  return data_.horizontal_viewport_segments;
}

int MediaValuesCached::GetVerticalViewportSegments() const {
  return data_.vertical_viewport_segments;
}

device::mojom::blink::DevicePostureType MediaValuesCached::GetDevicePosture()
    const {
  return data_.device_posture;
}

}  // namespace blink
