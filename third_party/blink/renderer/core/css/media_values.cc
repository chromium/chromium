// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/display/screen_info.h"

namespace blink {

ForcedColors CSSValueIDToForcedColors(CSSValueID id) {
  switch (id) {
    case CSSValueID::kActive:
      return ForcedColors::kActive;
    case CSSValueID::kNone:
      return ForcedColors::kNone;
    default:
      NOTREACHED();
      return ForcedColors::kNone;
  }
}

mojom::blink::PreferredColorScheme CSSValueIDToPreferredColorScheme(
    CSSValueID id) {
  switch (id) {
    case CSSValueID::kLight:
      return mojom::blink::PreferredColorScheme::kLight;
    case CSSValueID::kDark:
      return mojom::blink::PreferredColorScheme::kDark;
    default:
      NOTREACHED();
      return mojom::blink::PreferredColorScheme::kLight;
  }
}

mojom::blink::PreferredContrast CSSValueIDToPreferredContrast(CSSValueID id) {
  switch (id) {
    case CSSValueID::kMore:
      return mojom::blink::PreferredContrast::kMore;
    case CSSValueID::kLess:
      return mojom::blink::PreferredContrast::kLess;
    case CSSValueID::kNoPreference:
      return mojom::blink::PreferredContrast::kNoPreference;
    case CSSValueID::kCustom:
      return mojom::blink::PreferredContrast::kCustom;
    default:
      NOTREACHED();
      return mojom::blink::PreferredContrast::kNoPreference;
  }
}

absl::optional<double> MediaValues::InlineSize() const {
  if (IsHorizontalWritingMode(GetWritingMode()))
    return Width();
  return Height();
}

absl::optional<double> MediaValues::BlockSize() const {
  if (IsHorizontalWritingMode(GetWritingMode()))
    return Height();
  return Width();
}

MediaValues* MediaValues::CreateDynamicIfFrameExists(LocalFrame* frame) {
  if (frame)
    return MediaValuesDynamic::Create(frame);
  return MakeGarbageCollected<MediaValuesCached>();
}

double MediaValues::ViewportInlineSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? ViewportWidth()
                                                   : ViewportHeight();
}

double MediaValues::ViewportBlockSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? ViewportHeight()
                                                   : ViewportWidth();
}

double MediaValues::SmallViewportInlineSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? SmallViewportWidth()
                                                   : SmallViewportHeight();
}

double MediaValues::SmallViewportBlockSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? SmallViewportHeight()
                                                   : SmallViewportWidth();
}

double MediaValues::LargeViewportInlineSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? LargeViewportWidth()
                                                   : LargeViewportHeight();
}

double MediaValues::LargeViewportBlockSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? LargeViewportHeight()
                                                   : LargeViewportWidth();
}

double MediaValues::DynamicViewportInlineSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? DynamicViewportWidth()
                                                   : DynamicViewportHeight();
}

double MediaValues::DynamicViewportBlockSize() const {
  return IsHorizontalWritingMode(GetWritingMode()) ? DynamicViewportHeight()
                                                   : DynamicViewportWidth();
}

double MediaValues::CalculateViewportWidth(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->ViewportSizeForMediaQueries().width();
}

double MediaValues::CalculateViewportHeight(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->ViewportSizeForMediaQueries().height();
}

double MediaValues::CalculateSmallViewportWidth(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->SmallViewportSizeForViewportUnits().width();
}

double MediaValues::CalculateSmallViewportHeight(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->SmallViewportSizeForViewportUnits().height();
}

double MediaValues::CalculateLargeViewportWidth(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->LargeViewportSizeForViewportUnits().width();
}

double MediaValues::CalculateLargeViewportHeight(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->LargeViewportSizeForViewportUnits().height();
}

double MediaValues::CalculateDynamicViewportWidth(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->DynamicViewportSizeForViewportUnits().width();
}

double MediaValues::CalculateDynamicViewportHeight(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->View());
  DCHECK(frame->GetDocument());
  return frame->View()->DynamicViewportSizeForViewportUnits().height();
}

