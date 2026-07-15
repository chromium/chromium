// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
// Constants for regular (non-glass) toolbar element shadows.
constexpr CGFloat kShadowOpacity = 0.12;
constexpr CGFloat kShadowYOffset = 1;

// Constants for glass prototype element background colors.
constexpr CGFloat kGlassElementDarkAlpha = 0.50;
constexpr CGFloat kGlassElementLightAlpha = 0.80;

// Constants for glass prototype element shadows in dark and incognito modes.
constexpr CGFloat kGlassShadowDarkOpacity = 0.05;
constexpr CGFloat kGlassShadowDarkRadius = 10.0;

// Constants for glass prototype element shadows in light mode.
constexpr CGFloat kGlassShadowLightOpacity = 0.16;
constexpr CGFloat kGlassShadowLightRadius = 8.0;

// Shared vertical shadow offset for elements in the glass prototype.
constexpr CGFloat kGlassShadowVerticalOffset = 8.0;
}  // namespace

UIColor* ToolbarElementBackgroundColor(BOOL incognito) {
  if (incognito) {
    return [UIColor colorNamed:kStaticGrey900Color];
  }
  return [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
          return [UIColor colorNamed:kStaticGrey700Color];
        }
        return [UIColor colorNamed:kStaticGrey300Color];
      }];
}

UIColor* ToolbarGlassPrototypeElementBackgroundColor(BOOL incognito) {
  CHECK(IsToolbarGlassPrototypeEnabled());
  if (incognito) {
    return [UIColor.blackColor colorWithAlphaComponent:kGlassElementDarkAlpha];
  }
  return [UIColor colorWithDynamicProvider:^UIColor*(
                      UITraitCollection* traitCollection) {
    if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
      return
          [UIColor.blackColor colorWithAlphaComponent:kGlassElementDarkAlpha];
    }
    return [UIColor.whiteColor colorWithAlphaComponent:kGlassElementLightAlpha];
  }];
}

void ConfigureGlassToolbarElementShadow(UIView* container,
                                        BOOL incognito,
                                        UITraitCollection* trait_collection) {
  CHECK(IsToolbarGlassPrototypeEnabled());
  BOOL isDark = incognito || (trait_collection.userInterfaceStyle ==
                              UIUserInterfaceStyleDark);
  container.layer.shadowColor =
      isDark ? UIColor.whiteColor.CGColor : UIColor.blackColor.CGColor;
  container.layer.shadowOpacity =
      isDark ? kGlassShadowDarkOpacity : kGlassShadowLightOpacity;
  container.layer.shadowRadius =
      isDark ? kGlassShadowDarkRadius : kGlassShadowLightRadius;
  container.layer.shadowOffset = CGSizeMake(0, kGlassShadowVerticalOffset);
}

void ConfigureShadowForToolbarElement(UIView* container, BOOL remove_shadow) {
  container.layer.shadowColor =
      remove_shadow ? nil : UIColor.whiteColor.CGColor;
  container.layer.shadowOpacity = remove_shadow ? 0.0 : kShadowOpacity;
  container.layer.shadowOffset =
      remove_shadow ? CGSizeZero : CGSizeMake(0, kShadowYOffset);
  container.layer.shadowRadius = 0;
}

void ConfigureCornerRadiusForToolbarButtonContainer(
    UIView* container,
    UITraitCollection* trait_collection) {
  // Always enforce circular or capsule pill shapes under the glass prototype.
  if (IsToolbarGlassPrototypeEnabled()) {
    container.layer.cornerRadius = container.bounds.size.height > 0
                                       ? container.bounds.size.height / 2.0
                                       : kToolbarButtonSize / 2.0;
    return;
  }

  // Whether the window has a regular height x compact width size class,
  // corresponding to iPhone portrait mode or a skinny iPad window.
  BOOL isRegularXCompactSizeClass =
      !IsCompactHeight(trait_collection) && IsCompactWidth(trait_collection);
  container.layer.cornerRadius = isRegularXCompactSizeClass
                                     ? kToolbarButtonSquareCornerRadius
                                     : kToolbarButtonSize / 2;
}
