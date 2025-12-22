// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "skia/ext/skia_utils_ios.h"

// Creates and returns a color palette from a seed color.
NewTabPageColorPalette* CreateColorPaletteFromSeedColor(
    UIColor* seed_color,
    ui::ColorProviderKey::SchemeVariant variant) {
  if (!seed_color) {
    return nil;
  }

  NewTabPageColorPalette* ntp_palette = [[NewTabPageColorPalette alloc] init];

  std::unique_ptr<ui::Palette> palette =
      ui::GeneratePalette(skia::UIColorToSkColor(seed_color), variant);
  const ui::TonalPalette& primary = palette->primary();
  const ui::TonalPalette& secondary = palette->secondary();

  ntp_palette.seedColor = seed_color;
  ntp_palette.variant = variant;

  ntp_palette.lightColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 90)),
                                   FromTone(PaletteTone(primary, 30)))];

  ntp_palette.mediumColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 80)),
                                   FromTone(PaletteTone(primary, 50)))];

  ntp_palette.darkColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(primary, 80)))];

  ntp_palette.tintColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(primary, 90)))];
  ntp_palette.primaryColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 98)),
                                   FromTone(PaletteTone(secondary, 20)))];
  ntp_palette.secondaryCellColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromColor(UIColor.whiteColor),
                                   FromTone(PaletteTone(secondary, 10)))];

  ntp_palette.secondaryColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 95)),
                                   FromTone(PaletteTone(secondary, 10)))];

  ntp_palette.tertiaryColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 95)),
                                   FromTone(PaletteTone(secondary, 30)))];

  ntp_palette.omniboxColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 90)),
                                   FromTone(PaletteTone(secondary, 40)))];

  ntp_palette.omniboxIconColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(secondary, 80)))];

  ntp_palette.omniboxIconDividerColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 70)),
                                   FromTone(PaletteTone(secondary, 60)))];

  ntp_palette.monogramColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(primary, 40)),
                                   FromTone(PaletteTone(primary, 80)))];

  ntp_palette.headerButtonColor = [UIColor
      colorWithDynamicProvider:GetDynamicProvider(
                                   FromTone(PaletteTone(secondary, 95)),
                                   FromTone(PaletteTone(secondary, 30)))];

  return ntp_palette;
}