int MediaValues::CalculateDeviceWidth(LocalFrame* frame) {
  DCHECK(frame && frame->View() && frame->GetSettings() && frame->GetPage());
  const display::ScreenInfo& screen_info =
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame);
  int device_width = screen_info.rect.width();
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    device_width = static_cast<int>(
        lroundf(device_width * screen_info.device_scale_factor));
  }
  return device_width;
}

int MediaValues::CalculateDeviceHeight(LocalFrame* frame) {
  DCHECK(frame && frame->View() && frame->GetSettings() && frame->GetPage());
  const display::ScreenInfo& screen_info =
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame);
  int device_height = screen_info.rect.height();
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    device_height = static_cast<int>(
        lroundf(device_height * screen_info.device_scale_factor));
  }
  return device_height;
}

bool MediaValues::CalculateStrictMode(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  return !frame->GetDocument()->InQuirksMode();
}

float MediaValues::CalculateDevicePixelRatio(LocalFrame* frame) {
  return frame->DevicePixelRatio();
}

bool MediaValues::CalculateDeviceSupportsHDR(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetPage());
  return frame->GetPage()
      ->GetChromeClient()
      .GetScreenInfo(*frame)
      .display_color_spaces.SupportsHDR();
}

int MediaValues::CalculateColorBitsPerComponent(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetPage());
  const display::ScreenInfo& screen_info =
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame);
  if (screen_info.is_monochrome)
    return 0;
  return screen_info.depth_per_component;
}

int MediaValues::CalculateMonochromeBitsPerComponent(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetPage());
  const display::ScreenInfo& screen_info =
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame);
  if (!screen_info.is_monochrome)
    return 0;
  return screen_info.depth_per_component;
}

float MediaValues::CalculateEmSize(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  const ComputedStyle* style = frame->GetDocument()->GetComputedStyle();
  DCHECK(style);
  CSSToLengthConversionData::FontSizes font_sizes(style, style);
  return font_sizes.Em();
}

float MediaValues::CalculateExSize(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  const ComputedStyle* style = frame->GetDocument()->GetComputedStyle();
  DCHECK(style);
  CSSToLengthConversionData::FontSizes font_sizes(style, style);
  // Font metrics are based on the used font which is scaled to match the size
  // of CSS pixels. Need to scale back to CSS pixels.
  return font_sizes.Ex() / font_sizes.Zoom();
}

float MediaValues::CalculateChSize(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  const ComputedStyle* style = frame->GetDocument()->GetComputedStyle();
  DCHECK(style);
  CSSToLengthConversionData::FontSizes font_sizes(style, style);
  // Font metrics are based on the used font which is scaled to match the size
  // of CSS pixels. Need to scale back to CSS pixels.
  return font_sizes.Ch() / font_sizes.Zoom();
}

const String MediaValues::CalculateMediaType(LocalFrame* frame) {
  DCHECK(frame);
  if (!frame->View())
    return g_empty_atom;
  return frame->View()->MediaType();
}

mojom::blink::DisplayMode MediaValues::CalculateDisplayMode(LocalFrame* frame) {
  DCHECK(frame);

  blink::mojom::DisplayMode mode =
      frame->GetPage()->GetSettings().GetDisplayModeOverride();
  if (mode != mojom::blink::DisplayMode::kUndefined)
    return mode;

  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  if (!widget)  // Is null in non-ordinary Pages.
    return mojom::blink::DisplayMode::kBrowser;

  return widget->DisplayMode();
}

bool MediaValues::CalculateThreeDEnabled(LocalFrame* frame) {
  return frame->GetPage()->GetSettings().GetAcceleratedCompositingEnabled();
}

bool MediaValues::CalculateInImmersiveMode(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetImmersiveModeEnabled();
}

mojom::blink::PointerType MediaValues::CalculatePrimaryPointerType(
    LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetPrimaryPointerType();
}

int MediaValues::CalculateAvailablePointerTypes(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetAvailablePointerTypes();
}

mojom::blink::HoverType MediaValues::CalculatePrimaryHoverType(
    LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetPrimaryHoverType();
}

