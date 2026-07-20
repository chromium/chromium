// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/features.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

UIImage* CreatePreviewImageForColorPalette(
    const NewTabPageColorPalette* color_palette,
    CGFloat size,
    UITraitCollection* trait_collection) {
  CHECK(IsOverflowMenuHomeCustomizationEntrypointEnabled());
  if (!color_palette) {
    return nil;
  }
  const CGSize kPreviewSize = CGSizeMake(size, size);
  const CGSize kPreviewQuadrantSize = CGSizeMake(size / 2.0, size / 2.0);

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:kPreviewSize format:format];

  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    CGContextRef ctx = context.CGContext;

    // Resolve dynamic colors using the provided trait collection.
    UITraitCollection* resolved_trait_collection =
        trait_collection ?: [UITraitCollection currentTraitCollection];
    UIColor* light_color = [color_palette.lightColor
        resolvedColorWithTraitCollection:resolved_trait_collection];
    UIColor* medium_color = [color_palette.mediumColor
        resolvedColorWithTraitCollection:resolved_trait_collection];
    UIColor* dark_color = [color_palette.darkColor
        resolvedColorWithTraitCollection:resolved_trait_collection];

    // Top half rectangle.
    [light_color setFill];
    CGContextFillRect(
        ctx, CGRectMake(0, 0, kPreviewSize.width, kPreviewQuadrantSize.height));

    // Bottom-left quadrant square.
    [medium_color setFill];
    CGContextFillRect(ctx, CGRectMake(0, kPreviewQuadrantSize.height,
                                      kPreviewQuadrantSize.width,
                                      kPreviewQuadrantSize.height));

    // Bottom-right quadrant square.
    [dark_color setFill];
    CGContextFillRect(
        ctx,
        CGRectMake(kPreviewQuadrantSize.width, kPreviewQuadrantSize.height,
                   kPreviewQuadrantSize.width, kPreviewQuadrantSize.height));
  }];
}

NewTabPageColorPalette* DefaultNTPColorPalette() {
  CHECK(IsOverflowMenuHomeCustomizationEntrypointEnabled());
  NewTabPageColorPalette* color_palette = [[NewTabPageColorPalette alloc] init];
  color_palette.lightColor = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* trait_collection) {
        BOOL isDark =
            (trait_collection.userInterfaceStyle == UIUserInterfaceStyleDark);
        return [UIColor
            colorNamed:isDark ? kGrey100Color : @"ntp_background_color"];
      }];
  color_palette.mediumColor =
      [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
  color_palette.darkColor = [UIColor colorNamed:kBlueColor];
  return color_palette;
}
