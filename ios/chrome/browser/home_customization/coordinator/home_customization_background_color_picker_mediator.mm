// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "skia/ext/skia_utils_ios.h"

namespace {

// Represents a seed color and its associated scheme variant.
struct SeedColor {
  SkColor color;
  ui::ColorProviderKey::SchemeVariant variant;
};

// Array of seed colors (in ARGB integer format) and variants used to generate
// background color palette configurations in the color picker.
const SeedColor kSeedColors[] = {
    {0xff8cabe4, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Blue
    {0xff26a69a, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Aqua
    {0xff00ff00, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Green
    {0xff87ba81, ui::ColorProviderKey::SchemeVariant::kNeutral},    // Viridian
    {0xfffadf73, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Citron
    {0xffff8000, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Orange
    {0xfff3b2be, ui::ColorProviderKey::SchemeVariant::kNeutral},    // Rose
    {0xffff00ff, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Fuchsia
    {0xffe5d5fc, ui::ColorProviderKey::SchemeVariant::kTonalSpot},  // Violet
};

// Returns a dynamic UIColor using two named color assets for light and dark
// mode.
UIColor* DynamicNamedColor(NSString* lightName, NSString* darkName) {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        BOOL isDark = (traits.userInterfaceStyle == UIUserInterfaceStyleDark);
        return [UIColor colorNamed:isDark ? darkName : lightName];
      }];
}

}  // namespace

@implementation HomeCustomizationBackgroundColorPickerMediator {
  // Used to get and observe the background state.
  raw_ptr<HomeBackgroundCustomizationService> _backgroundCustomizationService;
}

- (instancetype)initWithBackgroundCustomizationService:
    (HomeBackgroundCustomizationService*)backgroundCustomizationService {
  self = [super init];
  if (self) {
    _backgroundCustomizationService = backgroundCustomizationService;
  }

  return self;
}
- (void)configureColorPalettes {
  NSMutableArray* colorPalettes = [NSMutableArray array];
  std::optional<sync_pb::UserColorTheme> colorTheme =
      _backgroundCustomizationService->GetCurrentColorTheme();
  NSNumber* selectedColorIndex = nil;

  NewTabPageColorPalette* defaultColorPalette =
      [[NewTabPageColorPalette alloc] init];

  // The first choice should be the "no background" option (default appearance
  // colors).
  defaultColorPalette.lightColor =
      DynamicNamedColor(@"ntp_background_color", kGrey100Color);
  defaultColorPalette.mediumColor =
      [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
  defaultColorPalette.darkColor =
      DynamicNamedColor(kBlueColor, kTextPrimaryColor);

  [colorPalettes addObject:defaultColorPalette];

  for (SeedColor seedColor : kSeedColors) {
    [colorPalettes
        addObject:CreateColorPaletteFromSeedColor(
                      UIColorFromRGB(seedColor.color), seedColor.variant)];

    if (colorTheme && colorTheme->color() &&
        seedColor.color == colorTheme->color()) {
      selectedColorIndex = @(colorPalettes.count - 1);
    }
  }

  // If no color is currently selected, set selectedColorIndex to nil
  // when a background image is active, or to 0 when there is no background.
  if (!selectedColorIndex) {
    selectedColorIndex =
        _backgroundCustomizationService->GetCurrentCustomBackground() ||
                _backgroundCustomizationService
                    ->GetCurrentUserUploadedBackground()
            ? nil
            : @(0);
  }

  [_consumer setColorPalettes:colorPalettes
           selectedColorIndex:selectedColorIndex];
}

@end
