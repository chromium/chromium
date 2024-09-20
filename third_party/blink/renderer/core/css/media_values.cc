// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values.h"

#include "third_party/blink/public/common/css/scripting.h"
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
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/preferences/preference_overrides.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/base/mojom/window_show_state.mojom-blink.h"
#include "ui/display/screen_info.h"

namespace blink {

ForcedColors CSSValueIDToForcedColors(CSSValueID id) {
  switch (id) {
    case CSSValueID::kActive:
      return ForcedColors::kActive;
    case CSSValueID::kNone:
      return ForcedColors::kNone;
    default:
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return mojom::blink::PreferredContrast::kNoPreference;
  }
}

std::optional<double> MediaValues::InlineSize() const {
  if (blink::IsHorizontalWritingMode(GetWritingMode())) {
    return Width();
  }
  return Height();
}

std::optional<double> MediaValues::BlockSize() const {
  if (blink::IsHorizontalWritingMode(GetWritingMode())) {
    return Height();
  }
  return Width();
}

bool MediaValues::SnappedBlock() const {
  if (blink::IsHorizontalWritingMode(GetWritingMode())) {
    return SnappedY();
  }
  return SnappedX();
}

bool MediaValues::SnappedInline() const {
  if (blink::IsHorizontalWritingMode(GetWritingMode())) {
    return SnappedX();
  }
  return SnappedY();
}

MediaValues* MediaValues::CreateDynamicIfFrameExists(LocalFrame* frame) {
  if (frame) {
    return MediaValuesDynamic::Create(frame);
  }
  return MakeGarbageCollected<MediaValuesCached>();
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
  if (screen_info.is_monochrome) {
    return 0;
  }
  return screen_info.depth_per_component;
}

int MediaValues::CalculateMonochromeBitsPerComponent(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetPage());
  const display::ScreenInfo& screen_info =
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame);
  if (!screen_info.is_monochrome) {
    return 0;
  }
  return screen_info.depth_per_component;
}

bool MediaValues::CalculateInvertedColors(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetInvertedColors();
}

float MediaValues::CalculateEmSize(LocalFrame* frame) {
  CHECK(frame);
  CHECK(frame->ContentLayoutObject());
  const ComputedStyle& style = frame->ContentLayoutObject()->StyleRef();
  return CSSToLengthConversionData::FontSizes(style.GetFontSizeStyle(), &style)
      .Em(/* zoom */ 1.0f);
}

float MediaValues::CalculateExSize(LocalFrame* frame) {
  CHECK(frame);
  CHECK(frame->ContentLayoutObject());
  const ComputedStyle& style = frame->ContentLayoutObject()->StyleRef();
  return CSSToLengthConversionData::FontSizes(style.GetFontSizeStyle(), &style)
      .Ex(/* zoom */ 1.0f);
}

float MediaValues::CalculateChSize(LocalFrame* frame) {
  CHECK(frame);
  CHECK(frame->ContentLayoutObject());
  const ComputedStyle& style = frame->ContentLayoutObject()->StyleRef();
  return CSSToLengthConversionData::FontSizes(style.GetFontSizeStyle(), &style)
      .Ch(/* zoom */ 1.0f);
}

float MediaValues::CalculateIcSize(LocalFrame* frame) {
  CHECK(frame);
  CHECK(frame->ContentLayoutObject());
  const ComputedStyle& style = frame->ContentLayoutObject()->StyleRef();
  return CSSToLengthConversionData::FontSizes(style.GetFontSizeStyle(), &style)
      .Ic(/* zoom */ 1.0f);
}

float MediaValues::CalculateCapSize(LocalFrame* frame) {
  CHECK(frame);
  CHECK(frame->ContentLayoutObject());
  const ComputedStyle& style = frame->ContentLayoutObject()->StyleRef();
  return CSSToLengthConversionData::FontSizes(style.GetFontSizeStyle(), &style)
      .Cap(/* zoom */ 1.0f);
}

float MediaValues::CalculateLineHeight(LocalFrame* frame) {
  CHECK(frame);
  CHECK(frame->ContentLayoutObject());
  const ComputedStyle& style = frame->ContentLayoutObject()->StyleRef();
  return AdjustForAbsoluteZoom::AdjustFloat(style.ComputedLineHeight(), style);
}

