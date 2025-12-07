// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_customization_seed_colors.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/color/color_provider_key.h"

const std::array<SeedColor, kSeedColorsCount> kSeedColors = {{
    {0xff8cabe4, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_BLUE_ACCESSIBILITY_LABEL},  // Blue
    {0xff26a69a, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_AQUA_ACCESSIBILITY_LABEL},  // Aqua
    {0xff00ff00, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_GREEN_ACCESSIBILITY_LABEL},  // Green
    {0xff87ba81, ui::ColorProviderKey::SchemeVariant::kNeutral,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_VIRIDIAN_ACCESSIBILITY_LABEL},  // Viridian
    {0xfffadf73, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_CITRON_ACCESSIBILITY_LABEL},  // Citron
    {0xffff8000, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_ORANGE_ACCESSIBILITY_LABEL},  // Orange
    {0xfff3b2be, ui::ColorProviderKey::SchemeVariant::kNeutral,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_ROSE_ACCESSIBILITY_LABEL},  // Rose
    {0xffff00ff, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_FUCHSIA_ACCESSIBILITY_LABEL},  // Fuchsia
    {0xffe5d5fc, ui::ColorProviderKey::SchemeVariant::kTonalSpot,
     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_VIOLET_ACCESSIBILITY_LABEL},  // Violet
}};