int MediaValues::CalculateAvailableHoverTypes(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetAvailableHoverTypes();
}

ColorSpaceGamut MediaValues::CalculateColorGamut(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetPage());
  if (const auto* overrides = frame->GetPage()->GetMediaFeatureOverrides()) {
    MediaQueryExpValue value = overrides->GetOverride("color-gamut");
    if (value.IsValid()) {
      if (value.Id() == CSSValueID::kSRGB)
        return ColorSpaceGamut::SRGB;
      if (value.Id() == CSSValueID::kP3)
        return ColorSpaceGamut::P3;
      // Rec. 2020 is also known as ITU-R-Empfehlung BT.2020.
      if (value.Id() == CSSValueID::kRec2020)
        return ColorSpaceGamut::BT2020;
      NOTREACHED();
    }
  }
  return color_space_utilities::GetColorSpaceGamut(
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame));
}

mojom::blink::PreferredColorScheme MediaValues::CalculatePreferredColorScheme(
    LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  DCHECK(frame->GetDocument());
  DCHECK(frame->GetPage());
  if (const auto* overrides = frame->GetPage()->GetMediaFeatureOverrides()) {
    MediaQueryExpValue value = overrides->GetOverride("prefers-color-scheme");
    if (value.IsValid())
      return CSSValueIDToPreferredColorScheme(value.Id());
  }
  return frame->GetDocument()->GetStyleEngine().GetPreferredColorScheme();
}

mojom::blink::PreferredContrast MediaValues::CalculatePreferredContrast(
    LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  DCHECK(frame->GetPage());
  if (const auto* overrides = frame->GetPage()->GetMediaFeatureOverrides()) {
    MediaQueryExpValue value = overrides->GetOverride("prefers-contrast");
    if (value.IsValid())
      return CSSValueIDToPreferredContrast(value.Id());
  }
  return frame->GetSettings()->GetPreferredContrast();
}

bool MediaValues::CalculatePrefersReducedMotion(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  if (const auto* overrides = frame->GetPage()->GetMediaFeatureOverrides()) {
    MediaQueryExpValue value = overrides->GetOverride("prefers-reduced-motion");
    if (value.IsValid())
      return value.Id() == CSSValueID::kReduce;
  }
  return frame->GetSettings()->GetPrefersReducedMotion();
}

bool MediaValues::CalculatePrefersReducedData(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  if (const auto* overrides = frame->GetPage()->GetMediaFeatureOverrides()) {
    MediaQueryExpValue value = overrides->GetOverride("prefers-reduced-data");
    if (value.IsValid())
      return value.Id() == CSSValueID::kReduce;
  }
  return (GetNetworkStateNotifier().SaveDataEnabled() &&
          !frame->GetSettings()->GetDataSaverHoldbackWebApi());
}

ForcedColors MediaValues::CalculateForcedColors(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  if (const auto* overrides = frame->GetPage()->GetMediaFeatureOverrides()) {
    MediaQueryExpValue value = overrides->GetOverride("forced-colors");
    if (value.IsValid())
      return CSSValueIDToForcedColors(value.Id());
  }
  if (Platform::Current() && Platform::Current()->ThemeEngine())
    return Platform::Current()->ThemeEngine()->GetForcedColors();
  else
    return ForcedColors::kNone;
}

NavigationControls MediaValues::CalculateNavigationControls(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetNavigationControls();
}

int MediaValues::CalculateHorizontalViewportSegments(LocalFrame* frame) {
  if (!frame->GetWidgetForLocalRoot())
    return 1;

  WebVector<gfx::Rect> window_segments =
      frame->GetWidgetForLocalRoot()->WindowSegments();
  WTF::HashSet<int> unique_x;
  for (const auto& segment : window_segments) {
    // HashSet can't have 0 as a key, so add 1 to all the values we see.
    unique_x.insert(segment.x() + 1);
  }

  return static_cast<int>(unique_x.size());
}