const String MediaValues::CalculateMediaType(LocalFrame* frame) {
  DCHECK(frame);
  if (!frame->View()) {
    return g_empty_atom;
  }
  return frame->View()->MediaType();
}

mojom::blink::DisplayMode MediaValues::CalculateDisplayMode(LocalFrame* frame) {
  DCHECK(frame);

  blink::mojom::DisplayMode mode =
      frame->GetPage()->GetSettings().GetDisplayModeOverride();
  if (mode != mojom::blink::DisplayMode::kUndefined) {
    return mode;
  }

  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  if (!widget) {  // Is null in non-ordinary Pages.
    return mojom::blink::DisplayMode::kBrowser;
  }

  return widget->DisplayMode();
}

ui::mojom::blink::WindowShowState MediaValues::CalculateWindowShowState(
    LocalFrame* frame) {
  DCHECK(frame);

  ui::mojom::blink::WindowShowState show_state =
      frame->GetPage()->GetSettings().GetWindowShowState();
  // Initial state set in /third_party/blink/renderer/core/frame/settings.json5
  // should match with this.
  if (show_state != ui::mojom::blink::WindowShowState::kDefault) {
    return show_state;
  }

  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  if (!widget) {  // Is null in non-ordinary Pages.
    return ui::mojom::blink::WindowShowState::kDefault;
  }

  return widget->WindowShowState();
}

bool MediaValues::CalculateResizable(LocalFrame* frame) {
  DCHECK(frame);

  bool resizable = frame->GetPage()->GetSettings().GetResizable();
  // Initial state set in /third_party/blink/renderer/core/frame/settings.json5
  // should match with this.
  if (!resizable) {
    // Only non-default value should be returned "early" from the settings
    // without checking from widget. Settings are only used for testing.
    return resizable;
  }

  FrameWidget* widget = frame->GetWidgetForLocalRoot();
  if (!widget) {
    return true;
  }

  return widget->Resizable();
}

bool MediaValues::CalculateThreeDEnabled(LocalFrame* frame) {
  return frame->GetPage()->GetSettings().GetAcceleratedCompositingEnabled();
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

mojom::blink::OutputDeviceUpdateAbilityType
MediaValues::CalculateOutputDeviceUpdateAbilityType(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetOutputDeviceUpdateAbilityType();
}

int MediaValues::CalculateAvailableHoverTypes(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetAvailableHoverTypes();
}

ColorSpaceGamut MediaValues::CalculateColorGamut(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetPage());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<ColorSpaceGamut> override_value =
      overrides ? overrides->GetColorGamut() : std::nullopt;
  return override_value.value_or(color_space_utilities::GetColorSpaceGamut(
      frame->GetPage()->GetChromeClient().GetScreenInfo(*frame)));
}

mojom::blink::PreferredColorScheme MediaValues::CalculatePreferredColorScheme(
    LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  DCHECK(frame->GetDocument());
  DCHECK(frame->GetPage());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<mojom::blink::PreferredColorScheme> override_value =
      overrides ? overrides->GetPreferredColorScheme() : std::nullopt;
  if (override_value.has_value()) {
    return override_value.value();
  }

  const PreferenceOverrides* preference_overrides =
      frame->GetPage()->GetPreferenceOverrides();
  std::optional<mojom::blink::PreferredColorScheme> preference_override_value =
      preference_overrides ? preference_overrides->GetPreferredColorScheme()
                           : std::nullopt;
  return preference_override_value.value_or(
      frame->GetDocument()->GetStyleEngine().GetPreferredColorScheme());
}

mojom::blink::PreferredContrast MediaValues::CalculatePreferredContrast(
    LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  DCHECK(frame->GetPage());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<mojom::blink::PreferredContrast> override_value =
      overrides ? overrides->GetPreferredContrast() : std::nullopt;
  if (override_value.has_value()) {
    return override_value.value();
  }

  const PreferenceOverrides* preference_overrides =
      frame->GetPage()->GetPreferenceOverrides();
  std::optional<mojom::blink::PreferredContrast> preference_override_value =
      preference_overrides ? preference_overrides->GetPreferredContrast()
                           : std::nullopt;
  return preference_override_value.value_or(
      frame->GetSettings()->GetPreferredContrast());
}

