// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_style_utils.h"

#import <algorithm>
#import <string_view>

#import "base/logging.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/prefs/pref_service.h"

namespace {

constexpr int kMinOpacityPercent = 0;
constexpr int kMaxOpacityPercent = 100;
constexpr double kPercentFactor = 100.0;

constexpr int kMinFontSize = 1;
constexpr int kMaxFontSize = 500;

// Clamps `value` to [`min`, `max`]. Logs a warning if `value` falls outside
// the safe range.
int ClampAndLog(int value,
                int min,
                int max,
                std::string_view property_name,
                std::string_view unit) {
  if (value < min || value > max) {
    DLOG(WARNING) << "Invalid watermark " << property_name << ": " << value
                  << unit << ". Clamping to safe range [" << min << ", " << max
                  << "].";
    return std::clamp(value, min, max);
  }
  return value;
}

}  // namespace

WatermarkStyle GetWatermarkStyleFromPrefs(const PrefService* pref_service) {
  WatermarkStyle style;
  if (!pref_service) {
    return style;
  }

  int fill_opacity_percent =
      ClampAndLog(pref_service->GetInteger(
                      enterprise_connectors::kWatermarkStyleFillOpacityPref),
                  kMinOpacityPercent, kMaxOpacityPercent, "fill opacity", "%");
  style.fill_opacity = fill_opacity_percent / kPercentFactor;

  int outline_opacity_percent = ClampAndLog(
      pref_service->GetInteger(
          enterprise_connectors::kWatermarkStyleOutlineOpacityPref),
      kMinOpacityPercent, kMaxOpacityPercent, "outline opacity", "%");
  style.outline_opacity = outline_opacity_percent / kPercentFactor;

  style.font_size =
      ClampAndLog(pref_service->GetInteger(
                      enterprise_connectors::kWatermarkStyleFontSizePref),
                  kMinFontSize, kMaxFontSize, "font size", "pt");

  return style;
}
