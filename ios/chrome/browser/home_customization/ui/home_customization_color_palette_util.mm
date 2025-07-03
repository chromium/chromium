// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_util.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
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

// Represents a tone value tied to a specific TonalPalette.
struct PaletteTone {
  raw_ptr<const ui::TonalPalette> palette;
  int tone;

  PaletteTone(const ui::TonalPalette& associatedPalette, int toneValue)
      : palette(&associatedPalette), tone(toneValue) {}

  SkColor color() const { return palette->get(tone); }
};

// Represents a color input that can be either a palette-based tone or a fixed
// UIColor. Only one of `tone` or `fixedColor` should be set. If neither is set,
// `resolveColor()` returns opaque black (`SK_ColorBLACK`) as a fallback.
struct DynamicColorInput {
  std::optional<PaletteTone> tone;
  std::optional<UIColor*> fixedColor;

  explicit DynamicColorInput(const PaletteTone& t) : tone(t) {}
  explicit DynamicColorInput(UIColor* color) : fixedColor(color) {}

  SkColor resolveColor() const {
    if (tone.has_value()) {
      return tone->color();
    }
    if (fixedColor.has_value()) {
      return skia::UIColorToSkColor(fixedColor.value());
    }
    return SK_ColorBLACK;
  }
};

// Factory helper for creating DynamicColorInput from a tone.
inline DynamicColorInput FromTone(const PaletteTone& tone) {
  return DynamicColorInput(tone);
}

// Factory helper for creating DynamicColorInput from a fixed UIColor.
inline DynamicColorInput FromColor(UIColor* color) {
  return DynamicColorInput(color);
}

// Returns a dynamic `UIColor` that adapts to the system's light or dark
// appearance using tones derived from the given `DynamicColorInput`.
DynamicColorProviderBlock GetDynamicProvider(
    const DynamicColorInput& lightInput,
    const DynamicColorInput& darkInput) {
  uint32_t lightARGB = lightInput.resolveColor();
  uint32_t darkARGB = darkInput.resolveColor();

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

  std::unique_ptr<ui::Palette> palette =
      ui::GeneratePalette(skia::UIColorToSkColor(seed_color),
                          ui::ColorProviderKey::SchemeVariant::kTonalSpot);
  const ui::TonalPalette& primary = palette->primary();
  const ui::TonalPalette& secondary = palette->secondary();

  config.seedColor = seed_color;

  config.lightColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 90)),
                                   FromTone(PaletteTone(primary, 30)))];

  config.mediumColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 80)),
                                   FromTone(PaletteTone(primary, 50)))];

  config.darkColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(primary, 80)))];

  config.tintColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(primary, 90)))];
  config.primaryColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 98)),
                                   FromTone(PaletteTone(secondary, 20)))];
  config.secondaryCellColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromColor(UIColor.whiteColor),
                                   FromTone(PaletteTone(secondary, 10)))];

  config.secondaryColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 95)),
                                   FromTone(PaletteTone(secondary, 10)))];

  config.tertiaryColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 95)),
                                   FromTone(PaletteTone(secondary, 30)))];

  config.omniboxColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 90)),
                                   FromTone(PaletteTone(secondary, 40)))];

  config.omniboxIconColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(secondary, 80)))];

  config.omniboxIconDividerColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 70)),
                                   FromTone(PaletteTone(secondary, 60)))];

  return config;
}