int MediaValues::CalculateVerticalViewportSegments(LocalFrame* frame) {
  if (!frame->GetWidgetForLocalRoot())
    return 1;

  WebVector<gfx::Rect> window_segments =
      frame->GetWidgetForLocalRoot()->WindowSegments();
  WTF::HashSet<int> unique_y;
  for (const auto& segment : window_segments) {
    // HashSet can't have 0 as a key, so add 1 to all the values we see.
    unique_y.insert(segment.y() + 1);
  }

  return static_cast<int>(unique_y.size());
}

device::mojom::blink::DevicePostureType MediaValues::CalculateDevicePosture(
    LocalFrame* frame) {
  return frame->GetDevicePosture();
}

bool MediaValues::ComputeLengthImpl(double value,
                                    CSSPrimitiveValue::UnitType type,
                                    double& result) const {
  // The logic in this function is duplicated from
  // CSSToLengthConversionData::ZoomedComputedPixels() because
  // MediaValues::ComputeLength() needs nearly identical logic, but we haven't
  // found a way to make CSSToLengthConversionData::ZoomedComputedPixels() more
  // generic (to solve both cases) without hurting performance.
  // TODO: Unite the logic here with CSSToLengthConversionData in a performant
  // way.
  switch (type) {
    case CSSPrimitiveValue::UnitType::kEms:
      result = value * EmSize();
      return true;
    case CSSPrimitiveValue::UnitType::kRems:
      result = value * RemSize();
      return true;
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      result = value;
      return true;
    case CSSPrimitiveValue::UnitType::kExs:
      result = value * ExSize();
      return true;
    case CSSPrimitiveValue::UnitType::kChs:
      result = value * ChSize();
      return true;
    case CSSPrimitiveValue::UnitType::kViewportWidth:
      result = (value * ViewportWidth()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportHeight:
      result = (value * ViewportHeight()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportInlineSize:
      result = (value * ViewportInlineSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportBlockSize:
      result = (value * ViewportBlockSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportMin:
      result = (value * std::min(ViewportWidth(), ViewportHeight())) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kViewportMax:
      result = (value * std::max(ViewportWidth(), ViewportHeight())) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportWidth:
      result = (value * SmallViewportWidth()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportHeight:
      result = (value * SmallViewportHeight()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportInlineSize:
      result = (value * SmallViewportInlineSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportBlockSize:
      result = (value * SmallViewportBlockSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportMin:
      result = (value * std::min(SmallViewportWidth(), SmallViewportHeight())) /
               100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kSmallViewportMax:
      result = (value * std::max(SmallViewportWidth(), SmallViewportHeight())) /
               100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportWidth:
      result = (value * LargeViewportWidth()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportHeight:
      result = (value * LargeViewportHeight()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportInlineSize:
      result = (value * LargeViewportInlineSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportBlockSize:
      result = (value * LargeViewportBlockSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportMin:
      result = (value * std::min(LargeViewportWidth(), LargeViewportHeight())) /
               100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kLargeViewportMax:
      result = (value * std::max(LargeViewportWidth(), LargeViewportHeight())) /
               100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
      result = (value * DynamicViewportWidth()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
      result = (value * DynamicViewportHeight()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
      result = (value * DynamicViewportInlineSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
      result = (value * DynamicViewportBlockSize()) / 100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
      result =
          (value * std::min(DynamicViewportWidth(), DynamicViewportHeight())) /
          100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
      result =
          (value * std::max(DynamicViewportWidth(), DynamicViewportHeight())) /
          100.0;
      return true;
    case CSSPrimitiveValue::UnitType::kCentimeters:
      result = value * kCssPixelsPerCentimeter;
      return true;
    case CSSPrimitiveValue::UnitType::kMillimeters:
      result = value * kCssPixelsPerMillimeter;
      return true;
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      result = value * kCssPixelsPerQuarterMillimeter;
      return true;
    case CSSPrimitiveValue::UnitType::kInches:
      result = value * kCssPixelsPerInch;
      return true;
    case CSSPrimitiveValue::UnitType::kPoints:
      result = value * kCssPixelsPerPoint;
      return true;
    case CSSPrimitiveValue::UnitType::kPicas:
      result = value * kCssPixelsPerPica;
      return true;
    default:
      return false;
  }
}

}  // namespace blink
