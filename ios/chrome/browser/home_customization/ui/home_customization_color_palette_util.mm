// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_util.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"
#import "skia/ext/skia_utils_ios.h"
#import "third_party/material_color_utilities/src/cpp/cam/hct.h"
#import "third_party/material_color_utilities/src/cpp/palettes/core.h"
#import "third_party/material_color_utilities/src/cpp/utils/utils.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/color/dynamic_color/palette_factory.h"

// Define constants within the namespace
namespace {

// A block type that provides a dynamic color based on the current trait
// collection.
typedef UIColor* (^DynamicColorProviderBlock)(UITraitCollection* traits);

// Represents a pair of tone values for a given color tone,
// with separate values for light and dark UI modes.
struct ToneSet {
  // The tone value to use in light mode.
  int light_mode;

  // The tone value to use in dark mode.
  int dark_mode;
};

// The tone value used for generating a light variant of the seed color.
const ToneSet kLightTone = {
    /*light_mode=*/90,
    /*dark_mode=*/30,
};

// The tone value used for generating a medium variant of the seed color.
const ToneSet kMediumTone = {
    /*light_mode=*/80,
    /*dark_mode=*/50,
};

// The tone value used for generating a dark variant of the seed color.
const ToneSet kDarkTone = {
    /*light_mode=*/40,
    /*dark_mode=*/80,
};

// Returns a dynamic `UIColor` that adapts to the system's light or dark
// appearance using tones derived from the given `TonalPalette`.
DynamicColorProviderBlock GetDynamicProviderForPrimary(
    const ui::TonalPalette& primary,
    const ToneSet& toneSet) {
  uint32_t lightARGB = primary.get(toneSet.light_mode);
  uint32_t darkARGB = primary.get(toneSet.dark_mode);

  return ^UIColor*(UITraitCollection* traits) {
    BOOL isDark = (traits.userInterfaceStyle == UIUserInterfaceStyleDark);
    return skia::UIColorFromSkColor(isDark ? darkARGB : lightARGB);
  };
}

}  // namespace

// Creates and returns a color palette configuration from a seed color.
HomeCustomizationColorPaletteConfiguration*
CreateColorPaletteConfigurationFromSeedColor(UIColor* seed_color) {
  if (!seed_color) {
    return nil;
  }

  HomeCustomizationColorPaletteConfiguration* config =
      [[HomeCustomizationColorPaletteConfiguration alloc] init];

  CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
  [seed_color getRed:&red green:&green blue:&blue alpha:&alpha];

  SkColor sk_color = SkColorSetARGB(
      static_cast<U8CPU>(alpha * 255.0), static_cast<U8CPU>(red * 255.0),
      static_cast<U8CPU>(green * 255.0), static_cast<U8CPU>(blue * 255.0));

  std::unique_ptr<ui::Palette> palette = ui::GeneratePalette(
      sk_color, ui::ColorProviderKey::SchemeVariant::kTonalSpot);
  const ui::TonalPalette& primary = palette->primary();

  config.seedColor = seed_color;
  config.lightColor =
      [UIColor colorWithDynamicProvider:GetDynamicProviderForPrimary(
                                            primary, kLightTone)];
  config.mediumColor =
      [UIColor colorWithDynamicProvider:GetDynamicProviderForPrimary(
                                            primary, kMediumTone)];
  config.darkColor =
      [UIColor colorWithDynamicProvider:GetDynamicProviderForPrimary(
                                            primary, kDarkTone)];

  return config;
}