bool MediaValues::CalculatePrefersReducedMotion(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<bool> override_value =
      overrides ? overrides->GetPrefersReducedMotion() : std::nullopt;
  if (override_value.has_value()) {
    return override_value.value();
  }

  const PreferenceOverrides* preference_overrides =
      frame->GetPage()->GetPreferenceOverrides();
  std::optional<bool> preference_override_value =
      preference_overrides ? preference_overrides->GetPrefersReducedMotion()
                           : std::nullopt;
  return preference_override_value.value_or(
      frame->GetSettings()->GetPrefersReducedMotion());
}

bool MediaValues::CalculatePrefersReducedData(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<bool> override_value =
      overrides ? overrides->GetPrefersReducedData() : std::nullopt;
  if (override_value.has_value()) {
    return override_value.value();
  }

  const PreferenceOverrides* preference_overrides =
      frame->GetPage()->GetPreferenceOverrides();
  std::optional<bool> preference_override_value =
      preference_overrides ? preference_overrides->GetPrefersReducedData()
                           : std::nullopt;
  return preference_override_value.value_or(
      GetNetworkStateNotifier().SaveDataEnabled());
}

bool MediaValues::CalculatePrefersReducedTransparency(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<bool> override_value =
      overrides ? overrides->GetPrefersReducedTransparency() : std::nullopt;
  if (override_value.has_value()) {
    return override_value.value();
  }

  const PreferenceOverrides* preference_overrides =
      frame->GetPage()->GetPreferenceOverrides();
  std::optional<bool> preference_override_value =
      preference_overrides
          ? preference_overrides->GetPrefersReducedTransparency()
          : std::nullopt;
  return preference_override_value.value_or(
      frame->GetSettings()->GetPrefersReducedTransparency());
}

ForcedColors MediaValues::CalculateForcedColors(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  const MediaFeatureOverrides* overrides =
      frame->GetPage()->GetMediaFeatureOverrides();
  std::optional<ForcedColors> override_value =
      overrides ? overrides->GetForcedColors() : std::nullopt;
  return override_value.value_or(
      frame->GetDocument()->GetStyleEngine().GetForcedColors());
}

NavigationControls MediaValues::CalculateNavigationControls(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetSettings());
  return frame->GetSettings()->GetNavigationControls();
}

int MediaValues::CalculateHorizontalViewportSegments(LocalFrame* frame) {
  if (!frame->GetWidgetForLocalRoot()) {
    return 1;
  }

  WebVector<gfx::Rect> viewport_segments =
      frame->GetWidgetForLocalRoot()->ViewportSegments();
  WTF::HashSet<int> unique_x;
  for (const auto& segment : viewport_segments) {
    // HashSet can't have 0 as a key, so add 1 to all the values we see.
    unique_x.insert(segment.x() + 1);
  }

  return static_cast<int>(unique_x.size());
}

int MediaValues::CalculateVerticalViewportSegments(LocalFrame* frame) {
  if (!frame->GetWidgetForLocalRoot()) {
    return 1;
  }

  WebVector<gfx::Rect> viewport_segments =
      frame->GetWidgetForLocalRoot()->ViewportSegments();
  WTF::HashSet<int> unique_y;
  for (const auto& segment : viewport_segments) {
    // HashSet can't have 0 as a key, so add 1 to all the values we see.
    unique_y.insert(segment.y() + 1);
  }

  return static_cast<int>(unique_y.size());
}

mojom::blink::DevicePostureType MediaValues::CalculateDevicePosture(
    LocalFrame* frame) {
  return frame->GetDevicePosture();
}

Scripting MediaValues::CalculateScripting(LocalFrame* frame) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  if (!frame->GetDocument()->GetExecutionContext()->CanExecuteScripts(
          kNotAboutToExecuteScript)) {
    return Scripting::kNone;
  }

  return Scripting::kEnabled;
}

bool MediaValues::ComputeLengthImpl(double value,
                                    CSSPrimitiveValue::UnitType type,
                                    double& result) const {
  if (!CSSPrimitiveValue::IsLength(type)) {
    return false;
  }
  result = ZoomedComputedPixels(value, type);
  return true;
}

}  // namespace blink
